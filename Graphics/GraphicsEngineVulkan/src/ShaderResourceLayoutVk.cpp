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

#include "ShaderResourceLayoutVk.h"
#include "ShaderResourceCacheVk.h"
#include "BufferVkImpl.h"
#include "BufferViewVk.h"
#include "TextureVkImpl.h"
#include "TextureViewVkImpl.h"
#include "SamplerVkImpl.h"
#include "ShaderVkImpl.h"
#include "PipelineLayout.h"
#include "PipelineStateVkImpl.h"

namespace Diligent
{
 
ShaderResourceLayoutVk::ShaderResourceLayoutVk(IObject&                                    Owner, 
                                               const VulkanUtilities::VulkanLogicalDevice& LogicalDevice) :
    m_Owner(Owner),
    m_LogicalDevice(LogicalDevice)
{
}

ShaderResourceLayoutVk::~ShaderResourceLayoutVk()
{
    auto* Resources = reinterpret_cast<VkResource*>(m_ResourceBuffer.get());
    for(Uint32 r=0; r < GetTotalResourceCount(); ++r)
        Resources[r].~VkResource();
}

void ShaderResourceLayoutVk::AllocateMemory(std::shared_ptr<const SPIRVShaderResources>  pSrcResources, 
                                            IMemoryAllocator&                            Allocator,
                                            const SHADER_VARIABLE_TYPE*                  AllowedVarTypes,
                                            Uint32                                       NumAllowedTypes)
{
    VERIFY(!m_ResourceBuffer, "Memory has already been initialized");
    VERIFY_EXPR(!m_pResources);
    VERIFY_EXPR(pSrcResources);
    
    m_pResources = std::move(pSrcResources);

    Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    
    // Count number of resources to allocate all needed memory
    m_pResources->ProcessResources(
        AllowedVarTypes, NumAllowedTypes,

        [&](const SPIRVShaderResourceAttribs& ResAttribs, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(ResAttribs.VarType, AllowedTypeBits));
            VERIFY( Uint32{m_NumResources[ResAttribs.VarType]} + 1 <= Uint32{std::numeric_limits<Uint16>::max()}, "Number of resources exceeds max representable value");
            ++m_NumResources[ResAttribs.VarType];
        }
    );

    Uint32 TotalResources = 0;
    for(SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType+1))
    {
        TotalResources += m_NumResources[VarType];
    }
    VERIFY(TotalResources <= Uint32{std::numeric_limits<Uint16>::max()}, "Total number of resources exceeds Uint16 max representable value" );
    m_NumResources[SHADER_VARIABLE_TYPE_NUM_TYPES] = static_cast<Uint16>(TotalResources);

    size_t MemSize = TotalResources * sizeof(VkResource);
    if(MemSize == 0)
        return;

    auto *pRawMem = ALLOCATE(Allocator, "Raw memory buffer for shader resource layout resources", MemSize);
    m_ResourceBuffer = std::unique_ptr<void, STDDeleterRawMem<void> >(pRawMem, Allocator);
}

void ShaderResourceLayoutVk::InitializeStaticResourceLayout(std::shared_ptr<const SPIRVShaderResources> pSrcResources,
                                                            IMemoryAllocator&                           LayoutDataAllocator,
                                                            ShaderResourceCacheVk&                      StaticResourceCache)
{
    auto AllowedVarType = SHADER_VARIABLE_TYPE_STATIC;
    AllocateMemory(std::move(pSrcResources), LayoutDataAllocator, &AllowedVarType, 1);

    std::array<Uint32, SHADER_VARIABLE_TYPE_NUM_TYPES> CurrResInd = {};
    Uint32 StaticResCacheSize = 0;

    m_pResources->ProcessResources(
        &AllowedVarType, 1,
        [&](const SPIRVShaderResourceAttribs& Attribs, Uint32)
        {
            Uint32 Binding       = Attribs.Type;
            Uint32 DescriptorSet = 0;
            Uint32 CacheOffset   = StaticResCacheSize;
            StaticResCacheSize += Attribs.ArraySize;
            
            Uint32 SamplerInd = VkResource::InvalidSamplerInd;
            if (Attribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage)
            {
                // Separate samplers are enumerated before separate images, so the sampler
                // assigned to this separate image must already be created.
                SamplerInd = FindAssignedSampler(Attribs, CurrResInd[Attribs.VarType]);
            }
            ::new (&GetResource(Attribs.VarType, CurrResInd[Attribs.VarType]++)) VkResource(*this, Attribs, Binding, DescriptorSet, CacheOffset, SamplerInd);
        }
    );

#ifdef _DEBUG
    for (SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType + 1))
    {
        VERIFY(CurrResInd[VarType] == m_NumResources[VarType], "Not all resources are initialized, which will cause a crash when dtor is called");
    }
#endif

    StaticResourceCache.InitializeSets(GetRawAllocator(), 1, &StaticResCacheSize);
    InitializeResourceMemoryInCache(StaticResourceCache);
}


void ShaderResourceLayoutVk::Initialize(Uint32 NumShaders,
                                        ShaderResourceLayoutVk                       Layouts[],
                                        std::shared_ptr<const SPIRVShaderResources>  pShaderResources[],
                                        IMemoryAllocator&                            LayoutDataAllocator,
                                        std::vector<uint32_t>                        SPIRVs[],
                                        class PipelineLayout&                        PipelineLayout)
{
    SHADER_VARIABLE_TYPE* AllowedVarTypes = nullptr;
    Uint32                NumAllowedTypes = 0;
    Uint32                AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);

    for(Uint32 s=0; s < NumShaders; ++s)
    {
        Layouts[s].AllocateMemory(std::move(pShaderResources[s]), LayoutDataAllocator, AllowedVarTypes, NumAllowedTypes);
    }
    
    VERIFY_EXPR(NumShaders <= MaxShadersInPipeline);
    std::array<std::array<Uint32, SHADER_VARIABLE_TYPE_NUM_TYPES>, MaxShadersInPipeline> CurrResInd = {};
