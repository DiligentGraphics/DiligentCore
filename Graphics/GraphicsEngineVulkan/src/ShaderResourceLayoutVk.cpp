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
                                               const VulkanUtilities::VulkanLogicalDevice& LogicalDevice,
                                               IMemoryAllocator&                           ResourceLayoutDataAllocator) :
    m_Owner(Owner),
    m_LogicalDevice(LogicalDevice),
#if USE_VARIABLE_HASH_MAP
    m_VariableHash(STD_ALLOCATOR_RAW_MEM(VariableHashElemType, GetRawAllocator(), "Allocator for unordered_map<HashMapStringKey, IShaderVariable*>")),
#endif
    m_ResourceBuffer(nullptr, STDDeleterRawMem<void>(ResourceLayoutDataAllocator))
{
}

ShaderResourceLayoutVk::~ShaderResourceLayoutVk()
{
    auto* Resources = reinterpret_cast<VkResource*>(m_ResourceBuffer.get());
    for(Uint32 r=0; r < GetTotalResourceCount(); ++r)
        Resources[r].~VkResource();
}

void ShaderResourceLayoutVk::AllocateMemory(IMemoryAllocator &Allocator)
{
    VERIFY( &m_ResourceBuffer.get_deleter().m_Allocator == &Allocator, "Inconsistent memory allocators" );
    Uint32 TotalResource = GetTotalResourceCount();
    size_t MemSize = TotalResource * sizeof(VkResource);
    if(MemSize == 0)
        return;

    auto *pRawMem = ALLOCATE(Allocator, "Raw memory buffer for shader resource layout resources", MemSize);
    m_ResourceBuffer.reset(pRawMem);
}

