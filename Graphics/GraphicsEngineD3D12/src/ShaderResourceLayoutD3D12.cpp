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

#include "ShaderResourceLayoutD3D12.hpp"
#include "ShaderResourceCacheD3D12.hpp"
#include "BufferD3D12Impl.hpp"
#include "BufferViewD3D12.h"
#include "TextureD3D12Impl.hpp"
#include "TextureViewD3D12Impl.hpp"
#include "SamplerD3D12Impl.hpp"
#include "ShaderD3D12Impl.hpp"
#include "RootSignature.hpp"
#include "PipelineStateD3D12Impl.hpp"
#include "ShaderResourceVariableBase.hpp"
#include "ShaderVariableD3DBase.hpp"
#include "FixedLinearAllocator.hpp"
#include "TopLevelASD3D12.h"

namespace Diligent
{

ShaderResourceLayoutD3D12::~ShaderResourceLayoutD3D12()
{
    for (Uint32 r = 0; r < GetTotalResourceCount(); ++r)
        GetResource(r).~D3D12Resource();
}

D3D12_DESCRIPTOR_RANGE_TYPE GetDescriptorRangeType(CachedResourceType ResType)
{
    class ResTypeToD3D12DescrRangeType
    {
    public:
        ResTypeToD3D12DescrRangeType()
        {
            // clang-format off
            m_Map[(size_t)CachedResourceType::CBV]         = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            m_Map[(size_t)CachedResourceType::TexSRV]      = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            m_Map[(size_t)CachedResourceType::BufSRV]      = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            m_Map[(size_t)CachedResourceType::TexUAV]      = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            m_Map[(size_t)CachedResourceType::BufUAV]      = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            m_Map[(size_t)CachedResourceType::Sampler]     = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
            m_Map[(size_t)CachedResourceType::AccelStruct] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            // clang-format on
        }

        D3D12_DESCRIPTOR_RANGE_TYPE operator[](CachedResourceType ResType) const
        {
            auto Ind = static_cast<size_t>(ResType);
            VERIFY(Ind >= 0 && Ind < (size_t)CachedResourceType::NumTypes, "Unexpected resource type");
            return m_Map[Ind];
        }

    private:
        std::array<D3D12_DESCRIPTOR_RANGE_TYPE, static_cast<size_t>(CachedResourceType::NumTypes)> m_Map;
    };

    static const ResTypeToD3D12DescrRangeType ResTypeToDescrRangeTypeMap;
    return ResTypeToDescrRangeTypeMap[ResType];
}


StringPool ShaderResourceLayoutD3D12::AllocateMemory(IMemoryAllocator&                                                  Allocator,
                                                     const std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES>& CbvSrvUavCount,
                                                     const std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES>& SamplerCount,
                                                     size_t                                                             StringPoolSize)
{
    m_CbvSrvUavOffsets[0] = 0;
    for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
         VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES;
         VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
    {
        VERIFY(m_CbvSrvUavOffsets[VarType] + CbvSrvUavCount[VarType] <= std::numeric_limits<Uint16>::max(), "Offset is not representable in 16 bits");
        m_CbvSrvUavOffsets[VarType + 1] = static_cast<Uint16>(m_CbvSrvUavOffsets[VarType] + CbvSrvUavCount[VarType]);
        VERIFY_EXPR(GetCbvSrvUavCount(VarType) == CbvSrvUavCount[VarType]);
    }

    m_SamplersOffsets[0] = m_CbvSrvUavOffsets[SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES];
    for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
         VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES;
         VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
    {
        VERIFY(m_SamplersOffsets[VarType] + SamplerCount[VarType] <= std::numeric_limits<Uint16>::max(), "Offset is not representable in 16 bits");
        m_SamplersOffsets[VarType + 1] = static_cast<Uint16>(m_SamplersOffsets[VarType] + SamplerCount[VarType]);
        VERIFY_EXPR(GetSamplerCount(VarType) == SamplerCount[VarType]);
    }

    FixedLinearAllocator MemPool{Allocator};
    MemPool.AddSpace<D3D12Resource>(GetTotalResourceCount());
    MemPool.AddSpace<char>(StringPoolSize);

    MemPool.Reserve();

    auto* pResources      = MemPool.Allocate<D3D12Resource>(GetTotalResourceCount());
    auto* pStringPoolData = MemPool.ConstructArray<char>(StringPoolSize);

    m_ResourceBuffer = std::unique_ptr<void, STDDeleterRawMem<void>>(MemPool.Release(), Allocator);
    VERIFY_EXPR(pResources == nullptr || m_ResourceBuffer.get() == pResources);
    VERIFY_EXPR(pStringPoolData == GetStringPoolData());

    StringPool stringPool;
    stringPool.AssignMemory(pStringPoolData, StringPoolSize);
    return stringPool;
}


// http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-layout#Initializing-Shader-Resource-Layouts-and-Root-Signature-in-a-Pipeline-State-Object
// http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-cache#Initializing-Shader-Resource-Layouts-in-a-Pipeline-State
void ShaderResourceLayoutD3D12::Initialize(PIPELINE_TYPE                        PipelineType,
                                           const PipelineResourceLayoutDesc&    ResourceLayout,
                                           const std::vector<ShaderD3D12Impl*>& Shaders,
                                           IMemoryAllocator&                    LayoutDataAllocator,
                                           class RootSignatureBuilder&          RootSgnBldr,
                                           LocalRootSignature*                  pLocalRootSig)
{
    VERIFY_EXPR(!Shaders.empty());

    m_IsUsingSeparateSamplers = !Shaders[0]->GetShaderResources()->IsUsingCombinedTextureSamplers();
    m_ShaderType              = Shaders[0]->GetDesc().ShaderType;

    std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES> CbvSrvUavCount = {};
    std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES> SamplerCount   = {};

    // Mapping from the resource name to its index in m_ResourceBuffer that is used
    // to de-duplicate resources.
    std::unordered_map<HashMapStringKey, Uint32, HashMapStringKey::Hasher> ResourceNameToIndex;

    // Construct shader or shader group name
    const auto ShaderName = GetShaderGroupName(Shaders);

    // Start calculating the pool size required to keep all strings in the layout
    size_t StringPoolSize = StringPool::GetRequiredReserveSize(ShaderName);

    static constexpr Uint32 InvalidResourceIndex = ~0u;

    // Count resources to calculate the required memory size.
    for (auto* pShader : Shaders) // Iterate over all shaders in the stage.
    {
        const auto& ShaderRes = *pShader->GetShaderResources();
        VERIFY_EXPR(ShaderRes.GetShaderType() == m_ShaderType);

        auto AddResource = [&](const D3DShaderResourceAttribs& Res, Uint32 /*Index*/ = 0) //
        {
            const auto IsNewResource = ResourceNameToIndex.emplace(HashMapStringKey{Res.Name}, InvalidResourceIndex).second;
            if (IsNewResource)
            {
                const auto VarType = ShaderRes.FindVariableType(Res, ResourceLayout);
                StringPoolSize += StringPool::GetRequiredReserveSize(Res.Name);
                if (Res.GetInputType() == D3D_SIT_SAMPLER)
                    ++SamplerCount[VarType];
                else
                    ++CbvSrvUavCount[VarType];
            }
            return IsNewResource;
        };

        ShaderRes.ProcessResources(
            [&](const auto& CB, Uint32) //
            {
                if (pLocalRootSig != nullptr && pLocalRootSig->SetOrMerge(CB))
                    return;

                AddResource(CB);
            },
            [&](const D3DShaderResourceAttribs& Sam, Uint32) //
            {
                constexpr bool LogImtblSamplerArrayError = true;

                const auto ImtblSamplerInd = ShaderRes.FindImmutableSampler(Sam, ResourceLayout, LogImtblSamplerArrayError);
                // Skip immutable samplers
                if (ImtblSamplerInd >= 0)
                    return;

                AddResource(Sam);
            },
            [&](const D3DShaderResourceAttribs& TexSRV, Uint32) //
            {
                if (AddResource(TexSRV))
                {
#if DILIGENT_DEVELOPMENT
                    if (TexSRV.IsCombinedWithSampler())
                    {
                        const auto& SamplerAttribs = ShaderRes.GetCombinedSampler(TexSRV);
                        const auto  SamplerVarType = ShaderRes.FindVariableType(SamplerAttribs, ResourceLayout);
                        const auto  TexSrvVarType  = ShaderRes.FindVariableType(TexSRV, ResourceLayout);
                        DEV_CHECK_ERR(SamplerVarType == TexSrvVarType,
                                      "The type (", GetShaderVariableTypeLiteralName(TexSrvVarType), ") of texture SRV variable '", TexSRV.Name,
                                      "' is not consistent with the type (", GetShaderVariableTypeLiteralName(SamplerVarType),
                                      ") of the sampler '", SamplerAttribs.Name, "' that is assigned to it");
                    }
#endif
                }
            },
            AddResource,
            AddResource,
            AddResource,
            AddResource //
        );
    }

    auto stringPool = AllocateMemory(LayoutDataAllocator, CbvSrvUavCount, SamplerCount, StringPoolSize);

    stringPool.CopyString(ShaderName);

    std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES> CurrCbvSrvUav = {};
    std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES> CurrSampler   = {};

    for (auto* pShader : Shaders)
    {
        const auto& ShaderRes = *pShader->GetShaderResources();

        auto InitResource = [&](const D3DShaderResourceAttribs& Attribs,
                                CachedResourceType              ResType,
                                Uint32                          SamplerId = D3DShaderResourceAttribs::InvalidSamplerId) //
        {
            const auto VarType = ShaderRes.FindVariableType(Attribs, ResourceLayout);

            auto ResIter = ResourceNameToIndex.find(HashMapStringKey{Attribs.Name});
            VERIFY(ResIter != ResourceNameToIndex.end(),
                   "Resource '", Attribs.Name,
                   "' is not found in ResourceNameToIndex map. This should never happen as "
                   "all resources are added to the map when they are being counted.");

            if (ResIter->second == InvalidResourceIndex)
            {
                Uint32 RootIndex = D3D12Resource::InvalidRootIndex;
                Uint32 Offset    = D3D12Resource::InvalidOffset;
                Uint32 BindPoint = D3DShaderResourceAttribs::InvalidBindPoint;

                D3D12_DESCRIPTOR_RANGE_TYPE DescriptorRangeType = GetDescriptorRangeType(ResType);

                RootSgnBldr.AllocateResourceSlot(GetShaderType(), PipelineType, Attribs, VarType, DescriptorRangeType, BindPoint, RootIndex, Offset);
                VERIFY(RootIndex <= D3D12Resource::MaxRootIndex, "Root index excceeds allowed limit");
                VERIFY(RootIndex != D3D12Resource::InvalidRootIndex, "Root index must be valid");
                VERIFY(BindPoint <= D3DShaderResourceAttribs::MaxBindPoint, "Bind point excceeds allowed limit");
                VERIFY(Offset != D3D12Resource::InvalidOffset, "Offset must be valid");

                // Immutable samplers are never copied, and SamplerId == InvalidSamplerId
                Uint32 ResOffset = (ResType == CachedResourceType::Sampler) ?
                    GetSamplerOffset(VarType, CurrSampler[VarType]++) :
                    GetSrvCbvUavOffset(VarType, CurrCbvSrvUav[VarType]++);
                ResIter->second   = ResOffset;
                auto& NewResource = GetResource(ResOffset);
                ::new (&NewResource) D3D12Resource //
                    {
                        *this,
                        stringPool,
                        Attribs,
                        SamplerId,
                        VarType,
                        ResType,
                        BindPoint,
                        RootIndex,
                        Offset //
                    };
            }
            else
            {
                // Merge with existing
                auto& ExistingRes = GetResource(ResIter->second);
                VERIFY(ExistingRes.VariableType == VarType,
                       "The type of variable '", Attribs.Name, "' does not match the type determined for previous shaders. This appears to be a bug.");

                DEV_CHECK_ERR(ExistingRes.Attribs.GetInputType() == Attribs.GetInputType(),
                              "Shader variable '", Attribs.Name,
                              "' exists in multiple shaders from the same shader stage, but its input type is not consistent between "
                              "shaders. All variables with the same name from the same shader stage must have the same input type.");

                DEV_CHECK_ERR(ExistingRes.Attribs.GetSRVDimension() == Attribs.GetSRVDimension(),
                              "Shader variable '", Attribs.Name,
                              "' exists in multiple shaders from the same shader stage, but its SRV dimension is not consistent between "
                              "shaders. All variables with the same name from the same shader stage must have the same SRV dimension.");

                DEV_CHECK_ERR(ExistingRes.Attribs.BindCount == Attribs.BindCount,
                              "Shader variable '", Attribs.Name,
                              "' exists in multiple shaders from the same shader stage, but its array size is not consistent between "
                              "shaders. All variables with the same name from the same shader stage must have the same array size.");
            }
        };

        ShaderRes.ProcessResources(
            [&](const D3DShaderResourceAttribs& CB, Uint32) //
            {
                if (pLocalRootSig != nullptr && pLocalRootSig->SetOrMerge(CB))
                    return;

                InitResource(CB, CachedResourceType::CBV);
            },
            [&](const D3DShaderResourceAttribs& Sam, Uint32) //
            {
                // The errors (if any) have already been logged when counting the resources
                constexpr bool LogImtblSamplerArrayError = false;

                const auto ImtblSamplerInd = ShaderRes.FindImmutableSampler(Sam, ResourceLayout, LogImtblSamplerArrayError);
                if (ImtblSamplerInd >= 0)
                {
                    // Note that there may be multiple immutable samplers with the same name in different ray tracing shaders
                    // that are assigned to different registers. InitImmutableSampler() handles this by allocating new
                    // register only first time the sampler is encountered. All bindings will be remapped afterwards.
                    RootSgnBldr.InitImmutableSampler(ShaderRes.GetShaderType(), Sam.Name, ShaderRes.GetCombinedSamplerSuffix(), Sam);
                }
                else
                {
                    InitResource(Sam, CachedResourceType::Sampler);
                }
            },
            [&](const D3DShaderResourceAttribs& TexSRV, Uint32) //
            {
                static_assert(SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES == 3, "Unexpected number of shader variable types");
                VERIFY(CurrSampler[SHADER_RESOURCE_VARIABLE_TYPE_STATIC] + CurrSampler[SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE] + CurrSampler[SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC] == GetTotalSamplerCount(), "All samplers must be initialized before texture SRVs");

                Uint32 SamplerId = D3DShaderResourceAttribs::InvalidSamplerId;
                if (TexSRV.IsCombinedWithSampler())
                {
                    const auto& SamplerAttribs = ShaderRes.GetCombinedSampler(TexSRV);
                    const auto  SamplerVarType = ShaderRes.FindVariableType(SamplerAttribs, ResourceLayout);
                    const auto  TexSrvVarType  = ShaderRes.FindVariableType(TexSRV, ResourceLayout);
                    DEV_CHECK_ERR(SamplerVarType == TexSrvVarType,
                                  "The type (", GetShaderVariableTypeLiteralName(TexSrvVarType), ") of texture SRV variable '", TexSRV.Name,
                                  "' is not consistent with the type (", GetShaderVariableTypeLiteralName(SamplerVarType),
                                  ") of the sampler '", SamplerAttribs.Name, "' that is assigned to it");

                    // The errors (if any) have already been logged when counting the resources
                    constexpr bool LogImtblSamplerArrayError = false;
                    const auto     ImtblSamplerInd           = ShaderRes.FindImmutableSampler(SamplerAttribs, ResourceLayout, LogImtblSamplerArrayError);
                    if (ImtblSamplerInd >= 0)
                    {
                        SamplerId = D3DShaderResourceAttribs::InvalidSamplerId;
                        // Immutable samplers are never copied, and should not be found in resources
                        DEV_CHECK_ERR(FindSamplerByName(SamplerAttribs.Name) == D3DShaderResourceAttribs::InvalidSamplerId,
                                      "Immutable sampler '", SamplerAttribs.Name, "' was found among shader resources. This seems to be a bug");
                    }
                    else
                    {
                        SamplerId = FindSamplerByName(SamplerAttribs.Name);
                        DEV_CHECK_ERR(SamplerId != D3DShaderResourceAttribs::InvalidSamplerId,
                                      "Unable to find sampler '", SamplerAttribs.Name, "' assigned to texture SRV '", TexSRV.Name,
                                      "' in the list of already created shader resources. This seems to be a bug.");
                    }
                }
                InitResource(TexSRV, CachedResourceType::TexSRV, SamplerId);
            },
            [&](const D3DShaderResourceAttribs& TexUAV, Uint32) //
            {
                InitResource(TexUAV, CachedResourceType::TexUAV);
            },
            [&](const D3DShaderResourceAttribs& BufSRV, Uint32) //
            {
                InitResource(BufSRV, CachedResourceType::BufSRV);
            },
            [&](const D3DShaderResourceAttribs& BufUAV, Uint32) //
            {
                InitResource(BufUAV, CachedResourceType::BufUAV);
            },
            [&](const D3DShaderResourceAttribs& AccelStruct, Uint32) //
            {
                InitResource(AccelStruct, CachedResourceType::AccelStruct);
            } //
        );
    }

#ifdef DILIGENT_DEBUG
    VERIFY_EXPR(stringPool.GetRemainingSize() == 0);
    for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC; VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
    {
        VERIFY(CurrCbvSrvUav[VarType] == CbvSrvUavCount[VarType], "Not all Srv/Cbv/Uavs are initialized, which result in a crash when dtor is called");
        VERIFY(CurrSampler[VarType] == SamplerCount[VarType], "Not all Samplers are initialized, which result in a crash when dtor is called");
    }
#endif
}

void ShaderResourceLayoutD3D12::InitializeStaticReourceLayout(const ShaderResourceLayoutD3D12& SrcLayout,
                                                              IMemoryAllocator&                LayoutDataAllocator,
                                                              ShaderResourceCacheD3D12&        ResourceCache)
{
    m_IsUsingSeparateSamplers = SrcLayout.m_IsUsingSeparateSamplers;
    m_ShaderType              = SrcLayout.m_ShaderType;

    const auto        AllowedTypeBits = GetAllowedTypeBit(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    const auto* const ShaderName      = SrcLayout.GetShaderName();

    std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES> CbvSrvUavCount = {};
    std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES> SamplerCount   = {};

    size_t StringPoolSize = StringPool::GetRequiredReserveSize(ShaderName);

    for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
         VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES;
         VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
    {
        if (!IsAllowedType(VarType, AllowedTypeBits))
            continue;

        CbvSrvUavCount[VarType] = SrcLayout.GetCbvSrvUavCount(VarType);
        SamplerCount[VarType]   = SrcLayout.GetSamplerCount(VarType);

        for (Uint32 i = 0; i < CbvSrvUavCount[VarType]; ++i)
            StringPoolSize += StringPool::GetRequiredReserveSize(SrcLayout.GetSrvCbvUav(VarType, i).Attribs.Name);
        for (Uint32 i = 0; i < SamplerCount[VarType]; ++i)
            StringPoolSize += StringPool::GetRequiredReserveSize(SrcLayout.GetSampler(VarType, i).Attribs.Name);
    }

    auto stringPool = AllocateMemory(LayoutDataAllocator, CbvSrvUavCount, SamplerCount, StringPoolSize);
    stringPool.CopyString(ShaderName);

    std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES> CurrCbvSrvUav = {};
    std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES> CurrSampler   = {};

    std::array<Uint32, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER + 1> StaticResCacheTblSizes = {};

    auto InitResource = [&](const D3D12Resource& SrcRes,
                            Uint32               SamplerId = D3DShaderResourceAttribs::InvalidSamplerId) //
    {
        const auto ResType = SrcRes.GetResType();
        const auto VarType = SrcRes.GetVariableType();

        const Uint32 ResOffset = (ResType == CachedResourceType::Sampler) ?
            GetSamplerOffset(VarType, CurrSampler[VarType]++) :
            GetSrvCbvUavOffset(VarType, CurrCbvSrvUav[VarType]++);

        // http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-layout#Initializing-Special-Resource-Layout-for-Managing-Static-Shader-Resources
        // Use artifial root signature:
        // SRVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_SRV (0)
        // UAVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_UAV (1)
        // CBVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_CBV (2)
        // Samplers at root index D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER (3)
        const Uint32 RootIndex = GetDescriptorRangeType(ResType);
        const Uint32 Offset    = StaticResCacheTblSizes[RootIndex];

        auto& NewResource = GetResource(ResOffset);
        ::new (&NewResource) D3D12Resource //
            {
                *this,
                stringPool,
                SrcRes.Attribs,
                SamplerId,
                VarType,
                ResType,
                SrcRes.Attribs.BindPoint,
                RootIndex,
                Offset //
            };

        StaticResCacheTblSizes[RootIndex] += NewResource.Attribs.BindCount;
    };

    // Process samplers first
    for (Uint32 s = 0; s < SrcLayout.GetTotalSamplerCount(); ++s)
    {
        const auto& SrcSmplr = SrcLayout.GetSampler(s);
        if (IsAllowedType(SrcSmplr.GetVariableType(), AllowedTypeBits))
        {
            InitResource(SrcSmplr);
        }
    }

    // Process SRVs, CBVs, UAVs
    for (Uint32 res = 0; res < SrcLayout.GetTotalSrvCbvUavCount(); ++res)
    {
        const auto& SrcRes  = SrcLayout.GetSrvCbvUav(res);
        const auto  VarType = SrcRes.GetVariableType();
        if (!IsAllowedType(VarType, AllowedTypeBits))
            continue;

        Uint32 SamplerId = D3DShaderResourceAttribs::InvalidSamplerId;
        if (SrcRes.Attribs.IsCombinedWithSampler())
        {
            // If source resource is combined with the sampler, there must also be
            // a corresponding sampler in this layout.

            const auto& SrcAssignedSmplr = SrcLayout.GetAssignedSampler(SrcRes);
            VERIFY(SrcAssignedSmplr.GetVariableType() == SrcRes.GetVariableType(),
                   "The type of the sampler does not match the type of the texture it is assigned to. This is likely a bug.");

            SamplerId = FindSamplerByName(SrcAssignedSmplr.Attribs.Name);
            VERIFY(SamplerId != D3DShaderResourceAttribs::InvalidSamplerId,
                   "Unable to find sampler '", SrcAssignedSmplr.Attribs.Name, "' among resources. This seems to be a bug.");
        }

        InitResource(SrcRes, SamplerId);
    }

#ifdef DILIGENT_DEBUG
    VERIFY_EXPR(stringPool.GetRemainingSize() == 0);
    for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC; VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
    {
        VERIFY(CurrCbvSrvUav[VarType] == CbvSrvUavCount[VarType], "Not all Srv/Cbv/Uavs are initialized, which result in a crash when dtor is called");
        VERIFY(CurrSampler[VarType] == SamplerCount[VarType], "Not all Samplers are initialized, which result in a crash when dtor is called");
    }
#endif

    // Initialize resource cache to store static resources
    // http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-cache#Initializing-the-Cache-for-Static-Shader-Resources
    // http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-cache#Initializing-Shader-Objects
    ResourceCache.Initialize(GetRawAllocator(), static_cast<Uint32>(StaticResCacheTblSizes.size()), StaticResCacheTblSizes.data());
#ifdef DILIGENT_DEBUG
    ResourceCache.GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV).SetDebugAttribs(StaticResCacheTblSizes[D3D12_DESCRIPTOR_RANGE_TYPE_SRV], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, GetShaderType());
    ResourceCache.GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV).SetDebugAttribs(StaticResCacheTblSizes[D3D12_DESCRIPTOR_RANGE_TYPE_UAV], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, GetShaderType());
    ResourceCache.GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_CBV).SetDebugAttribs(StaticResCacheTblSizes[D3D12_DESCRIPTOR_RANGE_TYPE_CBV], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, GetShaderType());
    ResourceCache.GetRootTable(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER).SetDebugAttribs(StaticResCacheTblSizes[D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, GetShaderType());
#endif
}