#ifdef _DEBUG
    std::unordered_map<Uint32, std::pair<Uint32, Uint32>> dbgBindings_CacheOffsets;
#endif

    auto AddResource = [&](Uint32                            ShaderInd,
                           ShaderResourceLayoutVk&           ResLayout,
                           const SPIRVShaderResources&       Resources, 
                           const SPIRVShaderResourceAttribs& Attribs,
                           Uint32                            SamplerInd = VkResource::InvalidSamplerInd)
    {
        Uint32 Binding = 0;
        Uint32 DescriptorSet = 0;
        Uint32 CacheOffset = 0;

        auto* pStaticSampler = Resources.GetStaticSampler(Attribs);
        VkSampler vkStaticSampler = VK_NULL_HANDLE;
        if (pStaticSampler != nullptr)
            vkStaticSampler = ValidatedCast<SamplerVkImpl>(pStaticSampler)->GetVkSampler();

        auto& ShaderSPIRV = SPIRVs[ShaderInd];
        PipelineLayout.AllocateResourceSlot(Attribs, vkStaticSampler, Resources.GetShaderType(), DescriptorSet, Binding, CacheOffset, ShaderSPIRV);
        VERIFY(DescriptorSet <= std::numeric_limits<decltype(VkResource::DescriptorSet)>::max(), "Descriptor set (", DescriptorSet, ") excceeds max representable value");
        VERIFY(Binding <= std::numeric_limits<decltype(VkResource::Binding)>::max(), "Binding (", Binding, ") excceeds max representable value");

#ifdef _DEBUG
        // Verify that bindings and cache offsets monotonically increase in every descriptor set
        auto Binding_OffsetIt = dbgBindings_CacheOffsets.find(DescriptorSet);
        if(Binding_OffsetIt != dbgBindings_CacheOffsets.end())
        {
            VERIFY(Binding     > Binding_OffsetIt->second.first,  "Binding for descriptor set ", DescriptorSet, " is not strictly monotonic");
            VERIFY(CacheOffset > Binding_OffsetIt->second.second, "Cache offset for descriptor set ", DescriptorSet, " is not strictly monotonic");
        }
        dbgBindings_CacheOffsets[DescriptorSet] = std::make_pair(Binding, CacheOffset);
#endif

        auto& ResInd = CurrResInd[ShaderInd][Attribs.VarType];
        ::new (&ResLayout.GetResource(Attribs.VarType, ResInd++)) VkResource(ResLayout, Attribs, Binding, DescriptorSet, CacheOffset, SamplerInd);
    };

    // First process uniform buffers for all shader stages to make sure all UBs go first in every descriptor set
    for (Uint32 s = 0; s < NumShaders; ++s)
    {
        auto& Layout = Layouts[s];
        const auto& Resources = *Layout.m_pResources;
        for (Uint32 n = 0; n < Resources.GetNumUBs(); ++n)
        {
            const auto& UB = Resources.GetUB(n);
            if (IsAllowedType(UB.VarType, AllowedTypeBits))
            {
                AddResource(s, Layout, Resources, UB);
            }
        }
    }

    // Second, process all storage buffers
    for (Uint32 s = 0; s < NumShaders; ++s)
    {
        auto& Layout = Layouts[s];
        const auto& Resources = *Layout.m_pResources;
        for (Uint32 n = 0; n < Resources.GetNumSBs(); ++n)
        {
            const auto& SB = Resources.GetSB(n);
            if (IsAllowedType(SB.VarType, AllowedTypeBits))
            {
                AddResource(s, Layout, Resources, SB);
            }
        }
    }

    // Finally, process all other resource types
    for (Uint32 s = 0; s < NumShaders; ++s)
    {
        auto& Layout = Layouts[s];
        const auto& Resources = *Layout.m_pResources;
        Resources.ProcessResources(
            AllowedVarTypes, NumAllowedTypes,

            [&](const SPIRVShaderResourceAttribs& UB, Uint32)
            {
                VERIFY_EXPR(UB.Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer);
                VERIFY_EXPR(IsAllowedType(UB.VarType, AllowedTypeBits));
                // Skip
            },
            [&](const SPIRVShaderResourceAttribs& SB, Uint32)
            {
                VERIFY_EXPR(SB.Type == SPIRVShaderResourceAttribs::ResourceType::StorageBuffer);
                VERIFY_EXPR(IsAllowedType(SB.VarType, AllowedTypeBits));
                // Skip
            },
            [&](const SPIRVShaderResourceAttribs& Img, Uint32)
            {
                VERIFY_EXPR(Img.Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage || Img.Type == SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer);
                VERIFY_EXPR(IsAllowedType(Img.VarType, AllowedTypeBits));
                AddResource(s, Layout, Resources, Img);
            },
            [&](const SPIRVShaderResourceAttribs& SmplImg, Uint32)
            {
                VERIFY_EXPR(SmplImg.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage || SmplImg.Type == SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer);
                VERIFY_EXPR(IsAllowedType(SmplImg.VarType, AllowedTypeBits));
                AddResource(s, Layout, Resources, SmplImg);
            },
            [&](const SPIRVShaderResourceAttribs& AC, Uint32)
            {
                VERIFY_EXPR(AC.Type == SPIRVShaderResourceAttribs::ResourceType::AtomicCounter);
                VERIFY_EXPR(IsAllowedType(AC.VarType, AllowedTypeBits));
                AddResource(s, Layout, Resources, AC);
            },
            [&](const SPIRVShaderResourceAttribs& SepSmpl, Uint32)
            {
                VERIFY_EXPR(SepSmpl.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler);
                VERIFY_EXPR(IsAllowedType(SepSmpl.VarType, AllowedTypeBits));
                AddResource(s, Layout, Resources, SepSmpl);
            },
            [&](const SPIRVShaderResourceAttribs& SepImg, Uint32)
            {
                VERIFY_EXPR(SepImg.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage);
                VERIFY_EXPR(IsAllowedType(SepImg.VarType, AllowedTypeBits));
                Uint32 SamplerInd = Layout.FindAssignedSampler(SepImg, CurrResInd[s][SepImg.VarType]);
                AddResource(s, Layout, Resources, SepImg, SamplerInd);
            }
        );
    }

