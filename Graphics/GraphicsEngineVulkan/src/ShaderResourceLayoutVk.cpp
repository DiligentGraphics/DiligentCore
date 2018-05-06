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

class ResourceTypeToVkDescriptorType
{
public:
    ResourceTypeToVkDescriptorType()
    {
        m_Map[SPIRVShaderResourceAttribs::ResourceType::UniformBuffer]   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::StorageBuffer]   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::StorageImage]    = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::SampledImage]    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::AtomicCounter]   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::SeparateImage]   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        m_Map[SPIRVShaderResourceAttribs::ResourceType::SeparateSampler] = VK_DESCRIPTOR_TYPE_SAMPLER;
    }

    VkDescriptorType operator[](SPIRVShaderResourceAttribs::ResourceType ResType)const
    {
        return m_Map[static_cast<int>(ResType)];
    }

private:
    std::array<VkDescriptorType, SPIRVShaderResourceAttribs::ResourceType::NumResourceTypes> m_Map = {};
};

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

#if 0
// Clones layout from the reference layout maintained by the pipeline state
// Root indices and descriptor table offsets must be correct
// Resource cache is not initialized.
ShaderResourceLayoutVk::ShaderResourceLayoutVk(IObject &Owner,
                                                     const ShaderResourceLayoutVk& SrcLayout, 
                                                     IMemoryAllocator &ResourceLayoutDataAllocator,
                                                     const SHADER_VARIABLE_TYPE *AllowedVarTypes, 
                                                     Uint32 NumAllowedTypes, 
                                                     ShaderResourceCacheVk &ResourceCache) :
    ShaderResourceLayoutVk(Owner, ResourceLayoutDataAllocator)
{
    m_pVkDevice = SrcLayout.m_pVkDevice;
    m_pResources = SrcLayout.m_pResources;
    m_pResourceCache = &ResourceCache;
    
    Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    
    for(SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType+1))
    {
        if( !IsAllowedType(VarType, AllowedTypeBits))
            continue;

        m_NumCbvSrvUav[VarType] = SrcLayout.m_NumCbvSrvUav[VarType];
        m_NumSamplers[VarType] = SrcLayout.m_NumSamplers[VarType];
    }
    
    AllocateMemory(ResourceLayoutDataAllocator);

    Uint32 CurrCbvSrvUav[SHADER_VARIABLE_TYPE_NUM_TYPES] = {0,0,0};
    Uint32 CurrSampler[SHADER_VARIABLE_TYPE_NUM_TYPES] = {0,0,0};

    for(SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType+1))
    {
        if( !IsAllowedType(VarType, AllowedTypeBits))
            continue;

        Uint32 NumSrcCbvSrvUav = SrcLayout.m_NumCbvSrvUav[VarType];
        VERIFY_EXPR(NumSrcCbvSrvUav == m_NumCbvSrvUav[VarType]);
        for( Uint32 r=0; r < NumSrcCbvSrvUav; ++r )
        {
            const auto &SrcRes = SrcLayout.GetSrvCbvUav(VarType, r);
            Uint32 SamplerId = SRV_CBV_UAV::InvalidSamplerId;
            if (SrcRes.IsValidSampler())
            {
                const auto &SrcSamplerAttribs = SrcLayout.GetSampler(VarType, SrcRes.GetSamplerId());
                VERIFY(!SrcSamplerAttribs.Attribs.IsStaticSampler(), "Only non-static samplers can be assigned space in shader cache");
                VERIFY(SrcSamplerAttribs.Attribs.GetVariableType() == SrcRes.Attribs.GetVariableType(), "Inconsistent texture and sampler variable types" );
                VERIFY(SrcSamplerAttribs.IsValidRootIndex(), "Root index must be valid");
                VERIFY(SrcSamplerAttribs.IsValidOffset(), "Offset must be valid");
                VERIFY_EXPR(SrcSamplerAttribs.Attribs.BindCount == SrcRes.Attribs.BindCount || SrcSamplerAttribs.Attribs.BindCount == 1);

                SamplerId = CurrSampler[VarType];
                VERIFY(SamplerId <= SRV_CBV_UAV::MaxSamplerId, "SamplerId exceeds maximum allowed value (", SRV_CBV_UAV::MaxSamplerId, ")");
                VERIFY_EXPR(SamplerId == SrcRes.GetSamplerId());
                ::new (&GetSampler(VarType, CurrSampler[VarType]++)) Sampler( *this, SrcSamplerAttribs );
            }

            VERIFY(SrcRes.IsValidRootIndex(), "Root index must be valid");
            VERIFY(SrcRes.IsValidOffset(), "Offset must be valid");
            ::new (&GetSrvCbvUav(VarType, CurrCbvSrvUav[VarType]++)) SRV_CBV_UAV( *this, SrcRes, SamplerId );
        }
    }

#ifdef _DEBUG
    for(SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType+1))
    {
        VERIFY_EXPR( CurrCbvSrvUav[VarType] == m_NumCbvSrvUav[VarType] );
        VERIFY_EXPR( CurrSampler[VarType] == m_NumSamplers[VarType] );
    }
