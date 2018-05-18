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
 
ShaderResourceLayoutVk::ShaderResourceLayoutVk(IObject &Owner,
                                               IMemoryAllocator &ResourceLayoutDataAllocator) : 
    m_Owner(Owner),
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

// Clones layout from the reference layout maintained by the pipeline state
// Descriptor sets and bindings must be correct
// Resource cache is not initialized.
ShaderResourceLayoutVk::ShaderResourceLayoutVk(IObject &Owner,
                                               const ShaderResourceLayoutVk& SrcLayout, 
                                               IMemoryAllocator &ResourceLayoutDataAllocator,
                                               const SHADER_VARIABLE_TYPE *AllowedVarTypes, 
                                               Uint32 NumAllowedTypes, 
                                               ShaderResourceCacheVk &ResourceCache) :
    ShaderResourceLayoutVk(Owner, ResourceLayoutDataAllocator)
{
    m_pLogicalDevice = SrcLayout.m_pLogicalDevice;
    m_pResources = SrcLayout.m_pResources;
    m_pResourceCache = &ResourceCache;
    
    Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    for(SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType+1))
    {
        m_NumResources[VarType] = IsAllowedType(VarType, AllowedTypeBits) ? SrcLayout.m_NumResources[VarType] : 0;
    }
    
    AllocateMemory(ResourceLayoutDataAllocator);

    Uint32 CurrResInd[SHADER_VARIABLE_TYPE_NUM_TYPES] = {};
    for(SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType+1))
    {
        if( !IsAllowedType(VarType, AllowedTypeBits))
            continue;

        Uint32 NumResources = SrcLayout.m_NumResources[VarType];
        VERIFY_EXPR(NumResources == m_NumResources[VarType]);
        for( Uint32 r=0; r < NumResources; ++r )
        {
            const auto &SrcRes = SrcLayout.GetResource(VarType, r);
            ::new (&GetResource(VarType, CurrResInd[VarType]++)) VkResource( *this, SrcRes );
        }
    }

#ifdef _DEBUG
    for(SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType+1))
    {
        VERIFY_EXPR(CurrResInd[VarType] == m_NumResources[VarType] );
    }
#endif
}


void ShaderResourceLayoutVk::Initialize(const VulkanUtilities::VulkanLogicalDevice&         LogicalDevice,
                                        const std::shared_ptr<const SPIRVShaderResources>&  pSrcResources,
                                        IMemoryAllocator&                                   LayoutDataAllocator,
                                        const SHADER_VARIABLE_TYPE*                         AllowedVarTypes,
                                        Uint32                                              NumAllowedTypes,
                                        ShaderResourceCacheVk*                              pResourceCache,
                                        std::vector<uint32_t>*                              pSPIRV,
                                        PipelineLayout*                                     pPipelineLayout)
{
    m_pResources = pSrcResources;
    m_pResourceCache = pResourceCache;
    m_pLogicalDevice = LogicalDevice.GetSharedPtr();

    VERIFY_EXPR( (pResourceCache != nullptr) ^ (pPipelineLayout != nullptr && pSPIRV != nullptr) );

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
            // Uniform buffers   at index SPIRVShaderResourceAttribs::ResourceType::UniformBuffer  (0)
            // Storage buffers   at index SPIRVShaderResourceAttribs::ResourceType::StorageBuffer  (1)
            // Storage images    at index SPIRVShaderResourceAttribs::ResourceType::StorageImage   (2)
            // Sampled images    at index SPIRVShaderResourceAttribs::ResourceType::SampledImage   (3)
            // Atomic counters   at index SPIRVShaderResourceAttribs::ResourceType::AtomicCounter  (4)
            // Separate images   at index SPIRVShaderResourceAttribs::ResourceType::SeparateImage  (5)
            // Separate samplers at index SPIRVShaderResourceAttribs::ResourceType::SeparateSampler(6)
            VERIFY_EXPR(m_pResourceCache != nullptr);

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

    if(m_pResourceCache)
    {
        // Initialize resource cache to store static resources
        VERIFY_EXPR(pPipelineLayout == nullptr && pSPIRV == nullptr);
        m_pResourceCache->Initialize(GetRawAllocator(), static_cast<Uint32>(StaticResCacheSetSizes.size()), StaticResCacheSetSizes.data());
#ifdef _DEBUG
        for(SPIRVShaderResourceAttribs::ResourceType ResType = SPIRVShaderResourceAttribs::ResourceType::UniformBuffer; 
            ResType < SPIRVShaderResourceAttribs::ResourceType::NumResourceTypes;
            ResType = static_cast<SPIRVShaderResourceAttribs::ResourceType>(ResType +1))
        {
            VERIFY_EXPR(m_pResourceCache->GetDescriptorSet(ResType).GetSize() == StaticResCacheSetSizes[ResType]);
        }
#endif
    }

    InitVariablesHashMap();
}