#ifdef _DEBUG
    for (Uint32 s = 0; s < NumShaders; ++s)
    {
        auto& Layout = Layouts[s];
        for (SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType + 1))
        {
            VERIFY(CurrResInd[s][VarType] == Layout.m_NumResources[VarType], "Not all resources are initialized, which will cause a crash when dtor is called");
        }
    }
#endif
}


Uint32 ShaderResourceLayoutVk::FindAssignedSampler(const SPIRVShaderResourceAttribs& SepImg, Uint32 CurrResourceCount)const
{
    VERIFY_EXPR(SepImg.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage);
    
    Uint32 SamplerInd = VkResource::InvalidSamplerInd;
    if (m_pResources->IsUsingCombinedSamplers() && SepImg.ValidSepSamplerAssigned())
    {
        const auto& SepSampler = m_pResources->GetSepSmplr(SepImg.SepSmplrOrImgInd);
        DEV_CHECK_ERR(SepImg.VarType == SepSampler.VarType,
                        "The type (", GetShaderVariableTypeLiteralName(SepImg.VarType),") of separate image variable '", SepImg.Name,
                        "' is not consistent with the type (", GetShaderVariableTypeLiteralName(SepSampler.VarType),
                        ") of the separate sampler '", SepSampler.Name, "' that is assigned to it.");
                    
        for (SamplerInd = 0; SamplerInd < CurrResourceCount; ++SamplerInd)
        {
            const auto& Res = GetResource(SepSampler.VarType, SamplerInd);
            if (Res.SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler && 
                strcmp(Res.SpirvAttribs.Name, SepSampler.Name) == 0)
            {
                break;
            }
        }
        if (SamplerInd == CurrResourceCount)
        {
            LOG_ERROR("Unable to find separate sampler '", SepSampler.Name, "' assigned to separate image '", SepImg.Name, "' in the list of already created resources. This seems to be a bug.");
            SamplerInd = VkResource::InvalidSamplerInd;
        }
    }
    return SamplerInd;
}

#define LOG_RESOURCE_BINDING_ERROR(ResType, pResource, VarName, ShaderName, ...)\
{                                                                                             \
    const auto &ResName = pResource->GetDesc().Name;                                          \
    LOG_ERROR_MESSAGE( "Failed to bind ", ResType, " '", ResName, "' to variable '", VarName, \
                        "' in shader '", ShaderName, "'. ", __VA_ARGS__ );                    \
}

void ShaderResourceLayoutVk::VkResource::UpdateDescriptorHandle(VkDescriptorSet                vkDescrSet,
                                                                uint32_t                       ArrayElement,
                                                                const VkDescriptorImageInfo*   pImageInfo,
                                                                const VkDescriptorBufferInfo*  pBufferInfo,
                                                                const VkBufferView*            pTexelBufferView)const
{
    VERIFY_EXPR(vkDescrSet != VK_NULL_HANDLE);

    VkWriteDescriptorSet WriteDescrSet;
    WriteDescrSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    WriteDescrSet.pNext = nullptr;
    WriteDescrSet.dstSet = vkDescrSet;
    WriteDescrSet.dstBinding = Binding;
    WriteDescrSet.dstArrayElement = ArrayElement;
    WriteDescrSet.descriptorCount = 1;
    // descriptorType must be the same type as that specified in VkDescriptorSetLayoutBinding for dstSet at dstBinding. 
    // The type of the descriptor also controls which array the descriptors are taken from. (13.2.4)
    WriteDescrSet.descriptorType = PipelineLayout::GetVkDescriptorType(SpirvAttribs);
    WriteDescrSet.pImageInfo = pImageInfo;
    WriteDescrSet.pBufferInfo = pBufferInfo;
    WriteDescrSet.pTexelBufferView = pTexelBufferView;

    ParentResLayout.m_LogicalDevice.UpdateDescriptorSets(1, &WriteDescrSet, 0, nullptr);
}

bool ShaderResourceLayoutVk::VkResource::UpdateCachedResource(ShaderResourceCacheVk::Resource&   DstRes,
                                                              Uint32                             ArrayInd,
                                                              IDeviceObject*                     pObject, 
                                                              INTERFACE_ID                       InterfaceId,
                                                              const char*                        ResourceName)const
{
    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<IDeviceObject> pResource(pObject, InterfaceId);
    if(pResource)
    {
        if (SpirvAttribs.VarType != SHADER_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr)
        {
            if (DstRes.pObject != pResource)
            {
                auto VarTypeStr = GetShaderVariableTypeLiteralName(SpirvAttribs.VarType);
                LOG_ERROR_MESSAGE("Non-null resource is already bound to ", VarTypeStr, " shader variable '", SpirvAttribs.GetPrintName(ArrayInd), "' in shader '", ParentResLayout.GetShaderName(), "'. Attempring to bind another resource is an error and will be ignored. Use another shader resource binding instance or label the variable as dynamic.");
            }

            // Do not update resource if one is already bound unless it is dynamic. This may be 
            // dangerous as writing descriptors while they are used by the GPU is an undefined behavior
            return false;
        }

        DstRes.pObject.Attach(pResource.Detach());
        return true;
    }
    else
    {
        LOG_RESOURCE_BINDING_ERROR(ResourceName, pObject, SpirvAttribs.GetPrintName(ArrayInd), ParentResLayout.GetShaderName(), "Incorrect resource type: ", ResourceName, " is expected.");
        return false;
    }
}