#endif
}
#endif

void ShaderResourceLayoutVk::Initialize(const VulkanUtilities::VulkanLogicalDevice &LogicalDevice,
                                        const std::shared_ptr<const SPIRVShaderResources>& pSrcResources,
                                        IMemoryAllocator &LayoutDataAllocator,
                                        const SHADER_VARIABLE_TYPE *AllowedVarTypes,
                                        Uint32 NumAllowedTypes,
                                        ShaderResourceCacheVk *pResourceCache,
                                        std::vector<uint32_t> *pSPIRV,
                                        PipelineLayout *pPipelineLayout)
{
    m_pResources = pSrcResources;
    m_pResourceCache = pResourceCache;
    m_pLogicalDevice = LogicalDevice.GetSharedPtr();

    VERIFY_EXPR( (pResourceCache != nullptr) ^ (pPipelineLayout != nullptr) );

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
    std::array<Uint32, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC + 1> StaticResCacheSetSizes = {};

    auto AddResource = [&](const SPIRVShaderResourceAttribs &Attribs)
    {
        Uint32 Binding = 0;
        Uint32 DescriptorSet = 0;
        static const ResourceTypeToVkDescriptorType ResTypeToVkDescType;
        VkDescriptorType DescriptorType = ResTypeToVkDescType[Attribs.Type];
        if (pPipelineLayout)
        {
            pPipelineLayout->AllocateResourceSlot(Attribs.VarType, m_pResources->GetShaderType(), DescriptorType, Attribs.ArraySize, DescriptorSet, Binding);
            VERIFY(DescriptorSet <= std::numeric_limits<decltype(VkResource::DescriptorSet)>::max(), "Descriptor set (", DescriptorSet, ") excceeds representable max value");
            VERIFY(Binding <= std::numeric_limits<decltype(VkResource::Binding)>::max(), "Binding (", Binding, ") excceeds representable max value");
        }
        else
        {
            // If pipeline layout is not provided - use artifial layout to store
            // static shader resources:
            // Separate samplers at index VK_DESCRIPTOR_TYPE_SAMPLER (0)
            // SampledImages at index VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER (1)
            // Separate images at index VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE (2)
            // Storage images at index VK_DESCRIPTOR_TYPE_STORAGE_IMAGE (3)
            // Index VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER (4) is unused
            // Index VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER (5) is unused
            // Uniform buffers at index VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER (6)
            // Storage buffers at index VK_DESCRIPTOR_TYPE_STORAGE_BUFFER (7)
            // Index VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC (8) is unused
            // Index VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC (9) is unused
            VERIFY_EXPR(m_pResourceCache != nullptr);

            DescriptorSet = DescriptorType;
            Binding = StaticResCacheSetSizes[DescriptorSet];
            StaticResCacheSetSizes[DescriptorSet] += Attribs.ArraySize;
        }

        // Static samplers are never copied, and SamplerId == InvalidSamplerId
        ::new (&GetResource(Attribs.VarType, CurrResInd[Attribs.VarType]++)) VkResource( *this, Attribs, Binding, DescriptorSet );
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
        for(VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_SAMPLER; DescriptorType <= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC; DescriptorType = static_cast<VkDescriptorType>(DescriptorType+1))
        {
            m_pResourceCache->GetDescriptorSet(DescriptorType).SetDebugAttribs(StaticResCacheSetSizes[DescriptorType]);
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

#if 0

#define LOG_RESOURCE_BINDING_ERROR(ResType, pResource, VarName, ShaderName, ...)\
{                                                                                                   \
    const auto &ResName = pResource->GetDesc().Name;                                                \
    LOG_ERROR_MESSAGE( "Failed to bind ", ResType, " \"", ResName, "\" to variable \"", VarName,    \
                        "\" in shader \"", ShaderName, "\". ", __VA_ARGS__ );                \
}



void ShaderResourceLayoutVk::SRV_CBV_UAV::CacheCB(IDeviceObject *pBuffer, ShaderResourceCacheVk::Resource& DstRes, Uint32 ArrayInd, Vk_CPU_DESCRIPTOR_HANDLE ShdrVisibleHeapCPUDescriptorHandle)
{
    // http://diligentgraphics.com/diligent-engine/architecture/Vk/shader-resource-cache#Binding-Objects-to-Shader-Variables

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<BufferVkImpl> pBuffVk(pBuffer, IID_BufferVk);
    if( pBuffVk )
    {
        if( pBuffVk->GetDesc().BindFlags & BIND_UNIFORM_BUFFER )
        {
            if( Attribs.GetVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr )
            {
                if(DstRes.pObject != pBuffVk)
                {
                    auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.GetVariableType());
                    LOG_ERROR_MESSAGE( "Non-null constant buffer is already bound to ", VarTypeStr, " shader variable \"", Attribs.GetPrintName(ArrayInd), "\" in shader \"", m_ParentResLayout.GetShaderName(), "\". Attempring to bind another constant buffer is an error and will be ignored. Use another shader resource binding instance or mark shader variable as dynamic." );
                }

                // Do not update resource if one is already bound unless it is dynamic. This may be 
                // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
                return;
            }

            DstRes.Type = GetResType();
            DstRes.CPUDescriptorHandle = pBuffVk->GetCBVHandle();
            VERIFY(DstRes.CPUDescriptorHandle.ptr != 0 || pBuffVk->GetDesc().Usage == USAGE_DYNAMIC, "No relevant CBV CPU descriptor handle");
            
            if(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0 )
            {
                // Dynamic resources are assigned descriptor in the GPU-visible heap at every draw call, and
                // the descriptor is copied by the RootSignature when resources are committed
                VERIFY(DstRes.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");

                IVkDevice *pVkDevice = m_ParentResLayout.m_pVkDevice;
                pVkDevice->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, DstRes.CPUDescriptorHandle, Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            }

            DstRes.pObject = pBuffVk;
        }
        else
        {
            LOG_RESOURCE_BINDING_ERROR("buffer", pBuffer, Attribs.GetPrintName(ArrayInd), m_ParentResLayout.GetShaderName(), "Buffer was not created with BIND_UNIFORM_BUFFER flag.")
        }
    }
    else
    {
        LOG_RESOURCE_BINDING_ERROR("buffer", pBuffer, Attribs.GetPrintName(ArrayInd), m_ParentResLayout.GetShaderName(), "Incorrect resource type: buffer is expected.")
    }
}


template<typename TResourceViewType>
struct ResourceViewTraits{};

template<>
struct ResourceViewTraits<ITextureViewVk>
{
    static const Char *Name;
    static const INTERFACE_ID &IID;
};
const Char *ResourceViewTraits<ITextureViewVk>::Name = "texture view";
const INTERFACE_ID& ResourceViewTraits<ITextureViewVk>::IID = IID_TextureViewVk;

template<>
struct ResourceViewTraits<IBufferViewVk>
{
    static const Char *Name;
    static const INTERFACE_ID &IID;
};
const Char *ResourceViewTraits<IBufferViewVk>::Name = "buffer view";
const INTERFACE_ID& ResourceViewTraits<IBufferViewVk>::IID = IID_BufferViewVk;

template<typename TResourceViewType,        ///< ResType of the view (ITextureViewVk or IBufferViewVk)
         typename TViewTypeEnum,            ///< ResType of the expected view type enum (TEXTURE_VIEW_TYPE or BUFFER_VIEW_TYPE)
         typename TBindSamplerProcType>     ///< ResType of the procedure to set sampler
void ShaderResourceLayoutVk::SRV_CBV_UAV::CacheResourceView(IDeviceObject *pView, 
                                                               ShaderResourceCacheVk::Resource& DstRes, 
                                                               Uint32 ArrayIndex,
                                                               Vk_CPU_DESCRIPTOR_HANDLE ShdrVisibleHeapCPUDescriptorHandle, 
                                                               TViewTypeEnum dbgExpectedViewType,
                                                               TBindSamplerProcType BindSamplerProc)
{
    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<TResourceViewType> pViewVk(pView, ResourceViewTraits<TResourceViewType>::IID);
    if( pViewVk )
    {
#ifdef VERIFY_SHADER_BINDINGS
        const auto& ViewDesc = pViewVk->GetDesc();
        auto ViewType = ViewDesc.ViewType;
        if( ViewType != dbgExpectedViewType )
        {
            const auto *ExpectedViewTypeName = GetViewTypeLiteralName( dbgExpectedViewType );
            const auto *ActualViewTypeName = GetViewTypeLiteralName( ViewType );
            LOG_RESOURCE_BINDING_ERROR(ResourceViewTraits<TResourceViewType>::Name, pViewVk, Attribs.GetPrintName(ArrayIndex), m_ParentResLayout.GetShaderName(), 
                                        "Incorrect view type: ", ExpectedViewTypeName, " is expected, ", ActualViewTypeName, " provided." );
            return;
        }
#endif
        if( Attribs.GetVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr )
        {
            if(DstRes.pObject != pViewVk)
            {
                auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.GetVariableType());
                LOG_ERROR_MESSAGE( "Non-null resource is already bound to ", VarTypeStr, " shader variable \"", Attribs.GetPrintName(ArrayIndex), "\" in shader \"", m_ParentResLayout.GetShaderName(), "\". Attempting to bind another resource or null is an error and will be ignored. Use another shader resource binding instance or mark shader variable as dynamic." );
            }

            // Do not update resource if one is already bound unless it is dynamic. This may be 
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        DstRes.Type = GetResType();
        DstRes.CPUDescriptorHandle = pViewVk->GetCPUDescriptorHandle();
        VERIFY(DstRes.CPUDescriptorHandle.ptr != 0, "No relevant Vk view");

        if(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
        {
            // Dynamic resources are assigned descriptor in the GPU-visible heap at every draw call, and
            // the descriptor is copied by the RootSignature when resources are committed
            VERIFY(DstRes.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");

            IVkDevice *pVkDevice = m_ParentResLayout.m_pVkDevice;
            pVkDevice->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, DstRes.CPUDescriptorHandle, Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
        DstRes.pObject = pViewVk;

        BindSamplerProc(pViewVk);
    }
    else
    {
        LOG_RESOURCE_BINDING_ERROR("resource", pView, Attribs.GetPrintName(ArrayIndex), m_ParentResLayout.GetShaderName(), "Incorect resource type: ", ResourceViewTraits<TResourceViewType>::Name, " is expected.")
    }   
}

void ShaderResourceLayoutVk::Sampler::CacheSampler(ITextureViewVk *pTexViewVk, Uint32 ArrayIndex, Vk_CPU_DESCRIPTOR_HANDLE ShdrVisibleHeapCPUDescriptorHandle)
{
    auto *pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY(Attribs.IsValidBindPoint(), "Invalid bind point");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    auto &DstSam = pResourceCache->GetRootTable(RootIndex).GetResource(OffsetFromTableStart + ArrayIndex, Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER, m_ParentResLayout.m_pResources->GetShaderType());

#ifdef _DEBUG
    {
        if (pResourceCache->DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::StaticShaderResources)
        {
            VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Static shader resources of a shader should not be assigned shader visible descriptor space");
        }
        else if (pResourceCache->DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::SRBResources)
        {
            if(Attribs.GetVariableType() == SHADER_VARIABLE_TYPE_DYNAMIC)
                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Dynamic resources of a shader resource binding should be assigned shader visible descriptor space at every draw call");
            else
                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0 || pTexViewVk == nullptr, "Non-dynamics resources of a shader resource binding must be assigned shader visible descriptor space");
        }
        else
        {
            UNEXPECTED("Unknown content type");
        }
    }
#endif

    if( pTexViewVk )
    {
        auto pSampler = pTexViewVk->GetSampler();
        if( pSampler )
        {
            if( Attribs.GetVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC && DstSam.pObject != nullptr)
            {
                if(DstSam.pObject != pSampler)
                {
                    auto VarTypeStr = GetShaderVariableTypeLiteralName(Attribs.GetVariableType());
                    LOG_ERROR_MESSAGE( "Non-null sampler is already bound to ", VarTypeStr, " shader variable \"", Attribs.GetPrintName(ArrayIndex), "\" in shader \"", m_ParentResLayout.GetShaderName(), "\". Attempting to bind another sampler is an error and will be ignored. Use another shader resource binding instance or mark shader variable as dynamic." );
                }
                
                // Do not update resource if one is already bound unless it is dynamic. This may be 
                // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
                return;
            }

            DstSam.Type = CachedResourceType::Sampler;

            auto *pSamplerVk = ValidatedCast<SamplerVkImpl>(pSampler);
            DstSam.CPUDescriptorHandle = pSamplerVk->GetCPUDescriptorHandle();
            VERIFY(DstSam.CPUDescriptorHandle.ptr != 0, "No relevant Vk sampler descriptor handle");

            if(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
            {
                // Dynamic resources are assigned descriptor in the GPU-visible heap at every draw call, and
                // the descriptor is copied by the RootSignature when resources are committed
                VERIFY(DstSam.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");

                IVkDevice *pVkDevice = m_ParentResLayout.m_pVkDevice;
                pVkDevice->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, DstSam.CPUDescriptorHandle, Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            }

            DstSam.pObject = pSampler;
        }
        else
        {
            LOG_ERROR_MESSAGE( "Failed to bind sampler to variable \"", Attribs.Name, ". Sampler is not set in the texture view \"", pTexViewVk->GetDesc().Name, "\"" );
        }
    }
    else
    {
        DstSam = ShaderResourceCacheVk::Resource();
    }
} 


ShaderResourceLayoutVk::Sampler &ShaderResourceLayoutVk::GetAssignedSampler(const SRV_CBV_UAV &TexSrv)
{
    VERIFY(TexSrv.GetResType() == CachedResourceType::TexSRV, "Unexpected resource type: texture SRV is expected");
    VERIFY(TexSrv.IsValidSampler(), "Texture SRV has no associated sampler");
    auto &SamInfo = GetSampler(TexSrv.Attribs.GetVariableType(), TexSrv.GetSamplerId());
    VERIFY(SamInfo.Attribs.GetVariableType() == TexSrv.Attribs.GetVariableType(), "Inconsistent texture and sampler variable types");
    VERIFY(SamInfo.Attribs.Name == TexSrv.Attribs.Name + D3DSamplerSuffix, "Sampler name \"", SamInfo.Attribs.Name, "\" does not match texture name \"", TexSrv.Attribs.Name, '\"');
    return SamInfo;
}
#endif

void ShaderResourceLayoutVk::VkResource::BindResource(IDeviceObject *pObj, Uint32 ArrayIndex, const ShaderResourceLayoutVk *dbgResLayout)
{
#if 0
    auto *pResourceCache = m_ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY(dbgResLayout == nullptr || pResourceCache == dbgResLayout->m_pResourceCache, "Invalid resource cache");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    auto &DstRes = pResourceCache->GetRootTable(GetRootIndex()).GetResource(OffsetFromTableStart + ArrayIndex, Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_ParentResLayout.m_pResources->GetShaderType());
    auto ShdrVisibleHeapCPUDescriptorHandle = pResourceCache->GetShaderVisibleTableCPUDescriptorHandle<Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(GetRootIndex(), OffsetFromTableStart+ArrayIndex);

#ifdef _DEBUG
    {
        if (pResourceCache->DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::StaticShaderResources)
        {
            VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Static shader resources of a shader should not be assigned shader visible descriptor space");
        }
        else if (pResourceCache->DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::SRBResources)
        {
            if (GetResType() == CachedResourceType::CBV)
            {
                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Constant buffers are bound as root views and should not be assigned shader visible descriptor space");
            }
            else
            {
                if(Attribs.GetVariableType() == SHADER_VARIABLE_TYPE_DYNAMIC)
                    VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Dynamic resources of a shader resource binding should be assigned shader visible descriptor space at every draw call");
                else
                    VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0, "Non-dynamics resources of a shader resource binding must be assigned shader visible descriptor space");
            }
        }
        else
        {
            UNEXPECTED("Unknown content type");
        }
    }
#endif

    if( pObj )
    {
        switch (GetResType())
        {
            case CachedResourceType::CBV:
                CacheCB(pObj, DstRes, ArrayIndex, ShdrVisibleHeapCPUDescriptorHandle); 
            break;
            
            case CachedResourceType::TexSRV: 
                CacheResourceView<ITextureViewVk>(pObj, DstRes, ArrayIndex, ShdrVisibleHeapCPUDescriptorHandle, TEXTURE_VIEW_SHADER_RESOURCE, [&](ITextureViewVk* pTexView)
                {
                    if(IsValidSampler())
                    {
                        auto &Sam = m_ParentResLayout.GetAssignedSampler(*this);
                        VERIFY( !Sam.Attribs.IsStaticSampler(), "Static samplers should never be assigned space in the cache" );
                        VERIFY_EXPR(Attribs.BindCount == Sam.Attribs.BindCount || Sam.Attribs.BindCount == 1);
                        auto SamplerArrInd = Sam.Attribs.BindCount > 1 ? ArrayIndex : 0;
                        auto ShdrVisibleSamplerHeapCPUDescriptorHandle = pResourceCache->GetShaderVisibleTableCPUDescriptorHandle<Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER>(Sam.RootIndex, Sam.OffsetFromTableStart + SamplerArrInd);
                        Sam.CacheSampler(pTexView, SamplerArrInd, ShdrVisibleSamplerHeapCPUDescriptorHandle);
                    }
                });
            break;

            case CachedResourceType::TexUAV: 
                CacheResourceView<ITextureViewVk>(pObj, DstRes, ArrayIndex, ShdrVisibleHeapCPUDescriptorHandle, TEXTURE_VIEW_UNORDERED_ACCESS, [](ITextureViewVk*){});
            break;

            case CachedResourceType::BufSRV: 
                CacheResourceView<IBufferViewVk>(pObj, DstRes, ArrayIndex, ShdrVisibleHeapCPUDescriptorHandle, BUFFER_VIEW_SHADER_RESOURCE, [](IBufferViewVk*){});
            break;

            case CachedResourceType::BufUAV: 
                CacheResourceView<IBufferViewVk>( pObj, DstRes, ArrayIndex, ShdrVisibleHeapCPUDescriptorHandle, BUFFER_VIEW_UNORDERED_ACCESS, [](IBufferViewVk*){});
            break;

            default: UNEXPECTED("Unknown resource type ", static_cast<Int32>(GetResType()));
        }
    }
    else
    {
        if (DstRes.pObject && Attribs.GetVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC)
        {
            LOG_ERROR_MESSAGE( "Shader variable \"", Attribs.Name, "\" in shader \"", m_ParentResLayout.GetShaderName(), "\" is not dynamic but being unbound. This is an error and may cause unpredicted behavior. Use another shader resource binding instance or mark shader variable as dynamic if you need to bind another resource." );
        }

        DstRes = ShaderResourceCacheVk::Resource();
        if(IsValidSampler())
        {
            auto &Sam = m_ParentResLayout.GetAssignedSampler(*this);
            Vk_CPU_DESCRIPTOR_HANDLE NullHandle = {0};
            auto SamplerArrInd = Sam.Attribs.BindCount > 1 ? ArrayIndex : 0;
            Sam.CacheSampler(nullptr, SamplerArrInd, NullHandle);
        }
    }
#endif
}

bool ShaderResourceLayoutVk::VkResource::IsBound(Uint32 ArrayIndex)
{
    auto *pResourceCache = ParentResLayout.m_pResourceCache;
    VERIFY(pResourceCache, "Resource cache is null");
    VERIFY_EXPR(ArrayIndex < SpirvAttribs.ArraySize);

    if( DescriptorSet < pResourceCache->GetNumDescriptorSets() )
    {
        auto &RootTable = pResourceCache->GetDescriptorSet(DescriptorSet);
        if(Binding + ArrayIndex < RootTable.GetSize())
        {
            auto &CachedRes = RootTable.GetResource(Binding + ArrayIndex);
            if( CachedRes.pObject != nullptr )
            {
#if 0
                VERIFY(CachedRes.CPUDescriptorHandle.ptr != 0 || CachedRes.pObject.RawPtr<BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC, "No relevant descriptor handle");
#endif
                return true;
            }
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

            const auto& VarName = Res.SpirvAttribs.Name;
            RefCntAutoPtr<IDeviceObject> pObj;
            VERIFY_EXPR(pResourceMapping != nullptr);
            pResourceMapping->GetResource( VarName.c_str(), &pObj, ArrInd );
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
        if(Res.SpirvAttribs.Name.compare(Name) == 0)
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


#if 0
void ShaderResourceLayoutVk::CopyStaticResourceDesriptorHandles(const ShaderResourceLayoutVk &SrcLayout)
{
    if (!m_pResourceCache)
    {
        LOG_ERROR("Resource layout has no resource cache");
        return;
    }

    if (!SrcLayout.m_pResourceCache)
    {
        LOG_ERROR("Dst layout has no resource cache");
        return;
    }

    // Static shader resources are stored as follows:
    // CBVs at root index Vk_DESCRIPTOR_RANGE_TYPE_CBV,
    // SRVs at root index Vk_DESCRIPTOR_RANGE_TYPE_SRV,
    // UAVs at root index Vk_DESCRIPTOR_RANGE_TYPE_UAV, and
    // Samplers at root index Vk_DESCRIPTOR_RANGE_TYPE_SAMPLER
    // Every resource is stored at offset that equals resource bind point

    for(Uint32 r=0; r < m_NumCbvSrvUav[SHADER_VARIABLE_TYPE_STATIC]; ++r)
    {
        // Get resource attributes
        const auto &res = GetSrvCbvUav(SHADER_VARIABLE_TYPE_STATIC, r);
        VERIFY(SrcLayout.m_pResources->GetShaderType() == m_pResources->GetShaderType(), "Incosistent shader types");
        auto RangeType = GetDescriptorRangeType(res.GetResType());
        for(Uint32 ArrInd = 0; ArrInd < res.Attribs.BindCount; ++ArrInd)
        {
            auto BindPoint = res.Attribs.BindPoint + ArrInd;
            // Source resource in the static resource cache is in the root table at index RangeType, at offset BindPoint 
            // Vk_DESCRIPTOR_RANGE_TYPE_SRV = 0,
            // Vk_DESCRIPTOR_RANGE_TYPE_UAV = 1
            // Vk_DESCRIPTOR_RANGE_TYPE_CBV = 2
            const auto &SrcRes = SrcLayout.m_pResourceCache->GetRootTable(RangeType).GetResource(BindPoint, Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SrcLayout.m_pResources->GetShaderType());
            if( !SrcRes.pObject )
                LOG_ERROR_MESSAGE( "No resource assigned to static shader variable \"", res.Attribs.GetPrintName(ArrInd), "\" in shader \"", GetShaderName(), "\"." );
            // Destination resource is at the root index and offset defined by the resource layout
            auto &DstRes = m_pResourceCache->GetRootTable(res.GetRootIndex()).GetResource(res.OffsetFromTableStart + ArrInd, Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_pResources->GetShaderType());
        
            if(DstRes.pObject != SrcRes.pObject)
            {
                VERIFY(DstRes.pObject == nullptr, "Static resource has already been initialized, and the resource to be assigned from the shader does not match previously assigned resource");

                DstRes.pObject = SrcRes.pObject;
                DstRes.Type = SrcRes.Type;
                DstRes.CPUDescriptorHandle = SrcRes.CPUDescriptorHandle;

                auto ShdrVisibleHeapCPUDescriptorHandle = m_pResourceCache->GetShaderVisibleTableCPUDescriptorHandle<Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(res.GetRootIndex(), res.OffsetFromTableStart + ArrInd);
                VERIFY_EXPR(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0 || DstRes.Type == CachedResourceType::CBV);
                // Root views are not assigned space in the GPU-visible descriptor heap allocation
                if (ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
                {
                    m_pVkDevice->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, SrcRes.CPUDescriptorHandle, Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                }
            }
            else
            {
                VERIFY_EXPR(DstRes.pObject == SrcRes.pObject);
                VERIFY_EXPR(DstRes.Type == SrcRes.Type);
                VERIFY_EXPR(DstRes.CPUDescriptorHandle.ptr == SrcRes.CPUDescriptorHandle.ptr);
            }
        }

        if(res.IsValidSampler())
        {
            auto &SamInfo = GetAssignedSampler(res);

            VERIFY(!SamInfo.Attribs.IsStaticSampler(), "Static samplers should never be assigned space in the cache");
            
            VERIFY(SamInfo.Attribs.IsValidBindPoint(), "Sampler bind point must be valid");
            VERIFY_EXPR(SamInfo.Attribs.BindCount == res.Attribs.BindCount || SamInfo.Attribs.BindCount == 1);

            for(Uint32 ArrInd = 0; ArrInd < SamInfo.Attribs.BindCount; ++ArrInd)
            {
                auto BindPoint = SamInfo.Attribs.BindPoint + ArrInd;
                // Source sampler in the static resource cache is in the root table at index 3 
                // (Vk_DESCRIPTOR_RANGE_TYPE_SAMPLER = 3), at offset BindPoint 
                auto& SrcSampler = SrcLayout.m_pResourceCache->GetRootTable(Vk_DESCRIPTOR_RANGE_TYPE_SAMPLER).GetResource(BindPoint, Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER, SrcLayout.m_pResources->GetShaderType());
                if( !SrcSampler.pObject )
                    LOG_ERROR_MESSAGE( "No sampler assigned to static shader variable \"", res.Attribs.GetPrintName(ArrInd), "\" in shader \"", GetShaderName(), "\"." );
                auto &DstSampler = m_pResourceCache->GetRootTable(SamInfo.RootIndex).GetResource(SamInfo.OffsetFromTableStart + ArrInd, Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER, m_pResources->GetShaderType());
            
                if(DstSampler.pObject != SrcSampler.pObject)
                {
                    VERIFY(DstSampler.pObject == nullptr, "Static sampler resource has already been initialized, and the resource to be assigned from the shader does not match previously assigned resource");

                    DstSampler.pObject = SrcSampler.pObject;
                    DstSampler.Type = SrcSampler.Type;
                    DstSampler.CPUDescriptorHandle = SrcSampler.CPUDescriptorHandle;

                    auto ShdrVisibleSamplerHeapCPUDescriptorHandle = m_pResourceCache->GetShaderVisibleTableCPUDescriptorHandle<Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER>(SamInfo.RootIndex, SamInfo.OffsetFromTableStart + ArrInd);
                    VERIFY_EXPR(ShdrVisibleSamplerHeapCPUDescriptorHandle.ptr != 0);
                    if (ShdrVisibleSamplerHeapCPUDescriptorHandle.ptr != 0)
                    {
                        m_pVkDevice->CopyDescriptorsSimple(1, ShdrVisibleSamplerHeapCPUDescriptorHandle, SrcSampler.CPUDescriptorHandle, Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER);
                    }
                }
                else
                {
                    VERIFY_EXPR(DstSampler.pObject == SrcSampler.pObject);
                    VERIFY_EXPR(DstSampler.Type == SrcSampler.Type);
                    VERIFY_EXPR(DstSampler.CPUDescriptorHandle.ptr == SrcSampler.CPUDescriptorHandle.ptr);
                }
            }
        }
    }
}
#endif

#ifdef VERIFY_SHADER_BINDINGS
void ShaderResourceLayoutVk::dbgVerifyBindings()const
{
#if 0
    VERIFY(m_pResourceCache, "Resource cache is null");

    for(SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_STATIC; VarType < SHADER_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_VARIABLE_TYPE>(VarType+1))
    {
        for(Uint32 r=0; r < m_NumCbvSrvUav[VarType]; ++r)
        {
            const auto &res = GetSrvCbvUav(VarType, r);
            VERIFY(res.Attribs.GetVariableType() == VarType, "Unexpected variable type");

            for(Uint32 ArrInd = 0; ArrInd < res.Attribs.BindCount; ++ArrInd)
            {
                const auto &CachedRes = m_pResourceCache->GetRootTable(res.GetRootIndex()).GetResource(res.OffsetFromTableStart + ArrInd, Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_pResources->GetShaderType());
                if(CachedRes.pObject)
                    VERIFY(CachedRes.Type == res.GetResType(), "Inconsistent cached resource types");
                else
                    VERIFY(CachedRes.Type == CachedResourceType::Unknown, "Unexpected cached resource types");

                if( !CachedRes.pObject || 
                     // Dynamic buffers do not have CPU descriptor handle as they do not keep Vk buffer, and space is allocated from the GPU ring buffer
                     CachedRes.CPUDescriptorHandle.ptr == 0 && !(CachedRes.Type==CachedResourceType::CBV && CachedRes.pObject.RawPtr<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC) )
                    LOG_ERROR_MESSAGE( "No resource is bound to ", GetShaderVariableTypeLiteralName(res.Attribs.GetVariableType()), " variable \"", res.Attribs.GetPrintName(ArrInd), "\" in shader \"", GetShaderName(), "\"" );
                
                if (res.Attribs.BindCount > 1 && res.IsValidSampler())
                {
                    // Verify that if single sampler is used for all texture array elements, all samplers set in the resource views are consistent
                    const auto &SamInfo = const_cast<ShaderResourceLayoutVk*>(this)->GetAssignedSampler(res);
                    if(SamInfo.Attribs.BindCount == 1)
                    {
                        const auto &CachedSampler = m_pResourceCache->GetRootTable(SamInfo.RootIndex).GetResource(SamInfo.OffsetFromTableStart, Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER, m_pResources->GetShaderType());
                        if( auto *pTexView = CachedRes.pObject.RawPtr<const ITextureView>() )
                        {
                            auto *pSampler = const_cast<ITextureView*>(pTexView)->GetSampler();
                            if (pSampler != nullptr && CachedSampler.pObject != pSampler)
                                LOG_ERROR_MESSAGE( "All elements of texture array \"", res.Attribs.Name, "\" in shader \"", GetShaderName(), "\" share the same sampler. However, the sampler set in view for element ", ArrInd, " does not match bound sampler. This may cause incorrect behavior on GL platform."  );
                        }
                    }
                }

#ifdef _DEBUG
                {
                    auto ShdrVisibleHeapCPUDescriptorHandle = m_pResourceCache->GetShaderVisibleTableCPUDescriptorHandle<Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(res.GetRootIndex(), res.OffsetFromTableStart + ArrInd);
                    if (m_pResourceCache->DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::StaticShaderResources)
                    {
                        VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Static shader resources of a shader should not be assigned shader visible descriptor space");
                    }
                    else if (m_pResourceCache->DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::SRBResources)
                    {
                        if (res.GetResType() == CachedResourceType::CBV)
                        {
                            VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Constant buffers are bound as root views and should not be assigned shader visible descriptor space");
                        }
                        else
                        {
                            if(res.Attribs.GetVariableType() == SHADER_VARIABLE_TYPE_DYNAMIC)
                                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Dynamic resources of a shader resource binding should be assigned shader visible descriptor space at every draw call");
                            else
                                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0, "Non-dynamics resources of a shader resource binding must be assigned shader visible descriptor space");
                        }
                    }
                    else
                    {
                        UNEXPECTED("Unknown content type");
                    }
                }
#endif
            }

            if (res.IsValidSampler())
            {
                VERIFY(res.GetResType() == CachedResourceType::TexSRV, "Sampler can only be assigned to a texture SRV" );
                const auto &SamInfo = const_cast<ShaderResourceLayoutVk*>(this)->GetAssignedSampler(res);
                VERIFY( !SamInfo.Attribs.IsStaticSampler(), "Static samplers should never be assigned space in the cache" );
                VERIFY(SamInfo.Attribs.IsValidBindPoint(), "Sampler bind point must be valid");
                
                for(Uint32 ArrInd = 0; ArrInd < SamInfo.Attribs.BindCount; ++ArrInd)
                {
                    const auto &CachedSampler = m_pResourceCache->GetRootTable(SamInfo.RootIndex).GetResource(SamInfo.OffsetFromTableStart + ArrInd, Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER, m_pResources->GetShaderType());
                    if( CachedSampler.pObject )
                        VERIFY(CachedSampler.Type == CachedResourceType::Sampler, "Incorrect cached sampler type");
                    else
                        VERIFY(CachedSampler.Type == CachedResourceType::Unknown, "Unexpected cached sampler type");
                    if( !CachedSampler.pObject || CachedSampler.CPUDescriptorHandle.ptr == 0 )
                        LOG_ERROR_MESSAGE("No sampler is assigned to texture variable \"", res.Attribs.GetPrintName(ArrInd), "\" in shader \"", GetShaderName(), "\"");

    #ifdef _DEBUG
                    {
                        auto ShdrVisibleHeapCPUDescriptorHandle = m_pResourceCache->GetShaderVisibleTableCPUDescriptorHandle<Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER>(SamInfo.RootIndex, SamInfo.OffsetFromTableStart + ArrInd);
                        if (m_pResourceCache->DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::StaticShaderResources)
                        {
                            VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Static shader resources of a shader should not be assigned shader visible descriptor space");
                        }
                        else if (m_pResourceCache->DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::SRBResources)
                        {
                            if(SamInfo.Attribs.GetVariableType() == SHADER_VARIABLE_TYPE_DYNAMIC)
                                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Dynamic resources of a shader resource binding should be assigned shader visible descriptor space at every draw call");
                            else
                                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0, "Non-dynamics resources of a shader resource binding must be assigned shader visible descriptor space");
                        }
                        else
                        {
                            UNEXPECTED("Unknown content type");
                        }
                    }
    #endif
                }
            }
        }

        for(Uint32 s=0; s < m_NumSamplers[VarType]; ++s)
        {
            const auto &sam = GetSampler(VarType, s);
            VERIFY(sam.Attribs.GetVariableType() == VarType, "Unexpected sampler variable type");
            
            for(Uint32 ArrInd = 0; ArrInd < sam.Attribs.BindCount; ++ArrInd)
            {
                const auto &CachedSampler = m_pResourceCache->GetRootTable(sam.RootIndex).GetResource(sam.OffsetFromTableStart + ArrInd, Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER, m_pResources->GetShaderType());
                if( CachedSampler.pObject )
                    VERIFY(CachedSampler.Type == CachedResourceType::Sampler, "Incorrect cached sampler type");
                else
                    VERIFY(CachedSampler.Type == CachedResourceType::Unknown, "Unexpected cached sampler type");
                if( !CachedSampler.pObject || CachedSampler.CPUDescriptorHandle.ptr == 0 )
                    LOG_ERROR_MESSAGE( "No sampler is bound to sampler variable \"", sam.Attribs.GetPrintName(ArrInd), "\" in shader \"", GetShaderName(), "\"" );
            }
        }
    }
#endif
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
