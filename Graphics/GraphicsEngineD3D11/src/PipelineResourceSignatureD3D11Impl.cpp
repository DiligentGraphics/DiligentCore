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
#include "PipelineResourceSignatureD3D11Impl.hpp"
#include "RenderDeviceD3D11Impl.hpp"

namespace Diligent
{

DESCRIPTOR_RANGE ShaderResourceToDescriptorRange(SHADER_RESOURCE_TYPE Type)
{
    static_assert(SHADER_RESOURCE_TYPE_LAST == 8, "Please update the switch below to handle the new shader resource type");
    switch (Type)
    {
        // clang-format off
        case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER:  return DESCRIPTOR_RANGE_CBV;
        case SHADER_RESOURCE_TYPE_TEXTURE_SRV:      return DESCRIPTOR_RANGE_SRV;
        case SHADER_RESOURCE_TYPE_BUFFER_SRV:       return DESCRIPTOR_RANGE_SRV;
        case SHADER_RESOURCE_TYPE_TEXTURE_UAV:      return DESCRIPTOR_RANGE_UAV;
        case SHADER_RESOURCE_TYPE_BUFFER_UAV:       return DESCRIPTOR_RANGE_UAV;
        case SHADER_RESOURCE_TYPE_SAMPLER:          return DESCRIPTOR_RANGE_SAMPLER;
        case SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT: return DESCRIPTOR_RANGE_SRV;
            // clang-format on
        default:
            UNEXPECTED("Unsupported resource type");
            return DESCRIPTOR_RANGE_UNKNOWN;
    }
}


PipelineResourceSignatureD3D11Impl::PipelineResourceSignatureD3D11Impl(IReferenceCounters*                  pRefCounters,
                                                                       RenderDeviceD3D11Impl*               pDeviceD3D11,
                                                                       const PipelineResourceSignatureDesc& Desc,
                                                                       bool                                 bIsDeviceInternal) :
    TPipelineResourceSignatureBase{pRefCounters, pDeviceD3D11, Desc, bIsDeviceInternal}
{
    try
    {
        auto& RawAllocator{GetRawAllocator()};
        auto  MemPool = AllocateInternalObjects(RawAllocator, Desc,
                                               [&Desc](FixedLinearAllocator& MemPool) //
                                               {
                                                   MemPool.AddSpace<ResourceAttribs>(Desc.NumResources);
                                                   MemPool.AddSpace<ImmutableSamplerAttribs>(Desc.NumImmutableSamplers);
                                               });

        static_assert(std::is_trivially_destructible<ResourceAttribs>::value,
                      "ResourceAttribs objects must be constructed to be properly destructed in case an excpetion is thrown");
        m_pResourceAttribs  = MemPool.Allocate<ResourceAttribs>(m_Desc.NumResources);
        m_ImmutableSamplers = MemPool.ConstructArray<ImmutableSamplerAttribs>(m_Desc.NumImmutableSamplers);

        CreateLayout();

        const auto NumStaticResStages = GetNumStaticResStages();
        if (NumStaticResStages > 0)
        {
            constexpr SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_STATIC};
            for (Uint32 i = 0; i < m_StaticResStageIndex.size(); ++i)
            {
                Int8 Idx = m_StaticResStageIndex[i];
                if (Idx >= 0)
                {
                    VERIFY_EXPR(static_cast<Uint32>(Idx) < NumStaticResStages);
                    const auto ShaderType = GetShaderTypeFromPipelineIndex(i, GetPipelineType());
                    m_StaticVarsMgrs[Idx].Initialize(*this, GetRawAllocator(), AllowedVarTypes, _countof(AllowedVarTypes), ShaderType);
                }
            }
        }

        if (m_Desc.SRBAllocationGranularity > 1)
        {
            std::array<size_t, MAX_SHADERS_IN_PIPELINE> ShaderVariableDataSizes = {};
            for (Uint32 s = 0; s < GetNumActiveShaderStages(); ++s)
            {
                constexpr SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC};

                ShaderVariableDataSizes[s] = ShaderVariableManagerD3D11::GetRequiredMemorySize(*this, AllowedVarTypes, _countof(AllowedVarTypes), GetActiveShaderStageType(s));
            }

            const size_t CacheMemorySize = ShaderResourceCacheD3D11::GetRequriedMemorySize(m_ResourceCount);
            m_SRBMemAllocator.Initialize(m_Desc.SRBAllocationGranularity, GetNumActiveShaderStages(), ShaderVariableDataSizes.data(), 1, &CacheMemorySize);
        }

