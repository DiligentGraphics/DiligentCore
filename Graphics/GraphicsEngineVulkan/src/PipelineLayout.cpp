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

void PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayout::Release(RenderDeviceVkImpl *pRenderDeviceVk)
{
    pRenderDeviceVk->SafeReleaseVkObject(std::move(VkLayout));
    pBindings = nullptr;
    NumLayoutBindings = 0;
}

PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayout::~DescriptorSetLayout()
{
    VERIFY(VkLayout == VK_NULL_HANDLE, "Vulkan descriptor set layout has not been released. Did you forget to call Release()?");
}

bool PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayout::operator == (const DescriptorSetLayout& rhs)const
{
    if(NumLayoutBindings != rhs.NumLayoutBindings || 
       TotalDescriptors  != rhs.TotalDescriptors)
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
        if(B0.pImmutableSamplers != nullptr && B1.pImmutableSamplers != nullptr)
        {
            // If descriptorType is VK_DESCRIPTOR_TYPE_SAMPLER or VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
            // and descriptorCount is not 0 and pImmutableSamplers is not NULL, pImmutableSamplers must be a 
            // valid pointer to an array of descriptorCount valid VkSampler handles (13.2.1)
            if(memcmp(B0.pImmutableSamplers, B1.pImmutableSamplers, sizeof(VkSampler) * B0.descriptorCount) != 0)
                return false;
        }
    }
    return true;
}

size_t PipelineLayout::DescriptorSetLayoutManager::DescriptorSetLayout::GetHash()const
{
    size_t Hash = ComputeHash(NumLayoutBindings, TotalDescriptors);
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
    std::array<VkDescriptorSetLayout, 2> ActiveDescrSetLayouts;
    int CurrActiveSet = 0;
    for(size_t i=0; i < m_DescriptorSetLayouts.size(); ++i)
    {
        auto &Layout = m_DescriptorSetLayouts[i];
        if(Layout.SetIndex >= 0)
        {
            std::copy(Layout.pBindings, Layout.pBindings + Layout.NumLayoutBindings, m_LayoutBindings.begin() + BindingOffset);
            Layout.Finalize(LogicalDevice, m_MemAllocator, &m_LayoutBindings[BindingOffset]);
            BindingOffset += Layout.NumLayoutBindings;
            ActiveDescrSetLayouts[CurrActiveSet++] = Layout.VkLayout;
        }
    }
    VERIFY_EXPR(CurrActiveSet == m_ActiveSets);

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
        Layout.Release(pRenderDeviceVk);

    pRenderDeviceVk->SafeReleaseVkObject(std::move(m_VkPipelineLayout));
}

PipelineLayout::DescriptorSetLayoutManager::~DescriptorSetLayoutManager()
{
    VERIFY(m_VkPipelineLayout == VK_NULL_HANDLE, "Vulkan pipeline layout has not been released. Did you forget to call Release()?");
}

bool PipelineLayout::DescriptorSetLayoutManager::operator == (const DescriptorSetLayoutManager& rhs)const
{
    if(m_DescriptorSetLayouts.size() != rhs.m_DescriptorSetLayouts.size())
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

void PipelineLayout::DescriptorSetLayoutManager::AllocateResourceSlot(const SPIRVShaderResourceAttribs &ResAttribs,
                                                                      SHADER_TYPE ShaderType,
                                                                      Uint32 &DescriptorSet,
                                                                      Uint32 &Binding,
                                                                      Uint32 &OffsetFromTableStart)
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
    VkBinding.stageFlags = ShaderTypeToVkShaderStageFlagBit(ShaderType);
    VkBinding.pImmutableSamplers = nullptr;
    OffsetFromTableStart = DescrSet.TotalDescriptors;
    DescrSet.AddBinding(VkBinding, m_MemAllocator);
}

PipelineLayout::PipelineLayout() : 
    m_MemAllocator(GetRawAllocator()),
    m_LayoutMgr(m_MemAllocator)/*,
    m_StaticSamplers( STD_ALLOCATOR_RAW_MEM(StaticSamplerAttribs, GetRawAllocator(), "Allocator for vector<StaticSamplerAttribs>") )
    */
{
}

void PipelineLayout::Release(RenderDeviceVkImpl *pDeviceVkImpl)
{
    m_LayoutMgr.Release(pDeviceVkImpl);
}

#if 0
void PipelineLayout::InitStaticSampler(SHADER_TYPE ShaderType, const String &TextureName, const D3DShaderResourceAttribs &SamplerAttribs)
{
    auto ShaderVisibility = GetShaderVisibility(ShaderType);
    auto SamplerFound = false;
    for (auto &StSmplr : m_StaticSamplers)
    {
        if (StSmplr.ShaderVisibility == ShaderVisibility &&
            TextureName.compare(StSmplr.SamplerDesc.TextureName) == 0)
        {
            StSmplr.ShaderRegister = SamplerAttribs.BindPoint;
            StSmplr.ArraySize = SamplerAttribs.BindCount;
            StSmplr.RegisterSpace = 0;
            SamplerFound = true;
            break;
        }
    }

    if (!SamplerFound)
    {
        LOG_ERROR("Failed to find static sampler for variable \"", TextureName, '\"');
    }
}
#endif