void ShaderResourceLayoutVk::InitVariablesHashMap()
{
#if USE_VARIABLE_HASH_MAP
    Uint32 TotalResources = GetTotalResourceCount();
    for(Uint32 r=0; r < TotalResources; ++r)
    {
        auto &Res = GetResource(r);
        /* HashMapStringKey will make a copy of the string*/
        m_VariableHash.insert( std::make_pair( Diligent::HashMapStringKey(Res.Name), &Res ) );
    }
#endif
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
                                                                const VkBufferView*            pTexelBufferView)
{
    VERIFY(SpirvAttribs.VarType != SHADER_VARIABLE_TYPE_DYNAMIC, "Dynamic resource descriptors must be updated by CommitShaderResources()");

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

    ParentResLayout.m_pLogicalDevice->UpdateDescriptorSets(1, &WriteDescrSet, 0, nullptr);

}

bool ShaderResourceLayoutVk::VkResource::UpdateCachedResource(ShaderResourceCacheVk::Resource&   DstRes,
                                                              Uint32                             ArrayInd,
                                                              IDeviceObject*                     pObject, 
                                                              INTERFACE_ID                       InterfaceId,
                                                              const char*                        ResourceName)
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
                LOG_ERROR_MESSAGE("Non-null resource is already bound to ", VarTypeStr, " shader variable \"", SpirvAttribs.GetPrintName(ArrayInd), "\" in shader \"", ParentResLayout.GetShaderName(), "\". Attempring to bind another resource is an error and will be ignored. Use another shader resource binding instance or label shader variable as dynamic.");
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
                                                     Uint32                             ArrayInd)
{
    VERIFY(SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer || 
           SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::StorageBuffer,
           "Uniform or storage buffer resource is expected");

    if( UpdateCachedResource(DstRes, ArrayInd, pBuffer, IID_BufferVk, "buffer") )
    {
        auto* pBuffVk = DstRes.pObject.RawPtr<BufferVkImpl>(); // Use final type
        const auto IsUniformBuffer = SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer;
        const auto ExpectedBindFlag = IsUniformBuffer ? BIND_UNIFORM_BUFFER : BIND_UNORDERED_ACCESS;
        if( pBuffVk->GetDesc().BindFlags & ExpectedBindFlag)
        {
            if(vkDescrSet != VK_NULL_HANDLE)
            {
                VkDescriptorBufferInfo DescrBuffInfo;
                DescrBuffInfo.buffer = pBuffVk->GetVkBuffer();
                DescrBuffInfo.offset = 0;
                DescrBuffInfo.range = pBuffVk->GetDesc().uiSizeInBytes;
                UpdateDescriptorHandle(vkDescrSet, ArrayInd, nullptr, &DescrBuffInfo, nullptr);
            }
        }
        else
        {
            LOG_RESOURCE_BINDING_ERROR("buffer", pBuffer, SpirvAttribs.GetPrintName(ArrayInd), ParentResLayout.GetShaderName(), "Buffer was not created with BIND_UNIFORM_BUFFER flag.")
            DstRes.pObject.Release();
        }
    }
}