void ShaderResourceLayoutD3D12::D3D12Resource::CacheCB(IDeviceObject*                      pBuffer,
                                                       ShaderResourceCacheD3D12::Resource& DstRes,
                                                       Uint32                              ArrayInd,
                                                       D3D12_CPU_DESCRIPTOR_HANDLE         ShdrVisibleHeapCPUDescriptorHandle,
                                                       Uint32&                             BoundDynamicCBsCounter) const
{
    // http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-cache#Binding-Objects-to-Shader-Variables

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<BufferD3D12Impl> pBuffD3D12{pBuffer, IID_BufferD3D12};
#ifdef DILIGENT_DEVELOPMENT
    VerifyConstantBufferBinding(Attribs, GetVariableType(), ArrayInd, pBuffer, pBuffD3D12.RawPtr(), DstRes.pObject.RawPtr(), ParentResLayout.GetShaderName());
#endif
    if (pBuffD3D12)
    {
        if (GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        DstRes.Type                = GetResType();
        DstRes.CPUDescriptorHandle = pBuffD3D12->GetCBVHandle();
        VERIFY(DstRes.CPUDescriptorHandle.ptr != 0 || pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC, "No relevant CBV CPU descriptor handle");

        if (ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
        {
            // Dynamic resources are assigned descriptor in the GPU-visible heap at every draw call, and
            // the descriptor is copied by the RootSignature when resources are committed
            VERIFY(DstRes.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");

            ID3D12Device* pd3d12Device = ParentResLayout.m_pd3d12Device;
            pd3d12Device->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, DstRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        if (DstRes.pObject != nullptr && DstRes.pObject.RawPtr<const BufferD3D12Impl>()->GetDesc().Usage == USAGE_DYNAMIC)
        {
            VERIFY(BoundDynamicCBsCounter > 0, "There is a dynamic CB bound in the resource cache, but the dynamic CB counter is zero");
            --BoundDynamicCBsCounter;
        }
        if (pBuffD3D12->GetDesc().Usage == USAGE_DYNAMIC)
            ++BoundDynamicCBsCounter;
        DstRes.pObject = std::move(pBuffD3D12);
    }
}

template <typename TResourceViewType>
struct ResourceViewTraits
{};

template <>
struct ResourceViewTraits<ITextureViewD3D12>
{
    static const INTERFACE_ID& IID;

    static bool VerifyView(ITextureViewD3D12* pViewD3D12, const D3DShaderResourceAttribs& Attribs, const char* ShaderName)
    {
        return true;
    }
};
const INTERFACE_ID& ResourceViewTraits<ITextureViewD3D12>::IID = IID_TextureViewD3D12;

template <>
struct ResourceViewTraits<IBufferViewD3D12>
{
    static const INTERFACE_ID& IID;

    static bool VerifyView(IBufferViewD3D12* pViewD3D12, const D3DShaderResourceAttribs& Attribs, const char* ShaderName)
    {
        return VerifyBufferViewModeD3D(pViewD3D12, Attribs, ShaderName);
    }
};
const INTERFACE_ID& ResourceViewTraits<IBufferViewD3D12>::IID = IID_BufferViewD3D12;

template <typename TResourceViewType,    ///< ResType of the view (ITextureViewD3D12 or IBufferViewD3D12)
          typename TViewTypeEnum,        ///< ResType of the expected view type enum (TEXTURE_VIEW_TYPE or BUFFER_VIEW_TYPE)
          typename TBindSamplerProcType> ///< ResType of the procedure to set sampler
void ShaderResourceLayoutD3D12::D3D12Resource::CacheResourceView(IDeviceObject*                      pView,
                                                                 ShaderResourceCacheD3D12::Resource& DstRes,
                                                                 Uint32                              ArrayIndex,
                                                                 D3D12_CPU_DESCRIPTOR_HANDLE         ShdrVisibleHeapCPUDescriptorHandle,
                                                                 TViewTypeEnum                       dbgExpectedViewType,
                                                                 TBindSamplerProcType                BindSamplerProc) const
{
    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<TResourceViewType> pViewD3D12{pView, ResourceViewTraits<TResourceViewType>::IID};
#ifdef DILIGENT_DEVELOPMENT
    VerifyResourceViewBinding(Attribs, GetVariableType(), ArrayIndex, pView, pViewD3D12.RawPtr(), {dbgExpectedViewType}, DstRes.pObject.RawPtr(), ParentResLayout.GetShaderName());
    ResourceViewTraits<TResourceViewType>::VerifyView(pViewD3D12, Attribs, ParentResLayout.GetShaderName());
#endif
    if (pViewD3D12)
    {
        if (GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        DstRes.Type                = GetResType();
        DstRes.CPUDescriptorHandle = pViewD3D12->GetCPUDescriptorHandle();
        VERIFY(DstRes.CPUDescriptorHandle.ptr != 0, "No relevant D3D12 view");

        if (ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
        {
            // Dynamic resources are assigned descriptor in the GPU-visible heap at every draw call, and
            // the descriptor is copied by the RootSignature when resources are committed
            VERIFY(DstRes.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");

            ID3D12Device* pd3d12Device = ParentResLayout.m_pd3d12Device;
            pd3d12Device->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, DstRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        BindSamplerProc(pViewD3D12);

        DstRes.pObject = std::move(pViewD3D12);
    }
}

void ShaderResourceLayoutD3D12::D3D12Resource::CacheSampler(IDeviceObject*                      pSampler,
                                                            ShaderResourceCacheD3D12::Resource& DstSam,
                                                            Uint32                              ArrayIndex,
                                                            D3D12_CPU_DESCRIPTOR_HANDLE         ShdrVisibleHeapCPUDescriptorHandle) const
{
    VERIFY(Attribs.IsValidBindPoint(), "Invalid bind point");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    RefCntAutoPtr<ISamplerD3D12> pSamplerD3D12{pSampler, IID_SamplerD3D12};
    if (pSamplerD3D12)
    {
        if (GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && DstSam.pObject != nullptr)
        {
            if (DstSam.pObject != pSampler)
            {
                auto VarTypeStr = GetShaderVariableTypeLiteralName(GetVariableType());
                LOG_ERROR_MESSAGE("Non-null sampler is already bound to ", VarTypeStr, " shader variable '", Attribs.GetPrintName(ArrayIndex),
                                  "' in shader '", ParentResLayout.GetShaderName(), "'. Attempting to bind another sampler is an error and will "
                                                                                    "be ignored. Use another shader resource binding instance or label the variable as dynamic.");
            }

            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        DstSam.Type = CachedResourceType::Sampler;

        DstSam.CPUDescriptorHandle = pSamplerD3D12->GetCPUDescriptorHandle();
        VERIFY(DstSam.CPUDescriptorHandle.ptr != 0, "No relevant D3D12 sampler descriptor handle");

        if (ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
        {
            // Dynamic resources are assigned descriptor in the GPU-visible heap at every draw call, and
            // the descriptor is copied by the RootSignature when resources are committed
            VERIFY(DstSam.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");

            ID3D12Device* pd3d12Device = ParentResLayout.m_pd3d12Device;
            pd3d12Device->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, DstSam.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        }

        DstSam.pObject = std::move(pSamplerD3D12);
    }
    else
    {
        LOG_ERROR_MESSAGE("Failed to bind object '", pSampler->GetDesc().Name, "' to variable '", Attribs.GetPrintName(ArrayIndex),
                          "' in shader '", ParentResLayout.GetShaderName(), "'. Incorect object type: sampler is expected.");
    }
}

void ShaderResourceLayoutD3D12::D3D12Resource::CacheAccelStruct(IDeviceObject*                      pTLAS,
                                                                ShaderResourceCacheD3D12::Resource& DstRes,
                                                                Uint32                              ArrayIndex,
                                                                D3D12_CPU_DESCRIPTOR_HANDLE         ShdrVisibleHeapCPUDescriptorHandle) const
{
    VERIFY(Attribs.IsValidBindPoint(), "Invalid bind point");
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    RefCntAutoPtr<ITopLevelASD3D12> pTLASD3D12{pTLAS, IID_TopLevelASD3D12};
    if (pTLASD3D12)
    {
        if (GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as CopyDescriptorsSimple() may interfere with GPU reading the same descriptor.
            return;
        }

        DstRes.Type                = GetResType();
        DstRes.CPUDescriptorHandle = pTLASD3D12->GetCPUDescriptorHandle();
        VERIFY(DstRes.CPUDescriptorHandle.ptr != 0, "No relevant D3D12 resource");

        if (ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
        {
            // Dynamic resources are assigned descriptor in the GPU-visible heap at every draw call, and
            // the descriptor is copied by the RootSignature when resources are committed
            VERIFY(DstRes.pObject == nullptr, "Static and mutable resource descriptors must be copied only once");

            ID3D12Device* pd3d12Device = ParentResLayout.m_pd3d12Device;
            pd3d12Device->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, DstRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        DstRes.pObject = std::move(pTLASD3D12);
    }
}


Uint32 ShaderResourceLayoutD3D12::FindSamplerByName(const char* SamplerName) const
{
    const auto SamplerCount = GetTotalSamplerCount();
    for (Uint32 SamplerId = 0; SamplerId < SamplerCount; ++SamplerId)
    {
        const auto& Sampler = GetSampler(SamplerId);
        if (strcmp(Sampler.Attribs.Name, SamplerName) == 0)
        {
            VERIFY(SamplerId <= D3DShaderResourceAttribs::MaxSamplerId, "Sampler index excceeds allowed limit");
            return SamplerId;
        }
    }

    return D3DShaderResourceAttribs::InvalidSamplerId;
}

const ShaderResourceLayoutD3D12::D3D12Resource& ShaderResourceLayoutD3D12::GetAssignedSampler(const D3D12Resource& TexSrv) const
{
    VERIFY(TexSrv.GetResType() == CachedResourceType::TexSRV, "Unexpected resource type: texture SRV is expected");
    VERIFY(TexSrv.Attribs.IsCombinedWithSampler(), "Texture SRV has no associated sampler");
    const auto& SamInfo = GetSampler(TexSrv.Attribs.GetCombinedSamplerId());
    VERIFY(SamInfo.GetVariableType() == TexSrv.GetVariableType(), "Inconsistent texture and sampler variable types");
    //VERIFY(StreqSuff(SamInfo.Name, TexSrv.Name, GetCombinedSamplerSuffix()), "Sampler name '", SamInfo.Name, "' does not match texture name '", TexSrv.Name, '\'');
    return SamInfo;
}

ShaderResourceLayoutD3D12::D3D12Resource& ShaderResourceLayoutD3D12::GetAssignedSampler(const D3D12Resource& TexSrv)
{
    return const_cast<D3D12Resource&>(const_cast<const ShaderResourceLayoutD3D12*>(this)->GetAssignedSampler(TexSrv));
}


void ShaderResourceLayoutD3D12::D3D12Resource::BindResource(IDeviceObject*            pObj,
                                                            Uint32                    ArrayIndex,
                                                            ShaderResourceCacheD3D12& ResourceCache) const
{
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    const bool IsSampler          = GetResType() == CachedResourceType::Sampler;
    auto       DescriptorHeapType = IsSampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    auto&      DstRes             = ResourceCache.GetRootTable(RootIndex).GetResource(OffsetFromTableStart + ArrayIndex, DescriptorHeapType, ParentResLayout.GetShaderType());

    auto ShdrVisibleHeapCPUDescriptorHandle = IsSampler ?
        ResourceCache.GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(RootIndex, OffsetFromTableStart + ArrayIndex) :
        ResourceCache.GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(RootIndex, OffsetFromTableStart + ArrayIndex);

#ifdef DILIGENT_DEBUG
    {
        if (ResourceCache.DbgGetContentType() == ShaderResourceCacheD3D12::DbgCacheContentType::StaticShaderResources)
        {
            VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Static shader resources of a shader should not be assigned shader visible descriptor space");
        }
        else if (ResourceCache.DbgGetContentType() == ShaderResourceCacheD3D12::DbgCacheContentType::SRBResources)
        {
            if (GetResType() == CachedResourceType::CBV && Attribs.BindCount == 1)
            {
                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Non-array constant buffers are bound as root views and should not be assigned shader visible descriptor space");
            }
            else
            {
                if (GetVariableType() == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
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

    if (pObj)
    {
        static_assert(static_cast<int>(CachedResourceType::NumTypes) == 7, "Please update this function to handle the new resource type");
        switch (GetResType())
        {
            case CachedResourceType::CBV:
                CacheCB(pObj, DstRes, ArrayIndex, ShdrVisibleHeapCPUDescriptorHandle, ResourceCache.GetBoundDynamicCBsCounter());
                break;

            case CachedResourceType::TexSRV:
                CacheResourceView<ITextureViewD3D12>(
                    pObj, DstRes, ArrayIndex, ShdrVisibleHeapCPUDescriptorHandle, TEXTURE_VIEW_SHADER_RESOURCE,
                    [&](ITextureViewD3D12* pTexView) //
                    {
                        if (Attribs.IsCombinedWithSampler())
                        {
                            auto& Sam = ParentResLayout.GetAssignedSampler(*this);
                            //VERIFY( !Sam.IsImmutableSampler(), "Immutable samplers should never be assigned space in the cache" );
                            VERIFY_EXPR(Attribs.BindCount == Sam.Attribs.BindCount || Sam.Attribs.BindCount == 1);
                            auto SamplerArrInd = Sam.Attribs.BindCount > 1 ? ArrayIndex : 0;

                            auto ShdrVisibleSamplerHeapCPUDescriptorHandle = ResourceCache.GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(Sam.RootIndex, Sam.OffsetFromTableStart + SamplerArrInd);

                            auto& DstSam = ResourceCache.GetRootTable(Sam.RootIndex).GetResource(Sam.OffsetFromTableStart + SamplerArrInd, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, ParentResLayout.GetShaderType());
#ifdef DILIGENT_DEBUG
                            {
                                if (ResourceCache.DbgGetContentType() == ShaderResourceCacheD3D12::DbgCacheContentType::StaticShaderResources)
                                {
                                    VERIFY(ShdrVisibleSamplerHeapCPUDescriptorHandle.ptr == 0, "Static shader resources of a shader should not be assigned shader visible descriptor space");
                                }
                                else if (ResourceCache.DbgGetContentType() == ShaderResourceCacheD3D12::DbgCacheContentType::SRBResources)
                                {
                                    if (GetVariableType() == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                                        VERIFY(ShdrVisibleSamplerHeapCPUDescriptorHandle.ptr == 0, "Dynamic resources of a shader resource binding should be assigned shader visible descriptor space at every draw call");
                                    else
                                        VERIFY(ShdrVisibleSamplerHeapCPUDescriptorHandle.ptr != 0 || pTexView == nullptr, "Non-dynamics resources of a shader resource binding must be assigned shader visible descriptor space");
                                }
                                else
                                {
                                    UNEXPECTED("Unknown content type");
                                }
                            }
#endif
                            auto* pSampler = pTexView->GetSampler();
                            if (pSampler)
                            {
                                Sam.CacheSampler(pSampler, DstSam, SamplerArrInd, ShdrVisibleSamplerHeapCPUDescriptorHandle);
                            }
                            else
                            {
                                LOG_ERROR_MESSAGE("Failed to bind sampler to variable '", Sam.Attribs.Name, ". Sampler is not set in the texture view '", pTexView->GetDesc().Name, '\'');
                            }
                        }
                    });
                break;

            case CachedResourceType::TexUAV:
                CacheResourceView<ITextureViewD3D12>(pObj, DstRes, ArrayIndex, ShdrVisibleHeapCPUDescriptorHandle, TEXTURE_VIEW_UNORDERED_ACCESS, [](ITextureViewD3D12*) {});
                break;

            case CachedResourceType::BufSRV:
                CacheResourceView<IBufferViewD3D12>(pObj, DstRes, ArrayIndex, ShdrVisibleHeapCPUDescriptorHandle, BUFFER_VIEW_SHADER_RESOURCE, [](IBufferViewD3D12*) {});
                break;

            case CachedResourceType::BufUAV:
                CacheResourceView<IBufferViewD3D12>(pObj, DstRes, ArrayIndex, ShdrVisibleHeapCPUDescriptorHandle, BUFFER_VIEW_UNORDERED_ACCESS, [](IBufferViewD3D12*) {});
                break;

            case CachedResourceType::Sampler:
                DEV_CHECK_ERR(ParentResLayout.IsUsingSeparateSamplers(), "Samplers should not be set directly when using combined texture samplers");
                CacheSampler(pObj, DstRes, ArrayIndex, ShdrVisibleHeapCPUDescriptorHandle);
                break;

            case CachedResourceType::AccelStruct:
                CacheAccelStruct(pObj, DstRes, ArrayIndex, ShdrVisibleHeapCPUDescriptorHandle);
                break;

            default: UNEXPECTED("Unknown resource type ", static_cast<Int32>(GetResType()));
        }
    }
    else
    {
        if (DstRes.pObject != nullptr && GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            LOG_ERROR_MESSAGE("Shader variable '", Attribs.Name, "' in shader '", ParentResLayout.GetShaderName(), "' is not dynamic but is being reset to null. This is an error and may cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic if you need to bind another resource.");

        DstRes = ShaderResourceCacheD3D12::Resource{};
        if (Attribs.IsCombinedWithSampler())
        {
            auto&                       Sam           = ParentResLayout.GetAssignedSampler(*this);
            D3D12_CPU_DESCRIPTOR_HANDLE NullHandle    = {0};
            auto                        SamplerArrInd = Sam.Attribs.BindCount > 1 ? ArrayIndex : 0;
            auto&                       DstSam        = ResourceCache.GetRootTable(Sam.RootIndex).GetResource(Sam.OffsetFromTableStart + SamplerArrInd, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, ParentResLayout.GetShaderType());
            if (DstSam.pObject != nullptr && Sam.GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                LOG_ERROR_MESSAGE("Sampler variable '", Sam.Attribs.Name, "' in shader '", ParentResLayout.GetShaderName(), "' is not dynamic but is being reset to null. This is an error and may cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic if you need to bind another sampler.");
            DstSam = ShaderResourceCacheD3D12::Resource{};
        }
    }
}

bool ShaderResourceLayoutD3D12::D3D12Resource::IsBound(Uint32 ArrayIndex, const ShaderResourceCacheD3D12& ResourceCache) const
{
    VERIFY_EXPR(ArrayIndex < Attribs.BindCount);

    if (RootIndex < ResourceCache.GetNumRootTables())
    {
        const auto& RootTable = ResourceCache.GetRootTable(RootIndex);
        if (OffsetFromTableStart + ArrayIndex < RootTable.GetSize())
        {
            const auto& CachedRes =
                RootTable.GetResource(OffsetFromTableStart + ArrayIndex,
                                      GetResType() == CachedResourceType::Sampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                      ParentResLayout.GetShaderType());
            if (CachedRes.pObject != nullptr)
            {
                VERIFY(CachedRes.CPUDescriptorHandle.ptr != 0 || CachedRes.pObject.RawPtr<BufferD3D12Impl>()->GetDesc().Usage == USAGE_DYNAMIC, "No relevant descriptor handle");
                return true;
            }
        }
    }

    return false;
}

void ShaderResourceLayoutD3D12::CopyStaticResourceDesriptorHandles(const ShaderResourceCacheD3D12&  SrcCache,
                                                                   const ShaderResourceLayoutD3D12& DstLayout,
                                                                   ShaderResourceCacheD3D12&        DstCache) const
{
    // Static shader resources are stored as follows:
    // CBVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
    // SRVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
    // UAVs at root index D3D12_DESCRIPTOR_RANGE_TYPE_UAV, and
    // Samplers at root index D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER

    {
        const auto CbvSrvUavCount = DstLayout.GetCbvSrvUavCount(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
        VERIFY(GetCbvSrvUavCount(SHADER_RESOURCE_VARIABLE_TYPE_STATIC) == CbvSrvUavCount,
               "The number of static resources in the source cache (", GetCbvSrvUavCount(SHADER_RESOURCE_VARIABLE_TYPE_STATIC),
               ") is not consistent with the number of static resources in destination cache (", CbvSrvUavCount, ")");
        auto& DstBoundDynamicCBsCounter = DstCache.GetBoundDynamicCBsCounter();
        for (Uint32 r = 0; r < CbvSrvUavCount; ++r)
        {
            // Get resource attributes
            const auto& DstResInfo = DstLayout.GetSrvCbvUav(SHADER_RESOURCE_VARIABLE_TYPE_STATIC, r);
            const auto& SrcResInfo = GetSrvCbvUav(SHADER_RESOURCE_VARIABLE_TYPE_STATIC, r);
            VERIFY(strcmp(SrcResInfo.Attribs.Name, DstResInfo.Attribs.Name) == 0, "Src resource name ('", SrcResInfo.Attribs.Name, "') does match the dst resource name '(", DstResInfo.Attribs.Name, "'). This is a bug.");
            VERIFY(SrcResInfo.Attribs.IsCompatibleWith(DstResInfo.Attribs), "Src resource is incompatible with the dst resource. This is a bug.");

            // Source resource in the static resource cache is in the root table at index RangeType
            // D3D12_DESCRIPTOR_RANGE_TYPE_SRV = 0,
            // D3D12_DESCRIPTOR_RANGE_TYPE_UAV = 1
            // D3D12_DESCRIPTOR_RANGE_TYPE_CBV = 2
            VERIFY(SrcResInfo.RootIndex == static_cast<Uint32>(GetDescriptorRangeType(DstResInfo.GetResType())), "Unexpected root index for the source resource. This is a bug.");
            const auto& SrcRootTable = SrcCache.GetRootTable(SrcResInfo.RootIndex);
            auto&       DstRootTable = DstCache.GetRootTable(DstResInfo.RootIndex);
            for (Uint32 ArrInd = 0; ArrInd < DstResInfo.Attribs.BindCount; ++ArrInd)
            {
                const auto& SrcRes = SrcRootTable.GetResource(SrcResInfo.OffsetFromTableStart + ArrInd, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, GetShaderType());
                if (!SrcRes.pObject)
                    LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", DstResInfo.Attribs.GetPrintName(ArrInd), "' in shader '", GetShaderName(), "'.");

                auto& DstRes = DstRootTable.GetResource(DstResInfo.OffsetFromTableStart + ArrInd, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, GetShaderType());

                if (DstRes.pObject != SrcRes.pObject)
                {
                    DEV_CHECK_ERR(DstRes.pObject == nullptr, "Static resource has already been initialized, and the resource to be assigned from the shader does not match previously assigned resource");

                    if (SrcRes.Type == CachedResourceType::CBV)
                    {
                        if (DstRes.pObject && DstRes.pObject.RawPtr<const BufferD3D12Impl>()->GetDesc().Usage == USAGE_DYNAMIC)
                        {
                            VERIFY_EXPR(DstBoundDynamicCBsCounter > 0);
                            --DstBoundDynamicCBsCounter;
                        }
                        if (SrcRes.pObject && SrcRes.pObject.RawPtr<const BufferD3D12Impl>()->GetDesc().Usage == USAGE_DYNAMIC)
                        {
                            ++DstBoundDynamicCBsCounter;
                        }
                    }

                    DstRes.pObject             = SrcRes.pObject;
                    DstRes.Type                = SrcRes.Type;
                    DstRes.CPUDescriptorHandle = SrcRes.CPUDescriptorHandle;

                    auto ShdrVisibleHeapCPUDescriptorHandle =
                        DstCache.GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(
                            DstResInfo.RootIndex,
                            DstResInfo.OffsetFromTableStart + ArrInd);
                    VERIFY_EXPR(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0 || DstRes.Type == CachedResourceType::CBV);
                    // Root views are not assigned space in the GPU-visible descriptor heap allocation
                    if (ShdrVisibleHeapCPUDescriptorHandle.ptr != 0)
                    {
                        m_pd3d12Device->CopyDescriptorsSimple(1, ShdrVisibleHeapCPUDescriptorHandle, SrcRes.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                    }
                }
                else
                {
                    VERIFY_EXPR(DstRes.pObject == SrcRes.pObject);
                    VERIFY_EXPR(DstRes.Type == SrcRes.Type);
                    VERIFY_EXPR(DstRes.CPUDescriptorHandle.ptr == SrcRes.CPUDescriptorHandle.ptr);
                }
            }

            VERIFY(DstResInfo.Attribs.IsCombinedWithSampler() == SrcResInfo.Attribs.IsCombinedWithSampler(),
                   "When source resource is combined with the sampler, destination resource must also be combined with the sampler and vice versa");
            if (DstResInfo.Attribs.IsCombinedWithSampler())
            {
#ifdef DILIGENT_DEBUG
                const auto& DstSamInfo = DstLayout.GetAssignedSampler(DstResInfo);
                const auto& SrcSamInfo = GetAssignedSampler(SrcResInfo);
                VERIFY(strcmp(SrcSamInfo.Attribs.Name, DstSamInfo.Attribs.Name) == 0, "Src sampler name ('", SrcSamInfo.Attribs.Name, "') does match the dst sampler name '(", DstSamInfo.Attribs.Name, "'). This is a bug.");
                VERIFY(DstSamInfo.Attribs.IsCompatibleWith(SrcSamInfo.Attribs), "Source sampler is incompatible with destination sampler");

                //VERIFY(!SamInfo.IsImmutableSampler(), "Immutable samplers should never be assigned space in the cache");

                VERIFY(DstSamInfo.Attribs.IsValidBindPoint(), "Sampler bind point must be valid");
                VERIFY_EXPR(DstSamInfo.Attribs.BindCount == DstResInfo.Attribs.BindCount || DstSamInfo.Attribs.BindCount == 1);
#endif
            }
        }
    }

    {
        const auto SamplerCount = DstLayout.GetSamplerCount(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
        VERIFY(GetSamplerCount(SHADER_RESOURCE_VARIABLE_TYPE_STATIC) == SamplerCount,
               "The number of static-type samplers in the source cache (", GetSamplerCount(SHADER_RESOURCE_VARIABLE_TYPE_STATIC),
               ") is not consistent with the number of static-type samplers in destination cache (", SamplerCount, ")");
        for (Uint32 s = 0; s < SamplerCount; ++s)
        {
            const auto& DstSamInfo = DstLayout.GetSampler(SHADER_RESOURCE_VARIABLE_TYPE_STATIC, s);
            const auto& SrcSamInfo = GetSampler(SHADER_RESOURCE_VARIABLE_TYPE_STATIC, s);
            VERIFY(strcmp(SrcSamInfo.Attribs.Name, DstSamInfo.Attribs.Name) == 0, "Src sampler name ('", SrcSamInfo.Attribs.Name, "') does match the dst sampler name '(", DstSamInfo.Attribs.Name, "'). This is a bug.");
            VERIFY(SrcSamInfo.Attribs.IsCompatibleWith(DstSamInfo.Attribs), "Src sampler is incompatible with the dst sampler. This is a bug.");

            // Source sampler in the static resource cache is in the root table at index 3
            // (D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER = 3)
            VERIFY(SrcSamInfo.RootIndex == static_cast<Uint32>(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER), "Unexpected root index for the source sampler. This is a bug.");
            const auto& SrcRootTable = SrcCache.GetRootTable(SrcSamInfo.RootIndex);
            auto&       DstRootTable = DstCache.GetRootTable(DstSamInfo.RootIndex);
            for (Uint32 ArrInd = 0; ArrInd < DstSamInfo.Attribs.BindCount; ++ArrInd)
            {
                const auto& SrcSampler = SrcRootTable.GetResource(SrcSamInfo.OffsetFromTableStart + ArrInd, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, GetShaderType());
                if (!SrcSampler.pObject)
                    LOG_ERROR_MESSAGE("No sampler assigned to static shader variable '", DstSamInfo.Attribs.GetPrintName(ArrInd), "' in shader '", GetShaderName(), "'.");
                auto& DstSampler = DstRootTable.GetResource(DstSamInfo.OffsetFromTableStart + ArrInd, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, GetShaderType());

                if (DstSampler.pObject != SrcSampler.pObject)
                {
                    DEV_CHECK_ERR(DstSampler.pObject == nullptr, "Static-type sampler has already been initialized, and the sampler to be assigned from the shader does not match previously assigned resource");

                    DstSampler.pObject             = SrcSampler.pObject;
                    DstSampler.Type                = SrcSampler.Type;
                    DstSampler.CPUDescriptorHandle = SrcSampler.CPUDescriptorHandle;

                    auto ShdrVisibleSamplerHeapCPUDescriptorHandle =
                        DstCache.GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(
                            DstSamInfo.RootIndex,
                            DstSamInfo.OffsetFromTableStart + ArrInd);
                    VERIFY_EXPR(ShdrVisibleSamplerHeapCPUDescriptorHandle.ptr != 0);
                    if (ShdrVisibleSamplerHeapCPUDescriptorHandle.ptr != 0)
                    {
                        m_pd3d12Device->CopyDescriptorsSimple(1, ShdrVisibleSamplerHeapCPUDescriptorHandle, SrcSampler.CPUDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
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


#ifdef DILIGENT_DEVELOPMENT
bool ShaderResourceLayoutD3D12::dvpVerifyBindings(const ShaderResourceCacheD3D12& ResourceCache) const
{
    bool BindingsOK = true;
    for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC; VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
    {
        for (Uint32 r = 0; r < GetCbvSrvUavCount(VarType); ++r)
        {
            const auto& res = GetSrvCbvUav(VarType, r);
            VERIFY(res.GetVariableType() == VarType, "Unexpected variable type");

            for (Uint32 ArrInd = 0; ArrInd < res.Attribs.BindCount; ++ArrInd)
            {
                const auto& CachedRes = ResourceCache.GetRootTable(res.RootIndex).GetResource(res.OffsetFromTableStart + ArrInd, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, GetShaderType());
                if (CachedRes.pObject)
                    VERIFY(CachedRes.Type == res.GetResType(), "Inconsistent cached resource types");
                else
                    VERIFY(CachedRes.Type == CachedResourceType::Unknown, "Unexpected cached resource types");

                if (!CachedRes.pObject ||
                    // Dynamic buffers do not have CPU descriptor handle as they do not keep D3D12 buffer, and space is allocated from the GPU ring buffer
                    CachedRes.CPUDescriptorHandle.ptr == 0 && !(CachedRes.Type == CachedResourceType::CBV && CachedRes.pObject.RawPtr<const BufferD3D12Impl>()->GetDesc().Usage == USAGE_DYNAMIC))
                {
                    LOG_ERROR_MESSAGE("No resource is bound to ", GetShaderVariableTypeLiteralName(res.GetVariableType()), " variable '", res.Attribs.GetPrintName(ArrInd), "' in shader '", GetShaderName(), "'");
                    BindingsOK = false;
                }

                if (res.Attribs.BindCount > 1 && res.Attribs.IsCombinedWithSampler())
                {
                    // Verify that if single sampler is used for all texture array elements, all samplers set in the resource views are consistent
                    const auto& SamInfo = GetAssignedSampler(res);
                    if (SamInfo.Attribs.BindCount == 1)
                    {
                        const auto& CachedSampler = ResourceCache.GetRootTable(SamInfo.RootIndex).GetResource(SamInfo.OffsetFromTableStart, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, GetShaderType());
                        // Conversion must always succeed as the type is verified when resource is bound to the variable
                        if (const auto* pTexView = CachedRes.pObject.RawPtr<const TextureViewD3D12Impl>())
                        {
                            const auto* pSampler = pTexView->GetSampler();
                            if (pSampler != nullptr && CachedSampler.pObject != nullptr && CachedSampler.pObject != pSampler)
                            {
                                LOG_ERROR_MESSAGE("All elements of texture array '", res.Attribs.Name, "' in shader '", GetShaderName(), "' share the same sampler. However, the sampler set in view for element ", ArrInd, " does not match bound sampler. This may cause incorrect behavior on GL platform.");
                            }
                        }
                    }
                }

#    ifdef DILIGENT_DEBUG
                {
                    const auto ShdrVisibleHeapCPUDescriptorHandle = ResourceCache.GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>(res.RootIndex, res.OffsetFromTableStart + ArrInd);
                    if (ResourceCache.DbgGetContentType() == ShaderResourceCacheD3D12::DbgCacheContentType::StaticShaderResources)
                    {
                        VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Static shader resources of a shader should not be assigned shader visible descriptor space");
                    }
                    else if (ResourceCache.DbgGetContentType() == ShaderResourceCacheD3D12::DbgCacheContentType::SRBResources)
                    {
                        if (res.GetResType() == CachedResourceType::CBV && res.Attribs.BindCount == 1)
                        {
                            VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Non-array constant buffers are bound as root views and should not be assigned shader visible descriptor space");
                        }
                        else
                        {
                            if (res.GetVariableType() == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
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
#    endif
            }

            if (res.Attribs.IsCombinedWithSampler())
            {
                VERIFY(res.GetResType() == CachedResourceType::TexSRV, "Sampler can only be assigned to a texture SRV");
                const auto& SamInfo = GetAssignedSampler(res);
                //VERIFY(!SamInfo.IsImmutableSampler(), "Immutable samplers should never be assigned space in the cache" );
                VERIFY(SamInfo.Attribs.IsValidBindPoint(), "Sampler bind point must be valid");

                for (Uint32 ArrInd = 0; ArrInd < SamInfo.Attribs.BindCount; ++ArrInd)
                {
                    const auto& CachedSampler = ResourceCache.GetRootTable(SamInfo.RootIndex).GetResource(SamInfo.OffsetFromTableStart + ArrInd, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, GetShaderType());
                    if (CachedSampler.pObject)
                        VERIFY(CachedSampler.Type == CachedResourceType::Sampler, "Incorrect cached sampler type");
                    else
                        VERIFY(CachedSampler.Type == CachedResourceType::Unknown, "Unexpected cached sampler type");
                    if (!CachedSampler.pObject || CachedSampler.CPUDescriptorHandle.ptr == 0)
                    {
                        LOG_ERROR_MESSAGE("No sampler is assigned to texture variable '", res.Attribs.GetPrintName(ArrInd), "' in shader '", GetShaderName(), "'");
                        BindingsOK = false;
                    }

#    ifdef DILIGENT_DEBUG
                    {
                        const auto ShdrVisibleHeapCPUDescriptorHandle = ResourceCache.GetShaderVisibleTableCPUDescriptorHandle<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER>(SamInfo.RootIndex, SamInfo.OffsetFromTableStart + ArrInd);
                        if (ResourceCache.DbgGetContentType() == ShaderResourceCacheD3D12::DbgCacheContentType::StaticShaderResources)
                        {
                            VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Static shader resources of a shader should not be assigned shader visible descriptor space");
                        }
                        else if (ResourceCache.DbgGetContentType() == ShaderResourceCacheD3D12::DbgCacheContentType::SRBResources)
                        {
                            if (SamInfo.GetVariableType() == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr == 0, "Dynamic resources of a shader resource binding should be assigned shader visible descriptor space at every draw call");
                            else
                                VERIFY(ShdrVisibleHeapCPUDescriptorHandle.ptr != 0, "Non-dynamics resources of a shader resource binding must be assigned shader visible descriptor space");
                        }
                        else
                        {
                            UNEXPECTED("Unknown content type");
                        }
                    }
#    endif
                }
            }
        }

        for (Uint32 s = 0; s < GetSamplerCount(VarType); ++s)
        {
            const auto& sam = GetSampler(VarType, s);
            VERIFY(sam.GetVariableType() == VarType, "Unexpected sampler variable type");

            for (Uint32 ArrInd = 0; ArrInd < sam.Attribs.BindCount; ++ArrInd)
            {
                const auto& CachedSampler = ResourceCache.GetRootTable(sam.RootIndex).GetResource(sam.OffsetFromTableStart + ArrInd, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, GetShaderType());
                if (CachedSampler.pObject)
                    VERIFY(CachedSampler.Type == CachedResourceType::Sampler, "Incorrect cached sampler type");
                else
                    VERIFY(CachedSampler.Type == CachedResourceType::Unknown, "Unexpected cached sampler type");
                if (!CachedSampler.pObject || CachedSampler.CPUDescriptorHandle.ptr == 0)
                {
                    LOG_ERROR_MESSAGE("No sampler is bound to sampler variable '", sam.Attribs.GetPrintName(ArrInd), "' in shader '", GetShaderName(), "'");
                    BindingsOK = false;
                }
            }
        }
    }

    return BindingsOK;
}
#endif

bool ShaderResourceLayoutD3D12::IsCompatibleWith(const ShaderResourceLayoutD3D12& ResLayout) const
{
    if (GetTotalResourceCount() != ResLayout.GetTotalResourceCount())
        return false;

    for (Uint32 i = 0; i < GetTotalResourceCount(); ++i)
    {
        const auto& lRes = GetResource(i);
        const auto& rRes = ResLayout.GetResource(i);

        if (!lRes.Attribs.IsCompatibleWith(rRes.Attribs))
            return false;
    }

    return true;
}

} // namespace Diligent