void PipelineLayout::AllocateResourceSlot(const SPIRVShaderResourceAttribs &ResAttribs,
                                          SHADER_TYPE ShaderType,
                                          Uint32 &DescriptorSet, // Output parameter
                                          Uint32 &Binding, // Output parameter
                                          Uint32 &OffsetFromTableStart,
                                          std::vector<uint32_t> &SPIRV)
{
    m_LayoutMgr.AllocateResourceSlot(ResAttribs, ShaderType, DescriptorSet, Binding, OffsetFromTableStart);
    SPIRV[ResAttribs.BindingDecorationOffset] = Binding;
    SPIRV[ResAttribs.DescriptorSetDecorationOffset] = DescriptorSet;

#if 0
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    auto ShaderVisibility = GetShaderVisibility(ShaderType);
    if (RangeType == Vk_DESCRIPTOR_RANGE_TYPE_CBV && ShaderResAttribs.BindCount == 1)
    {
        // Allocate single CBV directly in the root signature

        // Get the next available root index past all allocated tables and root views
        RootIndex = m_RootParams.GetNumRootTables() + m_RootParams.GetNumRootViews();
        OffsetFromTableStart = 0;

        // Add new root view to existing root parameters
        m_RootParams.AddRootView(Vk_ROOT_PARAMETER_TYPE_CBV, RootIndex, ShaderResAttribs.BindPoint, ShaderVisibility, ShaderResAttribs.GetVariableType());
    }
    else
    {
        // Use the same table for static and mutable resources. Treat both as static
        auto RootTableType = (ShaderResAttribs.GetVariableType() == SHADER_VARIABLE_TYPE_DYNAMIC) ? SHADER_VARIABLE_TYPE_DYNAMIC : SHADER_VARIABLE_TYPE_STATIC;
        auto TableIndKey = ShaderInd * SHADER_VARIABLE_TYPE_NUM_TYPES + RootTableType;
        // Get the table array index (this is not the root index!)
        auto &RootTableArrayInd = (( RangeType == Vk_DESCRIPTOR_RANGE_TYPE_SAMPLER ) ? m_SamplerRootTablesMap : m_SrvCbvUavRootTablesMap)[ TableIndKey ];
        if (RootTableArrayInd == InvalidRootTableIndex)
        {
            // Root table has not been assigned to this combination yet

            // Get the next available root index past all allocated tables and root views
            RootIndex = m_RootParams.GetNumRootTables() +  m_RootParams.GetNumRootViews();
            VERIFY_EXPR(m_RootParams.GetNumRootTables() < 255);
            RootTableArrayInd = static_cast<Uint8>( m_RootParams.GetNumRootTables() );
            // Add root table with one single-descriptor range
            m_RootParams.AddRootTable(RootIndex, ShaderVisibility, RootTableType, 1);
        }
        else
        {
            // Add a new single-descriptor range to the existing table at index RootTableArrayInd
            m_RootParams.AddDescriptorRanges(RootTableArrayInd, 1);
        }
        
        // Reference to either existing or just added table
        auto &CurrParam = m_RootParams.GetRootTable(RootTableArrayInd);
        RootIndex = CurrParam.GetRootIndex();

        const auto& VkRootParam = static_cast<const Vk_ROOT_PARAMETER&>(CurrParam);

        VERIFY( VkRootParam.ShaderVisibility == ShaderVisibility, "Shader visibility is not correct" );
        
        // Descriptors are tightly packed, so the next descriptor offset is the
        // current size of the table
        OffsetFromTableStart = CurrParam.GetDescriptorTableSize();

        // New just added range is the last range in the descriptor table
        Uint32 NewDescriptorRangeIndex = VkRootParam.DescriptorTable.NumDescriptorRanges-1;
        CurrParam.SetDescriptorRange(NewDescriptorRangeIndex, 
                                     RangeType, // Range type (CBV, SRV, UAV or SAMPLER)
                                     ShaderResAttribs.BindPoint, // Shader register
                                     ShaderResAttribs.BindCount, // Number of registers used (1 for non-array resources)
                                     0, // Register space. Always 0 for now
                                     OffsetFromTableStart // Offset in descriptors from the table start
                                     );
    }
#endif
}


#if 0
void PipelineLayout::AllocateStaticSamplers(IShader* const*ppShaders, Uint32 NumShaders)
{
    Uint32 TotalSamplers = 0;
    for(Uint32 s=0;s < NumShaders; ++s)
        TotalSamplers += ppShaders[s]->GetDesc().NumStaticSamplers;
    if (TotalSamplers > 0)
    {
        m_StaticSamplers.reserve(TotalSamplers);
        for(Uint32 sh=0;sh < NumShaders; ++sh)
        {
            const auto &Desc = ppShaders[sh]->GetDesc();
            for(Uint32 sam=0; sam < Desc.NumStaticSamplers; ++sam)
            {
                m_StaticSamplers.emplace_back(Desc.StaticSamplers[sam], GetShaderVisibility(Desc.ShaderType));
            }
        }
        VERIFY_EXPR(m_StaticSamplers.size() == TotalSamplers);
    }
}
#endif

