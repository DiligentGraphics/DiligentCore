/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "DynamicTextureAtlas.h"

#include <mutex>
#include <algorithm>
#include <atomic>

#include "DynamicAtlasManager.hpp"
#include "ObjectBase.hpp"
#include "RefCntAutoPtr.hpp"
#include "FixedBlockMemoryAllocator.hpp"
#include "DefaultRawMemoryAllocator.hpp"
#include "GraphicsAccessories.hpp"
#include "Align.hpp"

namespace Diligent
{

class DynamicTextureAtlasImpl;

class TextureAtlasSuballocationImpl final : public ObjectBase<ITextureAtlasSuballocation>
{
public:
    using TBase = ObjectBase<ITextureAtlasSuballocation>;
    TextureAtlasSuballocationImpl(IReferenceCounters*           pRefCounters,
                                  DynamicTextureAtlasImpl*      pParentAtlas,
                                  DynamicAtlasManager::Region&& Subregion,
                                  Uint32                        Slice,
                                  const uint2&                  Size) noexcept :
        // clang-format off
        TBase         {pRefCounters},
        m_pParentAtlas{pParentAtlas},
        m_Subregion   {std::move(Subregion)},
        m_Slice       {Slice},
        m_Size        {Size}
    // clang-format on
    {
        VERIFY_EXPR(m_pParentAtlas);
        VERIFY_EXPR(!m_Subregion.IsEmpty());
    }

    ~TextureAtlasSuballocationImpl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_TextureAtlasSuballocation, TBase)

    virtual Atomics::Long DILIGENT_CALL_TYPE Release() override final
    {
        RefCntAutoPtr<DynamicTextureAtlasImpl> pAtlas;
        return TBase::Release(
            [&]() //
            {
                // We must keep parent alive while this object is being destroyed because
                // the parent keeps the memory allocator for the object.
                pAtlas = m_pParentAtlas;
            });
    }

    virtual uint2 GetOrigin() const override final;

    virtual Uint32 GetSlice() const override final
    {
        return m_Slice;
    }

    virtual uint2 GetSize() const override final
    {
        return m_Size;
    }

    virtual float4 GetUVScaleBias() const override final;

    virtual IDynamicTextureAtlas* GetAtlas() override final;

    virtual void SetUserData(IObject* pUserData) override final
    {
        m_pUserData = pUserData;
    }

    virtual IObject* GetUserData() const override final
    {
        return m_pUserData.RawPtr<IObject>();
    }

private:
    RefCntAutoPtr<DynamicTextureAtlasImpl> m_pParentAtlas;

    DynamicAtlasManager::Region m_Subregion;

    const Uint32 m_Slice;
    const uint2  m_Size;

    RefCntAutoPtr<IObject> m_pUserData;
};


class DynamicTextureAtlasImpl final : public ObjectBase<IDynamicTextureAtlas>
{
public:
    using TBase = ObjectBase<IDynamicTextureAtlas>;

    DynamicTextureAtlasImpl(IReferenceCounters*                  pRefCounters,
                            IRenderDevice*                       pDevice,
                            const DynamicTextureAtlasCreateInfo& CreateInfo) :
        // clang-format off
        TBase             {pRefCounters},
        m_Desc            {CreateInfo.Desc},
        m_Name            {CreateInfo.Desc.Name != nullptr ? CreateInfo.Desc.Name : "Dynamic texture atlas"},
        m_Granularity     {CreateInfo.TextureGranularity},
        m_ExtraSliceCount {CreateInfo.ExtraSliceCount},
        m_MaxSliceCount   {CreateInfo.Desc.Type == RESOURCE_DIM_TEX_2D_ARRAY ? std::min(CreateInfo.MaxSliceCount, Uint32{2048}) : 1},
        m_SuballocationsAllocator
        {
            DefaultRawMemoryAllocator::GetAllocator(),
            sizeof(TextureAtlasSuballocationImpl),
            CreateInfo.SuballocationObjAllocationGranularity
        }
    // clang-format on
    {
        if (m_Desc.Type != RESOURCE_DIM_TEX_2D && m_Desc.Type != RESOURCE_DIM_TEX_2D_ARRAY)
            LOG_ERROR_AND_THROW(GetResourceDimString(m_Desc.Type), " is not a valid resource dimension. Only 2D and 2D array textures are allowed");

        if (m_Desc.Format == TEX_FORMAT_UNKNOWN)
            LOG_ERROR_AND_THROW("Texture format must not be UNKNOWN");

        if (m_Granularity == 0)
            LOG_ERROR_AND_THROW("Texture granularity can't be zero");

        if (!IsPowerOfTwo(m_Granularity))
            LOG_ERROR_AND_THROW("Texture granularity (", m_Granularity, ") is not power of two");

        if (m_Desc.Width == 0)
            LOG_ERROR_AND_THROW("Texture width must not be zero");

        if (m_Desc.Height == 0)
            LOG_ERROR_AND_THROW("Texture height must not be zero");

        if ((m_Desc.Width % m_Granularity) != 0)
            LOG_ERROR_AND_THROW("Texture width (", m_Desc.Width, ") is not a multiple of granularity (", m_Granularity, ")");

        if ((m_Desc.Height % m_Granularity) != 0)
            LOG_ERROR_AND_THROW("Texture height (", m_Desc.Height, ") is not a multiple of granularity (", m_Granularity, ")");

        m_Desc.Name = m_Name.c_str();

        for (Uint32 slice = 0; slice < m_Desc.ArraySize; ++slice)
        {
            m_Slices.emplace_back(new SliceManager{m_Desc.Width / m_Granularity, m_Desc.Height / m_Granularity});
        }

        if (pDevice == nullptr)
            m_Desc.ArraySize = 0;
        if (m_Desc.ArraySize > 0)
        {
            pDevice->CreateTexture(m_Desc, nullptr, &m_pTexture);
            if (!m_pTexture)
                LOG_ERROR_AND_THROW("Failed to create texture atlas texture");
        }

        m_Version.store(0);
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_DynamicTextureAtlas, TBase)