void ShaderResourceLayoutVk::Initialize(const std::shared_ptr<const SPIRVShaderResources>&  pSrcResources,
                                        IMemoryAllocator&                                   LayoutDataAllocator,
                                        const SHADER_VARIABLE_TYPE*                         AllowedVarTypes,
                                        Uint32                                              NumAllowedTypes,
                                        ShaderResourceCacheVk*                              pStaticResourceCache,
                                        std::vector<uint32_t>*                              pSPIRV,
                                        PipelineLayout*                                     pPipelineLayout)
{
    m_pResources = pSrcResources;

    VERIFY_EXPR( (pStaticResourceCache != nullptr) ^ (pPipelineLayout != nullptr && pSPIRV != nullptr) );

    Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);

    // Count number of resources to allocate all needed memory
    m_pResources->ProcessResources(
        AllowedVarTypes, NumAllowedTypes,

        [&](const SPIRVShaderResourceAttribs &UB, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(UB.VarType, AllowedTypeBits));
            ++m_NumResources[UB.VarType];
        },
        [&](const SPIRVShaderResourceAttribs& SB, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SB.VarType, AllowedTypeBits));
            ++m_NumResources[SB.VarType];
        },
        [&](const SPIRVShaderResourceAttribs &Img, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(Img.VarType, AllowedTypeBits));
            ++m_NumResources[Img.VarType];
        },
        [&](const SPIRVShaderResourceAttribs &SmplImg, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SmplImg.VarType, AllowedTypeBits));
            ++m_NumResources[SmplImg.VarType];
        },
        [&](const SPIRVShaderResourceAttribs &AC, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(AC.VarType, AllowedTypeBits));
            ++m_NumResources[AC.VarType];
        },
        [&](const SPIRVShaderResourceAttribs &SepImg, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SepImg.VarType, AllowedTypeBits));
            ++m_NumResources[SepImg.VarType];
        },
        [&](const SPIRVShaderResourceAttribs &SepSmpl, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SepSmpl.VarType, AllowedTypeBits));
            ++m_NumResources[SepSmpl.VarType];
        }
    );


    AllocateMemory(LayoutDataAllocator);

    std::array<Uint32, SHADER_VARIABLE_TYPE_NUM_TYPES> CurrResInd = {};
    std::array<Uint32, SPIRVShaderResourceAttribs::ResourceType::NumResourceTypes > StaticResCacheSetSizes = {};

    auto AddResource = [&](const SPIRVShaderResourceAttribs &Attribs)
    {
        Uint32 Binding = 0;
        Uint32 DescriptorSet = 0;
        Uint32 CacheOffset = 0;
        if (pPipelineLayout)
        {
            VERIFY_EXPR(pSPIRV != nullptr);
            auto *pStaticSampler = m_pResources->GetStaticSampler(Attribs);
            VkSampler vkStaticSampler = VK_NULL_HANDLE;
            if(pStaticSampler != nullptr)
                vkStaticSampler = ValidatedCast<SamplerVkImpl>(pStaticSampler)->GetVkSampler();
            pPipelineLayout->AllocateResourceSlot(Attribs, vkStaticSampler, m_pResources->GetShaderType(), DescriptorSet, Binding, CacheOffset, *pSPIRV);
            VERIFY(DescriptorSet <= std::numeric_limits<decltype(VkResource::DescriptorSet)>::max(), "Descriptor set (", DescriptorSet, ") excceeds representable max value");
            VERIFY(Binding <= std::numeric_limits<decltype(VkResource::Binding)>::max(), "Binding (", Binding, ") excceeds representable max value");
        }
        else
        {
            // If pipeline layout is not provided - use artifial layout to store
            // static shader resources:
            // Uniform buffers   at index SPIRVShaderResourceAttribs::ResourceType::UniformBuffer      (0)
            // Storage buffers   at index SPIRVShaderResourceAttribs::ResourceType::StorageBuffer      (1)
            // Unifrom txl buffs at index SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer (2)
            // Storage txl buffs at index SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer (3)
            // Storage images    at index SPIRVShaderResourceAttribs::ResourceType::StorageImage       (4)
            // Sampled images    at index SPIRVShaderResourceAttribs::ResourceType::SampledImage       (5)
            // Atomic counters   at index SPIRVShaderResourceAttribs::ResourceType::AtomicCounter      (6)
            // Separate images   at index SPIRVShaderResourceAttribs::ResourceType::SeparateImage      (7)
            // Separate samplers at index SPIRVShaderResourceAttribs::ResourceType::SeparateSampler    (8)
            VERIFY_EXPR(pStaticResourceCache != nullptr);

            DescriptorSet = Attribs.Type;
            CacheOffset = StaticResCacheSetSizes[DescriptorSet];
            Binding = CurrResInd[Attribs.VarType];
            StaticResCacheSetSizes[DescriptorSet] += Attribs.ArraySize;
        }

        // Static samplers are never copied, and SamplerId == InvalidSamplerId
        ::new (&GetResource(Attribs.VarType, CurrResInd[Attribs.VarType]++)) VkResource( *this, Attribs, Binding, DescriptorSet, CacheOffset);
    };

    m_pResources->ProcessResources(
        AllowedVarTypes, NumAllowedTypes,

        [&](const SPIRVShaderResourceAttribs &UB, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(UB.VarType, AllowedTypeBits));
            AddResource(UB);
        },
        [&](const SPIRVShaderResourceAttribs& SB, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SB.VarType, AllowedTypeBits));
            AddResource(SB);
        },
        [&](const SPIRVShaderResourceAttribs &Img, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(Img.VarType, AllowedTypeBits));
            AddResource(Img);
        },
        [&](const SPIRVShaderResourceAttribs &SmplImg, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SmplImg.VarType, AllowedTypeBits));
            AddResource(SmplImg);
        },
        [&](const SPIRVShaderResourceAttribs &AC, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(AC.VarType, AllowedTypeBits));
            AddResource(AC);
        },
        [&](const SPIRVShaderResourceAttribs &SepImg, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SepImg.VarType, AllowedTypeBits));
            AddResource(SepImg);
        },

        [&](const SPIRVShaderResourceAttribs &SepSmpl, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SepSmpl.VarType, AllowedTypeBits));
            AddResource(SepSmpl);
        }
    );

#ifdef _DEBUG
    for(SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType+1))
    {
        VERIFY( CurrResInd[VarType] == m_NumResources[VarType], "Not all resources are initialized, which result in a crash when dtor is called" );
    }
#endif

    if(pStaticResourceCache)
    {
        // Initialize resource cache to store static resources
        VERIFY_EXPR(pPipelineLayout == nullptr && pSPIRV == nullptr);
        pStaticResourceCache->InitializeSets(GetRawAllocator(), static_cast<Uint32>(StaticResCacheSetSizes.size()), StaticResCacheSetSizes.data());
        InitializeResourceMemoryInCache(*pStaticResourceCache);
#ifdef _DEBUG
        for(SPIRVShaderResourceAttribs::ResourceType ResType = SPIRVShaderResourceAttribs::ResourceType::UniformBuffer; 
            ResType < SPIRVShaderResourceAttribs::ResourceType::NumResourceTypes;
            ResType = static_cast<SPIRVShaderResourceAttribs::ResourceType>(ResType +1))
        {
            VERIFY_EXPR(pStaticResourceCache->GetDescriptorSet(ResType).GetSize() == StaticResCacheSetSizes[ResType]);
        }
#endif
    }
}


