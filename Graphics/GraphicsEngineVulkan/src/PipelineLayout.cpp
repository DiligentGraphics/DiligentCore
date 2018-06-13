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

#include "PipelineLayout.h"
#include "ShaderResourceLayoutVk.h"
#include "ShaderVkImpl.h"
#include "CommandContext.h"
#include "RenderDeviceVkImpl.h"
#include "DeviceContextVkImpl.h"
#include "TextureVkImpl.h"
#include "BufferVkImpl.h"
#include "VulkanTypeConversions.h"
#include "HashUtils.h"

namespace Diligent
{


static VkShaderStageFlagBits ShaderTypeToVkShaderStageFlagBit(SHADER_TYPE ShaderType)
{
    switch(ShaderType)
    {
        case SHADER_TYPE_VERTEX:   return VK_SHADER_STAGE_VERTEX_BIT;
        case SHADER_TYPE_HULL:     return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case SHADER_TYPE_DOMAIN:   return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case SHADER_TYPE_GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case SHADER_TYPE_PIXEL:    return VK_SHADER_STAGE_FRAGMENT_BIT;
        case SHADER_TYPE_COMPUTE:  return VK_SHADER_STAGE_COMPUTE_BIT;
        
        default: 
            UNEXPECTED("Unknown shader type");
            return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

class ResourceTypeToVkDescriptorType
{
public:
    ResourceTypeToVkDescriptorType()
    {
        static_assert(SPIRVShaderResourceAttribs::ResourceType::NumResourceTypes == 9, "Please add corresponding decriptor type");
        m_Map[SPIRVShaderResourceAttribs::ResourceType::UniformBuffer]      = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::StorageBuffer]      = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer] = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer] = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::StorageImage]       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::SampledImage]       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::AtomicCounter]      = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::SeparateImage]      = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::SeparateSampler]    = VK_DESCRIPTOR_TYPE_SAMPLER;
    }

    VkDescriptorType operator[](SPIRVShaderResourceAttribs::ResourceType ResType)const
    {
        return m_Map[static_cast<int>(ResType)];
    }

private:
    std::array<VkDescriptorType, SPIRVShaderResourceAttribs::ResourceType::NumResourceTypes> m_Map = {};
};

VkDescriptorType PipelineLayout::GetVkDescriptorType(const SPIRVShaderResourceAttribs &Res)
{
    static const ResourceTypeToVkDescriptorType ResTypeToVkDescrType;
    return ResTypeToVkDescrType[Res.Type];
}

PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayoutManager(IMemoryAllocator &MemAllocator):
    m_MemAllocator(MemAllocator),
    m_LayoutBindings(STD_ALLOCATOR_RAW_MEM(VkDescriptorSetLayoutBinding, MemAllocator, "Allocator for Layout Bindings"))
{}


void PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayout::AddBinding(const VkDescriptorSetLayoutBinding &Binding, IMemoryAllocator &MemAllocator)
{
    VERIFY(VkLayout == VK_NULL_HANDLE, "Descriptor set must not be finalized");
    ReserveMemory(NumLayoutBindings + 1, MemAllocator);
    pBindings[NumLayoutBindings++] = Binding;
    TotalDescriptors += Binding.descriptorCount;
    if(Binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || 
       Binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
    {
        VERIFY(NumDynamicDescriptors + Binding.descriptorCount <= std::numeric_limits<decltype(NumDynamicDescriptors)>::max(), "Number of dynamic descriptors exceeds max representable value");
        NumDynamicDescriptors += static_cast<decltype(NumDynamicDescriptors)>(Binding.descriptorCount);
    }
}

size_t PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayout::GetMemorySize(Uint32 NumBindings)
{
    if(NumBindings == 0)
        return 0;

    // Align up to the nearest power of two
    size_t MemSize = 0;
    if(NumBindings == 1)
        MemSize = 1;
    else if(NumBindings > 1)
    {
        // NumBindings = 2^n
        //             n n-1        2  1  0
        //      2^n =  1  0    ...  0  0  0
        //    
        //             n n-1        2  1  0
        //    2^n-1 =  0  1    ...  1  1  1
        //    msb = n-1 
        //    MemSize = 2^n


        // NumBindings = 2^n + [1 .. 2^n-1]
        //             n n-1        
        //      2^n =  1  0  ...  1  ...  
        //    
        //             n n-1               
        //    2^n-1 =  1  0  ...        
        //    msb = n 
        //    MemSize = 2^(n+1)

        MemSize = Uint32{2U << PlatformMisc::GetMSB(NumBindings-1)};
    }
    VERIFY_EXPR( ((NumBindings & (NumBindings-1)) == 0) && NumBindings == MemSize || NumBindings < MemSize);

#ifdef _DEBUG
    static constexpr size_t MinMemSize = 1;
#else
    static constexpr size_t MinMemSize = 16;
#endif
    MemSize = std::max(MemSize, MinMemSize);
    return MemSize * sizeof(VkDescriptorSetLayoutBinding);
}

void PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayout::ReserveMemory(Uint32 NumBindings, IMemoryAllocator &MemAllocator)
{
    size_t ReservedMemory = GetMemorySize(NumLayoutBindings);
    size_t RequiredMemory = GetMemorySize(NumBindings);
    if(RequiredMemory > ReservedMemory)
    {
        void *pNewBindings = ALLOCATE(MemAllocator, "Memory buffer for descriptor set layout bindings", RequiredMemory);
        if(pBindings != nullptr)
        {
            memcpy(pNewBindings, pBindings, sizeof(VkDescriptorSetLayoutBinding) * NumLayoutBindings);
            MemAllocator.Free(pBindings);
        }
        pBindings = reinterpret_cast<VkDescriptorSetLayoutBinding*>(pNewBindings);
    }
}

void PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayout::Finalize(const VulkanUtilities::VulkanLogicalDevice &LogicalDevice, 
                                                                               IMemoryAllocator &MemAllocator, 
                                                                               VkDescriptorSetLayoutBinding* pNewBindings)
{
    VERIFY_EXPR( memcmp(pBindings, pNewBindings, sizeof(VkDescriptorSetLayoutBinding)*NumLayoutBindings) == 0 );

    VkDescriptorSetLayoutCreateInfo SetLayoutCI = {};
    SetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    SetLayoutCI.pNext = nullptr;
    SetLayoutCI.flags = 0;
    SetLayoutCI.bindingCount = NumLayoutBindings;
    SetLayoutCI.pBindings = pBindings;
    VkLayout = LogicalDevice.CreateDescriptorSetLayout(SetLayoutCI);

    MemAllocator.Free(pBindings);
    pBindings = pNewBindings;
}

void PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayout::Release(RenderDeviceVkImpl *pRenderDeviceVk, IMemoryAllocator &MemAllocator)
{
    pRenderDeviceVk->SafeReleaseVkObject(std::move(VkLayout));
    for(uint32_t b=0; b < NumLayoutBindings; ++b)
    {
        if(pBindings[b].pImmutableSamplers != nullptr)
            MemAllocator.Free(const_cast<VkSampler*>(pBindings[b].pImmutableSamplers));
    }
    pBindings = nullptr;
    NumLayoutBindings = 0;
}

PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayout::~DescriptorSetLayout()
{
    VERIFY(VkLayout == VK_NULL_HANDLE, "Vulkan descriptor set layout has not been released. Did you forget to call Release()?");
}

bool PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayout::operator == (const DescriptorSetLayout& rhs)const
{
    if(TotalDescriptors      != rhs.TotalDescriptors      ||
       SetIndex              != rhs.SetIndex              ||
       NumDynamicDescriptors != rhs.NumDynamicDescriptors ||
       NumLayoutBindings     != rhs.NumLayoutBindings)
        return false;

    for(uint32_t b=0; b < NumLayoutBindings; ++b)
    {
        const auto &B0 = pBindings[b];
        const auto &B1 = rhs.pBindings[b];
        if(B0.binding         != B1.binding || 
           B0.descriptorType  != B1.descriptorType ||
           B0.descriptorCount != B1.descriptorCount ||
           B0.stageFlags      != B1.stageFlags)
            return false;

        if( B0.pImmutableSamplers != nullptr && B1.pImmutableSamplers == nullptr || 
            B0.pImmutableSamplers == nullptr && B1.pImmutableSamplers != nullptr)
            return false;
        // Static samplers themselves should not affect compatibility
    }
    return true;
}

size_t PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayout::GetHash()const
{
    size_t Hash = ComputeHash(SetIndex, NumLayoutBindings, TotalDescriptors, NumDynamicDescriptors);
    for (uint32_t b = 0; b < NumLayoutBindings; ++b)
    {
        const auto &B = pBindings[b];
        HashCombine(Hash, B.binding, static_cast<size_t>(B.descriptorType), B.descriptorCount, static_cast<size_t>(B.stageFlags), B.pImmutableSamplers != nullptr);
    }

    return Hash;
}

void PipelineLayout::DescriptorSetLayoutManager::Finalize(const VulkanUtilities::VulkanLogicalDevice &LogicalDevice)
{
    size_t TotalBindings = 0;
    for (const auto &Layout : m_DescriptorSetLayouts)
    {
        TotalBindings += Layout.NumLayoutBindings;
    }
    m_LayoutBindings.resize(TotalBindings);
    size_t BindingOffset = 0;
    std::array<VkDescriptorSetLayout, 2> ActiveDescrSetLayouts = {};
    for(auto &Layout : m_DescriptorSetLayouts)
    {
        if(Layout.SetIndex >= 0)
        {
            std::copy(Layout.pBindings, Layout.pBindings + Layout.NumLayoutBindings, m_LayoutBindings.begin() + BindingOffset);
            Layout.Finalize(LogicalDevice, m_MemAllocator, &m_LayoutBindings[BindingOffset]);
            BindingOffset += Layout.NumLayoutBindings;
            ActiveDescrSetLayouts[Layout.SetIndex] = Layout.VkLayout;
        }
    }
    VERIFY_EXPR(BindingOffset == TotalBindings);
    VERIFY_EXPR(m_ActiveSets == 0 && ActiveDescrSetLayouts[0] == VK_NULL_HANDLE && ActiveDescrSetLayouts[1] == VK_NULL_HANDLE ||
                m_ActiveSets == 1 && ActiveDescrSetLayouts[0] != VK_NULL_HANDLE && ActiveDescrSetLayouts[1] == VK_NULL_HANDLE ||
                m_ActiveSets == 2 && ActiveDescrSetLayouts[0] != VK_NULL_HANDLE && ActiveDescrSetLayouts[1] != VK_NULL_HANDLE);

    VkPipelineLayoutCreateInfo PipelineLayoutCI = {};
    PipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutCI.pNext = nullptr;
    PipelineLayoutCI.flags = 0; // reserved for future use
    PipelineLayoutCI.setLayoutCount = m_ActiveSets;
    PipelineLayoutCI.pSetLayouts = PipelineLayoutCI.setLayoutCount != 0 ? ActiveDescrSetLayouts.data() : nullptr;
    PipelineLayoutCI.pushConstantRangeCount = 0;
    PipelineLayoutCI.pPushConstantRanges = nullptr;
    m_VkPipelineLayout = LogicalDevice.CreatePipelineLayout(PipelineLayoutCI);

    VERIFY_EXPR(BindingOffset == TotalBindings);
}

void PipelineLayout::DescriptorSetLayoutManager::Release(RenderDeviceVkImpl *pRenderDeviceVk)
{
    for (auto &Layout : m_DescriptorSetLayouts)
        Layout.Release(pRenderDeviceVk, m_MemAllocator);

    pRenderDeviceVk->SafeReleaseVkObject(std::move(m_VkPipelineLayout));
}

PipelineLayout::DescriptorSetLayoutManager::~DescriptorSetLayoutManager()
{
    VERIFY(m_VkPipelineLayout == VK_NULL_HANDLE, "Vulkan pipeline layout has not been released. Did you forget to call Release()?");
}

bool PipelineLayout::DescriptorSetLayoutManager::operator == (const DescriptorSetLayoutManager& rhs)const
{
    // Two pipeline layouts are defined to be “compatible for set N” if they were created with identically 
    // defined descriptor set layouts for sets zero through N, and if they were created with identical push 
    // constant ranges (13.2.2)

    if(m_ActiveSets != rhs.m_ActiveSets)
        return false;

    for(size_t i=0; i < m_DescriptorSetLayouts.size(); ++i)
        if(m_DescriptorSetLayouts[i] != rhs.m_DescriptorSetLayouts[i])
            return false;

    return true;
}

size_t PipelineLayout::DescriptorSetLayoutManager::GetHash()const
{
    size_t Hash = 0;
    for(const auto &SetLayout : m_DescriptorSetLayouts)
        HashCombine(Hash, SetLayout.GetHash());

    return Hash;
}

void PipelineLayout::DescriptorSetLayoutManager::AllocateResourceSlot(const SPIRVShaderResourceAttribs& ResAttribs,
                                                                      VkSampler                         vkStaticSampler,
                                                                      SHADER_TYPE                       ShaderType,
                                                                      Uint32&                           DescriptorSet,
                                                                      Uint32&                           Binding,
                                                                      Uint32&                           OffsetInCache)
{
    auto& DescrSet = GetDescriptorSet(ResAttribs.VarType);
    if (DescrSet.SetIndex < 0)
    {
        DescrSet.SetIndex = m_ActiveSets++;
    }
    DescriptorSet = DescrSet.SetIndex;

    VkDescriptorSetLayoutBinding VkBinding = {};
    Binding = DescrSet.NumLayoutBindings;
    VkBinding.binding = Binding;
    VkBinding.descriptorType = GetVkDescriptorType(ResAttribs);
    VkBinding.descriptorCount = ResAttribs.ArraySize;
    // There are no limitations on what combinations of stages can use a descriptor binding (13.2.1)
    VkBinding.stageFlags = ShaderTypeToVkShaderStageFlagBit(ShaderType);
    if (ResAttribs.StaticSamplerInd >= 0)
    {
        VERIFY(vkStaticSampler != VK_NULL_HANDLE, "No static sampler provided");
        // If descriptorType is VK_DESCRIPTOR_TYPE_SAMPLER or VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, and 
        // descriptorCount is not 0 and pImmutableSamplers is not NULL, pImmutableSamplers must be a valid pointer 
        // to an array of descriptorCount valid VkSampler handles (13.2.1)
        auto *pStaticSamplers = reinterpret_cast<VkSampler*>(ALLOCATE(m_MemAllocator, "Memory buffer for immutable samplers", sizeof(VkSampler) * VkBinding.descriptorCount));
        for(uint32_t s=0; s < VkBinding.descriptorCount; ++s)
            pStaticSamplers[s] = vkStaticSampler;
        VkBinding.pImmutableSamplers = pStaticSamplers;
    }
    else
        VkBinding.pImmutableSamplers = nullptr;

    OffsetInCache = DescrSet.TotalDescriptors;
    DescrSet.AddBinding(VkBinding, m_MemAllocator);
}

PipelineLayout::PipelineLayout() : 
    m_MemAllocator(GetRawAllocator()),
    m_LayoutMgr(m_MemAllocator)
{
}

void PipelineLayout::Release(RenderDeviceVkImpl *pDeviceVkImpl)
{
    m_LayoutMgr.Release(pDeviceVkImpl);
}

void PipelineLayout::AllocateResourceSlot(const SPIRVShaderResourceAttribs& ResAttribs,
                                          VkSampler                         vkStaticSampler,
                                          SHADER_TYPE                       ShaderType,
                                          Uint32&                           DescriptorSet, // Output parameter
                                          Uint32&                           Binding, // Output parameter
                                          Uint32&                           OffsetInCache,
                                          std::vector<uint32_t>&            SPIRV)
{
    m_LayoutMgr.AllocateResourceSlot(ResAttribs, vkStaticSampler, ShaderType, DescriptorSet, Binding, OffsetInCache);
    SPIRV[ResAttribs.BindingDecorationOffset] = Binding;
    SPIRV[ResAttribs.DescriptorSetDecorationOffset] = DescriptorSet;
}

void PipelineLayout::Finalize(const VulkanUtilities::VulkanLogicalDevice& LogicalDevice)
{
    m_LayoutMgr.Finalize(LogicalDevice);
}


void PipelineLayout::InitResourceCache(RenderDeviceVkImpl *pDeviceVkImpl, ShaderResourceCacheVk& ResourceCache, IMemoryAllocator &CacheMemAllocator)const
{
    Uint32 NumSets = 0;
    std::array<Uint32, 2> SetSizes = {};

    const auto &StaticAndMutSet = m_LayoutMgr.GetDescriptorSet(SHADER_VARIABLE_TYPE_STATIC);
    if(StaticAndMutSet.SetIndex >= 0)
    {
        NumSets = std::max(NumSets, static_cast<Uint32>(StaticAndMutSet.SetIndex + 1));
        SetSizes[StaticAndMutSet.SetIndex] = StaticAndMutSet.TotalDescriptors;
    }

    const auto &DynamicSet = m_LayoutMgr.GetDescriptorSet(SHADER_VARIABLE_TYPE_DYNAMIC);
    if(DynamicSet.SetIndex >= 0)
    {
        NumSets = std::max(NumSets, static_cast<Uint32>(DynamicSet.SetIndex + 1));
        SetSizes[DynamicSet.SetIndex] = DynamicSet.TotalDescriptors;
    }
  
    // This call only initializes descriptor sets (ShaderResourceCacheVk::DescriptorSet) in the resource cache
    // Resources are initialized by source layout when shader resource binding objects are created
    ResourceCache.InitializeSets(CacheMemAllocator, NumSets, SetSizes.data());
    if (StaticAndMutSet.SetIndex >= 0)
    {
        DescriptorPoolAllocation SetAllocation = pDeviceVkImpl->AllocateDescriptorSet(StaticAndMutSet.VkLayout);
        ResourceCache.GetDescriptorSet(StaticAndMutSet.SetIndex).AssignDescriptorSetAllocation(std::move(SetAllocation));
    }
}

void PipelineLayout::AllocateDynamicDescriptorSet(DeviceContextVkImpl*    pCtxVkImpl,
                                                  ShaderResourceCacheVk&  ResourceCache)const
{
    const auto &DynSet = m_LayoutMgr.GetDescriptorSet(SHADER_VARIABLE_TYPE_DYNAMIC);
    if (DynSet.SetIndex >= 0)
    {
        auto DynamicSetAllocation = pCtxVkImpl->AllocateDynamicDescriptorSet(DynSet.VkLayout);
        auto &DynamicSetCache = ResourceCache.GetDescriptorSet(DynSet.SetIndex);
        DynamicSetCache.AssignDescriptorSetAllocation(std::move(DynamicSetAllocation));
    }
}

void PipelineLayout::BindDescriptorSets(DeviceContextVkImpl*    pCtxVkImpl,
                                        bool                    IsCompute,
                                        ShaderResourceCacheVk&  ResourceCache)const
{
    uint32_t SetCount = 0;
    std::array<VkDescriptorSet, 2> vkSets = {};

    VERIFY(m_LayoutMgr.GetDescriptorSet(SHADER_VARIABLE_TYPE_STATIC).SetIndex == m_LayoutMgr.GetDescriptorSet(SHADER_VARIABLE_TYPE_MUTABLE).SetIndex, 
           "Static and mutable variables are expected to share the same descriptor set");
    Uint32 TotalDynamicDescriptors = 0;
    for(SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_MUTABLE; VarType <= SHADER_VARIABLE_TYPE_DYNAMIC; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType+1))
    {
        const auto &Set = m_LayoutMgr.GetDescriptorSet(VarType);
        if(Set.SetIndex >= 0)
        {
            SetCount = std::max(SetCount, static_cast<uint32_t>(Set.SetIndex+1));
            VERIFY_EXPR(vkSets[Set.SetIndex] == VK_NULL_HANDLE);
            vkSets[Set.SetIndex] = ResourceCache.GetDescriptorSet(Set.SetIndex).GetVkDescriptorSet();
            VERIFY(vkSets[Set.SetIndex] != VK_NULL_HANDLE, "Descriptor set must not be null");
        }
        TotalDynamicDescriptors += Set.NumDynamicDescriptors;
    }

#ifdef _DEBUG
    for (uint32_t i = 0; i < SetCount; ++i)
        VERIFY(vkSets[i] != VK_NULL_HANDLE, "Descriptor set must not be null");
#endif

    auto& DynamicOffsets = pCtxVkImpl->GetDynamicBufferOffsets();
    DynamicOffsets.resize(TotalDynamicDescriptors);
    ResourceCache.GetDynamicBufferOffset(pCtxVkImpl->GetContextId(), DynamicOffsets);

    auto& CmdBuffer = pCtxVkImpl->GetCommandBuffer();
    // vkCmdBindDescriptorSets causes the sets numbered [firstSet .. firstSet+descriptorSetCount-1] to use the 
    // bindings stored in pDescriptorSets[0 .. descriptorSetCount-1] for subsequent rendering commands 
    // (either compute or graphics, according to the pipelineBindPoint). Any bindings that were previously 
    // applied via these sets are no longer valid (13.2.5)
    CmdBuffer.BindDescriptorSets(IsCompute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 m_LayoutMgr.GetVkPipelineLayout(), 
                                 0, // First set
                                 SetCount,
                                 vkSets.data(),
                                 // dynamicOffsetCount must equal the total number of dynamic descriptors in the sets being bound (13.2.5)
                                 static_cast<uint32_t>(DynamicOffsets.size()),
                                 DynamicOffsets.data());
}

}