        CalculateHash();
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

void PipelineResourceSignatureD3D11Impl::CreateLayout()
{
    using TBindings32         = std::array<Uint32, DESCRIPTOR_RANGE_COUNT>;
    using TBindingsPerStage32 = std::array<TBindings32, NumShaderTypes>;

    if (m_pStaticResCache)
    {
        TBindings32 StaticCounter32 = {};
        const auto  ResIdxRange     = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
        for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
        {
            const auto& ResDesc = m_Desc.Resources[r];
            const auto  Range   = ShaderResourceToDescriptorRange(ResDesc.ResourceType);
            StaticCounter32[Range] += ResDesc.ArraySize;
        }

        TBindings StaticCounter8 = {};
        for (Uint32 i = 0; i < StaticCounter8.size(); ++i)
        {
            VERIFY_EXPR(StaticCounter8[i] < std::numeric_limits<Uint8>::max());
            StaticCounter8[i] = static_cast<Uint8>(StaticCounter32[i]);
        }

        m_pStaticResCache->Initialize(StaticCounter8, GetRawAllocator());
    }

    // Index of the assigned sampler, for every texture SRV in m_Desc.Resources, or InvalidSamplerInd.
    std::vector<Uint32> TextureSrvToAssignedSamplerInd(m_Desc.NumResources, ResourceAttribs::InvalidSamplerInd);
    // Index of the immutable sampler for every sampler or texture in m_Desc.Resources, or InvalidImmutableSamplerIndex.
    std::vector<Uint32> ResourceToImmutableSamplerInd(m_Desc.NumResources, InvalidImmutableSamplerIndex);
    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& ResDesc = m_Desc.Resources[i];

        if (ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER ||
            ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV)
        {
            const char* SamplerSuffix          = ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER ? GetCombinedSamplerSuffix() : nullptr;
            const auto  SrcImmutableSamplerInd = Diligent::FindImmutableSampler(m_Desc.ImmutableSamplers, m_Desc.NumImmutableSamplers, ResDesc.ShaderStages, ResDesc.Name, SamplerSuffix);
            if (SrcImmutableSamplerInd != InvalidImmutableSamplerIndex)
            {
                ResourceToImmutableSamplerInd[i] = SrcImmutableSamplerInd;
                // Set the immutable sampler array size to match the resource array size
                auto& DstImtblSampAttribs = m_ImmutableSamplers[SrcImmutableSamplerInd];
                // One immutable sampler may be used by different arrays in different shader stages - use the maximum array size
                const auto ImtblSampArraySize = std::max(DstImtblSampAttribs.ArraySize, ResDesc.ArraySize);
                DstImtblSampAttribs.ArraySize = ImtblSampArraySize;
                VERIFY(DstImtblSampAttribs.ArraySize == ImtblSampArraySize, "Immutable sampler array size exceeds maximum representable value");
            }
        }

        if (ResourceToImmutableSamplerInd[i] == InvalidImmutableSamplerIndex &&
            ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV)
        {
            TextureSrvToAssignedSamplerInd[i] = FindAssignedSampler(ResDesc, ResourceAttribs::InvalidSamplerInd);
        }
    }

