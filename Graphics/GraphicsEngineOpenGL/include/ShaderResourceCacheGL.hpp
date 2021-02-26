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

#pragma once

#include "BufferGLImpl.hpp"
#include "TextureBaseGL.hpp"
#include "SamplerGLImpl.hpp"

namespace Diligent
{

/// The class implements a cache that holds resources bound to a specific GL program
// All resources are stored in the continuous memory using the following layout:
//
//   |        Cached UBs        |     Cached Textures     |       Cached Images      | Cached Storage Blocks     |
//   |----------------------------------------------------|--------------------------|---------------------------|
//   |  0 | 1 | ... | UBCount-1 | 0 | 1 | ...| SmpCount-1 | 0 | 1 | ... | ImgCount-1 | 0 | 1 |  ... | SBOCount-1 |
//    -----------------------------------------------------------------------------------------------------------
//
class ShaderResourceCacheGL
{
public:
    enum class CacheContentType : Uint8
    {
        Signature = 0, // The cache is used by the pipeline resource signature to hold static resources.
        SRB       = 1  // The cache is used by SRB to hold resources of all types (static, mutable, dynamic).
    };

    explicit ShaderResourceCacheGL(CacheContentType ContentType) noexcept :
        m_ContentType{ContentType}
    {}

    ~ShaderResourceCacheGL();

    // clang-format off
    ShaderResourceCacheGL             (const ShaderResourceCacheGL&) = delete;
    ShaderResourceCacheGL& operator = (const ShaderResourceCacheGL&) = delete;
    ShaderResourceCacheGL             (ShaderResourceCacheGL&&)      = delete;
    ShaderResourceCacheGL& operator = (ShaderResourceCacheGL&&)      = delete;
    // clang-format on

    /// Describes a resource bound to a uniform buffer or a shader storage block slot
    struct CachedUB
    {
        /// Strong reference to the buffer
        RefCntAutoPtr<BufferGLImpl> pBuffer;
    };

    /// Describes a resource bound to a sampler or an image slot
    struct CachedResourceView
    {
        /// We keep strong reference to the view instead of the reference
        /// to the texture or buffer because this is more efficient from
        /// performance point of view: this avoids one pair of
        /// AddStrongRef()/ReleaseStrongRef(). The view holds a strong reference
        /// to the texture or the buffer, so it makes no difference.
        RefCntAutoPtr<IDeviceObject> pView;

        TextureBaseGL* pTexture = nullptr;
        union
        {
            BufferGLImpl*  pBuffer = nullptr; // When pTexture == nullptr
            SamplerGLImpl* pSampler;          // When pTexture != nullptr
        };
        CachedResourceView() noexcept {}

        void Set(RefCntAutoPtr<TextureViewGLImpl>&& pTexView, bool SetSampler)
        {
            // Do not null out pSampler as it could've been initialized by PipelineStateGLImpl::InitializeSRBResourceCache!
            // pSampler = nullptr;

            // Avoid unnecessary virtual function calls
            pTexture = pTexView ? ValidatedCast<TextureBaseGL>(pTexView->TextureViewGLImpl::GetTexture()) : nullptr;
            if (pTexView && SetSampler)
            {
                pSampler = ValidatedCast<SamplerGLImpl>(pTexView->GetSampler());
            }

            pView.Attach(pTexView.Detach());
        }

        void Set(RefCntAutoPtr<BufferViewGLImpl>&& pBufView)
        {
            pTexture = nullptr;
            // Avoid unnecessary virtual function calls
            pBuffer = pBufView ? ValidatedCast<BufferGLImpl>(pBufView->BufferViewGLImpl::GetBuffer()) : nullptr;
            pView.Attach(pBufView.Detach());
        }
    };

    struct CachedSSBO
    {
        /// Strong reference to the buffer
        RefCntAutoPtr<BufferViewGLImpl> pBufferView;
    };


    static size_t GetRequriedMemorySize(Uint32 UBCount, Uint32 TextureCount, Uint32 ImageCount, Uint32 SSBOCount);

    void Initialize(Uint32 UBCount, Uint32 TextureCount, Uint32 ImageCount, Uint32 SSBOCount, IMemoryAllocator& MemAllocator);
    void Destroy(IMemoryAllocator& MemAllocator);

    void SetUniformBuffer(Uint32 CacheOffset, RefCntAutoPtr<BufferGLImpl>&& pBuff)
    {
        GetUB(CacheOffset).pBuffer = std::move(pBuff);
    }

    void SetTexture(Uint32 CacheOffset, RefCntAutoPtr<TextureViewGLImpl>&& pTexView, bool SetSampler)
    {
        GetTexture(CacheOffset).Set(std::move(pTexView), SetSampler);
    }

    void SetSampler(Uint32 CacheOffset, ISampler* pSampler)
    {
        GetTexture(CacheOffset).pSampler = ValidatedCast<SamplerGLImpl>(pSampler);
    }

    void SetTexelBuffer(Uint32 CacheOffset, RefCntAutoPtr<BufferViewGLImpl>&& pBuffView)
    {
        GetTexture(CacheOffset).Set(std::move(pBuffView));
    }

    void SetTexImage(Uint32 CacheOffset, RefCntAutoPtr<TextureViewGLImpl>&& pTexView)
    {
        GetImage(CacheOffset).Set(std::move(pTexView), false);
    }

    void SetBufImage(Uint32 CacheOffset, RefCntAutoPtr<BufferViewGLImpl>&& pBuffView)
    {
        GetImage(CacheOffset).Set(std::move(pBuffView));
    }

    void SetSSBO(Uint32 CacheOffset, RefCntAutoPtr<BufferViewGLImpl>&& pBuffView)
    {
        GetSSBO(CacheOffset).pBufferView = std::move(pBuffView);
    }