void ShaderResourceLayoutVk::VkResource::CacheUniformBuffer(IDeviceObject*                     pBuffer,
                                                            ShaderResourceCacheVk::Resource&   DstRes, 
                                                            VkDescriptorSet                    vkDescrSet, 
                                                            Uint32                             ArrayInd)const
{
    VERIFY(SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer, "Uniform buffer resource is expected");

    if( UpdateCachedResource(DstRes, ArrayInd, pBuffer, IID_BufferVk, "buffer") )
    {
#ifdef DEVELOPMENT
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC descriptor type require
        // buffer to be created with VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
        auto* pBuffVk = DstRes.pObject.RawPtr<BufferVkImpl>(); // Use final type
        if( (pBuffVk->GetDesc().BindFlags & BIND_UNIFORM_BUFFER) == 0)
        {
            LOG_RESOURCE_BINDING_ERROR("buffer", pBuffer, SpirvAttribs.GetPrintName(ArrayInd), ParentResLayout.GetShaderName(), "Buffer was not created with BIND_UNIFORM_BUFFER flag.")
            DstRes.pObject.Release();
            return;
        }
#endif

        // Do not update descriptor for a dynamic uniform buffer. All dynamic resource 
        // descriptors are updated at once by CommitDynamicResources() when SRB is committed.
        if(vkDescrSet != VK_NULL_HANDLE && SpirvAttribs.VarType != SHADER_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorBufferInfo DescrBuffInfo = DstRes.GetUniformBufferDescriptorWriteInfo();
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, nullptr, &DescrBuffInfo, nullptr);
        }
    }
}

void ShaderResourceLayoutVk::VkResource::CacheStorageBuffer(IDeviceObject*                     pBuffer,
                                                            ShaderResourceCacheVk::Resource&   DstRes, 
                                                            VkDescriptorSet                    vkDescrSet, 
                                                            Uint32                             ArrayInd)const
{
    VERIFY(SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::StorageBuffer, "Storage buffer resource is expected");

    if( UpdateCachedResource(DstRes, ArrayInd, pBuffer, IID_BufferViewVk, "buffer view") )
    {
#ifdef DEVELOPMENT
        // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC descriptor type 
        // require buffer to be created with VK_BUFFER_USAGE_STORAGE_BUFFER_BIT (13.2.4)
        auto* pBuffViewVk = DstRes.pObject.RawPtr<BufferViewVkImpl>();
        auto* pBuffVk = pBuffViewVk->GetBufferVk(); // Use final type
        if( (pBuffVk->GetDesc().BindFlags & BIND_UNORDERED_ACCESS) == 0)
        {
            LOG_RESOURCE_BINDING_ERROR("buffer", pBuffer, SpirvAttribs.GetPrintName(ArrayInd), ParentResLayout.GetShaderName(), "Buffer was not created with BIND_UNORDERED_ACCESS flag.")
            DstRes.pObject.Release();
            return;
        }
#endif

        // Do not update descriptor for a dynamic storage buffer. All dynamic resource 
        // descriptors are updated at once by CommitDynamicResources() when SRB is committed.
        if(vkDescrSet != VK_NULL_HANDLE && SpirvAttribs.VarType != SHADER_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorBufferInfo DescrBuffInfo = DstRes.GetStorageBufferDescriptorWriteInfo();
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, nullptr, &DescrBuffInfo, nullptr);
        }
    }
}

void ShaderResourceLayoutVk::VkResource::CacheTexelBuffer(IDeviceObject*                     pBufferView,
                                                          ShaderResourceCacheVk::Resource&   DstRes, 
                                                          VkDescriptorSet                    vkDescrSet,
                                                          Uint32                             ArrayInd)const
{
    VERIFY(SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer || 
           SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer,
           "Uniform or storage buffer resource is expected");

    if (UpdateCachedResource(DstRes, ArrayInd, pBufferView, IID_BufferViewVk, "buffer view"))
    {
        auto* pBuffViewVk = DstRes.pObject.RawPtr<BufferViewVkImpl>();

#ifdef DEVELOPMENT
        // The following bits must have been set at buffer creation time:
        //  * VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER  ->  VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
        //  * VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER  ->  VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
        const auto ViewType = pBuffViewVk->GetDesc().ViewType;
        const bool IsStorageBuffer = SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer;
        const auto dbgExpectedViewType = IsStorageBuffer ? BUFFER_VIEW_UNORDERED_ACCESS : BUFFER_VIEW_SHADER_RESOURCE;
        if (ViewType != dbgExpectedViewType)
        {
            const auto *ExpectedViewTypeName = GetViewTypeLiteralName(dbgExpectedViewType);
            const auto *ActualViewTypeName = GetViewTypeLiteralName(ViewType);
            LOG_RESOURCE_BINDING_ERROR("Texture view", pBuffViewVk, SpirvAttribs.GetPrintName(ArrayInd), ParentResLayout.GetShaderName(),
                                       "Incorrect view type: ", ExpectedViewTypeName, " is expected, but ", ActualViewTypeName, " is provided.");
            DstRes.pObject.Release();
            return;
        }
#endif

        // Do not update descriptor for a dynamic texel buffer. All dynamic resource descriptors 
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && SpirvAttribs.VarType != SHADER_VARIABLE_TYPE_DYNAMIC)
        {
            VkBufferView BuffView = pBuffViewVk->GetVkBufferView();
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, nullptr, nullptr, &BuffView);
        }
    }
}