void ShaderResourceLayoutVk::VkResource::CacheTexelBuffer(IDeviceObject*                     pBufferView,
                                                          ShaderResourceCacheVk::Resource&   DstRes, 
                                                          VkDescriptorSet                    vkDescrSet,
                                                          Uint32                             ArrayInd)
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
        const bool IsStorageBuffer = SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::StorageBuffer;
        const auto dbgExpectedViewType = IsStorageBuffer ? TEXTURE_VIEW_UNORDERED_ACCESS : TEXTURE_VIEW_SHADER_RESOURCE;
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

        if (vkDescrSet != VK_NULL_HANDLE)
        {
            VkBufferView BuffView = pBuffViewVk->GetVkBufferView();
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, nullptr, nullptr, &BuffView);
        }
    }
}
void ShaderResourceLayoutVk::VkResource::CacheImage(IDeviceObject *pTexView,
                                                    ShaderResourceCacheVk::Resource& DstRes,
                                                    VkDescriptorSet vkDescrSet,
                                                    Uint32 ArrayInd)
{
    VERIFY(SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage || 
           SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage ||
           SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage,
           "Storage image, separate image or sampled image resource is expected");

    if (UpdateCachedResource(DstRes, ArrayInd, pTexView, IID_TextureViewVk, "texture view") )
    {
        auto* pTexViewVk = DstRes.pObject.RawPtr<TextureViewVkImpl>();

#ifdef VERIFY_SHADER_BINDINGS
        const auto& ViewDesc = pTexViewVk->GetDesc();
        const auto ViewType = ViewDesc.ViewType;
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
#endif

        if (vkDescrSet != VK_NULL_HANDLE)
        {
            VkDescriptorImageInfo DescrImgInfo;
            DescrImgInfo.sampler = VK_NULL_HANDLE;
            if(SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage && SpirvAttribs.StaticSamplerInd < 0)
            {
                auto *pSamplerVk = ValidatedCast<ISamplerVk>(pTexViewVk->GetSampler());
                if(pSamplerVk != nullptr)
                {
                    DescrImgInfo.sampler = pSamplerVk->GetVkSampler();
                }
                else
                {
                    LOG_RESOURCE_BINDING_ERROR("resource", pTexView, SpirvAttribs.GetPrintName(ArrayInd), ParentResLayout.GetShaderName(), "No sampler assigned to texture view.")
                }
            }
            DescrImgInfo.imageView = pTexViewVk->GetVulkanImageView();
            DescrImgInfo.imageLayout = IsStorageImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, &DescrImgInfo, nullptr, nullptr);
        }
    }
}

void ShaderResourceLayoutVk::VkResource::CacheSeparateSampler(IDeviceObject *pSampler,
                                                              ShaderResourceCacheVk::Resource& DstRes,
                                                              VkDescriptorSet vkDescrSet,
                                                              Uint32 ArrayInd)
{
    VERIFY(SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler, "Separate sampler resource is expected");

    if (UpdateCachedResource(DstRes, ArrayInd, pSampler, IID_TextureViewVk, "sampler"))
    {
        auto* pSamplerVk = DstRes.pObject.RawPtr<SamplerVkImpl>();
        if (vkDescrSet != VK_NULL_HANDLE)
        {
            VkDescriptorImageInfo DescrImgInfo;
            DescrImgInfo.sampler = pSamplerVk->GetVkSampler();
            DescrImgInfo.imageView = VK_NULL_HANDLE;
            DescrImgInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, &DescrImgInfo, nullptr, nullptr);
        }
    }
    else
    {
        LOG_RESOURCE_BINDING_ERROR("Sampler", pSampler, SpirvAttribs.GetPrintName(ArrayInd), ParentResLayout.GetShaderName(), "Incorrect resource type: sampler is expected.")
    }
}


void ShaderResourceLayoutVk::VkResource::BindResource(IDeviceObject *pObj, Uint32 ArrayIndex, const ShaderResourceLayoutVk *dbgResLayout)
{
    auto *pResourceCache = ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY(dbgResLayout == nullptr || pResourceCache == dbgResLayout->m_pResourceCache, "Invalid resource cache");
    VERIFY_EXPR(ArrayIndex < SpirvAttribs.ArraySize);

    auto &DstDescrSet = pResourceCache->GetDescriptorSet(DescriptorSet);
    auto vkDescrSet = DstDescrSet.GetVkDescriptorSet();
    if (pResourceCache->DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::SRBResources)
    {
        VERIFY(SpirvAttribs.VarType == SHADER_VARIABLE_TYPE_DYNAMIC && vkDescrSet == VK_NULL_HANDLE ||
               SpirvAttribs.VarType != SHADER_VARIABLE_TYPE_DYNAMIC && vkDescrSet != VK_NULL_HANDLE,
               "Static and mutable variables and only them are expected to have valid descriptor set assigned");
    }
    auto &DstRes = DstDescrSet.GetResource(CacheOffset + ArrayIndex);

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
                    LOG_ERROR_MESSAGE("Attempting to assign sampler to static sampler '", SpirvAttribs.Name, '\'');
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

        DstRes = ShaderResourceCacheVk::Resource();
    }
}