    TBindings32         ResourceCount   = {};
    TBindingsPerStage32 BindingPerStage = {};
    const auto          AllocBindPoints = [&BindingPerStage](TBindPoints& BindPoints, SHADER_TYPE ShaderStages, Uint32 ArraySize, DESCRIPTOR_RANGE Range) //
    {
        while (ShaderStages != 0)
        {
            auto   Stage     = ExtractLSB(ShaderStages);
            Uint32 ShaderInd = GetShaderTypeIndex(Stage);

            BindPoints[ShaderInd] = static_cast<Uint8>(BindingPerStage[ShaderInd][Range]);
            VERIFY_EXPR(BindPoints[ShaderInd] == BindingPerStage[ShaderInd][Range]);
            BindingPerStage[ShaderInd][Range] += ArraySize;
        }
    };

    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& ResDesc = m_Desc.Resources[i];
        const auto  Range   = ShaderResourceToDescriptorRange(ResDesc.ResourceType);
        VERIFY(i == 0 || ResDesc.VarType >= m_Desc.Resources[i - 1].VarType, "Resources must be sorted by variable type");

        auto AssignedSamplerInd     = TextureSrvToAssignedSamplerInd[i];
        auto SrcImmutableSamplerInd = ResourceToImmutableSamplerInd[i];
        if (AssignedSamplerInd != ResourceAttribs::InvalidSamplerInd)
        {
            VERIFY_EXPR(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV);
            SrcImmutableSamplerInd = ResourceToImmutableSamplerInd[AssignedSamplerInd];
        }

        constexpr SHADER_TYPE UAVStages = SHADER_TYPE_PIXEL | SHADER_TYPE_COMPUTE;
        if (Range == DESCRIPTOR_RANGE_UAV && (ResDesc.ShaderStages & ~UAVStages) != 0)
        {
            LOG_ERROR_AND_THROW("Description of a pipeline resource signature '", m_Desc.Name, "' is invalid: ",
                                "Desc.Resources[", i, "].ResourceType (", GetShaderResourceTypeLiteralName(ResDesc.ResourceType),
                                ") - UAV resource is not supported in shader stages ", GetShaderStagesString(ResDesc.ShaderStages & ~UAVStages));
        }

        // Allocate space for immutable sampler which is assigned to sampler or texture resource.
        if (SrcImmutableSamplerInd != InvalidImmutableSamplerIndex)
        {
            const auto& ImtblSamp        = GetImmutableSamplerDesc(SrcImmutableSamplerInd);
            auto&       ImtblSampAttribs = m_ImmutableSamplers[SrcImmutableSamplerInd];
            VERIFY_EXPR(ResDesc.ArraySize <= ImtblSampAttribs.ArraySize);

            if (!ImtblSampAttribs.IsAllocated())
            {
                ImtblSampAttribs.CacheOffset = ResourceCount[DESCRIPTOR_RANGE_SAMPLER];
                AllocBindPoints(ImtblSampAttribs.BindPoints, ImtblSamp.ShaderStages, ImtblSampAttribs.ArraySize, DESCRIPTOR_RANGE_SAMPLER);
                ResourceCount[DESCRIPTOR_RANGE_SAMPLER] += ImtblSampAttribs.ArraySize;
            }
        }