template<typename TCacheSampler>
void ShaderResourceLayoutVk::VkResource::CacheImage(IDeviceObject*                   pTexView,
                                                    ShaderResourceCacheVk::Resource& DstRes,
                                                    VkDescriptorSet                  vkDescrSet,
                                                    Uint32                           ArrayInd,
                                                    TCacheSampler                    CacheSampler)const
{
    VERIFY(SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage  || 
           SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage ||
           SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage,
           "Storage image, separate image or sampled image resource is expected");

    if (UpdateCachedResource(DstRes, ArrayInd, pTexView, IID_TextureViewVk, "texture view") )
    {
        // We can do RawPtr here safely since UpdateCachedResource() returned true
        auto* pTexViewVk = DstRes.pObject.RawPtr<TextureViewVkImpl>();
#ifdef DEVELOPMENT
        const auto ViewType = pTexViewVk->GetDesc().ViewType;
        const bool IsStorageImage = SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage;
        const auto dbgExpectedViewType = IsStorageImage ? TEXTURE_VIEW_UNORDERED_ACCESS : TEXTURE_VIEW_SHADER_RESOURCE;
        if (ViewType != dbgExpectedViewType)
        {
            const auto *ExpectedViewTypeName = GetViewTypeLiteralName(dbgExpectedViewType);
            const auto *ActualViewTypeName = GetViewTypeLiteralName(ViewType);
            LOG_RESOURCE_BINDING_ERROR("Texture view", pTexViewVk, SpirvAttribs.GetPrintName(ArrayInd), ParentResLayout.GetShaderName(),
                                       "Incorrect view type: ", ExpectedViewTypeName, " is expected, but ", ActualViewTypeName, " is provided.");
            DstRes.pObject.Release();
            return;
        }

        if (SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage && SpirvAttribs.StaticSamplerInd < 0)
        {
            if(pTexViewVk->GetSampler() == nullptr)
            {
                LOG_RESOURCE_BINDING_ERROR("resource", pTexView, SpirvAttribs.GetPrintName(ArrayInd), ParentResLayout.GetShaderName(), "No sampler assigned to texture view '", pTexViewVk->GetDesc().Name, "'");
            }
        }
#endif

        // Do not update descriptor for a dynamic image. All dynamic resource descriptors 
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && SpirvAttribs.VarType != SHADER_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorImageInfo DescrImgInfo = DstRes.GetImageDescriptorWriteInfo(SpirvAttribs.StaticSamplerInd >= 0);
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, &DescrImgInfo, nullptr, nullptr);
        }

        if (SamplerInd != InvalidSamplerInd)
        {
            VERIFY_EXPR(SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage);
            VERIFY_EXPR(SpirvAttribs.StaticSamplerInd < 0);
            auto* pSampler = pTexViewVk->GetSampler();
            const auto& SamplerAttribs = ParentResLayout.GetResource(SpirvAttribs.VarType, SamplerInd);
            if (pSampler != nullptr)
            {
                CacheSampler(SamplerAttribs, pSampler);
            }
            else
            {
                LOG_ERROR_MESSAGE( "Failed to bind sampler to sampler variable '", SamplerAttribs.SpirvAttribs.Name,
                                   "' assigned to separate image '", SpirvAttribs.GetPrintName(ArrayInd), "' in shader '",
                                   ParentResLayout.GetShaderName(), "': no sampler is set in texture view '", pTexViewVk->GetDesc().Name, '\'');                    \
            }
        }
    }
}

void ShaderResourceLayoutVk::VkResource::CacheSeparateSampler(IDeviceObject*                    pSampler,
                                                              ShaderResourceCacheVk::Resource&  DstRes,
                                                              VkDescriptorSet                   vkDescrSet,
                                                              Uint32                            ArrayInd)const
{
    VERIFY(SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler, "Separate sampler resource is expected");

    if (UpdateCachedResource(DstRes, ArrayInd, pSampler, IID_Sampler, "sampler"))
    {
        // Do not update descriptor for a dynamic sampler. All dynamic resource descriptors 
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && SpirvAttribs.VarType != SHADER_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorImageInfo DescrImgInfo = DstRes.GetSamplerDescriptorWriteInfo();
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, &DescrImgInfo, nullptr, nullptr);
        }
    }
}


void ShaderResourceLayoutVk::VkResource::BindResource(IDeviceObject *pObj, Uint32 ArrayIndex, ShaderResourceCacheVk& ResourceCache)const
{
    VERIFY_EXPR(ArrayIndex < SpirvAttribs.ArraySize);

    auto &DstDescrSet = ResourceCache.GetDescriptorSet(DescriptorSet);
    auto vkDescrSet = DstDescrSet.GetVkDescriptorSet();
#ifdef _DEBUG
    if (ResourceCache.DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::SRBResources)
    {
        if(SpirvAttribs.VarType == SHADER_VARIABLE_TYPE_STATIC || SpirvAttribs.VarType == SHADER_VARIABLE_TYPE_MUTABLE)
        {
            VERIFY(vkDescrSet != VK_NULL_HANDLE, "Static and mutable variables must have valid vulkan descriptor set assigned");
            // Dynamic variables do not have vulkan descriptor set only until they are assigned one the first time
        }
    }
    else if (ResourceCache.DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::StaticShaderResources)
    {
        VERIFY(vkDescrSet == VK_NULL_HANDLE, "Static shader resource cache should not have vulkan descriptor set allocation");
    }
    else
    {
        UNEXPECTED("Unexpected shader resource cache content type");
    }
#endif
    auto &DstRes = DstDescrSet.GetResource(CacheOffset + ArrayIndex);
    VERIFY(DstRes.Type == SpirvAttribs.Type, "Inconsistent types");

    if( pObj )
    {
        switch (SpirvAttribs.Type)
        {
            case SPIRVShaderResourceAttribs::ResourceType::UniformBuffer:
                CacheUniformBuffer(pObj, DstRes, vkDescrSet, ArrayIndex);
            break;

            case SPIRVShaderResourceAttribs::ResourceType::StorageBuffer:
                CacheStorageBuffer(pObj, DstRes, vkDescrSet, ArrayIndex);
            break;

            case SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer:
            case SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer:
                CacheTexelBuffer(pObj, DstRes, vkDescrSet, ArrayIndex);
                break;

            case SPIRVShaderResourceAttribs::ResourceType::StorageImage:
            case SPIRVShaderResourceAttribs::ResourceType::SeparateImage:
            case SPIRVShaderResourceAttribs::ResourceType::SampledImage:
                CacheImage(pObj, DstRes, vkDescrSet, ArrayIndex, 
                    [&](const VkResource& SeparateSampler, ISampler* pSampler)
                    {
                        DEV_CHECK_ERR(SeparateSampler.SpirvAttribs.ArraySize == 1 || SeparateSampler.SpirvAttribs.ArraySize == SpirvAttribs.ArraySize,
                                      "Array size (", SeparateSampler.SpirvAttribs.ArraySize,") of separate sampler variable '",
                                      SeparateSampler.SpirvAttribs.Name, "' must be one or same as the array size (", SpirvAttribs.ArraySize,
                                      ") of separate image variable '", SpirvAttribs.Name, "' it is assigned to");
                        Uint32 SamplerArrInd = SeparateSampler.SpirvAttribs.ArraySize == 0 ? ArrayIndex : 0;
                        SeparateSampler.BindResource(pSampler, SamplerArrInd, ResourceCache);
                    }
                );
            break;

            case SPIRVShaderResourceAttribs::ResourceType::SeparateSampler:
                if(SpirvAttribs.StaticSamplerInd < 0)
                {
                    CacheSeparateSampler(pObj, DstRes, vkDescrSet, ArrayIndex);
                }
                else
                {
                    // Immutable samplers are permanently bound into the set layout; later binding a sampler 
                    // into an immutable sampler slot in a descriptor set is not allowed (13.2.1)
                    LOG_ERROR_MESSAGE("Attempting to assign a sampler to a static sampler '", SpirvAttribs.Name, '\'');
                }
            break;

            default: UNEXPECTED("Unknown resource type ", static_cast<Int32>(SpirvAttribs.Type));
        }
    }
    else
    {
        if (DstRes.pObject && SpirvAttribs.VarType != SHADER_VARIABLE_TYPE_DYNAMIC)
        {
            LOG_ERROR_MESSAGE( "Shader variable '", SpirvAttribs.Name, "' in shader '", ParentResLayout.GetShaderName(), "' is not dynamic but being unbound. This is an error and may cause unpredicted behavior. Use another shader resource binding instance or label shader variable as dynamic if you need to bind another resource." );
        }

        DstRes.pObject.Release();
    }
}

