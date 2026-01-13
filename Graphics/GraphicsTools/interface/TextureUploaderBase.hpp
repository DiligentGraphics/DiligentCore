/*
 *  Copyright 2019-2025 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#pragma once

#include <vector>

#include "TextureUploader.hpp"
#include "../../../Common/interface/ObjectBase.hpp"
#include "../../../Common/interface/HashUtils.hpp"
#include "../../../Common/interface/RefCntAutoPtr.hpp"
#include "../../GraphicsAccessories/interface/GraphicsAccessories.hpp"

namespace std
{

template <>
struct hash<Diligent::UploadBufferDesc>
{
    size_t operator()(const Diligent::UploadBufferDesc& Desc) const
    {
        return Diligent::ComputeHash(Desc.Width, Desc.Height, Desc.Depth, Desc.MipLevels, Desc.ArraySize, static_cast<Diligent::Int32>(Desc.Format));
    }
};

} // namespace std

namespace Diligent
{

class UploadBufferBase : public ObjectBase<IUploadBuffer>
{
public:
    UploadBufferBase(IReferenceCounters*     pRefCounters,
                     const UploadBufferDesc& Desc,
                     bool                    AllocateStagingData = false) :
        // clang-format off
        ObjectBase<IUploadBuffer>{pRefCounters},
        m_Desc                   {Desc},
        m_MappedData             (size_t{m_Desc.ArraySize} * size_t{m_Desc.MipLevels})
    // clang-format on
    {
        if (AllocateStagingData)
        {
            TextureDesc StagingTexDesc;
            StagingTexDesc.Width  = Desc.Width;
            StagingTexDesc.Height = Desc.Height;
            if (Desc.Depth > 1)
            {
                StagingTexDesc.Depth = Desc.Depth;
                VERIFY(Desc.ArraySize == 1, "3D textures cannot have array size greater than 1");
                StagingTexDesc.Type = RESOURCE_DIM_TEX_3D;
            }
            else if (Desc.ArraySize > 1)
            {
                StagingTexDesc.ArraySize = Desc.ArraySize;
                StagingTexDesc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
            }
            else
            {
                StagingTexDesc.Type = RESOURCE_DIM_TEX_2D;
            }
            StagingTexDesc.MipLevels = Desc.MipLevels;
            StagingTexDesc.Format    = Desc.Format;

            constexpr Uint32 Alignment              = 4;
            const Uint64     StagingTextureDataSize = GetStagingTextureDataSize(StagingTexDesc, Alignment);
            m_StagingData.resize(static_cast<size_t>(StagingTextureDataSize));

            for (Uint32 Slice = 0; Slice < Desc.ArraySize; ++Slice)
            {
                for (Uint32 Mip = 0; Mip < Desc.MipLevels; ++Mip)
                {
                    const Uint64             SubresOffset = GetStagingTextureSubresourceOffset(StagingTexDesc, Slice, Mip, Alignment);
                    const MipLevelProperties MipProps     = GetMipLevelProperties(StagingTexDesc, Mip);

                    MappedTextureSubresource MappedData;
                    MappedData.pData       = &m_StagingData[static_cast<size_t>(SubresOffset)];
                    MappedData.Stride      = MipProps.RowSize;
                    MappedData.DepthStride = MipProps.DepthSliceSize;
                    SetMappedData(Mip, Slice, MappedData);
                }
            }
        }
    }

    virtual MappedTextureSubresource GetMappedData(Uint32 Mip, Uint32 Slice) override final
    {
        VERIFY_EXPR(Mip < m_Desc.MipLevels && Slice < m_Desc.ArraySize);
        return m_MappedData[size_t{m_Desc.MipLevels} * size_t{Slice} + size_t{Mip}];
    }
    virtual const UploadBufferDesc& GetDesc() const override final { return m_Desc; }

    void SetMappedData(Uint32 Mip, Uint32 Slice, const MappedTextureSubresource& MappedData)
    {
        VERIFY_EXPR(Mip < m_Desc.MipLevels && Slice < m_Desc.ArraySize);
        m_MappedData[size_t{m_Desc.MipLevels} * size_t{Slice} + size_t{Mip}] = MappedData;
    }

    bool IsMapped(Uint32 Mip, Uint32 Slice) const
    {
        VERIFY_EXPR(Mip < m_Desc.MipLevels && Slice < m_Desc.ArraySize);
        return m_MappedData[size_t{m_Desc.MipLevels} * size_t{Slice} + size_t{Mip}].pData != nullptr;
    }

    void Reset()
    {
        if (!HasStagingData())
        {
            for (auto& MappedData : m_MappedData)
                MappedData = MappedTextureSubresource{};
        }
    }

    bool HasStagingData() const { return !m_StagingData.empty(); }

protected:
    const UploadBufferDesc                m_Desc;
    std::vector<MappedTextureSubresource> m_MappedData;
    std::vector<Uint8>                    m_StagingData;
};

class TextureUploaderBase : public ObjectBase<ITextureUploader>
{
public:
    TextureUploaderBase(IReferenceCounters* pRefCounters, IRenderDevice* pDevice, const TextureUploaderDesc& Desc) :
        ObjectBase<ITextureUploader>{pRefCounters},
        m_Desc{Desc},
        m_pDevice{pDevice}
    {}

    template <typename UploadBufferType>
    struct PendingOperation
    {
        enum class Type
        {
            Map,
            Copy
        } OpType;

        bool AutoRecycle = false;

        RefCntAutoPtr<UploadBufferType> pUploadBuffer;
        RefCntAutoPtr<ITexture>         pDstTexture;

        Uint32 DstSlice = 0;
        Uint32 DstMip   = 0;

        PendingOperation(Type Op, UploadBufferType* pBuff) :
            OpType{Op},
            pUploadBuffer{pBuff}
        {
            VERIFY_EXPR(OpType == Type::Map);
        }

        PendingOperation(Type              Op,
                         UploadBufferType* pBuff,
                         ITexture*         pDstTex,
                         Uint32            dstSlice,
                         Uint32            dstMip,
                         bool              Recycle) :
            OpType{Op},
            AutoRecycle{Recycle},
            pUploadBuffer{pBuff},
            pDstTexture{pDstTex},
            DstSlice{dstSlice},
            DstMip{dstMip}
        {
            VERIFY_EXPR(OpType == Type::Copy);
        }
    };

protected:
    const TextureUploaderDesc    m_Desc;
    RefCntAutoPtr<IRenderDevice> m_pDevice;
};

} // namespace Diligent