    virtual ITexture* GetTexture(IRenderDevice* pDevice, IDeviceContext* pContext) override final
    {
        Uint32 ArraySize = 0;
        {
            std::lock_guard<std::mutex> Lock{m_SlicesMtx};
            ArraySize = static_cast<Uint32>(m_Slices.size());
        }
        if (m_Desc.ArraySize != ArraySize)
        {
            DEV_CHECK_ERR(pDevice != nullptr && pContext != nullptr,
                          "Texture atlas must be resized, but pDevice or pContext is null");

            m_Desc.ArraySize = ArraySize;
            RefCntAutoPtr<ITexture> pNewTexture;
            pDevice->CreateTexture(m_Desc, nullptr, &pNewTexture);
            VERIFY_EXPR(pNewTexture);
            m_Version.fetch_add(1);

            LOG_INFO_MESSAGE("Dynamic texture atlas: expanding texture array '", m_Desc.Name,
                             "' (", m_Desc.Width, " x ", m_Desc.Height, " ", m_Desc.MipLevels, "-mip ",
                             GetTextureFormatAttribs(m_Desc.Format).Name, ") to ",
                             m_Desc.ArraySize, " slices. Version: ", GetVersion());

            if (m_pTexture)
            {
                const auto& StaleTexDesc = m_pTexture->GetDesc();

                CopyTextureAttribs CopyAttribs;
                CopyAttribs.pSrcTexture              = m_pTexture;
                CopyAttribs.pDstTexture              = pNewTexture;
                CopyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
                CopyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

                for (Uint32 slice = 0; slice < StaleTexDesc.ArraySize; ++slice)
                {
                    for (Uint32 mip = 0; mip < StaleTexDesc.MipLevels; ++mip)
                    {
                        CopyAttribs.SrcSlice    = slice;
                        CopyAttribs.DstSlice    = slice;
                        CopyAttribs.SrcMipLevel = mip;
                        CopyAttribs.DstMipLevel = mip;
                        pContext->CopyTexture(CopyAttribs);
                    }
                }
            }

            m_pTexture = std::move(pNewTexture);
        }

        return m_pTexture;
    }

    virtual void Allocate(Uint32                       Width,
                          Uint32                       Height,
                          ITextureAtlasSuballocation** ppSuballocation) override final
    {
        if (Width == 0 || Height == 0)
        {
            UNEXPECTED("Subregion size must not be zero");
            return;
        }

        if (Width > m_Desc.Width || Height > m_Desc.Height)
        {
            LOG_ERROR_MESSAGE("Requested region size ", Width, " x ", Height, " exceeds atlas dimensions ", m_Desc.Width, " x ", m_Desc.Height);
            return;
        }

        DynamicAtlasManager::Region Subregion;

        Uint32 Slice = 0;
        while (Slice < m_MaxSliceCount)
        {
            SliceManager* pSliceMgr = nullptr;
            {
                std::lock_guard<std::mutex> Lock{m_SlicesMtx};
                if (Slice == m_Slices.size())
                {
                    const auto ExtraSliceCount = m_ExtraSliceCount != 0 ?
                        m_ExtraSliceCount :
                        static_cast<Uint32>(m_Slices.size());

                    for (Uint32 ExtraSlice = 0; ExtraSlice < ExtraSliceCount && Slice + ExtraSlice < m_MaxSliceCount; ++ExtraSlice)
                    {
                        m_Slices.emplace_back(new SliceManager{m_Desc.Width / m_Granularity, m_Desc.Height / m_Granularity});
                    }
                }
                pSliceMgr = m_Slices[Slice].get();
            }

            Subregion = pSliceMgr->Allocate((Width + m_Granularity - 1) / m_Granularity,
                                            (Height + m_Granularity - 1) / m_Granularity);
            if (!Subregion.IsEmpty())
                break;
            else
                ++Slice;
        }

        if (Subregion.IsEmpty())
        {
            LOG_ERROR_MESSAGE("Failed to suballocate texture subregion ", Width, " x ", Height, " from texture atlas");
            return;
        }

        // clang-format off
        TextureAtlasSuballocationImpl* pSuballocation{
            NEW_RC_OBJ(m_SuballocationsAllocator, "TextureAtlasSuballocationImpl instance", TextureAtlasSuballocationImpl)
            (
                this, 
                std::move(Subregion),
                Slice,
                uint2{Width, Height}
            )
        };
        // clang-format on

        pSuballocation->QueryInterface(IID_TextureAtlasSuballocation, reinterpret_cast<IObject**>(ppSuballocation));
    }