bool ShaderResourceLayoutVk::VkResource::IsBound(Uint32 ArrayIndex, const ShaderResourceCacheVk& ResourceCache)const
{
    VERIFY_EXPR(ArrayIndex < SpirvAttribs.ArraySize);

    if( DescriptorSet < ResourceCache.GetNumDescriptorSets() )
    {
        auto &Set = ResourceCache.GetDescriptorSet(DescriptorSet);
        if(CacheOffset + ArrayIndex < Set.GetSize())
        {
            auto &CachedRes = Set.GetResource(CacheOffset + ArrayIndex);
            return CachedRes.pObject != nullptr;
        }
    }

    return false;
}


void ShaderResourceLayoutVk::InitializeStaticResources(const ShaderResourceLayoutVk& SrcLayout,
                                                       ShaderResourceCacheVk&        SrcResourceCache,
                                                       ShaderResourceCacheVk&        DstResourceCache)const
{
    auto NumStaticResources = m_NumResources[SHADER_VARIABLE_TYPE_STATIC];
    VERIFY(NumStaticResources == SrcLayout.m_NumResources[SHADER_VARIABLE_TYPE_STATIC], "Inconsistent number of static resources");
    VERIFY(SrcLayout.m_pResources->GetShaderType() == m_pResources->GetShaderType(), "Incosistent shader types");

    // Static shader resources are stored in one large continuous descriptor set
    for(Uint32 r=0; r < NumStaticResources; ++r)
    {
        // Get resource attributes
        auto &DstRes = GetResource(SHADER_VARIABLE_TYPE_STATIC, r);
        const auto &SrcRes = SrcLayout.GetResource(SHADER_VARIABLE_TYPE_STATIC, r);
        VERIFY(SrcRes.Binding == SrcRes.SpirvAttribs.Type, "Unexpected binding");
        VERIFY(SrcRes.SpirvAttribs.ArraySize == DstRes.SpirvAttribs.ArraySize, "Inconsistent array size");

        if(DstRes.SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler && 
           DstRes.SpirvAttribs.StaticSamplerInd >= 0)
            continue; // Skip static samplers

        for(Uint32 ArrInd = 0; ArrInd < DstRes.SpirvAttribs.ArraySize; ++ArrInd)
        {
            auto SrcOffset = SrcRes.CacheOffset + ArrInd;
            IDeviceObject* pObject = SrcResourceCache.GetDescriptorSet(SrcRes.DescriptorSet).GetResource(SrcOffset).pObject;
            if (!pObject)
                LOG_ERROR_MESSAGE("No resource assigned to static shader variable '", SrcRes.SpirvAttribs.GetPrintName(ArrInd), "' in shader '", GetShaderName(), "'.");
            
            auto DstOffset = DstRes.CacheOffset + ArrInd;
            IDeviceObject* pCachedResource = DstResourceCache.GetDescriptorSet(DstRes.DescriptorSet).GetResource(DstOffset).pObject;
            if(pCachedResource != pObject)
            {
                VERIFY(pCachedResource == nullptr, "Static resource has already been initialized, and the resource to be assigned from the shader does not match previously assigned resource");
                DstRes.BindResource(pObject, ArrInd, DstResourceCache);
            }
        }
    }
}