    void CopyTexture(Uint32 Binding, const CachedResourceView& SrcSam)
    {
        GetTexture(Binding) = SrcSam;
    }

    void CopyImage(Uint32 CacheOffset, const CachedResourceView& SrcImg)
    {
        GetImage(CacheOffset) = SrcImg;
    }

    bool IsUBBound(Uint32 CacheOffset) const
    {
        if (CacheOffset >= GetUBCount())
            return false;

        const auto& UB = GetConstUB(CacheOffset);
        return UB.pBuffer;
    }

    bool IsTextureBound(Uint32 CacheOffset, bool dbgIsTextureView) const
    {
        if (CacheOffset >= GetTextureCount())
            return false;

        const auto& Texture = GetConstTexture(CacheOffset);
        VERIFY_EXPR(dbgIsTextureView || Texture.pTexture == nullptr);
        return Texture.pView;
    }

    bool IsImageBound(Uint32 CacheOffset, bool dbgIsTextureView) const
    {
        if (CacheOffset >= GetImageCount())
            return false;

        const auto& Image = GetConstImage(CacheOffset);
        VERIFY_EXPR(dbgIsTextureView || Image.pTexture == nullptr);
        return Image.pView;
    }

    bool IsSSBOBound(Uint32 CacheOffset) const
    {
        if (CacheOffset >= GetSSBOCount())
            return false;

        const auto& SSBO = GetConstSSBO(CacheOffset);
        return SSBO.pBufferView;
    }

    // clang-format off
    Uint32 GetUBCount()      const { return (m_TexturesOffset  - m_UBsOffset)      / sizeof(CachedUB);            }
    Uint32 GetTextureCount() const { return (m_ImagesOffset    - m_TexturesOffset) / sizeof(CachedResourceView);  }
    Uint32 GetImageCount()   const { return (m_SSBOsOffset     - m_ImagesOffset)   / sizeof(CachedResourceView);  }
    Uint32 GetSSBOCount()    const { return (m_MemoryEndOffset - m_SSBOsOffset)    / sizeof(CachedSSBO);          }
    // clang-format on

    const CachedUB& GetConstUB(Uint32 CacheOffset) const
    {
        VERIFY(CacheOffset < GetUBCount(), "Uniform buffer index (", CacheOffset, ") is out of range");
        return reinterpret_cast<CachedUB*>(m_pResourceData + m_UBsOffset)[CacheOffset];
    }

    const CachedResourceView& GetConstTexture(Uint32 CacheOffset) const
    {
        VERIFY(CacheOffset < GetTextureCount(), "Texture index (", CacheOffset, ") is out of range");
        return reinterpret_cast<CachedResourceView*>(m_pResourceData + m_TexturesOffset)[CacheOffset];
    }

    const CachedResourceView& GetConstImage(Uint32 CacheOffset) const
    {
        VERIFY(CacheOffset < GetImageCount(), "Image buffer index (", CacheOffset, ") is out of range");
        return reinterpret_cast<CachedResourceView*>(m_pResourceData + m_ImagesOffset)[CacheOffset];
    }

    const CachedSSBO& GetConstSSBO(Uint32 CacheOffset) const
    {
        VERIFY(CacheOffset < GetSSBOCount(), "Shader storage block index (", CacheOffset, ") is out of range");
        return reinterpret_cast<CachedSSBO*>(m_pResourceData + m_SSBOsOffset)[CacheOffset];
    }

    bool IsInitialized() const
    {
        return m_MemoryEndOffset != InvalidResourceOffset;
    }

    CacheContentType GetContentType() const { return m_ContentType; }

#ifdef DILIGENT_DEVELOPMENT
    void SetStaticResourcesInitialized()
    {
        m_bStaticResourcesInitialized = true;
    }
    bool StaticResourcesInitialized() const { return m_bStaticResourcesInitialized; }
#endif

private:
    CachedUB& GetUB(Uint32 CacheOffset)
    {
        return const_cast<CachedUB&>(const_cast<const ShaderResourceCacheGL*>(this)->GetConstUB(CacheOffset));
    }

    CachedResourceView& GetTexture(Uint32 CacheOffset)
    {
        return const_cast<CachedResourceView&>(const_cast<const ShaderResourceCacheGL*>(this)->GetConstTexture(CacheOffset));
    }

    CachedResourceView& GetImage(Uint32 CacheOffset)
    {
        return const_cast<CachedResourceView&>(const_cast<const ShaderResourceCacheGL*>(this)->GetConstImage(CacheOffset));
    }

    CachedSSBO& GetSSBO(Uint32 CacheOffset)
    {
        return const_cast<CachedSSBO&>(const_cast<const ShaderResourceCacheGL*>(this)->GetConstSSBO(CacheOffset));
    }

    static constexpr const Uint16 InvalidResourceOffset = 0xFFFF;
    static constexpr const Uint16 m_UBsOffset           = 0;

    Uint16 m_TexturesOffset  = InvalidResourceOffset;
    Uint16 m_ImagesOffset    = InvalidResourceOffset;
    Uint16 m_SSBOsOffset     = InvalidResourceOffset;
    Uint16 m_MemoryEndOffset = InvalidResourceOffset;

    Uint8* m_pResourceData = nullptr;

    // Indicates what types of resources are stored in the cache
    const CacheContentType m_ContentType;

#ifdef DILIGENT_DEBUG
    IMemoryAllocator* m_pdbgMemoryAllocator = nullptr;
#endif
#ifdef DILIGENT_DEVELOPMENT
    bool m_bStaticResourcesInitialized = false;
#endif
};

} // namespace Diligent
