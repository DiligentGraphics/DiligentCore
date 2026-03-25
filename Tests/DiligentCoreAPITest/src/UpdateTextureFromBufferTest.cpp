/*
 *  Copyright 2026 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include "GPUTestingEnvironment.hpp"
#include "FastRand.hpp"

#include "gtest/gtest.h"

#include <vector>

using namespace Diligent;
using namespace Diligent::Testing;


namespace
{

struct UploadRegion
{
    Uint32 Slice          = 0;
    Uint32 Mip            = 0;
    Uint32 DstX           = 0;
    Uint32 DstY           = 0;
    Uint32 DstZ           = 0;
    Uint32 Width          = 0;
    Uint32 Height         = 0;
    Uint32 Depth          = 1;
    Uint64 SrcOffset      = 0;
    Uint32 SrcStride      = 0;
    Uint32 SrcDepthStride = 0;
};

struct AxisSplit
{
    Uint32 Offset = 0;
    Uint32 Size   = 0;
};

Uint32 GetMipDimension(Uint32 BaseDim, Uint32 Mip)
{
    return std::max(BaseDim >> Mip, 1u);
}

AxisSplit GetAxisSplit(Uint32 Dim, Uint32 Split)
{
    if (Dim <= 1)
        return {0, 1};

    const Uint32 Half = Dim / 2;
    return Split == 0 ?
        AxisSplit{0, Half} :
        AxisSplit{Half, Dim - Half};
}

TextureDesc CreateTestTextureDesc(const char*        Name,
                                  RESOURCE_DIMENSION Type,
                                  Uint32             Width,
                                  Uint32             Height,
                                  Uint32             ArraySizeOrDepth,
                                  Uint32             MipLevels)
{
    TextureDesc TexDesc;
    TexDesc.Name      = Name;
    TexDesc.Type      = Type;
    TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
    TexDesc.BindFlags = BIND_SHADER_RESOURCE;
    TexDesc.Width     = Width;
    TexDesc.Height    = Height;
    TexDesc.MipLevels = MipLevels;
    TexDesc.Usage     = USAGE_DEFAULT;

    if (Type == RESOURCE_DIM_TEX_3D)
        TexDesc.Depth = ArraySizeOrDepth;
    else
        TexDesc.ArraySize = ArraySizeOrDepth;

    return TexDesc;
}

template <typename Fn>
void ForEachSubresource(const TextureDesc& TexDesc, Fn&& Callback)
{
    const Uint32 NumSlices = TexDesc.IsArray() ? TexDesc.ArraySize : 1u;

    for (Uint32 mip = 0; mip < TexDesc.MipLevels; ++mip)
    {
        const Uint32 MipWidth  = GetMipDimension(TexDesc.Width, mip);
        const Uint32 MipHeight = GetMipDimension(TexDesc.Height, mip);
        const Uint32 MipDepth  = TexDesc.Is3D() ? GetMipDimension(TexDesc.Depth, mip) : 1u;

        for (Uint32 slice = 0; slice < NumSlices; ++slice)
            Callback(mip, slice, MipWidth, MipHeight, MipDepth);
    }
}

void RunCopyBufferToTextureTest(const TextureDesc& BaseTexDesc)
{
    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();
    if (pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D11)
    {
        GTEST_SKIP() << "Updating texture from buffer is not supported in D3D11.";
    }

    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;
    IDeviceContext*                    pContext = pEnv->GetDeviceContext();

    const Uint32 TextureUpdateOffsetAlignment = pDevice->GetAdapterInfo().Buffer.TextureUpdateOffsetAlignment;
    const Uint32 TextureUpdateStrideAlignment = pDevice->GetAdapterInfo().Buffer.TextureUpdateStrideAlignment;
    const Uint32 NumSlices                    = BaseTexDesc.IsArray() ? BaseTexDesc.ArraySize : 1u;

    for (USAGE BufferUsage : {USAGE_DEFAULT, USAGE_STAGING})
    {
        RefCntAutoPtr<ITexture> pTexture;
        pDevice->CreateTexture(BaseTexDesc, nullptr, &pTexture);
        ASSERT_NE(pTexture, nullptr);

        TextureDesc StagingTexDesc    = BaseTexDesc;
        StagingTexDesc.Name           = "UpdateTextureFromBuffer.StagingTexture";
        StagingTexDesc.Usage          = USAGE_STAGING;
        StagingTexDesc.BindFlags      = BIND_NONE;
        StagingTexDesc.CPUAccessFlags = CPU_ACCESS_READ;

        RefCntAutoPtr<ITexture> pStagingTexture;
        pDevice->CreateTexture(StagingTexDesc, nullptr, &pStagingTexture);
        ASSERT_NE(pStagingTexture, nullptr);

        std::vector<std::vector<Uint32>> RefSubresources(BaseTexDesc.MipLevels * NumSlices);
        std::vector<Uint8>               UploadData;
        std::vector<UploadRegion>        UploadRegions;

        auto GetSubresourceIndex = [NumSlices](Uint32 Mip, Uint32 Slice) {
            return Mip * NumSlices + Slice;
        };

        FastRandInt rnd{0, 0, 32766};

        ForEachSubresource(BaseTexDesc, [&](Uint32 mip, Uint32 slice, Uint32 MipWidth, Uint32 MipHeight, Uint32 MipDepth) {
            auto& Subres = RefSubresources[GetSubresourceIndex(mip, slice)];
            Subres.resize(static_cast<size_t>(MipWidth) * MipHeight * MipDepth);

            for (Uint32& pixel : Subres)
                pixel = rnd() | (rnd() << 16);

            const Uint32 NumXSplits = MipWidth > 1 ? 2u : 1u;
            const Uint32 NumYSplits = MipHeight > 1 ? 2u : 1u;
            const Uint32 NumZSplits = MipDepth > 1 ? 2u : 1u;

            for (Uint32 zSplit = 0; zSplit < NumZSplits; ++zSplit)
            {
                const AxisSplit Z = GetAxisSplit(MipDepth, zSplit);

                for (Uint32 ySplit = 0; ySplit < NumYSplits; ++ySplit)
                {
                    const AxisSplit Y = GetAxisSplit(MipHeight, ySplit);

                    for (Uint32 xSplit = 0; xSplit < NumXSplits; ++xSplit)
                    {
                        const AxisSplit X = GetAxisSplit(MipWidth, xSplit);

                        const Uint64 AlignedOffset = AlignUp(static_cast<Uint64>(UploadData.size()),
                                                             static_cast<Uint64>(TextureUpdateOffsetAlignment));
                        UploadData.resize(static_cast<size_t>(AlignedOffset));

                        const Uint32 Stride      = AlignUp(MipWidth * Uint32{sizeof(Uint32)}, TextureUpdateStrideAlignment);
                        const Uint32 DepthStride = Y.Size * Stride;
                        const Uint64 SrcOffset   = static_cast<Uint64>(UploadData.size());
                        const Uint64 RegionSize  = static_cast<Uint64>(Z.Size) * DepthStride;

                        UploadData.resize(static_cast<size_t>(SrcOffset + RegionSize));

                        for (Uint32 z = 0; z < Z.Size; ++z)
                        {
                            for (Uint32 row = 0; row < Y.Size; ++row)
                            {
                                const Uint32* pSrcRow =
                                    &Subres[((Z.Offset + z) * MipHeight + (Y.Offset + row)) * MipWidth + X.Offset];

                                Uint8* pDstRow =
                                    UploadData.data() +
                                    static_cast<size_t>(SrcOffset) +
                                    static_cast<size_t>(z) * DepthStride +
                                    static_cast<size_t>(row) * Stride;

                                memcpy(pDstRow, pSrcRow, X.Size * sizeof(Uint32));
                            }
                        }

                        UploadRegions.push_back(
                            UploadRegion{
                                slice,
                                mip,
                                X.Offset,
                                Y.Offset,
                                Z.Offset,
                                X.Size,
                                Y.Size,
                                Z.Size,
                                SrcOffset,
                                Stride,
                                DepthStride});
                    }
                }
            }
        });

        BufferDesc BuffDesc;
        BuffDesc.Name           = "UpdateTextureFromBuffer.UploadBuffer";
        BuffDesc.Size           = static_cast<Uint64>(UploadData.size());
        BuffDesc.BindFlags      = BIND_NONE;
        BuffDesc.Usage          = BufferUsage;
        BuffDesc.CPUAccessFlags = BufferUsage == USAGE_STAGING ? CPU_ACCESS_WRITE : CPU_ACCESS_NONE;

        BufferData BuffData{UploadData.data(), BuffDesc.Size};

        RefCntAutoPtr<IBuffer> pBuffer;
        pDevice->CreateBuffer(BuffDesc, BufferUsage == USAGE_DEFAULT ? &BuffData : nullptr, &pBuffer);
        ASSERT_NE(pBuffer, nullptr);

        if (BufferUsage == USAGE_STAGING)
        {
            void* pData = nullptr;
            pContext->MapBuffer(pBuffer, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT, pData);
            ASSERT_NE(pData, nullptr);
            memcpy(pData, UploadData.data(), static_cast<size_t>(BuffDesc.Size));
            pContext->UnmapBuffer(pBuffer, MAP_WRITE);
        }

        pContext->TransitionResourceState({pBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE});
        pContext->TransitionResourceState({pTexture, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_DEST, STATE_TRANSITION_FLAG_UPDATE_STATE});
        for (const auto& Region : UploadRegions)
        {
            Box DstBox{
                Region.DstX,
                Region.DstX + Region.Width,
                Region.DstY,
                Region.DstY + Region.Height,
                Region.DstZ,
                Region.DstZ + Region.Depth,
            };

            TextureSubResData SrcSubresData;
            SrcSubresData.pSrcBuffer  = pBuffer;
            SrcSubresData.SrcOffset   = Region.SrcOffset;
            SrcSubresData.Stride      = Region.SrcStride;
            SrcSubresData.DepthStride = Region.SrcDepthStride;
            pContext->UpdateTexture(pTexture, Region.Mip, Region.Slice, DstBox, SrcSubresData,
                                    RESOURCE_STATE_TRANSITION_MODE_VERIFY,
                                    RESOURCE_STATE_TRANSITION_MODE_VERIFY);
        }

        pContext->TransitionResourceState({pTexture, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE});
        pContext->TransitionResourceState({pStagingTexture, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_DEST, STATE_TRANSITION_FLAG_UPDATE_STATE});
        ForEachSubresource(BaseTexDesc, [&](Uint32 mip, Uint32 slice, Uint32, Uint32, Uint32) {
            CopyTextureAttribs CopyAttribs;
            CopyAttribs.pSrcTexture              = pTexture;
            CopyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_VERIFY;
            CopyAttribs.SrcMipLevel              = mip;
            CopyAttribs.SrcSlice                 = slice;
            CopyAttribs.pDstTexture              = pStagingTexture;
            CopyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_VERIFY;
            CopyAttribs.DstMipLevel              = mip;
            CopyAttribs.DstSlice                 = slice;
            pContext->CopyTexture(CopyAttribs);
        });

        pContext->WaitForIdle();

        ForEachSubresource(BaseTexDesc, [&](Uint32 mip, Uint32 slice, Uint32 MipWidth, Uint32 MipHeight, Uint32 MipDepth) {
            const auto& Subres = RefSubresources[GetSubresourceIndex(mip, slice)];

            MappedTextureSubresource MappedData;
            pContext->MapTextureSubresource(pStagingTexture, mip, slice, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, MappedData);

            for (Uint32 z = 0; z < MipDepth; ++z)
            {
                for (Uint32 row = 0; row < MipHeight; ++row)
                {
                    const void* pRefRow = &Subres[(z * MipHeight + row) * MipWidth];
                    const void* pTexRow =
                        reinterpret_cast<const Uint8*>(MappedData.pData) +
                        static_cast<size_t>(z) * static_cast<size_t>(MappedData.DepthStride) +
                        static_cast<size_t>(row) * static_cast<size_t>(MappedData.Stride);

                    EXPECT_EQ(memcmp(pRefRow, pTexRow, MipWidth * sizeof(Uint32)), 0)
                        << "Mip level: " << mip
                        << ", slice: " << slice
                        << ", z: " << z
                        << ", row: " << row;
                }
            }

            pContext->UnmapTextureSubresource(pStagingTexture, mip, slice);
        });
    }
}

TEST(UpdateTextureFromBuffer, Texture2D)
{
    RunCopyBufferToTextureTest(
        CreateTestTextureDesc("UpdateTextureFromBuffer.Texture2D",
                              RESOURCE_DIM_TEX_2D,
                              128,
                              128,
                              1,
                              4));
}

TEST(UpdateTextureFromBuffer, Texture2DArray)
{
    RunCopyBufferToTextureTest(
        CreateTestTextureDesc("UpdateTextureFromBuffer.Texture2DArray",
                              RESOURCE_DIM_TEX_2D_ARRAY,
                              128,
                              128,
                              4,
                              4));
}

TEST(UpdateTextureFromBuffer, Texture3D)
{
    RunCopyBufferToTextureTest(
        CreateTestTextureDesc("UpdateTextureFromBuffer.Texture3D",
                              RESOURCE_DIM_TEX_3D,
                              128,
                              128,
                              8,
                              4));
}

} // namespace