#ifdef DEVELOPMENT
void ShaderResourceLayoutVk::dvpVerifyBindings(const ShaderResourceCacheVk& ResourceCache)const
{
    for(SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType+1))
    {
        for(Uint32 r=0; r < m_NumResources[VarType]; ++r)
        {
            const auto &Res = GetResource(VarType, r);
            VERIFY(Res.SpirvAttribs.VarType == VarType, "Unexpected variable type");
            for(Uint32 ArrInd = 0; ArrInd < Res.SpirvAttribs.ArraySize; ++ArrInd)
            {
                auto &CachedDescrSet = ResourceCache.GetDescriptorSet(Res.DescriptorSet);
                const auto &CachedRes = CachedDescrSet.GetResource(Res.CacheOffset + ArrInd);
                VERIFY(CachedRes.Type == Res.SpirvAttribs.Type, "Inconsistent types");
                if(CachedRes.pObject == nullptr && 
                   !(Res.SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler && Res.SpirvAttribs.StaticSamplerInd >= 0))
                {
                    LOG_ERROR_MESSAGE("No resource is bound to ", GetShaderVariableTypeLiteralName(Res.SpirvAttribs.VarType), " variable '", Res.SpirvAttribs.GetPrintName(ArrInd), "' in shader '", GetShaderName(), "'");
                }
#ifdef _DEBUG
                auto vkDescSet = CachedDescrSet.GetVkDescriptorSet();
                auto dbgCacheContentType = ResourceCache.DbgGetContentType();
                if(dbgCacheContentType == ShaderResourceCacheVk::DbgCacheContentType::StaticShaderResources)
                    VERIFY(vkDescSet == VK_NULL_HANDLE, "Static resource cache should never have vulkan descriptor set");
                else if (dbgCacheContentType == ShaderResourceCacheVk::DbgCacheContentType::SRBResources)
                {
                    if (VarType == SHADER_VARIABLE_TYPE_STATIC || VarType == SHADER_VARIABLE_TYPE_MUTABLE)
                    {
                        VERIFY(vkDescSet != VK_NULL_HANDLE, "Static and mutable variables must have valid vulkan descriptor set assigned");
                    }
                    else if (VarType == SHADER_VARIABLE_TYPE_DYNAMIC )
                    {
                        VERIFY(vkDescSet == VK_NULL_HANDLE, "Dynamic variables must not be assigned a vulkan descriptor set");
                    }
                }
                else
                    UNEXPECTED("Unexpected cache content type");
#endif
            }
        }
    }
}
#endif


const Char* ShaderResourceLayoutVk::GetShaderName()const
{
    RefCntAutoPtr<IShader> pShader(&m_Owner, IID_Shader);
    if (pShader)
    {
        return pShader->GetDesc().Name;
    }
    else
    {
        RefCntAutoPtr<IPipelineState> pPSO(&m_Owner, IID_PipelineState);
        if(pPSO)
        {
            auto *pPSOVk = pPSO.RawPtr<PipelineStateVkImpl>();
            auto *ppShaders = pPSOVk->GetShaders();
            auto NumShaders = pPSOVk->GetNumShaders();
            for (Uint32 s = 0; s < NumShaders; ++s)
            {
                const auto &ShaderDesc = ppShaders[s]->GetDesc();
                if(ShaderDesc.ShaderType == m_pResources->GetShaderType())
                    return ShaderDesc.Name;
            }
            UNEXPECTED("Shader not found");
        }
        else
        {
            UNEXPECTED("Shader resource layout owner must be a shader or a pipeline state");
        }
    }
    return "";
}

void ShaderResourceLayoutVk::InitializeResourceMemoryInCache(ShaderResourceCacheVk& ResourceCache)const
{
    auto TotalResources = GetTotalResourceCount();
    for(Uint32 r = 0; r < TotalResources; ++r)
    {
        const auto& Res = GetResource(r);
        ResourceCache.InitializeResources(Res.DescriptorSet, Res.CacheOffset, Res.SpirvAttribs.ArraySize, Res.SpirvAttribs.Type);
    }
}