        if (!(ResDesc.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER && SrcImmutableSamplerInd != InvalidImmutableSamplerIndex))
        {
            auto* pAttrib = new (m_pResourceAttribs + i) ResourceAttribs //
                {
                    ResourceCount[Range],
                    AssignedSamplerInd,
                    SrcImmutableSamplerInd != InvalidImmutableSamplerIndex //
                };
            AllocBindPoints(pAttrib->BindPoints, ResDesc.ShaderStages, ResDesc.ArraySize, Range);
            ResourceCount[Range] += ResDesc.ArraySize;
        }
        else
        {
            VERIFY_EXPR(AssignedSamplerInd == ResourceAttribs::InvalidSamplerInd);

            auto& ImtblSampAttribs = m_ImmutableSamplers[SrcImmutableSamplerInd];
            auto* pAttrib          = new (m_pResourceAttribs + i) ResourceAttribs //
                {
                    ImtblSampAttribs.CacheOffset,
                    ResourceAttribs::InvalidSamplerInd,
                    SrcImmutableSamplerInd != InvalidImmutableSamplerIndex //
                };
            pAttrib->BindPoints = ImtblSampAttribs.BindPoints;
        }
    }

    // Add bindings for immutable samplers that is not assigned to texture or separate sampler.
    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        const auto  Range            = DESCRIPTOR_RANGE_SAMPLER;
        const auto& ImtblSamp        = GetImmutableSamplerDesc(i);
        auto&       ImtblSampAttribs = m_ImmutableSamplers[i];

        if (IsUsingCombinedSamplers() && !ImtblSampAttribs.IsAllocated())
            continue;

        GetDevice()->CreateSampler(ImtblSamp.Desc, &ImtblSampAttribs.pSampler);

        // Add as separate sampler.
        if (!ImtblSampAttribs.IsAllocated())
        {
            ImtblSampAttribs.ArraySize   = 1;
            ImtblSampAttribs.CacheOffset = ResourceCount[Range];
            ResourceCount[Range] += ImtblSampAttribs.ArraySize;
            AllocBindPoints(ImtblSampAttribs.BindPoints, ImtblSamp.ShaderStages, ImtblSampAttribs.ArraySize, Range);
        }
    }

    for (Uint32 i = 0; i < ResourceCount.size(); ++i)
    {
        using T = std::remove_reference<decltype(m_ResourceCount[i])>::type;
        VERIFY_EXPR(ResourceCount[i] < std::numeric_limits<T>::max());
        m_ResourceCount[i] = static_cast<T>(ResourceCount[i]);
    }

    for (Uint32 s = 0; s < BindingPerStage.size(); ++s)
    {
        for (Uint32 i = 0; i < BindingPerStage[s].size(); ++i)
        {
            using T = std::remove_reference<decltype(m_BindingCountPerStage[s][i])>::type;
            VERIFY_EXPR(BindingPerStage[s][i] < std::numeric_limits<T>::max());
            m_BindingCountPerStage[s][i] = static_cast<T>(BindingPerStage[s][i]);
        }
    }
}

PipelineResourceSignatureD3D11Impl::~PipelineResourceSignatureD3D11Impl()
{
    Destruct();
}

void PipelineResourceSignatureD3D11Impl::Destruct()
{
    if (m_ImmutableSamplers != nullptr)
    {
        for (Uint32 s = 0; s < m_Desc.NumImmutableSamplers; ++s)
            m_ImmutableSamplers[s].~ImmutableSamplerAttribs();

        m_ImmutableSamplers = nullptr;
    }

    m_pResourceAttribs = nullptr;

    TPipelineResourceSignatureBase::Destruct();
}