bool ShaderResourceLayoutVk::VkResource::IsBound(Uint32 ArrayIndex)
{
    auto *pResourceCache = ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < SpirvAttribs.ArraySize);

    if( DescriptorSet < pResourceCache->GetNumDescriptorSets() )
    {
        auto &Set = pResourceCache->GetDescriptorSet(DescriptorSet);
        if(CacheOffset + ArrayIndex < Set.GetSize())
        {
            auto &CachedRes = Set.GetResource(CacheOffset + ArrayIndex);
            return CachedRes.pObject != nullptr;
        }
    }

    return false;
}



void ShaderResourceLayoutVk::BindResources( IResourceMapping* pResourceMapping, Uint32 Flags, const ShaderResourceCacheVk *dbgResourceCache )
{
    VERIFY(dbgResourceCache == m_pResourceCache, "Resource cache does not match the cache provided at initialization");

    if( !pResourceMapping )
    {
        LOG_ERROR_MESSAGE( "Failed to bind resources in shader \"", GetShaderName(), "\": resource mapping is null" );
        return;
    }

    Uint32 TotalResources = GetTotalResourceCount();
    for(Uint32 r=0; r < TotalResources; ++r)
    {
        auto &Res = GetResource(r);
        for(Uint32 ArrInd = 0; ArrInd < Res.SpirvAttribs.ArraySize; ++ArrInd)
        {
            if( Flags & BIND_SHADER_RESOURCES_RESET_BINDINGS )
                Res.BindResource(nullptr, ArrInd, this);

            if( (Flags & BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED) && Res.IsBound(ArrInd) )
                return;

            const auto* VarName = Res.SpirvAttribs.Name;
            RefCntAutoPtr<IDeviceObject> pObj;
            VERIFY_EXPR(pResourceMapping != nullptr);
            pResourceMapping->GetResource( VarName, &pObj, ArrInd );
            if( pObj )
            {
                //  Call non-virtual function
                Res.BindResource(pObj, ArrInd, this);
            }
            else
            {
                if( (Flags & BIND_SHADER_RESOURCES_ALL_RESOLVED) && !Res.IsBound(ArrInd) )
                    LOG_ERROR_MESSAGE( "Cannot bind resource to shader variable \"", Res.SpirvAttribs.GetPrintName(ArrInd), "\": resource view not found in the resource mapping" );
            }
        }
    }
}

IShaderVariable* ShaderResourceLayoutVk::GetShaderVariable(const Char* Name)
{
    IShaderVariable* pVar = nullptr;
#if USE_VARIABLE_HASH_MAP
    // Name will be implicitly converted to HashMapStringKey without making a copy
    auto it = m_VariableHash.find( Name );
    if( it != m_VariableHash.end() )
        pVar = it->second;
#else
    Uint32 TotalResources = GetTotalResourceCount();
    for(Uint32 r=0; r < TotalResources; ++r)
    {
        auto &Res = GetResource(r);
        if(strcmp(Res.SpirvAttribs.Name, Name) == 0)
        {
            pVar = &Res;
            break;
        }
    }
#endif

    if(pVar == nullptr)
    {
        LOG_ERROR_MESSAGE( "Shader variable \"", Name, "\" is not found in shader \"", GetShaderName(), "\" (", GetShaderTypeLiteralName(m_pResources->GetShaderType()), "). Attempts to set the variable will be silently ignored." );
    }
    return pVar;
}