void ShaderResourceLayoutVk::CommitDynamicResources(const ShaderResourceCacheVk& ResourceCache, 
                                                    VkDescriptorSet              vkDynamicDescriptorSet)const
{
    Uint32 NumDynamicResources = m_NumResources[SHADER_VARIABLE_TYPE_DYNAMIC];
    VERIFY(NumDynamicResources != 0, "This shader resource layout does not contain dynamic resources");
    VERIFY_EXPR(vkDynamicDescriptorSet != VK_NULL_HANDLE);

#ifdef _DEBUG
    static constexpr size_t ImgUpdateBatchSize = 4;
    static constexpr size_t BuffUpdateBatchSize = 2;
    static constexpr size_t TexelBuffUpdateBatchSize = 2;
    static constexpr size_t WriteDescriptorSetBatchSize = 2;
#else
    static constexpr size_t ImgUpdateBatchSize = 128;
    static constexpr size_t BuffUpdateBatchSize = 64;
    static constexpr size_t TexelBuffUpdateBatchSize = 32;
    static constexpr size_t WriteDescriptorSetBatchSize = 32;
#endif

    // Do not zero-initiaize arrays!
    std::array<VkDescriptorImageInfo,  ImgUpdateBatchSize>       DescrImgInfoArr;
    std::array<VkDescriptorBufferInfo, BuffUpdateBatchSize>      DescrBuffInfoArr;
    std::array<VkBufferView,           TexelBuffUpdateBatchSize> DescrBuffViewArr;
    std::array<VkWriteDescriptorSet,   WriteDescriptorSetBatchSize> WriteDescrSetArr;

    Uint32 ResNum = 0, ArrElem = 0;
    auto DescrImgIt  = DescrImgInfoArr.begin();
    auto DescrBuffIt = DescrBuffInfoArr.begin();
    auto BuffViewIt  = DescrBuffViewArr.begin();
    auto WriteDescrSetIt = WriteDescrSetArr.begin();

#ifdef _DEBUG
    Int32 DynamicDescrSetIndex = -1;
#endif

    while(ResNum < NumDynamicResources)
    {
        const auto& Res = GetResource(SHADER_VARIABLE_TYPE_DYNAMIC, ResNum);
        VERIFY_EXPR(Res.SpirvAttribs.VarType == SHADER_VARIABLE_TYPE_DYNAMIC);
#ifdef _DEBUG
        if(DynamicDescrSetIndex < 0)
            DynamicDescrSetIndex = Res.DescriptorSet;
        else
            VERIFY(DynamicDescrSetIndex == Res.DescriptorSet, "Inconsistent dynamic resource desriptor set index");
#endif
        auto& SetResources = ResourceCache.GetDescriptorSet(Res.DescriptorSet);
        WriteDescrSetIt->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        WriteDescrSetIt->pNext = nullptr;
        VERIFY(SetResources.GetVkDescriptorSet() == VK_NULL_HANDLE, "Dynamic descriptor set must not be assigned to the resource cache");
        WriteDescrSetIt->dstSet = vkDynamicDescriptorSet;
        VERIFY(WriteDescrSetIt->dstSet != VK_NULL_HANDLE, "Vulkan descriptor set must not be null");
        WriteDescrSetIt->dstBinding = Res.Binding;
        WriteDescrSetIt->dstArrayElement = ArrElem;
        // descriptorType must be the same type as that specified in VkDescriptorSetLayoutBinding for dstSet at dstBinding. 
        // The type of the descriptor also controls which array the descriptors are taken from. (13.2.4)
        WriteDescrSetIt->descriptorType = PipelineLayout::GetVkDescriptorType(Res.SpirvAttribs);
        
        // For every resource type, try to batch as many descriptor updates as we can
        switch(Res.SpirvAttribs.Type)
        {
            case SPIRVShaderResourceAttribs::ResourceType::UniformBuffer:
                WriteDescrSetIt->pBufferInfo = &(*DescrBuffIt);
                while(ArrElem < Res.SpirvAttribs.ArraySize && DescrBuffIt != DescrBuffInfoArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + ArrElem);
                    *DescrBuffIt = CachedRes.GetUniformBufferDescriptorWriteInfo();
                    ++DescrBuffIt;
                    ++ArrElem;
                }
            break;
            
            case SPIRVShaderResourceAttribs::ResourceType::StorageBuffer:
                WriteDescrSetIt->pBufferInfo = &(*DescrBuffIt);
                while(ArrElem < Res.SpirvAttribs.ArraySize && DescrBuffIt != DescrBuffInfoArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + ArrElem);
                    *DescrBuffIt = CachedRes.GetStorageBufferDescriptorWriteInfo();
                    ++DescrBuffIt;
                    ++ArrElem;
                }
            break;

            case SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer:
            case SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer:
                WriteDescrSetIt->pTexelBufferView = &(*BuffViewIt);
                while(ArrElem < Res.SpirvAttribs.ArraySize && BuffViewIt != DescrBuffViewArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + ArrElem);
                    *BuffViewIt = CachedRes.GetBufferViewWriteInfo();
                    ++BuffViewIt;
                    ++ArrElem;
                }
            break;

            case SPIRVShaderResourceAttribs::ResourceType::SeparateImage:
            case SPIRVShaderResourceAttribs::ResourceType::StorageImage:
            case SPIRVShaderResourceAttribs::ResourceType::SampledImage:
                WriteDescrSetIt->pImageInfo = &(*DescrImgIt);
                while(ArrElem < Res.SpirvAttribs.ArraySize && DescrImgIt != DescrImgInfoArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + ArrElem);
                    *DescrImgIt = CachedRes.GetImageDescriptorWriteInfo(Res.SpirvAttribs.StaticSamplerInd >= 0);
                    ++DescrImgIt;
                    ++ArrElem;
                }
            break;

            case SPIRVShaderResourceAttribs::ResourceType::AtomicCounter:
                // Do nothing
            break;
           

            case SPIRVShaderResourceAttribs::ResourceType::SeparateSampler:
                // Immutable samplers are permanently bound into the set layout; later binding a sampler 
                // into an immutable sampler slot in a descriptor set is not allowed (13.2.1)
                if(Res.SpirvAttribs.StaticSamplerInd < 0)
                {
                    WriteDescrSetIt->pImageInfo = &(*DescrImgIt);
                    while(ArrElem < Res.SpirvAttribs.ArraySize && DescrImgIt != DescrImgInfoArr.end())
                    {
                        const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + ArrElem);
                        *DescrImgIt = CachedRes.GetSamplerDescriptorWriteInfo();
                        ++DescrImgIt;
                        ++ArrElem;
                    }
                }
                else
                {
                    ArrElem = Res.SpirvAttribs.ArraySize;
                    WriteDescrSetIt->dstArrayElement = Res.SpirvAttribs.ArraySize;
                }
            break;

            default:
                UNEXPECTED("Unexpected resource type");
        }

        WriteDescrSetIt->descriptorCount = ArrElem - WriteDescrSetIt->dstArrayElement;
        if(ArrElem == Res.SpirvAttribs.ArraySize)
        {
            ArrElem = 0;
            ++ResNum;
        }
        // descriptorCount == 0 for immutable separate samplers
        if(WriteDescrSetIt->descriptorCount > 0)
            ++WriteDescrSetIt;

        // If we ran out of space in any of the arrays or if we processed all resources,
        // flush pending updates and reset iterators
        if(ResNum == NumDynamicResources || 
           DescrImgIt      == DescrImgInfoArr.end() || 
           DescrBuffIt     == DescrBuffInfoArr.end() ||
           BuffViewIt      == DescrBuffViewArr.end() ||
           WriteDescrSetIt == WriteDescrSetArr.end())
        {
            auto DescrWriteCount = static_cast<Uint32>(std::distance(WriteDescrSetArr.begin(), WriteDescrSetIt));
            if(DescrWriteCount > 0)
                m_LogicalDevice.UpdateDescriptorSets(DescrWriteCount, WriteDescrSetArr.data(), 0, nullptr);

            DescrImgIt  = DescrImgInfoArr.begin();
            DescrBuffIt = DescrBuffInfoArr.begin();
            BuffViewIt  = DescrBuffViewArr.begin();
            WriteDescrSetIt = WriteDescrSetArr.begin();
        }
    }
}

}
