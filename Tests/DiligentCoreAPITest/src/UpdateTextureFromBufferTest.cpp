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
#include "GraphicsAccessories.hpp"

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

AxisSplit GetAxisSplit(Uint32 Dim, Uint32 NumSplits, Uint32 Split)
{
    return AxisSplit{
        Split * (Dim / NumSplits),
        Dim / NumSplits,
    };
}

TextureDesc CreateTestTextureDesc(const char*        Name,
                                  RESOURCE_DIMENSION Type,
                                  TEXTURE_FORMAT     Format,
                                  Uint32             Width,
                                  Uint32             Height,
                                  Uint32             ArraySizeOrDepth)
{
    TextureDesc TexDesc;
    TexDesc.Name      = Name;
    TexDesc.Type      = Type;
    TexDesc.Format    = Format;
    TexDesc.BindFlags = BIND_SHADER_RESOURCE;
    TexDesc.Width     = Width;
    TexDesc.Height    = Height;
    TexDesc.MipLevels = ComputeMipLevelsCount(TexDesc);
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
        MipLevelProperties Mip = GetMipLevelProperties(TexDesc, mip);
        for (Uint32 slice = 0; slice < NumSlices; ++slice)
            Callback(mip, slice, Mip);
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
    const Uint32 NumSlices                    = BaseTexDesc.GetArraySize();

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

        std::vector<std::vector<Uint8>> RefSubresources(BaseTexDesc.MipLevels * NumSlices);
        std::vector<Uint8>              UploadData;
        std::vector<UploadRegion>       UploadRegions;

        auto GetSubresourceIndex = [NumSlices](Uint32 Mip, Uint32 Slice) {
            return Mip * NumSlices + Slice;
        };

        FastRandInt rnd{0, 0, 255};

        const TextureFormatAttribs& FmtAttribs = pDevice->GetTextureFormatInfo(BaseTexDesc.Format);

        const Uint32 ElementSize = FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED ?
            FmtAttribs.ComponentSize :
            FmtAttribs.ComponentSize * FmtAttribs.NumComponents;

        ForEachSubresource(BaseTexDesc, [&](Uint32 mip, Uint32 slice, const MipLevelProperties& Mip) {
            auto& Subres = RefSubresources[GetSubresourceIndex(mip, slice)];
            Subres.resize(static_cast<size_t>(Mip.MipSize));

            VERIFY((Mip.StorageWidth % FmtAttribs.BlockWidth) == 0, "Storage mip width (", Mip.StorageWidth, ") is expected to be a multiple of block width (", FmtAttribs.BlockWidth, ")");
            VERIFY((Mip.StorageHeight % FmtAttribs.BlockHeight) == 0, "Storage mip height (", Mip.StorageHeight, ") is expected to be a multiple of block height (", FmtAttribs.BlockHeight, ")");

            for (Uint8& pixel : Subres)
                pixel = static_cast<Uint8>(rnd());

            const Uint32 NumXSplits = Mip.LogicalWidth > FmtAttribs.BlockWidth ? 2u : 1u;
            const Uint32 NumYSplits = Mip.LogicalHeight > FmtAttribs.BlockHeight ? 2u : 1u;
            const Uint32 NumZSplits = Mip.Depth > 1 ? 2u : 1u;

            for (Uint32 zSplit = 0; zSplit < NumZSplits; ++zSplit)
            {
                const AxisSplit Z = GetAxisSplit(Mip.Depth, NumZSplits, zSplit);

                for (Uint32 ySplit = 0; ySplit < NumYSplits; ++ySplit)
                {
                    const AxisSplit Y = GetAxisSplit(Mip.LogicalHeight, NumYSplits, ySplit);

                    for (Uint32 xSplit = 0; xSplit < NumXSplits; ++xSplit)
                    {
                        const AxisSplit X = GetAxisSplit(Mip.LogicalWidth, NumXSplits, xSplit);

                        const Uint64 AlignedOffset = AlignUp(static_cast<Uint64>(UploadData.size()),
                                                             static_cast<Uint64>(TextureUpdateOffsetAlignment));
                        UploadData.resize(static_cast<size_t>(AlignedOffset));

                        const Uint32 Stride      = AlignUp(AlignUp(X.Size, FmtAttribs.BlockWidth) / FmtAttribs.BlockWidth * ElementSize, TextureUpdateStrideAlignment);
                        const Uint32 DepthStride = AlignUp(Y.Size, FmtAttribs.BlockHeight) / FmtAttribs.BlockHeight * Stride;
                        const Uint64 SrcOffset   = static_cast<Uint64>(UploadData.size());
                        const Uint64 RegionSize  = static_cast<Uint64>(Z.Size) * DepthStride;

                        UploadData.resize(static_cast<size_t>(SrcOffset + RegionSize));

                        for (Uint32 z = 0; z < Z.Size; ++z)
                        {
                            for (Uint32 row = 0; row < AlignUp(Y.Size, FmtAttribs.BlockHeight) / FmtAttribs.BlockHeight; ++row)
                            {
                                const Uint8* pSrcRow =
                                    &Subres[static_cast<size_t>(
                                        (Z.Offset + z) * Mip.DepthSliceSize +
                                        (Y.Offset / FmtAttribs.BlockHeight + row) * Mip.RowSize +
                                        X.Offset / FmtAttribs.BlockWidth * ElementSize)];

                                Uint8* pDstRow =
                                    UploadData.data() +
                                    static_cast<size_t>(SrcOffset) +
                                    static_cast<size_t>(z) * DepthStride +
                                    static_cast<size_t>(row) * Stride;

                                memcpy(pDstRow, pSrcRow, AlignUp(X.Size, FmtAttribs.BlockWidth) / FmtAttribs.BlockWidth * ElementSize);
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

        if (pDevice->GetDeviceInfo().IsGLDevice() && FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED)
        {
            // Readback of compressed textures is not supported in OpenGL backend, so we cannot verify texture data on CPU.
            continue;
        }

        pContext->TransitionResourceState({pTexture, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE});
        pContext->TransitionResourceState({pStagingTexture, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_DEST, STATE_TRANSITION_FLAG_UPDATE_STATE});
        ForEachSubresource(BaseTexDesc, [&](Uint32 mip, Uint32 slice, const MipLevelProperties&) {
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

        ForEachSubresource(BaseTexDesc, [&](Uint32 mip, Uint32 slice, const MipLevelProperties& Mip) {
            const auto& Subres = RefSubresources[GetSubresourceIndex(mip, slice)];

            MappedTextureSubresource MappedData;
            pContext->MapTextureSubresource(pStagingTexture, mip, slice, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, MappedData);

            for (Uint32 z = 0; z < Mip.Depth; ++z)
            {
                for (Uint32 row = 0; row < AlignUp(Mip.LogicalHeight, FmtAttribs.BlockHeight) / FmtAttribs.BlockHeight; ++row)
                {
                    const void* pRefRow = &Subres[static_cast<size_t>(z * Mip.DepthSliceSize + row * Mip.RowSize)];
                    const void* pTexRow =
                        reinterpret_cast<const Uint8*>(MappedData.pData) +
                        static_cast<size_t>(z) * static_cast<size_t>(MappedData.DepthStride) +
                        static_cast<size_t>(row) * static_cast<size_t>(MappedData.Stride);

                    EXPECT_EQ(memcmp(pRefRow, pTexRow, static_cast<size_t>(Mip.RowSize)), 0)
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

TEST(UpdateTextureFromBuffer, Texture2D_RGBA8)
{
    RunCopyBufferToTextureTest(
        CreateTestTextureDesc("UpdateTextureFromBuffer.Texture2D",
                              RESOURCE_DIM_TEX_2D,
                              TEX_FORMAT_RGBA8_UNORM,
                              128,
                              128,
                              1));
}


TEST(UpdateTextureFromBuffer, Texture2D_BC1)
{
    RunCopyBufferToTextureTest(
        CreateTestTextureDesc("UpdateTextureFromBuffer.Texture2D",
                              RESOURCE_DIM_TEX_2D,
                              TEX_FORMAT_BC1_UNORM,
                              512,
                              512,
                              1));
}

TEST(UpdateTextureFromBuffer, Texture2DArray_RGBA8)
{
    RunCopyBufferToTextureTest(
        CreateTestTextureDesc("UpdateTextureFromBuffer.Texture2DArray",
                              RESOURCE_DIM_TEX_2D_ARRAY,
                              TEX_FORMAT_RGBA8_UNORM,
                              128,
                              128,
                              4));
}


TEST(UpdateTextureFromBuffer, Texture2DArray_BC1)
{
    RunCopyBufferToTextureTest(
        CreateTestTextureDesc("UpdateTextureFromBuffer.Texture2DArray",
                              RESOURCE_DIM_TEX_2D_ARRAY,
                              TEX_FORMAT_BC1_UNORM,
                              512,
                              512,
                              4));
}

TEST(UpdateTextureFromBuffer, Texture3D_RGBA8)
{
    RunCopyBufferToTextureTest(
        CreateTestTextureDesc("UpdateTextureFromBuffer.Texture3D",
                              RESOURCE_DIM_TEX_3D,
                              TEX_FORMAT_RGBA8_UNORM,
                              128,
                              128,
                              8));
}

} // namespace