void PipelineLayout::Finalize(const VulkanUtilities::VulkanLogicalDevice& LogicalDevice)
{
    m_LayoutMgr.Finalize(LogicalDevice);

#if 0
    Vk_ROOT_SIGNATURE_DESC PipelineLayoutDesc;
    PipelineLayoutDesc.Flags = Vk_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    
    auto TotalParams = m_RootParams.GetNumRootTables() + m_RootParams.GetNumRootViews();
    std::vector<Vk_ROOT_PARAMETER, STDAllocatorRawMem<Vk_ROOT_PARAMETER> > VkParameters( TotalParams, Vk_ROOT_PARAMETER(), STD_ALLOCATOR_RAW_MEM(Vk_ROOT_PARAMETER, GetRawAllocator(), "Allocator for vector<Vk_ROOT_PARAMETER>") );
    for(Uint32 rt = 0; rt < m_RootParams.GetNumRootTables(); ++rt)
    {
        auto &RootTable = m_RootParams.GetRootTable(rt);
        const Vk_ROOT_PARAMETER &SrcParam = RootTable;
        VERIFY( SrcParam.ParameterType == Vk_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && SrcParam.DescriptorTable.NumDescriptorRanges > 0, "Non-empty descriptor table is expected" );
        VkParameters[RootTable.GetRootIndex()] = SrcParam;
    }
    for(Uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
    {
        auto &RootView = m_RootParams.GetRootView(rv);
        const Vk_ROOT_PARAMETER &SrcParam = RootView;
        VERIFY( SrcParam.ParameterType == Vk_ROOT_PARAMETER_TYPE_CBV, "Root CBV is expected" );
        VkParameters[RootView.GetRootIndex()] = SrcParam;
    }


    PipelineLayoutDesc.NumParameters = static_cast<UINT>(VkParameters.size());
    PipelineLayoutDesc.pParameters = VkParameters.size() ? VkParameters.data() : nullptr;

    UINT TotalVkStaticSamplers = 0;
    for(const auto &StSam : m_StaticSamplers)
        TotalVkStaticSamplers += StSam.ArraySize;
    PipelineLayoutDesc.NumStaticSamplers = TotalVkStaticSamplers;
    PipelineLayoutDesc.pStaticSamplers = nullptr;
    std::vector<Vk_STATIC_SAMPLER_DESC, STDAllocatorRawMem<Vk_STATIC_SAMPLER_DESC> > VkStaticSamplers( STD_ALLOCATOR_RAW_MEM(Vk_STATIC_SAMPLER_DESC, GetRawAllocator(), "Allocator for vector<Vk_STATIC_SAMPLER_DESC>") );
    VkStaticSamplers.reserve(TotalVkStaticSamplers);
    if ( !m_StaticSamplers.empty() )
    {
        for(size_t s=0; s < m_StaticSamplers.size(); ++s)
        {
            const auto &StSmplrDesc = m_StaticSamplers[s];
            const auto &SamDesc = StSmplrDesc.SamplerDesc.Desc;
            for(UINT ArrInd = 0; ArrInd < StSmplrDesc.ArraySize; ++ArrInd)
            {
                VkStaticSamplers.emplace_back(
                    Vk_STATIC_SAMPLER_DESC{
                        FilterTypeToVkFilter(SamDesc.MinFilter, SamDesc.MagFilter, SamDesc.MipFilter),
                        TexAddressModeToVkAddressMode(SamDesc.AddressU),
                        TexAddressModeToVkAddressMode(SamDesc.AddressV),
                        TexAddressModeToVkAddressMode(SamDesc.AddressW),
                        SamDesc.MipLODBias,
                        SamDesc.MaxAnisotropy,
                        ComparisonFuncToVkComparisonFunc(SamDesc.ComparisonFunc),
                        BorderColorToVkStaticBorderColor(SamDesc.BorderColor),
                        SamDesc.MinLOD,
                        SamDesc.MaxLOD,
                        StSmplrDesc.ShaderRegister + ArrInd,
                        StSmplrDesc.RegisterSpace,
                        StSmplrDesc.ShaderVisibility
                    }
                );
            }
        }
        PipelineLayoutDesc.pStaticSamplers = VkStaticSamplers.data();
        
        // Release static samplers array, we no longer need it
        std::vector<StaticSamplerAttribs, STDAllocatorRawMem<StaticSamplerAttribs> > EmptySamplers( STD_ALLOCATOR_RAW_MEM(StaticSamplerAttribs, GetRawAllocator(), "Allocator for vector<StaticSamplerAttribs>") );
        m_StaticSamplers.swap( EmptySamplers );

        VERIFY_EXPR(VkStaticSamplers.size() == TotalVkStaticSamplers);
    }
    

	CComPtr<ID3DBlob> signature;
	CComPtr<ID3DBlob> error;
    HRESULT hr = VkSerializePipelineLayout(&PipelineLayoutDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    hr = pVkDevice->CreatePipelineLayout(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(m_pVkPipelineLayout), reinterpret_cast<void**>( static_cast<IVkPipelineLayout**>(&m_pVkPipelineLayout)));
    CHECK_D3D_RESULT_THROW(hr, "Failed to create root signature");

    bool bHasDynamicResources = m_TotalSrvCbvUavSlots[SHADER_VARIABLE_TYPE_DYNAMIC]!=0 || m_TotalSamplerSlots[SHADER_VARIABLE_TYPE_DYNAMIC]!=0;
    if(bHasDynamicResources)
    {
        CommitDescriptorHandles = &PipelineLayout::CommitDescriptorHandlesInternal_SMD<false>;
        TransitionAndCommitDescriptorHandles = &PipelineLayout::CommitDescriptorHandlesInternal_SMD<true>;
    }
    else
    {
        CommitDescriptorHandles = &PipelineLayout::CommitDescriptorHandlesInternal_SM<false>;
        TransitionAndCommitDescriptorHandles = &PipelineLayout::CommitDescriptorHandlesInternal_SM<true>;
    }
#endif
}

#if 0
//http://diligentgraphics.com/diligent-engine/architecture/Vk/shader-resource-cache#Initializing-the-Cache-for-Shader-Resource-Binding-Object
void PipelineLayout::InitResourceCache(RenderDeviceVkImpl *pDeviceVkImpl, ShaderResourceCacheVk& ResourceCache, IMemoryAllocator &CacheMemAllocator)const
{
    // Get root table size for every root index
    // m_RootParams keeps root tables sorted by the array index, not the root index
    // Root views are treated as one-descriptor tables
    std::vector<Uint32, STDAllocatorRawMem<Uint32> > CacheTableSizes(m_RootParams.GetNumRootTables() + m_RootParams.GetNumRootViews(), 0, STD_ALLOCATOR_RAW_MEM(Uint32, GetRawAllocator(), "Allocator for vector<Uint32>") );
    for(Uint32 rt = 0; rt < m_RootParams.GetNumRootTables(); ++rt)
    {
        auto &RootParam = m_RootParams.GetRootTable(rt);
        CacheTableSizes[RootParam.GetRootIndex()] = RootParam.GetDescriptorTableSize();
    }

    for(Uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
    {
        auto &RootParam = m_RootParams.GetRootView(rv);
        CacheTableSizes[RootParam.GetRootIndex()] = 1;
    }
    // Initialize resource cache to hold root tables 
    ResourceCache.Initialize(CacheMemAllocator, static_cast<Uint32>(CacheTableSizes.size()), CacheTableSizes.data());

    // Allocate space in GPU-visible descriptor heap for static and mutable variables only
    Uint32 TotalSrvCbvUavDescriptors =
                m_TotalSrvCbvUavSlots[SHADER_VARIABLE_TYPE_STATIC] + 
                m_TotalSrvCbvUavSlots[SHADER_VARIABLE_TYPE_MUTABLE];
    Uint32 TotalSamplerDescriptors =
                m_TotalSamplerSlots[SHADER_VARIABLE_TYPE_STATIC] +
                m_TotalSamplerSlots[SHADER_VARIABLE_TYPE_MUTABLE];

    DescriptorHeapAllocation CbcSrvUavHeapSpace, SamplerHeapSpace;
    if(TotalSrvCbvUavDescriptors)
        CbcSrvUavHeapSpace = pDeviceVkImpl->AllocateGPUDescriptors(Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, TotalSrvCbvUavDescriptors);
    VERIFY_EXPR(TotalSrvCbvUavDescriptors == 0 && CbcSrvUavHeapSpace.IsNull() || CbcSrvUavHeapSpace.GetNumHandles() == TotalSrvCbvUavDescriptors);

    if(TotalSamplerDescriptors)
        SamplerHeapSpace = pDeviceVkImpl->AllocateGPUDescriptors(Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER, TotalSamplerDescriptors);
    VERIFY_EXPR(TotalSamplerDescriptors == 0 && SamplerHeapSpace.IsNull() || SamplerHeapSpace.GetNumHandles() == TotalSamplerDescriptors);

    // Iterate through all root static/mutable tables and assign start offsets. The tables are tightly packed, so
    // start offset of table N+1 is start offset of table N plus the size of table N.
    // Root tables with dynamic resources as well as root views are not assigned space in GPU-visible allocation
    // (root views are simply not processed)
    Uint32 SrvCbvUavTblStartOffset = 0;
    Uint32 SamplerTblStartOffset = 0;
    for(Uint32 rt = 0; rt < m_RootParams.GetNumRootTables(); ++rt)
    {
        auto &RootParam = m_RootParams.GetRootTable(rt);
        const auto& VkRootParam = static_cast<const Vk_ROOT_PARAMETER&>(RootParam);
        auto &RootTableCache = ResourceCache.GetRootTable(RootParam.GetRootIndex());
        
        SHADER_TYPE dbgShaderType = SHADER_TYPE_UNKNOWN;
#ifdef _DEBUG
        dbgShaderType = ShaderTypeFromShaderVisibility(VkRootParam.ShaderVisibility);
#endif
        VERIFY_EXPR( VkRootParam.ParameterType == Vk_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE );
        
        auto TableSize = RootParam.GetDescriptorTableSize();
        VERIFY(TableSize > 0, "Unexpected empty descriptor table");

        auto HeapType = HeapTypeFromRangeType(VkRootParam.DescriptorTable.pDescriptorRanges[0].RangeType);

#ifdef _DEBUG
        RootTableCache.SetDebugAttribs( TableSize, HeapType, dbgShaderType );
#endif

        // Space for dynamic variables is allocated at every draw call
        if( RootParam.GetShaderVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC )
        {
            if( HeapType == Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV )
            {
                RootTableCache.m_TableStartOffset = SrvCbvUavTblStartOffset;
                SrvCbvUavTblStartOffset += TableSize;
            }
            else
            {
                RootTableCache.m_TableStartOffset = SamplerTblStartOffset;
                SamplerTblStartOffset += TableSize;
            }
        }
        else
        {
            VERIFY_EXPR(RootTableCache.m_TableStartOffset == ShaderResourceCacheVk::InvalidDescriptorOffset);
        }
    }

#ifdef _DEBUG
    for(Uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
    {
        auto &RootParam = m_RootParams.GetRootView(rv);
        const auto& VkRootParam = static_cast<const Vk_ROOT_PARAMETER&>(RootParam);
        auto &RootTableCache = ResourceCache.GetRootTable(RootParam.GetRootIndex());
        // Root views are not assigned valid table start offset
        VERIFY_EXPR(RootTableCache.m_TableStartOffset == ShaderResourceCacheVk::InvalidDescriptorOffset);
        
        SHADER_TYPE dbgShaderType = ShaderTypeFromShaderVisibility(VkRootParam.ShaderVisibility);
        VERIFY_EXPR(VkRootParam.ParameterType == Vk_ROOT_PARAMETER_TYPE_CBV);
        RootTableCache.SetDebugAttribs( 1, Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dbgShaderType );
    }
#endif
    
    VERIFY_EXPR(SrvCbvUavTblStartOffset == TotalSrvCbvUavDescriptors);
    VERIFY_EXPR(SamplerTblStartOffset == TotalSamplerDescriptors);

    ResourceCache.SetDescriptorHeapSpace(std::move(CbcSrvUavHeapSpace), std::move(SamplerHeapSpace));
}

const Vk_RESOURCE_STATES Vk_RESOURCE_STATE_SHADER_RESOURCE  = Vk_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | Vk_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

__forceinline
void TransitionResource(CommandContext &Ctx, 
                        ShaderResourceCacheVk::Resource &Res,
                        Vk_DESCRIPTOR_RANGE_TYPE RangeType)
{
    switch (Res.Type)
    {
        case CachedResourceType::CBV:
        {
            VERIFY(RangeType == Vk_DESCRIPTOR_RANGE_TYPE_CBV, "Unexpected descriptor range type");
            // Not using QueryInterface() for the sake of efficiency
            auto *pBuffToTransition = Res.pObject.RawPtr<BufferVkImpl>();
            if( !pBuffToTransition->CheckAllStates(Vk_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) )
                Ctx.TransitionResource(pBuffToTransition, Vk_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER );
        }
        break;

        case CachedResourceType::BufSRV:
        {
            VERIFY(RangeType == Vk_DESCRIPTOR_RANGE_TYPE_SRV, "Unexpected descriptor range type");
            auto *pBuffViewVk = Res.pObject.RawPtr<BufferViewVkImpl>();
            auto *pBuffToTransition = ValidatedCast<BufferVkImpl>(pBuffViewVk->GetBuffer());
            if( !pBuffToTransition->CheckAllStates(Vk_RESOURCE_STATE_SHADER_RESOURCE) )
                Ctx.TransitionResource(pBuffToTransition, Vk_RESOURCE_STATE_SHADER_RESOURCE );
        }
        break;

        case CachedResourceType::BufUAV:
        {
            VERIFY(RangeType == Vk_DESCRIPTOR_RANGE_TYPE_UAV, "Unexpected descriptor range type");
            auto *pBuffViewVk = Res.pObject.RawPtr<BufferViewVkImpl>();
            auto *pBuffToTransition = ValidatedCast<BufferVkImpl>(pBuffViewVk->GetBuffer());
            if( !pBuffToTransition->CheckAllStates(Vk_RESOURCE_STATE_UNORDERED_ACCESS) )
                Ctx.TransitionResource(pBuffToTransition, Vk_RESOURCE_STATE_UNORDERED_ACCESS );
        }
        break;

        case CachedResourceType::TexSRV:
        {
            VERIFY(RangeType == Vk_DESCRIPTOR_RANGE_TYPE_SRV, "Unexpected descriptor range type");
            auto *pTexViewVk = Res.pObject.RawPtr<TextureViewVkImpl>();
            auto *pTexToTransition = ValidatedCast<TextureVkImpl>(pTexViewVk->GetTexture());
            if( !pTexToTransition->CheckAllStates(Vk_RESOURCE_STATE_SHADER_RESOURCE) )
                Ctx.TransitionResource(pTexToTransition, Vk_RESOURCE_STATE_SHADER_RESOURCE );
        }
        break;

        case CachedResourceType::TexUAV:
        {
            VERIFY(RangeType == Vk_DESCRIPTOR_RANGE_TYPE_UAV, "Unexpected descriptor range type");
            auto *pTexViewVk = Res.pObject.RawPtr<TextureViewVkImpl>();
            auto *pTexToTransition = ValidatedCast<TextureVkImpl>(pTexViewVk->GetTexture());
            if( !pTexToTransition->CheckAllStates(Vk_RESOURCE_STATE_UNORDERED_ACCESS) )
                Ctx.TransitionResource(pTexToTransition, Vk_RESOURCE_STATE_UNORDERED_ACCESS );
        }
        break;

        case CachedResourceType::Sampler:
            VERIFY(RangeType == Vk_DESCRIPTOR_RANGE_TYPE_SAMPLER, "Unexpected descriptor range type");
        break;

        default:
            // Resource not bound
            VERIFY(Res.Type == CachedResourceType::Unknown, "Unexpected resource type");
            VERIFY(Res.pObject == nullptr && Res.CPUDescriptorHandle.ptr == 0, "Bound resource is unexpected");
    }
}


#ifdef _DEBUG
void DbgVerifyResourceState(ShaderResourceCacheVk::Resource &Res,
                            Vk_DESCRIPTOR_RANGE_TYPE RangeType)
{
    switch (Res.Type)
    {
        case CachedResourceType::CBV:
        {
            VERIFY(RangeType == Vk_DESCRIPTOR_RANGE_TYPE_CBV, "Unexpected descriptor range type");
            // Not using QueryInterface() for the sake of efficiency
            auto *pBuffToTransition = Res.pObject.RawPtr<BufferVkImpl>();
            auto State = pBuffToTransition->GetState();
            if( (State & Vk_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) != Vk_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER )
                LOG_ERROR_MESSAGE("Resource \"", pBuffToTransition->GetDesc().Name, "\" is not in Vk_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER state. Did you forget to call TransitionShaderResources() or specify COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag in a call to CommitShaderResources()?" );
        }
        break;

        case CachedResourceType::BufSRV:
        {
            VERIFY(RangeType == Vk_DESCRIPTOR_RANGE_TYPE_SRV, "Unexpected descriptor range type");
            auto *pBuffViewVk = Res.pObject.RawPtr<BufferViewVkImpl>();
            auto *pBuffToTransition = ValidatedCast<BufferVkImpl>(pBuffViewVk->GetBuffer());
            auto State = pBuffToTransition->GetState();
            if( (State & Vk_RESOURCE_STATE_SHADER_RESOURCE) != Vk_RESOURCE_STATE_SHADER_RESOURCE )
                LOG_ERROR_MESSAGE("Resource \"", pBuffToTransition->GetDesc().Name, "\" is not in correct state. Did you forget to call TransitionShaderResources() or specify COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag in a call to CommitShaderResources()?" );
        }
        break;

        case CachedResourceType::BufUAV:
        {
            VERIFY(RangeType == Vk_DESCRIPTOR_RANGE_TYPE_UAV, "Unexpected descriptor range type");
            auto *pBuffViewVk = Res.pObject.RawPtr<BufferViewVkImpl>();
            auto *pBuffToTransition = ValidatedCast<BufferVkImpl>(pBuffViewVk->GetBuffer());
            auto State = pBuffToTransition->GetState();
            if( (State & Vk_RESOURCE_STATE_UNORDERED_ACCESS) != Vk_RESOURCE_STATE_UNORDERED_ACCESS )
                LOG_ERROR_MESSAGE("Resource \"", pBuffToTransition->GetDesc().Name, "\" is not in Vk_RESOURCE_STATE_UNORDERED_ACCESS state. Did you forget to call TransitionShaderResources() or specify COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag in a call to CommitShaderResources()?" );
        }
        break;

        case CachedResourceType::TexSRV:
        {
            VERIFY(RangeType == Vk_DESCRIPTOR_RANGE_TYPE_SRV, "Unexpected descriptor range type");
            auto *pTexViewVk = Res.pObject.RawPtr<TextureViewVkImpl>();
            auto *pTexToTransition = ValidatedCast<TextureVkImpl>(pTexViewVk->GetTexture());
            auto State = pTexToTransition->GetState();
            if( (State & Vk_RESOURCE_STATE_SHADER_RESOURCE) != Vk_RESOURCE_STATE_SHADER_RESOURCE )
                LOG_ERROR_MESSAGE("Resource \"", pTexToTransition->GetDesc().Name, "\" is not in correct state. Did you forget to call TransitionShaderResources() or specify COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag in a call to CommitShaderResources()?" );
        }
        break;

        case CachedResourceType::TexUAV:
        {
            VERIFY(RangeType == Vk_DESCRIPTOR_RANGE_TYPE_UAV, "Unexpected descriptor range type");
            auto *pTexViewVk = Res.pObject.RawPtr<TextureViewVkImpl>();
            auto *pTexToTransition = ValidatedCast<TextureVkImpl>(pTexViewVk->GetTexture());
            auto State = pTexToTransition->GetState();
            if( (State & Vk_RESOURCE_STATE_UNORDERED_ACCESS) != Vk_RESOURCE_STATE_UNORDERED_ACCESS )
                LOG_ERROR_MESSAGE("Resource \"", pTexToTransition->GetDesc().Name, "\" is not in Vk_RESOURCE_STATE_UNORDERED_ACCESS state. Did you forget to call TransitionShaderResources() or specify COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag in a call to CommitShaderResources()?" );
        }
        break;

        case CachedResourceType::Sampler:
            VERIFY(RangeType == Vk_DESCRIPTOR_RANGE_TYPE_SAMPLER, "Unexpected descriptor range type");
        break;

        default:
            // Resource not bound
            VERIFY(Res.Type == CachedResourceType::Unknown, "Unexpected resource type");
            VERIFY(Res.pObject == nullptr && Res.CPUDescriptorHandle.ptr == 0, "Bound resource is unexpected");
    }
}
#endif

template<class TOperation>
__forceinline void PipelineLayout::DescriptorSetLayoutManager::ProcessRootTables(TOperation Operation)const
{
    for(Uint32 rt = 0; rt < m_NumRootTables; ++rt)
    {
        auto &RootTable = GetRootTable(rt);
        auto RootInd = RootTable.GetRootIndex();
        const Vk_ROOT_PARAMETER& VkParam = RootTable;

        VERIFY_EXPR(VkParam.ParameterType == Vk_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);

        auto &VkTable = VkParam.DescriptorTable;
        VERIFY(VkTable.NumDescriptorRanges > 0 && RootTable.GetDescriptorTableSize() > 0, "Unexepected empty descriptor table");
        bool IsResourceTable = VkTable.pDescriptorRanges[0].RangeType != Vk_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        Vk_DESCRIPTOR_HEAP_TYPE dbgHeapType = Vk_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
#ifdef _DEBUG
            dbgHeapType = IsResourceTable ? Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV : Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER;
#endif
        Operation(RootInd, RootTable, VkParam, IsResourceTable, dbgHeapType);
    }
}

template<class TOperation>
__forceinline void ProcessCachedTableResources(Uint32 RootInd, 
                                               const Vk_ROOT_PARAMETER& VkParam, 
                                               ShaderResourceCacheVk& ResourceCache, 
                                               Vk_DESCRIPTOR_HEAP_TYPE dbgHeapType, 
                                               TOperation Operation)
{
    for (UINT r = 0; r < VkParam.DescriptorTable.NumDescriptorRanges; ++r)
    {
        const auto &range = VkParam.DescriptorTable.pDescriptorRanges[r];
        for (UINT d = 0; d < range.NumDescriptors; ++d)
        {
            SHADER_TYPE dbgShaderType = SHADER_TYPE_UNKNOWN;
#ifdef _DEBUG
            dbgShaderType = ShaderTypeFromShaderVisibility(VkParam.ShaderVisibility);
            VERIFY(dbgHeapType == HeapTypeFromRangeType(range.RangeType), "Mistmatch between descriptor heap type and descriptor range type");
#endif
            auto OffsetFromTableStart = range.OffsetInDescriptorsFromTableStart + d;
            auto& Res = ResourceCache.GetRootTable(RootInd).GetResource(OffsetFromTableStart, dbgHeapType, dbgShaderType);

            Operation(OffsetFromTableStart, range, Res);
        }
    }
}


template<bool PerformResourceTransitions>
void PipelineLayout::CommitDescriptorHandlesInternal_SMD(RenderDeviceVkImpl *pRenderDeviceVk, 
                                                        ShaderResourceCacheVk& ResourceCache, 
                                                        CommandContext &Ctx, 
                                                        bool IsCompute)const
{
    auto *pVkDevice = pRenderDeviceVk->GetVkDevice();

    Uint32 NumDynamicCbvSrvUavDescriptors = m_TotalSrvCbvUavSlots[SHADER_VARIABLE_TYPE_DYNAMIC];
    Uint32 NumDynamicSamplerDescriptors = m_TotalSamplerSlots[SHADER_VARIABLE_TYPE_DYNAMIC];
    VERIFY_EXPR(NumDynamicCbvSrvUavDescriptors > 0 || NumDynamicSamplerDescriptors > 0);

    DescriptorHeapAllocation DynamicCbvSrvUavDescriptors, DynamicSamplerDescriptors;
    if(NumDynamicCbvSrvUavDescriptors)
        DynamicCbvSrvUavDescriptors = Ctx.AllocateDynamicGPUVisibleDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NumDynamicCbvSrvUavDescriptors);
    if(NumDynamicSamplerDescriptors)
        DynamicSamplerDescriptors = Ctx.AllocateDynamicGPUVisibleDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER, NumDynamicSamplerDescriptors);

    CommandContext::ShaderDescriptorHeaps Heaps(ResourceCache.GetSrvCbvUavDescriptorHeap(), ResourceCache.GetSamplerDescriptorHeap());
    if(Heaps.pSamplerHeap == nullptr)
        Heaps.pSamplerHeap = DynamicSamplerDescriptors.GetDescriptorHeap();

    if(Heaps.pSrvCbvUavHeap == nullptr)
        Heaps.pSrvCbvUavHeap = DynamicCbvSrvUavDescriptors.GetDescriptorHeap();

    if(NumDynamicCbvSrvUavDescriptors)
        VERIFY(DynamicCbvSrvUavDescriptors.GetDescriptorHeap() == Heaps.pSrvCbvUavHeap, "Inconsistent CbvSrvUav descriptor heaps" );
    if(NumDynamicSamplerDescriptors)
        VERIFY(DynamicSamplerDescriptors.GetDescriptorHeap() == Heaps.pSamplerHeap, "Inconsistent Sampler descriptor heaps" );

    if(Heaps)
        Ctx.SetDescriptorHeaps(Heaps);

    // Offset to the beginning of the current dynamic CBV_SRV_UAV/SAMPLER table from 
    // the start of the allocation
    Uint32 DynamicCbvSrvUavTblOffset = 0;
    Uint32 DynamicSamplerTblOffset = 0;

    m_RootParams.ProcessRootTables(
        [&](Uint32 RootInd, const RootParameter &RootTable, const Vk_ROOT_PARAMETER& VkParam, bool IsResourceTable, Vk_DESCRIPTOR_HEAP_TYPE dbgHeapType )
        {
            Vk_GPU_DESCRIPTOR_HANDLE RootTableGPUDescriptorHandle;
            bool IsDynamicTable = RootTable.GetShaderVariableType() == SHADER_VARIABLE_TYPE_DYNAMIC;
            if (IsDynamicTable)
            {
                if( IsResourceTable )
                    RootTableGPUDescriptorHandle = DynamicCbvSrvUavDescriptors.GetGpuHandle(DynamicCbvSrvUavTblOffset);
                else
                    RootTableGPUDescriptorHandle = DynamicSamplerDescriptors.GetGpuHandle(DynamicSamplerTblOffset);
            }
            else
            {
                RootTableGPUDescriptorHandle = IsResourceTable ? 
                    ResourceCache.GetShaderVisibleTableGPUDescriptorHandle<Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(RootInd) : 
                    ResourceCache.GetShaderVisibleTableGPUDescriptorHandle<Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER>(RootInd);
                VERIFY(RootTableGPUDescriptorHandle.ptr != 0, "Unexpected null GPU descriptor handle");
            }

            if(IsCompute)
                Ctx.GetCommandList()->SetComputeRootDescriptorTable(RootInd, RootTableGPUDescriptorHandle);
            else
                Ctx.GetCommandList()->SetGraphicsRootDescriptorTable(RootInd, RootTableGPUDescriptorHandle);

            ProcessCachedTableResources(RootInd, VkParam, ResourceCache, dbgHeapType, 
                [&](UINT OffsetFromTableStart, const Vk_DESCRIPTOR_RANGE &range, ShaderResourceCacheVk::Resource &Res)
                {
                    if(PerformResourceTransitions)
                    {
                        TransitionResource(Ctx, Res, range.RangeType);
                    }
#ifdef _DEBUG
                    else
                    {
                        DbgVerifyResourceState(Res, range.RangeType);
                    }
#endif

                    if(IsDynamicTable)
                    {
                        if (IsResourceTable)
                        {
                            if( Res.CPUDescriptorHandle.ptr == 0 )
                                LOG_ERROR_MESSAGE("No valid CbvSrvUav descriptor handle found for root parameter ", RootInd, ", descriptor slot ", OffsetFromTableStart);

                            VERIFY( DynamicCbvSrvUavTblOffset < NumDynamicCbvSrvUavDescriptors, "Not enough space in the descriptor heap allocation");
                            
                            pVkDevice->CopyDescriptorsSimple(1, DynamicCbvSrvUavDescriptors.GetCpuHandle(DynamicCbvSrvUavTblOffset), Res.CPUDescriptorHandle, Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                            ++DynamicCbvSrvUavTblOffset;
                        }
                        else
                        {
                            if( Res.CPUDescriptorHandle.ptr == 0 )
                                LOG_ERROR_MESSAGE("No valid sampler descriptor handle found for root parameter ", RootInd, ", descriptor slot ", OffsetFromTableStart);

                            VERIFY( DynamicSamplerTblOffset < NumDynamicSamplerDescriptors, "Not enough space in the descriptor heap allocation");
                            
                            pVkDevice->CopyDescriptorsSimple(1, DynamicSamplerDescriptors.GetCpuHandle(DynamicSamplerTblOffset), Res.CPUDescriptorHandle, Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER);
                            ++DynamicSamplerTblOffset;
                        }
                    }
                }
            );
        }
    );
    
    VERIFY_EXPR( DynamicCbvSrvUavTblOffset == NumDynamicCbvSrvUavDescriptors );
    VERIFY_EXPR( DynamicSamplerTblOffset == NumDynamicSamplerDescriptors );
}

template<bool PerformResourceTransitions>
void PipelineLayout::CommitDescriptorHandlesInternal_SM(RenderDeviceVkImpl *pRenderDeviceVk, 
                                                       ShaderResourceCacheVk& ResourceCache, 
                                                       CommandContext &Ctx, 
                                                       bool IsCompute)const
{
    VERIFY_EXPR(m_TotalSrvCbvUavSlots[SHADER_VARIABLE_TYPE_DYNAMIC] == 0 && m_TotalSamplerSlots[SHADER_VARIABLE_TYPE_DYNAMIC] == 0);

    CommandContext::ShaderDescriptorHeaps Heaps(ResourceCache.GetSrvCbvUavDescriptorHeap(), ResourceCache.GetSamplerDescriptorHeap());
    if(Heaps)
        Ctx.SetDescriptorHeaps(Heaps);

    m_RootParams.ProcessRootTables(
        [&](Uint32 RootInd, const RootParameter &RootTable, const Vk_ROOT_PARAMETER& VkParam, bool IsResourceTable, Vk_DESCRIPTOR_HEAP_TYPE dbgHeapType )
        {
            VERIFY(RootTable.GetShaderVariableType() != SHADER_VARIABLE_TYPE_DYNAMIC, "Unexpected dynamic resource");

            Vk_GPU_DESCRIPTOR_HANDLE RootTableGPUDescriptorHandle = IsResourceTable ? 
                ResourceCache.GetShaderVisibleTableGPUDescriptorHandle<Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(RootInd) : 
                ResourceCache.GetShaderVisibleTableGPUDescriptorHandle<Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER>(RootInd);
            VERIFY(RootTableGPUDescriptorHandle.ptr != 0, "Unexpected null GPU descriptor handle");

            if(IsCompute)
                Ctx.GetCommandList()->SetComputeRootDescriptorTable(RootInd, RootTableGPUDescriptorHandle);
            else
                Ctx.GetCommandList()->SetGraphicsRootDescriptorTable(RootInd, RootTableGPUDescriptorHandle);

            if(PerformResourceTransitions)
            {
                ProcessCachedTableResources(RootInd, VkParam, ResourceCache, dbgHeapType, 
                    [&](UINT OffsetFromTableStart, const Vk_DESCRIPTOR_RANGE &range, ShaderResourceCacheVk::Resource &Res)
                    {
                        TransitionResource(Ctx, Res, range.RangeType);
                    }
                );
            }
#ifdef _DEBUG
            else
            {
                ProcessCachedTableResources(RootInd, VkParam, ResourceCache, dbgHeapType, 
                    [&](UINT OffsetFromTableStart, const Vk_DESCRIPTOR_RANGE &range, ShaderResourceCacheVk::Resource &Res)
                    {
                        DbgVerifyResourceState(Res, range.RangeType);
                    }
                );
            }
#endif
        }
    );
}


void PipelineLayout::TransitionResources(ShaderResourceCacheVk& ResourceCache, 
                                        class CommandContext &Ctx)const
{
    m_RootParams.ProcessRootTables(
        [&](Uint32 RootInd, const RootParameter &RootTable, const Vk_ROOT_PARAMETER& VkParam, bool IsResourceTable, Vk_DESCRIPTOR_HEAP_TYPE dbgHeapType )
        {
            ProcessCachedTableResources(RootInd, VkParam, ResourceCache, dbgHeapType, 
                [&](UINT OffsetFromTableStart, const Vk_DESCRIPTOR_RANGE &range, ShaderResourceCacheVk::Resource &Res)
                {
                    TransitionResource(Ctx, Res, range.RangeType);
                }
            );
        }
    );
}


void PipelineLayout::CommitRootViews(ShaderResourceCacheVk& ResourceCache, 
                                    CommandContext &Ctx, 
                                    bool IsCompute,
                                    Uint32 ContextId)const
{
    for(Uint32 rv = 0; rv < m_RootParams.GetNumRootViews(); ++rv)
    {
        auto &RootView = m_RootParams.GetRootView(rv);
        auto RootInd = RootView.GetRootIndex();
       
        SHADER_TYPE dbgShaderType = SHADER_TYPE_UNKNOWN;
#ifdef _DEBUG
        auto &Param = static_cast<const Vk_ROOT_PARAMETER&>( RootView );
        VERIFY_EXPR(Param.ParameterType == Vk_ROOT_PARAMETER_TYPE_CBV);
        dbgShaderType = ShaderTypeFromShaderVisibility(Param.ShaderVisibility);
#endif

        auto& Res = ResourceCache.GetRootTable(RootInd).GetResource(0, Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dbgShaderType);
        auto *pBuffToTransition = Res.pObject.RawPtr<BufferVkImpl>();
        if( !pBuffToTransition->CheckAllStates(Vk_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) )
            Ctx.TransitionResource(pBuffToTransition, Vk_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        Vk_GPU_VIRTUAL_ADDRESS CBVAddress = pBuffToTransition->GetGPUAddress(ContextId);
        if(IsCompute)
            Ctx.GetCommandList()->SetComputeRootConstantBufferView(RootInd, CBVAddress);
        else
            Ctx.GetCommandList()->SetGraphicsRootConstantBufferView(RootInd, CBVAddress);
    }
}
#endif
}
