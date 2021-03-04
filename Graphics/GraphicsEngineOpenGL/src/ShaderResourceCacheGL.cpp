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

#include "pch.h"
#include "ShaderResourceCacheGL.hpp"
#include "PipelineResourceSignatureGLImpl.hpp"

namespace Diligent
{

size_t ShaderResourceCacheGL::GetRequriedMemorySize(const TResourceCount& ResCount)
{
    static_assert(std::is_same<TResourceCount, PipelineResourceSignatureGLImpl::TBindings>::value,
                  "ShaderResourceCacheGL::TResourceCount must be the same type as PipelineResourceSignatureGLImpl::TBindings");
    // clang-format off
    auto MemSize = 
                sizeof(CachedUB)           * ResCount[BINDING_RANGE_UNIFORM_BUFFER] + 
                sizeof(CachedResourceView) * ResCount[BINDING_RANGE_TEXTURE]        + 
                sizeof(CachedResourceView) * ResCount[BINDING_RANGE_IMAGE]          + 
                sizeof(CachedSSBO)         * ResCount[BINDING_RANGE_STORAGE_BUFFER];
    // clang-format on
    VERIFY(MemSize < InvalidResourceOffset, "Memory size exeed the maximum allowed size.");
    return MemSize;
}

void ShaderResourceCacheGL::Initialize(const TResourceCount& ResCount, IMemoryAllocator& MemAllocator)
{
    VERIFY(m_pAllocator == nullptr && m_pResourceData == nullptr, "Cache already initialized");
    m_pAllocator = &MemAllocator;

    // clang-format off
    m_TexturesOffset  = static_cast<Uint16>(m_UBsOffset      + sizeof(CachedUB)           * ResCount[BINDING_RANGE_UNIFORM_BUFFER]);
    m_ImagesOffset    = static_cast<Uint16>(m_TexturesOffset + sizeof(CachedResourceView) * ResCount[BINDING_RANGE_TEXTURE]);
    m_SSBOsOffset     = static_cast<Uint16>(m_ImagesOffset   + sizeof(CachedResourceView) * ResCount[BINDING_RANGE_IMAGE]);
    m_MemoryEndOffset = static_cast<Uint16>(m_SSBOsOffset    + sizeof(CachedSSBO)         * ResCount[BINDING_RANGE_STORAGE_BUFFER]);

    VERIFY_EXPR(GetUBCount()      == static_cast<Uint32>(ResCount[BINDING_RANGE_UNIFORM_BUFFER]));
    VERIFY_EXPR(GetTextureCount() == static_cast<Uint32>(ResCount[BINDING_RANGE_TEXTURE]));
    VERIFY_EXPR(GetImageCount()   == static_cast<Uint32>(ResCount[BINDING_RANGE_IMAGE]));
    VERIFY_EXPR(GetSSBOCount()    == static_cast<Uint32>(ResCount[BINDING_RANGE_STORAGE_BUFFER]));
    // clang-format on

    VERIFY_EXPR(m_pResourceData == nullptr);
    size_t BufferSize = m_MemoryEndOffset;

    VERIFY_EXPR(BufferSize == GetRequriedMemorySize(ResCount));

    if (BufferSize > 0)
    {
        m_pResourceData = ALLOCATE(MemAllocator, "Shader resource cache data buffer", Uint8, BufferSize);
        memset(m_pResourceData, 0, BufferSize);
    }

    // Explicitly construct all objects
    for (Uint32 cb = 0; cb < GetUBCount(); ++cb)
        new (&GetUB(cb)) CachedUB;

    for (Uint32 s = 0; s < GetTextureCount(); ++s)
        new (&GetTexture(s)) CachedResourceView;

    for (Uint32 i = 0; i < GetImageCount(); ++i)
        new (&GetImage(i)) CachedResourceView;

    for (Uint32 s = 0; s < GetSSBOCount(); ++s)
        new (&GetSSBO(s)) CachedSSBO;
}

ShaderResourceCacheGL::~ShaderResourceCacheGL()
{
    if (IsInitialized())
    {
        for (Uint32 cb = 0; cb < GetUBCount(); ++cb)
            GetUB(cb).~CachedUB();

        for (Uint32 s = 0; s < GetTextureCount(); ++s)
            GetTexture(s).~CachedResourceView();

        for (Uint32 i = 0; i < GetImageCount(); ++i)
            GetImage(i).~CachedResourceView();

        for (Uint32 s = 0; s < GetSSBOCount(); ++s)
            GetSSBO(s).~CachedSSBO();

        if (m_pResourceData != nullptr)
            m_pAllocator->Free(m_pResourceData);

        m_pResourceData   = nullptr;
        m_TexturesOffset  = InvalidResourceOffset;
        m_ImagesOffset    = InvalidResourceOffset;
        m_SSBOsOffset     = InvalidResourceOffset;
        m_MemoryEndOffset = InvalidResourceOffset;
    }
}

} // namespace Diligent