#define LOG_RESOURCE_BINDING_ERROR(ResType, pResource, VarName, ShaderName, ...)\
{                                                                                                   \
    const auto &ResName = pResource->GetDesc().Name;                                                \
    LOG_ERROR_MESSAGE( "Failed to bind ", ResType, " \"", ResName, "\" to variable \"", VarName,    \
                        "\" in shader \"", ShaderName, "\". ", __VA_ARGS__ );                \
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
                LOG_ERROR_MESSAGE("Non-null resource is already bound to ", VarTypeStr, " shader variable \"", SpirvAttribs.GetPrintName(ArrayInd), "\" in shader \"", ParentResLayout.GetShaderName(), "\". Attempring to bind another resource is an error and will be ignored. Use another shader resource binding instance or label the variable as dynamic.");
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

void ShaderResourceLayoutVk::VkResource::CacheBuffer(IDeviceObject*                     pBuffer,
                                                     ShaderResourceCacheVk::Resource&   DstRes, 
                                                     VkDescriptorSet                    vkDescrSet, 
                                                     Uint32                             ArrayInd)const
{
    VERIFY(SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer || 
           SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::StorageBuffer,
           "Uniform or storage buffer resource is expected");

    if( UpdateCachedResource(DstRes, ArrayInd, pBuffer, IID_BufferVk, "buffer") )
    {
#ifdef VERIFY_SHADER_BINDINGS
        auto* pBuffVk = DstRes.pObject.RawPtr<BufferVkImpl>(); // Use final type
        const auto IsUniformBuffer = SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer;
        const auto ExpectedBindFlag = IsUniformBuffer ? BIND_UNIFORM_BUFFER : BIND_UNORDERED_ACCESS;
        if( (pBuffVk->GetDesc().BindFlags & ExpectedBindFlag) == 0)
        {
            LOG_RESOURCE_BINDING_ERROR("buffer", pBuffer, SpirvAttribs.GetPrintName(ArrayInd), ParentResLayout.GetShaderName(), "Buffer was not created with ", (IsUniformBuffer ? "BIND_UNIFORM_BUFFER" : "BIND_UNORDERED_ACCESS"), " flag.")
            DstRes.pObject.Release();
            return;
        }
#endif

        // Do not update descriptor for a dynamic uniform/storage buffer. All dynamic resource 
        // descriptors are updated at once by CommitDynamicResources() when SRB is committed.
        if(vkDescrSet != VK_NULL_HANDLE && SpirvAttribs.VarType != SHADER_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorBufferInfo DescrBuffInfo = DstRes.GetBufferDescriptorWriteInfo();
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

#ifdef VERIFY_SHADER_BINDINGS
        const auto& ViewDesc = pBuffViewVk->GetDesc();
        const auto ViewType = ViewDesc.ViewType;
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

void ShaderResourceLayoutVk::VkResource::CacheImage(IDeviceObject*                   pTexView,
                                                    ShaderResourceCacheVk::Resource& DstRes,
                                                    VkDescriptorSet                  vkDescrSet,
                                                    Uint32                           ArrayInd)const
{
    VERIFY(SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage || 
           SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage ||
           SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage,
           "Storage image, separate image or sampled image resource is expected");

    if (UpdateCachedResource(DstRes, ArrayInd, pTexView, IID_TextureViewVk, "texture view") )
    {
#ifdef VERIFY_SHADER_BINDINGS
        auto* pTexViewVk = DstRes.pObject.RawPtr<TextureViewVkImpl>();
        const bool IsStorageImage = SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage;
        const auto& ViewDesc = pTexViewVk->GetDesc();
        const auto ViewType = ViewDesc.ViewType;
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
                LOG_RESOURCE_BINDING_ERROR("resource", pTexView, SpirvAttribs.GetPrintName(ArrayInd), ParentResLayout.GetShaderName(), "No sampler assigned to texture view.");
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
#endif
    auto &DstRes = DstDescrSet.GetResource(CacheOffset + ArrayIndex);
    VERIFY(DstRes.Type == SpirvAttribs.Type, "Inconsistent types");

    if( pObj )
    {
        switch (SpirvAttribs.Type)
        {
            case SPIRVShaderResourceAttribs::ResourceType::UniformBuffer:
            case SPIRVShaderResourceAttribs::ResourceType::StorageBuffer:
                CacheBuffer(pObj, DstRes, vkDescrSet, ArrayIndex);
            break;

            case SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer:
            case SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer:
                CacheTexelBuffer(pObj, DstRes, vkDescrSet, ArrayIndex);
                break;

            case SPIRVShaderResourceAttribs::ResourceType::StorageImage:
            case SPIRVShaderResourceAttribs::ResourceType::SeparateImage:
            case SPIRVShaderResourceAttribs::ResourceType::SampledImage:
                CacheImage(pObj, DstRes, vkDescrSet, ArrayIndex);
            break;

            case SPIRVShaderResourceAttribs::ResourceType::SeparateSampler:
                if(SpirvAttribs.StaticSamplerInd < 0)
                {
                    CacheSeparateSampler(pObj, DstRes, vkDescrSet, ArrayIndex);
                }
                else
                {
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
            LOG_ERROR_MESSAGE( "Shader variable \"", SpirvAttribs.Name, "\" in shader \"", ParentResLayout.GetShaderName(), "\" is not dynamic but being unbound. This is an error and may cause unpredicted behavior. Use another shader resource binding instance or label shader variable as dynamic if you need to bind another resource." );
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

    // Static shader resources are stored as follows:
    // Uniform buffers   at index SPIRVShaderResourceAttribs::ResourceType::UniformBuffer      (0)
    // Storage buffers   at index SPIRVShaderResourceAttribs::ResourceType::StorageBuffer      (1)
    // Unifrom txl buffs at index SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer (2)
    // Storage txl buffs at index SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer (3)
    // Storage images    at index SPIRVShaderResourceAttribs::ResourceType::StorageImage       (4)
    // Sampled images    at index SPIRVShaderResourceAttribs::ResourceType::SampledImage       (5)
    // Atomic counters   at index SPIRVShaderResourceAttribs::ResourceType::AtomicCounter      (6)
    // Separate images   at index SPIRVShaderResourceAttribs::ResourceType::SeparateImage      (7)
    // Separate samplers at index SPIRVShaderResourceAttribs::ResourceType::SeparateSampler    (8)

    for(Uint32 r=0; r < NumStaticResources; ++r)
    {
        // Get resource attributes
        auto &DstRes = GetResource(SHADER_VARIABLE_TYPE_STATIC, r);
        const auto &SrcRes = SrcLayout.GetResource(SHADER_VARIABLE_TYPE_STATIC, r);
        VERIFY(SrcRes.Binding == r, "Unexpected binding");
        VERIFY(SrcRes.SpirvAttribs.ArraySize == DstRes.SpirvAttribs.ArraySize, "Inconsistent array size");

        if(DstRes.SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler && 
           DstRes.SpirvAttribs.StaticSamplerInd >= 0)
            continue; // Skip static samplers

        for(Uint32 ArrInd = 0; ArrInd < DstRes.SpirvAttribs.ArraySize; ++ArrInd)
        {
            auto SrcOffset = SrcRes.CacheOffset + ArrInd;
            IDeviceObject* pObject = SrcResourceCache.GetDescriptorSet(SrcRes.DescriptorSet).GetResource(SrcOffset).pObject;
            if (!pObject)
                LOG_ERROR_MESSAGE("No resource assigned to static shader variable \"", SrcRes.SpirvAttribs.GetPrintName(ArrInd), "\" in shader \"", GetShaderName(), "\".");
            
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


#ifdef VERIFY_SHADER_BINDINGS
void ShaderResourceLayoutVk::dbgVerifyBindings(const ShaderResourceCacheVk& ResourceCache)const
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
                    LOG_ERROR_MESSAGE("No resource is bound to ", GetShaderVariableTypeLiteralName(Res.SpirvAttribs.VarType), " variable \"", Res.SpirvAttribs.GetPrintName(ArrInd), "\" in shader \"", GetShaderName(), "\"");
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
                        // Dynamic variables do not have vulkan descriptor set only until they are assigned one the first time
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
    for(Uint32 r = 0; r < GetTotalResourceCount(); ++r)
    {
        const auto& Res = GetResource(r);
        ResourceCache.InitializeResources(Res.DescriptorSet, Res.CacheOffset, Res.SpirvAttribs.ArraySize, Res.SpirvAttribs.Type);
    }
}

void ShaderResourceLayoutVk::CommitDynamicResources(const ShaderResourceCacheVk& ResourceCache)const
{
    Uint32 NumDynamicResources = m_NumResources[SHADER_VARIABLE_TYPE_DYNAMIC];
    if(NumDynamicResources == 0)
        return;

#if 0
#ifdef _DEBUG
    static constexpr size_t ImgUpdateBatchSize = 4;
    static constexpr size_t BuffUpdateBatchSize = 2;
    static constexpr size_t TexelBuffUpdateBatchSize = 2;
#else
    static constexpr size_t ImgUpdateBatchSize = 128;
    static constexpr size_t BuffUpdateBatchSize = 64;
    static constexpr size_t TexelBuffUpdateBatchSize = 32;
#endif

    std::array<VkDescriptorImageInfo, ImgUpdateBatchSize>  ImgInfo;
    std::array<VkDescriptorBufferInfo, BuffUpdateBatchSize> BuffInfo;
    std::array<VkBufferView, BuffUpdateBatchSize> TexelBuffInfo;
#endif

    for(Uint32 r=0; r < NumDynamicResources; ++r)
    {
        const auto& Res = GetResource(SHADER_VARIABLE_TYPE_DYNAMIC, r);
        VERIFY_EXPR(Res.SpirvAttribs.VarType == SHADER_VARIABLE_TYPE_DYNAMIC);
        auto& SetResources = ResourceCache.GetDescriptorSet(Res.DescriptorSet);
        auto vkSet = SetResources.GetVkDescriptorSet();
        VERIFY(vkSet != VK_NULL_HANDLE, "Vulkan descriptor set must not be null");
        switch(Res.SpirvAttribs.Type)
        {
            case SPIRVShaderResourceAttribs::ResourceType::UniformBuffer:
            case SPIRVShaderResourceAttribs::ResourceType::StorageBuffer:
                for (Uint32 i = 0; i < Res.SpirvAttribs.ArraySize; ++i)
                {
                    const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + i);
                    VkDescriptorBufferInfo DescrBuffInfo = CachedRes.GetBufferDescriptorWriteInfo();
                    Res.UpdateDescriptorHandle(vkSet, i, nullptr, &DescrBuffInfo, nullptr);
                }
            break;

            case SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer:
            case SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer:
                for (Uint32 i = 0; i < Res.SpirvAttribs.ArraySize; ++i)
                {
                    const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + i);
                    VkBufferView BuffView = CachedRes.GetBufferViewWriteInfo();
                    Res.UpdateDescriptorHandle(vkSet, i, nullptr, nullptr, &BuffView);
                }
            break;

            case SPIRVShaderResourceAttribs::ResourceType::SeparateImage:
            case SPIRVShaderResourceAttribs::ResourceType::StorageImage:
            case SPIRVShaderResourceAttribs::ResourceType::SampledImage:
                for (Uint32 i = 0; i < Res.SpirvAttribs.ArraySize; ++i)
                {
                    const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + i);
                    VkDescriptorImageInfo DescrImgInfo = CachedRes.GetImageDescriptorWriteInfo(Res.SpirvAttribs.StaticSamplerInd >= 0);
                    Res.UpdateDescriptorHandle(vkSet, i, &DescrImgInfo, nullptr, nullptr);
                }
            break;

            case SPIRVShaderResourceAttribs::ResourceType::AtomicCounter:
                // Do nothing
            break;
           

            case SPIRVShaderResourceAttribs::ResourceType::SeparateSampler:
                for (Uint32 i = 0; i < Res.SpirvAttribs.ArraySize; ++i)
                {
                    if(Res.SpirvAttribs.StaticSamplerInd < 0)
                    {
                        const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + i);
                        VkDescriptorImageInfo DescrImgInfo = CachedRes.GetSamplerDescriptorWriteInfo();
                        Res.UpdateDescriptorHandle(vkSet, i, &DescrImgInfo, nullptr, nullptr);
                    }
                }
            break;

            default:
                UNEXPECTED("Unexpected resource type");
        }
    }
}

}
