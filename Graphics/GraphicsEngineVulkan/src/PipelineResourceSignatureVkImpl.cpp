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
#include "PipelineResourceSignatureVkImpl.hpp"
#include "ShaderResourceBindingVkImpl.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "VulkanTypeConversions.hpp"

namespace Diligent
{

VkDescriptorType PipelineResourceSignatureVkImpl::GetVkDescriptorType(const PipelineResourceDesc& Res)
{
    const bool WithDynamicOffset = !(Res.Flags & PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_OFFSETS);
    const bool CombinedSampler   = (Res.Flags & PIPELINE_RESOURCE_FLAG_COMBINED_IMAGE);
    const bool UseTexelBuffer    = (Res.Flags & PIPELINE_RESOURCE_FLAG_TEXEL_BUFFER);

    VERIFY_EXPR(WithDynamicOffset ^ UseTexelBuffer);
    VERIFY_EXPR(CombinedSampler ? !(WithDynamicOffset | UseTexelBuffer) : true);

    static_assert(SHADER_RESOURCE_TYPE_LAST == SHADER_RESOURCE_TYPE_ACCEL_STRUCT, "Please update the switch below to handle the new shader resource type");
    switch (Res.ResourceType)
    {
        // clang-format off
        case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:  return WithDynamicOffset ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case SHADER_RESOURCE_TYPE_BUFFER_UAV:       return UseTexelBuffer ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : (WithDynamicOffset ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        case SHADER_RESOURCE_TYPE_TEXTURE_SRV:      return CombinedSampler ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case SHADER_RESOURCE_TYPE_BUFFER_SRV:       return UseTexelBuffer ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER : (WithDynamicOffset ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        case SHADER_RESOURCE_TYPE_TEXTURE_UAV:      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case SHADER_RESOURCE_TYPE_SAMPLER:          return VK_DESCRIPTOR_TYPE_SAMPLER;
        case SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        case SHADER_RESOURCE_TYPE_ACCEL_STRUCT:     return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        default:                                    UNEXPECTED("unknown resource type");
            // clang-format on
    }
    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

PipelineResourceSignatureVkImpl::PipelineResourceSignatureVkImpl(IReferenceCounters*                  pRefCounters,
                                                                 RenderDeviceVkImpl*                  pDevice,
                                                                 const PipelineResourceSignatureDesc& Desc,
                                                                 bool                                 bIsDeviceInternal) :
    TPipelineResourceSignatureBase{pRefCounters, pDevice, Desc, bIsDeviceInternal},
    m_SRBMemAllocator{GetRawAllocator()}
{
#define LOG_PRS_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of a pipeline resource signature '", (Desc.Name ? Desc.Name : ""), "' is invalid: ", ##__VA_ARGS__)

    try
    {
        FixedLinearAllocator MemPool{GetRawAllocator()};

        MemPool.AddSpace<PackedBindingIndex>(Desc.NumResources);

        ReserveSpaceForDescription(MemPool, Desc);

        Int8   StaticVarStageCount = 0;
        Uint32 StaticVarCount      = 0;
        {
            // get active shader stages
            SHADER_TYPE Stages          = SHADER_TYPE_UNKNOWN;
            SHADER_TYPE StaticResStages = SHADER_TYPE_UNKNOWN;

            for (Uint32 i = 0; i < Desc.NumResources; ++i)
            {
                const auto& Res = Desc.Resources[i];
                Stages |= Res.ShaderStages;

                if (Res.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
                {
                    StaticResStages |= Res.ShaderStages;
                    StaticVarCount += Res.ArraySize;
                }
            }

            m_ShaderStages = Stages;
            m_NumShaders   = static_cast<Uint8>(PlatformMisc::CountOneBits(static_cast<Uint32>(Stages)));

            if (Stages == SHADER_TYPE_COMPUTE)
            {
                m_PipelineType = PIPELINE_TYPE_COMPUTE;
            }
            else if (Stages & (SHADER_TYPE_AMPLIFICATION | SHADER_TYPE_MESH))
            {
                m_PipelineType = PIPELINE_TYPE_MESH;
            }
            else if (Stages < SHADER_TYPE_COMPUTE)
            {
                m_PipelineType = PIPELINE_TYPE_GRAPHICS;
            }
            else if (Stages >= SHADER_TYPE_RAY_GEN)
            {
                m_PipelineType = PIPELINE_TYPE_RAY_TRACING;
            }
            else
            {
                LOG_PRS_ERROR_AND_THROW("can not deduce pipeline type - used incompatible shader stages");
            }

            m_StaticVarIndex.fill(-1);

            for (; StaticResStages != SHADER_TYPE_UNKNOWN; ++StaticVarStageCount)
            {
                auto StageBit   = static_cast<SHADER_TYPE>(1 << PlatformMisc::GetLSB(Uint32{StaticResStages}));
                StaticResStages = StaticResStages & ~StageBit;

                const auto ShaderTypeInd        = GetShaderTypePipelineIndex(StageBit, m_PipelineType);
                m_StaticVarIndex[ShaderTypeInd] = StaticVarStageCount;
            }

            if (StaticVarStageCount > 0)
            {
                MemPool.AddSpace<ShaderResourceCacheVk>(1);
                MemPool.AddSpace<ShaderVariableManagerVk>(StaticVarStageCount);
            }
        }


        MemPool.Reserve();


        m_pBindingIndices = MemPool.Allocate<PackedBindingIndex>(m_Desc.NumResources);

        // The memory is now owned by PipelineResourceSignatureVkImpl and will be freed by Destruct().
        auto* Ptr = MemPool.ReleaseOwnership();
        VERIFY_EXPR(Ptr == m_pBindingIndices);
        (void)Ptr;

        CopyDescription(MemPool, Desc);

        if (StaticVarStageCount > 0)
        {
            m_pResourceCache = MemPool.Construct<ShaderResourceCacheVk>(ShaderResourceCacheVk::StaticShaderResources);
            m_StaticVarsMgrs = MemPool.Allocate<ShaderVariableManagerVk>(StaticVarStageCount);

            m_pResourceCache->InitializeSets(GetRawAllocator(), 1, &StaticVarCount);

            const SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_STATIC};

            for (Int8 i = 0; i < StaticVarStageCount; ++i)
            {
                new (m_StaticVarsMgrs + i) ShaderVariableManagerVk{*this, *m_pResourceCache};
                m_StaticVarsMgrs[i].Initialize(*this, GetRawAllocator(), AllowedVarTypes, _countof(AllowedVarTypes));
            }
        }

        std::vector<VkDescriptorSetLayoutBinding> LayoutBindings[2];

        for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
        {
            const auto& Res     = m_Desc.Resources[i];
            auto&       Binding = m_pBindingIndices[i];

            Binding.DescSet    = (Res.VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC ? 1 : 0);
            Binding.Binding    = static_cast<Uint16>(LayoutBindings[Binding.DescSet].size());
            Binding.SamplerInd = 0; // AZ TODO

            LayoutBindings[Binding.DescSet].emplace_back();
            auto& LayoutBinding = LayoutBindings[Binding.DescSet].back();

            LayoutBinding.binding            = Binding.Binding;
            LayoutBinding.descriptorCount    = Res.ArraySize;
            LayoutBinding.stageFlags         = ShaderTypesToVkShaderStageFlags(Res.ShaderStages);
            LayoutBinding.pImmutableSamplers = nullptr;
            LayoutBinding.descriptorType     = GetVkDescriptorType(Res);

            if (!(Res.Flags & PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_OFFSETS))
                m_DynamicBufferCount += static_cast<Uint16>(Res.ArraySize);

            if (m_pResourceCache && Res.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
                m_pResourceCache->InitializeResources(0, Binding.Binding, Res.ArraySize, LayoutBinding.descriptorType);
        }

        if (m_Desc.SRBAllocationGranularity > 1)
        {
            const SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC};

            Uint32       UnusedNumVars          = 0;
            const size_t ShaderVariableDataSize = ShaderVariableManagerVk::GetRequiredMemorySize(*this, AllowedVarTypes, _countof(AllowedVarTypes), UnusedNumVars);

            const Uint32 NumSets              = !LayoutBindings[0].empty() + !LayoutBindings[1].empty();
            const Uint32 DescriptorSetSizes[] = {static_cast<Uint32>(LayoutBindings[0].size()), static_cast<Uint32>(LayoutBindings[1].size())};
            const size_t CacheMemorySize      = ShaderResourceCacheVk::GetRequiredMemorySize(NumSets, DescriptorSetSizes);

            m_SRBMemAllocator.Initialize(m_Desc.SRBAllocationGranularity, 1, &ShaderVariableDataSize, 1, &CacheMemorySize);
        }

        VkDescriptorSetLayoutCreateInfo SetLayoutCI = {};

        SetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        SetLayoutCI.pNext = nullptr;
        SetLayoutCI.flags = 0;

        const auto& LogicalDevice = pDevice->GetLogicalDevice();

        for (Uint32 i = 0; i < _countof(LayoutBindings); ++i)
        {
            auto& LayoutBinding = LayoutBindings[i];
            if (LayoutBinding.empty())
                continue;

            SetLayoutCI.bindingCount = static_cast<Uint32>(LayoutBinding.size());
            SetLayoutCI.pBindings    = LayoutBinding.data();
            m_VkDescSetLayouts[i]    = LogicalDevice.CreateDescriptorSetLayout(SetLayoutCI);
        }
    }
    catch (...)
    {
        Destruct();
        throw;
    }
#undef LOG_PRS_ERROR_AND_THROW
}

PipelineResourceSignatureVkImpl::~PipelineResourceSignatureVkImpl()
{
    Destruct();
}

void PipelineResourceSignatureVkImpl::Destruct()
{
    TPipelineResourceSignatureBase::Destruct();

    for (auto& Layout : m_VkDescSetLayouts)
    {
        if (Layout)
            m_pDevice->SafeReleaseDeviceObject(std::move(Layout), ~0ull);
    }

    if (m_pBindingIndices == nullptr)
        return; // memory is not allocated

    auto& RawAllocator = GetRawAllocator();

    for (size_t i = 0; m_StaticVarsMgrs && i < m_StaticVarIndex.size(); ++i)
    {
        Int8 Idx = m_StaticVarIndex[i];
        if (Idx >= 0)
        {
            m_StaticVarsMgrs[Idx].DestroyVariables(RawAllocator);
            m_StaticVarsMgrs[Idx].~ShaderVariableManagerVk();
        }
    }
    m_StaticVarsMgrs = nullptr;

    if (m_pResourceCache != nullptr)
    {
        m_pResourceCache->~ShaderResourceCacheVk();
        m_pResourceCache = nullptr;
    }

    if (void* pRawMem = m_pBindingIndices)
    {
        RawAllocator.Free(pRawMem);
        m_pBindingIndices = nullptr;
    }
}

void PipelineResourceSignatureVkImpl::CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                  bool                     InitStaticResources)
{
    auto& SRBAllocator  = m_pDevice->GetSRBAllocator();
    auto  pResBindingVk = NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingVkImpl instance", ShaderResourceBindingVkImpl)(this, false);
    if (InitStaticResources)
        pResBindingVk->InitializeStaticResources(nullptr);
    pResBindingVk->QueryInterface(IID_ShaderResourceBinding, reinterpret_cast<IObject**>(ppShaderResourceBinding));
}

void PipelineResourceSignatureVkImpl::PrepareDescriptorSets(DeviceContextVkImpl*         pCtxVkImpl,
                                                            VkPipelineBindPoint          BindPoint,
                                                            const ShaderResourceCacheVk& ResourceCache,
                                                            DescriptorSetBindInfo&       BindInfo,
                                                            VkDescriptorSet              VkDynamicDescrSet) const
{
    /*
#ifdef DILIGENT_DEBUG
    BindInfo.vkSets.clear();
#endif

    // Do not use vector::resize for BindInfo.vkSets and BindInfo.DynamicOffsets as this
    // causes unnecessary work to zero-initialize new elements

    VERIFY(m_LayoutMgr.GetDescriptorSet(SHADER_RESOURCE_VARIABLE_TYPE_STATIC).SetIndex == m_LayoutMgr.GetDescriptorSet(SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE).SetIndex,
           "Static and mutable variables are expected to share the same descriptor set");
    Uint32 TotalDynamicDescriptors = 0;

    BindInfo.SetCout = 0;
    for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE; VarType <= SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC; VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
    {
        const auto& Set = m_LayoutMgr.GetDescriptorSet(VarType);
        if (Set.SetIndex >= 0)
        {
            BindInfo.SetCout = std::max(BindInfo.SetCout, static_cast<Uint32>(Set.SetIndex + 1));
            if (BindInfo.SetCout > BindInfo.vkSets.size())
                BindInfo.vkSets.resize(BindInfo.SetCout);
            VERIFY_EXPR(BindInfo.vkSets[Set.SetIndex] == VK_NULL_HANDLE);
            if (VarType == SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
                BindInfo.vkSets[Set.SetIndex] = ResourceCache.GetDescriptorSet(Set.SetIndex).GetVkDescriptorSet();
            else
            {
                VERIFY_EXPR(ResourceCache.GetDescriptorSet(Set.SetIndex).GetVkDescriptorSet() == VK_NULL_HANDLE);
                BindInfo.vkSets[Set.SetIndex] = VkDynamicDescrSet;
            }
            VERIFY(BindInfo.vkSets[Set.SetIndex] != VK_NULL_HANDLE, "Descriptor set must not be null");
        }
        TotalDynamicDescriptors += Set.NumDynamicDescriptors;
    }

#ifdef DILIGENT_DEBUG
    for (const auto& set : BindInfo.vkSets)
        VERIFY(set != VK_NULL_HANDLE, "Descriptor set must not be null");
#endif

    BindInfo.DynamicOffsetCount = TotalDynamicDescriptors;
    if (TotalDynamicDescriptors > BindInfo.DynamicOffsets.size())
        BindInfo.DynamicOffsets.resize(TotalDynamicDescriptors);
    BindInfo.BindPoint      = BindPoint;
    BindInfo.pResourceCache = &ResourceCache;
#ifdef DILIGENT_DEBUG
    BindInfo.pDbgPipelineLayout = this;
#endif
    BindInfo.DynamicBuffersPresent = ResourceCache.GetNumDynamicBuffers() > 0;

    if (TotalDynamicDescriptors == 0)
    {
        // There are no dynamic descriptors, so we can bind descriptor sets right now
        auto& CmdBuffer = pCtxVkImpl->GetCommandBuffer();
        CmdBuffer.BindDescriptorSets(BindInfo.BindPoint,
                                     m_LayoutMgr.GetVkPipelineLayout(),
                                     0, // First set
                                     BindInfo.SetCout,
                                     BindInfo.vkSets.data(), // BindInfo.vkSets is never empty
                                     0,
                                     nullptr);
    }

    BindInfo.DynamicDescriptorsBound = false;
    */
}

Uint32 PipelineResourceSignatureVkImpl::GetStaticVariableCount(SHADER_TYPE ShaderType) const
{
    const auto VarMngrInd = GetStaticVariableCountHelper(ShaderType, m_StaticVarIndex);
    if (VarMngrInd < 0)
        return 0;

    auto& StaticVarMgr = m_StaticVarsMgrs[VarMngrInd];
    return StaticVarMgr.GetVariableCount();
}

IShaderResourceVariable* PipelineResourceSignatureVkImpl::GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name)
{
    const auto VarMngrInd = GetStaticVariableByNameHelper(ShaderType, Name, m_StaticVarIndex);
    if (VarMngrInd < 0)
        return nullptr;

    auto& StaticVarMgr = m_StaticVarsMgrs[VarMngrInd];
    return StaticVarMgr.GetVariable(Name);
}

IShaderResourceVariable* PipelineResourceSignatureVkImpl::GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    const auto VarMngrInd = GetStaticVariableByIndexHelper(ShaderType, Index, m_StaticVarIndex);
    if (VarMngrInd < 0)
        return nullptr;

    const auto& StaticVarMgr = m_StaticVarsMgrs[VarMngrInd];
    return StaticVarMgr.GetVariable(Index);
}

void PipelineResourceSignatureVkImpl::InitResourceCache(ShaderResourceCacheVk& ResourceCache,
                                                        IMemoryAllocator&      CacheMemAllocator,
                                                        const char*            DbgPipelineName) const
{
    std::array<Uint32, 2> VarCount = {};
    for (Uint32 r = 0; r < m_Desc.NumResources; ++r)
    {
        const auto& Res  = GetResource(r);
        const auto& Bind = GetBinding(r);
        VarCount[Bind.DescSet] += Res.ArraySize;
    }

    // This call only initializes descriptor sets (ShaderResourceCacheVk::DescriptorSet) in the resource cache
    // Resources are initialized by source layout when shader resource binding objects are created
    ResourceCache.InitializeSets(CacheMemAllocator, static_cast<Uint32>(VarCount.size()), VarCount.data());

    if (auto VkLayout = GetStaticVkDescriptorSetLayout())
    {
        const char* DescrSetName = "Static/Mutable Descriptor Set";
#ifdef DILIGENT_DEVELOPMENT
        std::string _DescrSetName(DbgPipelineName);
        _DescrSetName.append(" - static/mutable set");
        DescrSetName = _DescrSetName.c_str();
#endif
        DescriptorSetAllocation SetAllocation = GetDevice()->AllocateDescriptorSet(~Uint64{0}, VkLayout, DescrSetName);
        ResourceCache.GetDescriptorSet(0).AssignDescriptorSetAllocation(std::move(SetAllocation));
    }
}

SHADER_TYPE PipelineResourceSignatureVkImpl::GetShaderStageType(Uint32 StageIndex) const
{
    SHADER_TYPE Stages = m_ShaderStages;
    for (Uint32 Index = 0; Stages != SHADER_TYPE_UNKNOWN; ++Index)
    {
        auto StageBit = static_cast<SHADER_TYPE>(1 << PlatformMisc::GetLSB(Uint32{Stages}));
        Stages        = Stages & ~StageBit;

        if (Index == StageIndex)
            return StageBit;
    }

    UNEXPECTED("index is out of range");
    return SHADER_TYPE_UNKNOWN;
}

void PipelineResourceSignatureVkImpl::InitializeResourceMemoryInCache(ShaderResourceCacheVk& ResourceCache) const
{
    auto TotalResources = GetTotalResourceCount();
    for (Uint32 r = 0; r < TotalResources; ++r)
    {
        const auto& Res  = GetResource(r);
        const auto& Bind = GetBinding(r);
        ResourceCache.InitializeResources(Bind.DescSet, Bind.Binding, Res.ArraySize, GetVkDescriptorType(Res));
    }
}

void PipelineResourceSignatureVkImpl::InitializeStaticSRBResources(ShaderResourceCacheVk& DstResourceCache) const
{
    const auto& SrcResourceCache = *m_pResourceCache;
    (void)(SrcResourceCache);

    for (Uint32 s = 0; s < GetNumShaderStages(); ++s)
    {
        // AZ TODO
    }
#ifdef DILIGENT_DEBUG
    DstResourceCache.DbgVerifyDynamicBuffersCounter();
#endif
}

} // namespace Diligent