void ShaderResourceLayoutVk::InitializeStaticResources(const ShaderResourceLayoutVk &SrcLayout)
{
    if (!m_pResourceCache)
    {
        LOG_ERROR("Resource layout has no resource cache");
        return;
    }

    if (!SrcLayout.m_pResourceCache)
    {
        LOG_ERROR("Src layout has no resource cache");
        return;
    }

    VERIFY(m_NumResources[SHADER_VARIABLE_TYPE_STATIC] == SrcLayout.m_NumResources[SHADER_VARIABLE_TYPE_STATIC], "Inconsistent number of static resources");
    VERIFY(SrcLayout.m_pResources->GetShaderType() == m_pResources->GetShaderType(), "Incosistent shader types");

    // Static shader resources are stored as follows:
    // Uniform buffers   at index SPIRVShaderResourceAttribs::ResourceType::UniformBuffer  (0)
    // Storage buffers   at index SPIRVShaderResourceAttribs::ResourceType::StorageBuffer  (1)
    // Storage images    at index SPIRVShaderResourceAttribs::ResourceType::StorageImage   (2)
    // Sampled images    at index SPIRVShaderResourceAttribs::ResourceType::SampledImage   (3)
    // Atomic counters   at index SPIRVShaderResourceAttribs::ResourceType::AtomicCounter  (4)
    // Separate images   at index SPIRVShaderResourceAttribs::ResourceType::SeparateImage  (5)
    // Separate samplers at index SPIRVShaderResourceAttribs::ResourceType::SeparateSampler(6)

    for(Uint32 r=0; r < m_NumResources[SHADER_VARIABLE_TYPE_STATIC]; ++r)
    {
        // Get resource attributes
        auto &DstRes = GetResource(SHADER_VARIABLE_TYPE_STATIC, r);
        const auto &SrcRes = SrcLayout.GetResource(SHADER_VARIABLE_TYPE_STATIC, r);
        VERIFY(SrcRes.Binding == r, "Unexpected binding");
        VERIFY(SrcRes.SpirvAttribs.ArraySize == DstRes.SpirvAttribs.ArraySize, "Inconsistent array size");

        for(Uint32 ArrInd = 0; ArrInd < DstRes.SpirvAttribs.ArraySize; ++ArrInd)
        {
            auto SrcOffset = SrcRes.CacheOffset + ArrInd;
            IDeviceObject *pObject = SrcLayout.m_pResourceCache->GetDescriptorSet(SrcRes.DescriptorSet).GetResource(SrcOffset).pObject;
            if (!pObject)
                LOG_ERROR_MESSAGE("No resource assigned to static shader variable \"", SrcRes.SpirvAttribs.GetPrintName(ArrInd), "\" in shader \"", GetShaderName(), "\".");
            
            auto DstOffset = DstRes.CacheOffset + ArrInd;
            IDeviceObject *pCachedResource = m_pResourceCache->GetDescriptorSet(DstRes.DescriptorSet).GetResource(DstOffset).pObject;
            if(pCachedResource != pObject)
            {
                VERIFY(pObject == nullptr, "Static resource has already been initialized, and the resource to be assigned from the shader does not match previously assigned resource");
                DstRes.SetArray(&pObject, ArrInd, 1);
            }
        }
    }
}


#ifdef VERIFY_SHADER_BINDINGS
void ShaderResourceLayoutVk::dbgVerifyBindings()const
{
    VERIFY(m_pResourceCache, "Resource cache is null");

    for(SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType+1))
    {
        for(Uint32 r=0; r < m_NumResources[VarType]; ++r)
        {
            const auto &Res = GetResource(VarType, r);
            VERIFY(Res.SpirvAttribs.VarType == VarType, "Unexpected variable type");
            for(Uint32 ArrInd = 0; ArrInd < Res.SpirvAttribs.ArraySize; ++ArrInd)
            {
                auto &CachedDescrSet = m_pResourceCache->GetDescriptorSet(Res.DescriptorSet);
                const auto &CachedRes = CachedDescrSet.GetResource(Res.CacheOffset + ArrInd);
                if(CachedRes.pObject == nullptr)
                {
                    LOG_ERROR_MESSAGE("No resource is bound to ", GetShaderVariableTypeLiteralName(Res.SpirvAttribs.VarType), " variable \"", Res.SpirvAttribs.GetPrintName(ArrInd), "\" in shader \"", GetShaderName(), "\"");
                }
                if(VarType == SHADER_VARIABLE_TYPE_DYNAMIC )
                {
                    if (CachedDescrSet.GetVkDescriptorSet() == VK_NULL_HANDLE)
                        LOG_ERROR_MESSAGE("Dynamic resources should not have Vulkan descriptor set in the cache");
                }
                else
                {
                    if(CachedDescrSet.GetVkDescriptorSet() != VK_NULL_HANDLE)
                        LOG_ERROR_MESSAGE("Static and mutable resources must have non-null vulkan descriptor set assigned"); 
                }
            }
        }
    }
#endif
}


const Char* ShaderResourceLayoutVk::GetShaderName()const
{
    RefCntAutoPtr<IShader> pShader(&m_Owner, IID_Shader);
    if (pShader)
    {
        return pShader->GetDesc().Name;
    }
    else
    {
        RefCntAutoPtr<IShaderResourceBinding> pSRB(&m_Owner, IID_ShaderResourceBinding);
        if(pSRB)
        {
            auto *pPSO = pSRB->GetPipelineState();
            auto *pPSOVk = ValidatedCast<PipelineStateVkImpl>(pPSO);
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
            UNEXPECTED("Owner is expected to be a shader or a shader resource binding");
        }
    }
    return "";
}

}