void PipelineResourceSignatureD3D11Impl::CopyStaticResources(ShaderResourceCacheD3D11& DstResourceCache) const
{
    if (m_pStaticResCache == nullptr)
        return;

    // SrcResourceCache contains only static resources.
    // DstResourceCache contains static, mutable and dynamic resources.
    const auto& SrcResourceCache = *m_pStaticResCache;
    const auto  ResIdxRange      = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

    VERIFY_EXPR(SrcResourceCache.GetContentType() == ResourceCacheContentType::Signature);
    VERIFY_EXPR(DstResourceCache.GetContentType() == ResourceCacheContentType::SRB);

    for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
    {
        const auto& ResDesc = GetResourceDesc(r);
        const auto& ResAttr = GetResourceAttribs(r);
        VERIFY_EXPR(ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

        static_assert(DESCRIPTOR_RANGE_COUNT == 4, "Please update the switch below to handle the new descriptor range");
        switch (ShaderResourceToDescriptorRange(ResDesc.ResourceType))
        {
            case DESCRIPTOR_RANGE_CBV:
                for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                {
                    const auto& SrcCachedRes = SrcResourceCache.GetCB(ResAttr.CacheOffset + ArrInd);
                    if (!SrcCachedRes.pBuff)
                        LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");

                    DstResourceCache.SetCB(ResAttr.CacheOffset, ArrInd, ResAttr.BindPoints, RefCntAutoPtr<BufferD3D11Impl>{SrcCachedRes.pBuff});
                }
                break;
            case DESCRIPTOR_RANGE_SRV:
                for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                {
                    const auto& SrcCachedRes = SrcResourceCache.GetSRV(ResAttr.CacheOffset + ArrInd);
                    if (!SrcCachedRes.pBuffer && !SrcCachedRes.pTexture)
                        LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");

                    if (SrcCachedRes.pTexture)
                        DstResourceCache.SetTexSRV(ResAttr.CacheOffset, ArrInd, ResAttr.BindPoints, RefCntAutoPtr<TextureViewD3D11Impl>{SrcCachedRes.pView.RawPtr<TextureViewD3D11Impl>()});
                    else
                        DstResourceCache.SetBufSRV(ResAttr.CacheOffset, ArrInd, ResAttr.BindPoints, RefCntAutoPtr<BufferViewD3D11Impl>{SrcCachedRes.pView.RawPtr<BufferViewD3D11Impl>()});
                }
                break;
            case DESCRIPTOR_RANGE_SAMPLER:
                // Immutable samplers will be copied in InitSRBResourceCache().
                if (!ResAttr.IsImmutableSamplerAssigned())
                {
                    for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                    {
                        const auto& SrcCachedRes = SrcResourceCache.GetSampler(ResAttr.CacheOffset + ArrInd);
                        if (!SrcCachedRes.pSampler)
                            LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");

                        DstResourceCache.SetSampler(ResAttr.CacheOffset, ArrInd, ResAttr.BindPoints, RefCntAutoPtr<SamplerD3D11Impl>{SrcCachedRes.pSampler});
                    }
                }
                break;
            case DESCRIPTOR_RANGE_UAV:
                for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
                {
                    const auto& SrcCachedRes = SrcResourceCache.GetUAV(ResAttr.CacheOffset + ArrInd);
                    if (!SrcCachedRes.pBuffer && !SrcCachedRes.pTexture)
                        LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", GetShaderResourcePrintName(ResDesc, ArrInd), "' in pipeline resource signature '", m_Desc.Name, "'.");

                    if (SrcCachedRes.pTexture)
                        DstResourceCache.SetTexUAV(ResAttr.CacheOffset, ArrInd, ResAttr.BindPoints, RefCntAutoPtr<TextureViewD3D11Impl>{SrcCachedRes.pView.RawPtr<TextureViewD3D11Impl>()});
                    else
                        DstResourceCache.SetBufUAV(ResAttr.CacheOffset, ArrInd, ResAttr.BindPoints, RefCntAutoPtr<BufferViewD3D11Impl>{SrcCachedRes.pView.RawPtr<BufferViewD3D11Impl>()});
                }
                break;
            default:
                UNEXPECTED("Unsupported descriptor range type.");
        }
    }
}

void PipelineResourceSignatureD3D11Impl::InitSRBResourceCache(ShaderResourceCacheD3D11& ResourceCache)
{
    ResourceCache.Initialize(m_ResourceCount, m_SRBMemAllocator.GetResourceCacheDataAllocator(0));

    // Copy immutable samplers.
    for (Uint32 i = 0; i < m_Desc.NumImmutableSamplers; ++i)
    {
        const auto& ImtblSampAttr = GetImmutableSamplerAttribs(i);

        if (ImtblSampAttr.IsAllocated())
        {
            SamplerD3D11Impl* pSampler = ImtblSampAttr.GetSamplerD3D11();
            VERIFY_EXPR(pSampler != nullptr);
            VERIFY_EXPR(ImtblSampAttr.ArraySize > 0);

            for (Uint32 ArrInd = 0; ArrInd < ImtblSampAttr.ArraySize; ++ArrInd)
                ResourceCache.SetSampler(ImtblSampAttr.CacheOffset, ArrInd, ImtblSampAttr.BindPoints, pSampler);
        }
    }
}

void PipelineResourceSignatureD3D11Impl::UpdateShaderResourceBindingMap(ResourceBinding::TMap& ResourceMap, SHADER_TYPE ShaderStage, const TBindingsPerStage& BaseBindings) const
{
    VERIFY(ShaderStage != SHADER_TYPE_UNKNOWN && IsPowerOfTwo(ShaderStage), "Only single shader stage must be provided.");
    const auto ShaderInd = GetShaderTypeIndex(ShaderStage);

    for (Uint32 r = 0, ResCount = GetTotalResourceCount(); r < ResCount; ++r)
    {
        const auto& ResDesc = GetResourceDesc(r);
        const auto& ResAttr = GetResourceAttribs(r);
        const auto  Range   = ShaderResourceToDescriptorRange(ResDesc.ResourceType);

        if ((ResDesc.ShaderStages & ShaderStage) != 0)
        {
            VERIFY_EXPR(ResAttr.BindPoints[ShaderInd] != InvalidBindPoint);
            ResourceBinding::BindInfo BindInfo //
                {
                    Uint32{BaseBindings[ShaderInd][Range]} + ResAttr.BindPoints[ShaderInd],
                    0u, // register space is not supported
                    ResDesc.ArraySize,
                    ResDesc.ResourceType //
                };
            auto IsUnique = ResourceMap.emplace(HashMapStringKey{ResDesc.Name}, BindInfo).second;
            VERIFY(IsUnique, "Shader resource '", ResDesc.Name,
                   "' already present in the binding map. Every shader resource in PSO must be unambiguously defined by "
                   "only one resource signature. This error should've been caught by ValidatePipelineResourceSignatures().");
        }
    }

    for (Uint32 samp = 0, SampCount = GetImmutableSamplerCount(); samp < SampCount; ++samp)
    {
        const auto& ImtblSam = GetImmutableSamplerDesc(samp);
        const auto& SampAttr = GetImmutableSamplerAttribs(samp);
        const auto  Range    = DESCRIPTOR_RANGE_SAMPLER;

        if ((ImtblSam.ShaderStages & ShaderStage) != 0 && SampAttr.IsAllocated())
        {
            VERIFY_EXPR(SampAttr.BindPoints[ShaderInd] != InvalidBindPoint);

            String SampName{ImtblSam.SamplerOrTextureName};
            if (IsUsingCombinedSamplers())
                SampName += GetCombinedSamplerSuffix();

            ResourceBinding::BindInfo BindInfo //
                {
                    Uint32{BaseBindings[ShaderInd][Range]} + SampAttr.BindPoints[ShaderInd],
                    0u, // register space is not supported
                    SampAttr.ArraySize,
                    SHADER_RESOURCE_TYPE_SAMPLER //
                };

            auto it_inserted = ResourceMap.emplace(HashMapStringKey{SampName}, BindInfo);
#ifdef DILIGENT_DEBUG
            if (!it_inserted.second)
            {
                const auto& ExistingBindInfo = it_inserted.first->second;
                VERIFY(ExistingBindInfo.BindPoint == BindInfo.BindPoint,
                       "Bind point defined by the immutable sampler attribs is inconsistent with the bind point defined by the sampler resource.");
            }
#endif
        }
    }
}

// AZ TODO
#if 0  //def DILIGENT_DEVELOPMENT
bool PipelineResourceSignatureGLImpl::DvpValidateCommittedResource(const D3DShaderResourceAttribs& D3DAttribs,
                                                                   RESOURCE_DIMENSION                          ResourceDim,
                                                                   bool                                        IsMultisample,
                                                                   Uint32                                      ResIndex,
                                                                   const ShaderResourceCacheD3D11&                ResourceCache,
                                                                   const char*                                 ShaderName,
                                                                   const char*                                 PSOName) const
{
}
#endif // DILIGENT_DEVELOPMENT

} // namespace Diligent
