/*     Copyright 2015-2018 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "ShaderResourceCacheVk.h"
#include "DeviceContextVkImpl.h"
#include "BufferVkImpl.h"
#include "BufferViewVkImpl.h"
#include "TextureViewVkImpl.h"
#include "TextureVkImpl.h"
#include "SamplerVkImpl.h"

namespace Diligent
{

void ShaderResourceCacheVk::InitializeSets(IMemoryAllocator &MemAllocator, Uint32 NumSets, Uint32 SetSizes[])
{
    // Memory layout:
    //
    //  m_pMemory
    //  |
    //  V
    // ||  DescriptorSet[0]  |   ....    |  DescriptorSet[Ns-1]  |  Res[0]  |  ... |  Res[n-1]  |    ....     | Res[0]  |  ... |  Res[m-1]  ||
    //
    //
    //  Ns = m_NumSets

    VERIFY(m_pAllocator == nullptr && m_pMemory == nullptr, "Cache already initialized");
    m_pAllocator = &MemAllocator;
    m_NumSets = NumSets;
    m_TotalResources = 0;
    for(Uint32 t=0; t < NumSets; ++t)
        m_TotalResources += SetSizes[t];
    auto MemorySize = NumSets * sizeof(DescriptorSet) + m_TotalResources * sizeof(Resource);
    if(MemorySize > 0)
    {
        m_pMemory = ALLOCATE( *m_pAllocator, "Memory for shader resource cache data", MemorySize);
        auto *pSets = reinterpret_cast<DescriptorSet*>(m_pMemory);
        auto *pCurrResPtr = reinterpret_cast<Resource*>(pSets + m_NumSets);
        for (Uint32 t = 0; t < NumSets; ++t)
        {
            new(&GetDescriptorSet(t)) DescriptorSet(SetSizes[t], SetSizes[t] > 0 ? pCurrResPtr : nullptr);
            pCurrResPtr += SetSizes[t];
        }
        VERIFY_EXPR((char*)pCurrResPtr == (char*)m_pMemory + MemorySize);
    }
}

void ShaderResourceCacheVk::InitializeResources(Uint32 Set, Uint32 Offset, Uint32 ArraySize, SPIRVShaderResourceAttribs::ResourceType Type)
{
    auto &DescrSet = GetDescriptorSet(Set);
    for (Uint32 res = 0; res < ArraySize; ++res)
        new(&DescrSet.GetResource(Offset + res)) Resource{Type};
}

ShaderResourceCacheVk::~ShaderResourceCacheVk()
{
    if (m_pMemory)
    {
        auto *pResources = reinterpret_cast<Resource*>( reinterpret_cast<DescriptorSet*>(m_pMemory) + m_NumSets);
        for(Uint32 res=0; res < m_TotalResources; ++res)
            pResources[res].~Resource();
        for (Uint32 t = 0; t < m_NumSets; ++t)
            GetDescriptorSet(t).~DescriptorSet();

        m_pAllocator->Free(m_pMemory);
    }
}

template<bool VerifyOnly>
void ShaderResourceCacheVk::TransitionResources(DeviceContextVkImpl *pCtxVkImpl)
{
    auto *pResources = reinterpret_cast<Resource*>(reinterpret_cast<DescriptorSet*>(m_pMemory) + m_NumSets);
    for (Uint32 res = 0; res < m_TotalResources; ++res)
    {
        auto &Res = pResources[res];
        switch (Res.Type)
        {
            case SPIRVShaderResourceAttribs::ResourceType::UniformBuffer:
            case SPIRVShaderResourceAttribs::ResourceType::StorageBuffer:
            {
                auto *pBufferVk = Res.pObject.RawPtr<BufferVkImpl>();
                VkAccessFlags RequiredAccessFlags = 
                    Res.Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer ? 
                        VK_ACCESS_UNIFORM_READ_BIT : 
                        (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                if(pBufferVk->GetAccessFlags() != RequiredAccessFlags)
                {
                    if(VerifyOnly)
                        LOG_ERROR_MESSAGE("Buffer \"", pBufferVk->GetDesc().Name, "\" is not in correct state. Did you forget to call TransitionShaderResources() or specify COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag in a call to CommitShaderResources()?");
                    else
                        pCtxVkImpl->BufferMemoryBarrier(*pBufferVk, RequiredAccessFlags);
                }
            }
            break;

            case SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer:
            case SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer:
            {
                auto *pBuffViewVk = Res.pObject.RawPtr<BufferViewVkImpl>();
                auto *pBufferVk = ValidatedCast<BufferVkImpl>(pBuffViewVk->GetBuffer());
                VkAccessFlags RequiredAccessFlags =
                    Res.Type == SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer ?
                    VK_ACCESS_SHADER_READ_BIT :
                    (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                if (pBufferVk->GetAccessFlags() != RequiredAccessFlags)
                {
                    if (VerifyOnly)
                        LOG_ERROR_MESSAGE("Buffer \"", pBufferVk->GetDesc().Name, "\" is not in correct state. Did you forget to call TransitionShaderResources() or specify COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag in a call to CommitShaderResources()?");
                    else
                        pCtxVkImpl->BufferMemoryBarrier(*pBufferVk, RequiredAccessFlags);
                }
            }
            break;

            case SPIRVShaderResourceAttribs::ResourceType::SeparateImage:
            case SPIRVShaderResourceAttribs::ResourceType::SampledImage:
            case SPIRVShaderResourceAttribs::ResourceType::StorageImage:
            {
                auto *pTextureViewVk = Res.pObject.RawPtr<TextureViewVkImpl>();
                auto *pTextureVk = ValidatedCast<TextureVkImpl>(pTextureViewVk->GetTexture());

                // The image subresources for a storage image must be in the VK_IMAGE_LAYOUT_GENERAL layout in 
                // order to access its data in a shader (13.1.1)
                // The image subresources for a sampled image or a combined image sampler must be in the 
                // VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
                // or VK_IMAGE_LAYOUT_GENERAL layout in order to access its data in a shader (13.1.3, 13.1.4).
                VkImageLayout RequiredLayout = 
                    Res.Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage ? 
                        VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                if(pTextureVk->GetLayout() != RequiredLayout)
                {
                    if (VerifyOnly)
                        LOG_ERROR_MESSAGE("Texture \"", pTextureVk->GetDesc().Name, "\" is not in correct state. Did you forget to call TransitionShaderResources() or specify COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag in a call to CommitShaderResources()?");
                    else
                        pCtxVkImpl->TransitionImageLayout(*pTextureVk, RequiredLayout);
                }
            }
            break;

            case SPIRVShaderResourceAttribs::ResourceType::AtomicCounter:
            {
                // Nothing to do with atomic counters
            }
            break;

            case SPIRVShaderResourceAttribs::ResourceType::SeparateSampler:
            {
                // Nothing to do with samplers
            }
            break;

            default: UNEXPECTED("Unexpected resource type");
        }
    }
}

template void ShaderResourceCacheVk::TransitionResources<false>(DeviceContextVkImpl *pCtxVkImpl);
template void ShaderResourceCacheVk::TransitionResources<true>(DeviceContextVkImpl *pCtxVkImpl);


VkDescriptorBufferInfo ShaderResourceCacheVk::Resource::GetBufferDescriptorWriteInfo()const
{
    VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer ||
           Type == SPIRVShaderResourceAttribs::ResourceType::StorageBuffer,
           "Uniform or storage buffer resource is expected");

    auto* pBuffVk = pObject.RawPtr<const BufferVkImpl>();

    // The buffer must be created with the following flags so that it can be bound to the specified descriptor (13.2.4):
    //  * VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC -> VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
    //  * VK_DESCRIPTOR_TYPE_STORAGE_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC -> VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    //  * VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER -> VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
    //  * VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER -> VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT

    VkDescriptorBufferInfo DescrBuffInfo;
    DescrBuffInfo.buffer = pBuffVk->GetVkBuffer();
    // If descriptorType is VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, the offset 
    // member of each element of pBufferInfo must be a multiple of VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment
    // If descriptorType is VK_DESCRIPTOR_TYPE_STORAGE_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, the offset 
    // member of each element of pBufferInfo must be a multiple of VkPhysicalDeviceLimits::minStorageBufferOffsetAlignment
    // (13.2.4)
    DescrBuffInfo.offset = 0;
    DescrBuffInfo.range = pBuffVk->GetDesc().uiSizeInBytes;
    return DescrBuffInfo;
}

VkDescriptorImageInfo ShaderResourceCacheVk::Resource::GetImageDescriptorWriteInfo(bool IsImmutableSampler)const
{
    VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage  ||
           Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage ||
           Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage,
           "Storage image, separate image or sampled image resource is expected");

    auto* pTexViewVk = pObject.RawPtr<const TextureViewVkImpl>();

    VkDescriptorImageInfo DescrImgInfo;
    DescrImgInfo.sampler = VK_NULL_HANDLE;
    if (Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage && !IsImmutableSampler)
    {
        // Immutable samplers are permanently bound into the set layout; later binding a sampler 
        // into an immutable sampler slot in a descriptor set is not allowed (13.2.1)
        auto *pSamplerVk = ValidatedCast<const SamplerVkImpl>(pTexViewVk->GetSampler());
        if (pSamplerVk != nullptr)
        {
            // If descriptorType is VK_DESCRIPTOR_TYPE_SAMPLER or VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
            // and dstSet was not allocated with a layout that included immutable samplers for dstBinding with 
            // descriptorType, the sampler member of each element of pImageInfo must be a valid VkSampler 
            // object (13.2.4)
            DescrImgInfo.sampler = pSamplerVk->GetVkSampler();
        }
        else
        {
            LOG_ERROR_MESSAGE("No sampler assigned to texture view \"", pTexViewVk->GetDesc().Name, "\"");
        }
    }
    DescrImgInfo.imageView = pTexViewVk->GetVulkanImageView();
    
    // If descriptorType is VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, for each descriptor that will be accessed 
    // via load or store operations the imageLayout member for corresponding elements of pImageInfo 
    // MUST be VK_IMAGE_LAYOUT_GENERAL (13.2.4)
    DescrImgInfo.imageLayout = 
        (Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage) ? 
            VK_IMAGE_LAYOUT_GENERAL : 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    return DescrImgInfo;
}

VkBufferView ShaderResourceCacheVk::Resource::GetBufferViewWriteInfo()const
{
    VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer ||
           Type == SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer,
           "Uniform or storage buffer resource is expected");
    
    auto* pBuffViewVk = pObject.RawPtr<const BufferViewVkImpl>();
    return pBuffViewVk->GetVkBufferView();
}

VkDescriptorImageInfo ShaderResourceCacheVk::Resource::GetSamplerDescriptorWriteInfo()const
{
    VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler, "Separate sampler resource is expected");

    auto* pSamplerVk = pObject.RawPtr<const SamplerVkImpl>();
    VkDescriptorImageInfo DescrImgInfo;
    // For VK_DESCRIPTOR_TYPE_SAMPLER, only the sample member of each element of VkWriteDescriptorSet::pImageInfo is accessed (13.2.4)
    DescrImgInfo.sampler = pSamplerVk->GetVkSampler();
    DescrImgInfo.imageView = VK_NULL_HANDLE;
    DescrImgInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return DescrImgInfo;
}

}