    virtual void Free(Uint32 Slice, DynamicAtlasManager::Region&& Subregion)
    {
        SliceManager* pSliceMgr = nullptr;
        {
            std::lock_guard<std::mutex> Lock{m_SlicesMtx};
            pSliceMgr = m_Slices[Slice].get();
        }
        pSliceMgr->Free(std::move(Subregion));
    }

    virtual const TextureDesc& GetAtlasDesc() const override final
    {
        return m_Desc;
    }

    virtual Uint32 GetVersion() const override final
    {
        return m_Version.load();
    }

    Uint32 GetGranularity() const
    {
        return m_Granularity;
    }

private:
    TextureDesc       m_Desc;
    const std::string m_Name;

    const Uint32 m_Granularity;
    const Uint32 m_ExtraSliceCount;
    const Uint32 m_MaxSliceCount;

    RefCntAutoPtr<ITexture> m_pTexture;

    FixedBlockMemoryAllocator m_SuballocationsAllocator;

    std::atomic_uint32_t m_Version = {};


    struct SliceManager
    {
        SliceManager(Uint32 Width, Uint32 Height) :
            Mgr{Width, Height}
        {}

        DynamicAtlasManager::Region Allocate(Uint32 Width, Uint32 Height)
        {
            std::lock_guard<std::mutex> Lock{Mtx};
            return Mgr.Allocate(Width, Height);
        }
        void Free(DynamicAtlasManager::Region&& Region)
        {
            std::lock_guard<std::mutex> Lock{Mtx};
            Mgr.Free(std::move(Region));
        }

    private:
        std::mutex          Mtx;
        DynamicAtlasManager Mgr;
    };
    std::mutex                                 m_SlicesMtx;
    std::vector<std::unique_ptr<SliceManager>> m_Slices;
};


TextureAtlasSuballocationImpl::~TextureAtlasSuballocationImpl()
{
    m_pParentAtlas->Free(m_Slice, std::move(m_Subregion));
}

uint2 TextureAtlasSuballocationImpl::GetOrigin() const
{
    const auto Granularity = m_pParentAtlas->GetGranularity();
    return uint2 //
        {
            m_Subregion.x * Granularity,
            m_Subregion.y * Granularity //
        };
}

IDynamicTextureAtlas* TextureAtlasSuballocationImpl::GetAtlas()
{
    return m_pParentAtlas;
}

float4 TextureAtlasSuballocationImpl::GetUVScaleBias() const
{
    const auto  Origin    = GetOrigin().Recast<float>();
    const auto  Size      = GetSize().Recast<float>();
    const auto& AtlasDesc = m_pParentAtlas->GetAtlasDesc();
    return float4 //
        {
            Size.x / static_cast<float>(AtlasDesc.Width),
            Size.y / static_cast<float>(AtlasDesc.Height),
            Origin.x / static_cast<float>(AtlasDesc.Width),
            Origin.y / static_cast<float>(AtlasDesc.Height) //
        };
}


void CreateDynamicTextureAtlas(IRenderDevice*                       pDevice,
                               const DynamicTextureAtlasCreateInfo& CreateInfo,
                               IDynamicTextureAtlas**               ppAtlas)
{
    try
    {
        auto* pAllocator = MakeNewRCObj<DynamicTextureAtlasImpl>()(pDevice, CreateInfo);
        pAllocator->QueryInterface(IID_DynamicTextureAtlas, reinterpret_cast<IObject**>(ppAtlas));
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to create buffer suballocator");
    }
}

} // namespace Diligent
