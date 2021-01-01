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

#include "ShaderResourceLayoutVk.hpp"
#include "ShaderResourceCacheVk.hpp"
#include "BufferVkImpl.hpp"
#include "BufferViewVk.h"
#include "TextureVkImpl.hpp"
#include "TextureViewVkImpl.hpp"
#include "SamplerVkImpl.hpp"
#include "ShaderVkImpl.hpp"
#include "PipelineLayout.hpp"
#include "ShaderResourceVariableBase.hpp"
#include "StringTools.hpp"
#include "PipelineStateVkImpl.hpp"
#include "TopLevelASVkImpl.hpp"

namespace Diligent
{

static constexpr auto RAY_TRACING_SHADER_TYPES =
    SHADER_TYPE_RAY_GEN |
    SHADER_TYPE_RAY_MISS |
    SHADER_TYPE_RAY_CLOSEST_HIT |
    SHADER_TYPE_RAY_ANY_HIT |
    SHADER_TYPE_RAY_INTERSECTION |
    SHADER_TYPE_CALLABLE;

static Int32 FindImmutableSampler(SHADER_TYPE                       ShaderType,
                                  const PipelineResourceLayoutDesc& ResourceLayoutDesc,
                                  const SPIRVShaderResourceAttribs& Attribs,
                                  const char*                       SamplerSuffix)
{
    if (Attribs.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage)
    {
        SamplerSuffix = nullptr;
    }
    else if (Attribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler)
    {
        // Use SamplerSuffix. If HLSL-style combined images samplers are not used,
        // SamplerSuffix will be null and we will be looking for the sampler itself.
    }
    else
    {
        UNEXPECTED("Immutable sampler can only be assigned to a sampled image or separate sampler");
        return -1;
    }

    for (Uint32 s = 0; s < ResourceLayoutDesc.NumImmutableSamplers; ++s)
    {
        const auto& ImtblSam = ResourceLayoutDesc.ImmutableSamplers[s];
        if (((ImtblSam.ShaderStages & ShaderType) != 0) && StreqSuff(Attribs.Name, ImtblSam.SamplerOrTextureName, SamplerSuffix))
            return s;
    }

    return -1;
}

static SHADER_RESOURCE_VARIABLE_TYPE FindShaderVariableType(SHADER_TYPE                       ShaderType,
                                                            const SPIRVShaderResourceAttribs& Attribs,
                                                            const PipelineResourceLayoutDesc& ResourceLayoutDesc,
                                                            const char*                       CombinedSamplerSuffix)
{
    if (Attribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler)
    {
        // Use texture or sampler name to derive separate sampler type
        // When HLSL-style combined image samplers are not used, CombinedSamplerSuffix is null
        return GetShaderVariableType(ShaderType, ResourceLayoutDesc.DefaultVariableType, ResourceLayoutDesc.Variables, ResourceLayoutDesc.NumVariables,
                                     [&](const char* VarName) {
                                         return StreqSuff(Attribs.Name, VarName, CombinedSamplerSuffix);
                                     });
    }
    else
    {
        return GetShaderVariableType(ShaderType, Attribs.Name, ResourceLayoutDesc);
    }
}


ShaderResourceLayoutVk::ShaderStageInfo::ShaderStageInfo(const ShaderVkImpl* pShader) :
    Type{pShader->GetDesc().ShaderType},
    Shaders{pShader},
    SPIRVs{pShader->GetSPIRV()}
{
}

void ShaderResourceLayoutVk::ShaderStageInfo::Append(const ShaderVkImpl* pShader)
{
    VERIFY_EXPR(pShader != nullptr);
    VERIFY(std::find(Shaders.begin(), Shaders.end(), pShader) == Shaders.end(),
           "Shader '", pShader->GetDesc().Name, "' already exists in the stage. Shaders must be deduplicated.");

    const auto NewShaderType = pShader->GetDesc().ShaderType;
    if (Type == SHADER_TYPE_UNKNOWN)
    {
        VERIFY_EXPR(Shaders.empty() && SPIRVs.empty());
        Type = NewShaderType;
    }
    else
    {
        VERIFY(Type == NewShaderType, "The type (", GetShaderTypeLiteralName(NewShaderType),
               ") of shader '", pShader->GetDesc().Name, "' being added to the stage is incosistent with the stage type (",
               GetShaderTypeLiteralName(Type), ").");
    }
    Shaders.push_back(pShader);
    SPIRVs.push_back(pShader->GetSPIRV());
}

size_t ShaderResourceLayoutVk::ShaderStageInfo::Count() const
{
    VERIFY_EXPR(Shaders.size() == SPIRVs.size());
    return Shaders.size();
}


ShaderResourceLayoutVk::~ShaderResourceLayoutVk()
{
    for (Uint32 r = 0; r < GetTotalResourceCount(); ++r)
        GetResource(r).~VkResource();

    for (Uint32 s = 0; s < m_NumImmutableSamplers; ++s)
        GetImmutableSampler(s).~ImmutableSamplerPtrType();
}

StringPool ShaderResourceLayoutVk::AllocateMemory(const std::vector<const ShaderVkImpl*>& Shaders,
                                                  IMemoryAllocator&                       Allocator,
                                                  const PipelineResourceLayoutDesc&       ResourceLayoutDesc,
                                                  const SHADER_RESOURCE_VARIABLE_TYPE*    AllowedVarTypes,
                                                  Uint32                                  NumAllowedTypes,
                                                  ResourceNameToIndex_t&                  UniqueNames,
                                                  bool                                    AllocateImmutableSamplers)
{
    VERIFY(!m_ResourceBuffer, "Memory has already been initialized");
    VERIFY_EXPR(Shaders.size() > 0);
    VERIFY_EXPR(m_ShaderType == SHADER_TYPE_UNKNOWN);

    m_ShaderType                 = Shaders[0]->GetDesc().ShaderType;
    m_IsUsingSeparateSamplers    = !Shaders[0]->GetShaderResources()->IsUsingCombinedSamplers();
    const Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);

    // Construct shader or shader group name
    const auto ShaderName = GetShaderGroupName(Shaders);

    size_t StringPoolSize = StringPool::GetRequiredReserveSize(ShaderName);

    // Count the number of resources to allocate all needed memory
    for (size_t s = 0; s < Shaders.size(); ++s)
    {
        const auto& Resources             = *Shaders[s]->GetShaderResources();
        const auto* CombinedSamplerSuffix = Resources.GetCombinedSamplerSuffix();
        VERIFY(Resources.GetShaderType() == m_ShaderType, "Unexpected shader type");
        VERIFY(m_IsUsingSeparateSamplers == !Resources.IsUsingCombinedSamplers(), "All shaders in the stage must either use or not use combined image samplers");

        Resources.ProcessResources(
            [&](const SPIRVShaderResourceAttribs& ResAttribs, Uint32) //
            {
                auto VarType = FindShaderVariableType(m_ShaderType, ResAttribs, ResourceLayoutDesc, CombinedSamplerSuffix);
                if (IsAllowedType(VarType, AllowedTypeBits))
                {
                    bool IsNewResource = UniqueNames.emplace(HashMapStringKey{ResAttribs.Name}, Uint32{InvalidResourceIndex}).second;
                    if (IsNewResource)
                    {
                        StringPoolSize += StringPool::GetRequiredReserveSize(ResAttribs.Name);

                        // For immutable separate samplers we still allocate VkResource instances, but they are never exposed to the app

                        VERIFY(Uint32{m_NumResources[VarType]} + 1 <= Uint32{std::numeric_limits<Uint16>::max()}, "Number of resources exceeds Uint16 maximum representable value");
                        ++m_NumResources[VarType];
                    }
                }
            } //
        );
    }

    Uint32 TotalResources = 0;
    for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC; VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
    {
        TotalResources += m_NumResources[VarType];
    }
    VERIFY(TotalResources <= Uint32{std::numeric_limits<Uint16>::max()}, "Total number of resources exceeds Uint16 maximum representable value");
    m_NumResources[SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES] = static_cast<Uint16>(TotalResources);

    m_NumImmutableSamplers = 0;
    if (AllocateImmutableSamplers)
    {
        // Reserve space for all immutable samplers that may potentially be used in this
        // shader stage. Note that not all samplers may actually be used/initialized.
        for (Uint32 s = 0; s < ResourceLayoutDesc.NumImmutableSamplers; ++s)
        {
            const auto& ImtblSamDesc = ResourceLayoutDesc.ImmutableSamplers[s];
            if ((ImtblSamDesc.ShaderStages & m_ShaderType) != 0)
                ++m_NumImmutableSamplers;
        }
    }

    FixedLinearAllocator MemPool{Allocator};

    MemPool.AddSpace<VkResource>(TotalResources);
    MemPool.AddSpace<ImmutableSamplerPtrType>(m_NumImmutableSamplers);
    MemPool.AddSpace<char>(StringPoolSize);

    MemPool.Reserve();

    auto* pResources     = MemPool.Allocate<VkResource>(TotalResources);
    auto* pImtblSamplers = MemPool.ConstructArray<ImmutableSamplerPtrType>(m_NumImmutableSamplers);
    auto* pStringData    = MemPool.ConstructArray<char>(StringPoolSize);

    m_ResourceBuffer = std::unique_ptr<void, STDDeleterRawMem<void>>(MemPool.Release(), Allocator);

    VERIFY_EXPR(pResources == nullptr || m_ResourceBuffer.get() == pResources);
    VERIFY_EXPR(pImtblSamplers == nullptr || pImtblSamplers == std::addressof(GetImmutableSampler(0)));
    VERIFY_EXPR(pStringData == GetStringPoolData());

    StringPool stringPool;
    stringPool.AssignMemory(pStringData, StringPoolSize);
    stringPool.CopyString(ShaderName);
    return stringPool;
}


static Uint32 FindAssignedSampler(const ShaderResourceLayoutVk&     Layout,
                                  const SPIRVShaderResources&       Resources,
                                  const SPIRVShaderResourceAttribs& SepImg,
                                  Uint32                            CurrResourceCount,
                                  SHADER_RESOURCE_VARIABLE_TYPE     ImgVarType)
{
    using VkResource = ShaderResourceLayoutVk::VkResource;
    VERIFY_EXPR(SepImg.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage);

    Uint32 SamplerInd = VkResource::InvalidSamplerInd;
    if (Resources.IsUsingCombinedSamplers() && SepImg.IsValidSepSamplerAssigned())
    {
        const auto& SepSampler = Resources.GetAssignedSepSampler(SepImg);
        for (SamplerInd = 0; SamplerInd < CurrResourceCount; ++SamplerInd)
        {
            const auto& Res = Layout.GetResource(ImgVarType, SamplerInd);
            if (Res.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler &&
                strcmp(Res.Name, SepSampler.Name) == 0)
            {
                VERIFY(ImgVarType == Res.GetVariableType(),
                       "The type (", GetShaderVariableTypeLiteralName(ImgVarType), ") of separate image variable '", SepImg.Name,
                       "' is not consistent with the type (", GetShaderVariableTypeLiteralName(Res.GetVariableType()),
                       ") of the separate sampler '", SepSampler.Name,
                       "' that is assigned to it. "
                       "This should never happen as when HLSL-style combined texture samplers are used, the type of the sampler "
                       "is derived from the type of the corresponding separate image.");
                break;
            }
        }
        if (SamplerInd == CurrResourceCount)
        {
            LOG_ERROR("Unable to find separate sampler '", SepSampler.Name, "' assigned to separate image '",
                      SepImg.Name, "' in the list of already created resources. This seems to be a bug.");
            SamplerInd = VkResource::InvalidSamplerInd;
        }
    }
    return SamplerInd;
}

static void VerifyResourceMerge(const ShaderResourceLayoutVk::VkResource& ExistingRes,
                                const SPIRVShaderResourceAttribs&         NewResAttribs,
                                SHADER_RESOURCE_VARIABLE_TYPE             VarType)
{
    VERIFY(ExistingRes.VariableType == VarType,
           "The type of variable '", NewResAttribs.Name, "' does not match the type determined for previous shaders. This appears to be a bug.");

    DEV_CHECK_ERR(ExistingRes.Type == NewResAttribs.Type,
                  "Shader variable '", NewResAttribs.Name,
                  "' exists in multiple shaders from the same shader stage, but its type is not consistent between "
                  "shaders. All variables with the same name from the same shader stage must have the same type.");

    DEV_CHECK_ERR(ExistingRes.ResourceDim == NewResAttribs.ResourceDim,
                  "Shader variable '", NewResAttribs.Name,
                  "' exists in multiple shaders from the same shader stage, but its resource dimension is not consistent between "
                  "shaders. All variables with the same name from the same shader stage must have the same resource dimension.");

    DEV_CHECK_ERR(ExistingRes.ArraySize == NewResAttribs.ArraySize,
                  "Shader variable '", NewResAttribs.Name,
                  "' exists in multiple shaders from the same shader stage, but its array size is not consistent between "
                  "shaders. All variables with the same name from the same shader stage must have the same array size.");

    DEV_CHECK_ERR(ExistingRes.IsMS == NewResAttribs.IsMS,
                  "Shader variable '", NewResAttribs.Name,
                  "' exists in multiple shaders from the same shader stage, but its multisample flag is not consistent between "
                  "shaders. All variables with the same name from the same shader stage must either be multisample or non-multisample.");
}

void ShaderResourceLayoutVk::InitializeStaticResourceLayout(const std::vector<const ShaderVkImpl*>& Shaders,
                                                            IMemoryAllocator&                       LayoutDataAllocator,
                                                            const PipelineResourceLayoutDesc&       ResourceLayoutDesc,
                                                            ShaderResourceCacheVk&                  StaticResourceCache)
{
    const auto   AllowedVarType  = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    const Uint32 AllowedTypeBits = GetAllowedTypeBits(&AllowedVarType, 1);

    // A mapping from the resource name to its index in m_ResourceBuffer that is used
    // to de-duplicate shader resources.
    ResourceNameToIndex_t ResourceNameToIndex;

    // We do not need immutable samplers in static shader resource layout as they
    // are relevant only when the main layout is initialized
    constexpr bool AllocateImmutableSamplers = false;

    auto stringPool = AllocateMemory(Shaders, LayoutDataAllocator, ResourceLayoutDesc, &AllowedVarType, 1, ResourceNameToIndex, AllocateImmutableSamplers);

    std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES> CurrResInd = {};

    Uint32 StaticResCacheSize = 0;
    for (const auto* pShader : Shaders)
    {
        const auto& Resources             = *pShader->GetShaderResources();
        const auto* CombinedSamplerSuffix = Resources.GetCombinedSamplerSuffix();
        Resources.ProcessResources(
            [&](const SPIRVShaderResourceAttribs& Attribs, Uint32) //
            {
                auto VarType = FindShaderVariableType(m_ShaderType, Attribs, ResourceLayoutDesc, CombinedSamplerSuffix);
                if (!IsAllowedType(VarType, AllowedTypeBits))
                    return;

                auto ResIter = ResourceNameToIndex.find(HashMapStringKey{Attribs.Name});
                VERIFY_EXPR(ResIter != ResourceNameToIndex.end());

                if (ResIter->second == InvalidResourceIndex)
                {
                    Int32 SrcImmutableSamplerInd = -1;
                    if (Attribs.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage ||
                        Attribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler)
                    {
                        // Only search for the immutable sampler for combined image samplers and separate samplers
                        SrcImmutableSamplerInd = FindImmutableSampler(m_ShaderType, ResourceLayoutDesc, Attribs, CombinedSamplerSuffix);
                        // NB: for immutable separate samplers we still allocate VkResource instances to be compliant with the main
                        //     layout, but they are never initialized or exposed to the app.
                    }

                    Uint32 Binding       = Uint32{Attribs.Type};
                    Uint32 DescriptorSet = 0;
                    Uint32 CacheOffset   = StaticResCacheSize;
                    StaticResCacheSize += Attribs.ArraySize;

                    Uint32 SamplerInd = VkResource::InvalidSamplerInd;
                    if (Attribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage)
                    {
                        // Separate samplers are enumerated before separate images, so the sampler
                        // assigned to this separate image must have already been created.
                        SamplerInd = FindAssignedSampler(*this, Resources, Attribs, CurrResInd[VarType], VarType);
                    }

                    // add new resource
                    ResIter->second = CurrResInd[VarType];
                    ::new (&GetResource(VarType, CurrResInd[VarType]++)) VkResource //
                        {
                            *this,
                            stringPool.CopyString(Attribs.Name),
                            Attribs,
                            VarType,
                            Binding,
                            DescriptorSet,
                            CacheOffset,
                            SamplerInd,
                            SrcImmutableSamplerInd >= 0 //
                        };
                }
                else
                {
                    // Merge with existing
                    VerifyResourceMerge(GetResource(VarType, ResIter->second), Attribs, VarType);
                }
            } //
        );
    }

#ifdef DILIGENT_DEBUG
    for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC; VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
    {
        VERIFY(CurrResInd[VarType] == m_NumResources[VarType], "Not all resources have been initialized, which will cause a crash when dtor is called");
    }

    VERIFY_EXPR(stringPool.GetRemainingSize() == 0);
#endif

    StaticResourceCache.InitializeSets(GetRawAllocator(), 1, &StaticResCacheSize);
    InitializeResourceMemoryInCache(StaticResourceCache);
#ifdef DILIGENT_DEBUG
    StaticResourceCache.DbgVerifyResourceInitialization();
#endif
}

#ifdef DILIGENT_DEVELOPMENT
void ShaderResourceLayoutVk::dvpVerifyResourceLayoutDesc(const TShaderStages&              ShaderStages,
                                                         const PipelineResourceLayoutDesc& ResourceLayoutDesc,
                                                         bool                              VerifyVariables,
                                                         bool                              VerifyImmutableSamplers)
{
    auto GetAllowedShadersString = [&](SHADER_TYPE Stages) //
    {
        std::string ShadersStr;
        while (Stages != SHADER_TYPE_UNKNOWN)
        {
            const auto ShaderType = Stages & static_cast<SHADER_TYPE>(~(static_cast<Uint32>(Stages) - 1));
            String     ShaderName;

            for (const auto& StageInfo : ShaderStages)
            {
                if ((Stages & StageInfo.Type) != 0)
                {
                    ShaderName = GetShaderGroupName(StageInfo.Shaders);
                    break;
                }
            }

            if (!ShadersStr.empty())
                ShadersStr.append(", ");
            ShadersStr.append(GetShaderTypeLiteralName(ShaderType));
            ShadersStr.append(" (");
            if (ShaderName.size())
            {
                ShadersStr.push_back('\'');
                ShadersStr.append(ShaderName);
                ShadersStr.push_back('\'');
            }
            else
            {
                ShadersStr.append("Not enabled in PSO");
            }
            ShadersStr.append(")");

            Stages &= ~ShaderType;
        }
        return ShadersStr;
    };

    if (VerifyVariables)
    {
        for (Uint32 v = 0; v < ResourceLayoutDesc.NumVariables; ++v)
        {
            const auto& VarDesc = ResourceLayoutDesc.Variables[v];
            if (VarDesc.ShaderStages == SHADER_TYPE_UNKNOWN)
            {
                LOG_WARNING_MESSAGE("No allowed shader stages are specified for ", GetShaderVariableTypeLiteralName(VarDesc.Type), " variable '", VarDesc.Name, "'.");
                continue;
            }

            bool VariableFound = false;
            for (size_t s = 0; s < ShaderStages.size() && !VariableFound; ++s)
            {
                const auto& Stage = ShaderStages[s];
                if ((Stage.Type & VarDesc.ShaderStages) == 0)
                    continue;

                for (size_t i = 0; i < Stage.Shaders.size() && !VariableFound; ++i)
                {
                    const auto& Resources = *Stage.Shaders[i]->GetShaderResources();
                    VERIFY_EXPR(Resources.GetShaderType() == Stage.Type);

                    for (Uint32 res = 0; res < Resources.GetTotalResources() && !VariableFound; ++res)
                    {
                        const auto& ResAttribs = Resources.GetResource(res);
                        VariableFound          = (strcmp(ResAttribs.Name, VarDesc.Name) == 0);
                    }
                }
            }
            if (!VariableFound)
            {
                LOG_WARNING_MESSAGE(GetShaderVariableTypeLiteralName(VarDesc.Type), " variable '", VarDesc.Name,
                                    "' is not found in any of the designated shader stages: ",
                                    GetAllowedShadersString(VarDesc.ShaderStages));
            }
        }
    }

    if (VerifyImmutableSamplers)
    {
        for (Uint32 sam = 0; sam < ResourceLayoutDesc.NumImmutableSamplers; ++sam)
        {
            const auto& ImtblSamDesc = ResourceLayoutDesc.ImmutableSamplers[sam];
            if (ImtblSamDesc.ShaderStages == SHADER_TYPE_UNKNOWN)
            {
                LOG_WARNING_MESSAGE("No allowed shader stages are specified for immutable sampler '", ImtblSamDesc.SamplerOrTextureName, "'.");
                continue;
            }

            bool SamplerFound = false;
            for (size_t s = 0; s < ShaderStages.size() && !SamplerFound; ++s)
            {
                const auto& Stage = ShaderStages[s];
                if ((Stage.Type & ImtblSamDesc.ShaderStages) == 0)
                    continue;

                for (size_t j = 0; j < Stage.Shaders.size() && !SamplerFound; ++j)
                {
                    const auto& Resources = *Stage.Shaders[j]->GetShaderResources();
                    VERIFY_EXPR(Resources.GetShaderType() == Stage.Type);

                    // Irrespective of whether HLSL-style combined image samplers are used,
                    // an immutable sampler can be assigned to GLSL sampled image (i.e. sampler2D g_tex)
                    for (Uint32 i = 0; i < Resources.GetNumSmpldImgs() && !SamplerFound; ++i)
                    {
                        const auto& SmplImg = Resources.GetSmpldImg(i);
                        SamplerFound        = (strcmp(SmplImg.Name, ImtblSamDesc.SamplerOrTextureName) == 0);
                    }

                    if (!SamplerFound)
                    {
                        // Check if immutable is assigned to a separate sampler.
                        // In case HLSL-style combined image samplers are used, the condition is  SepSmpl.Name == "g_Texture" + "_sampler".
                        // Otherwise the condition is  SepSmpl.Name == "g_Texture_sampler" + "".
                        const auto* CombinedSamplerSuffix = Resources.GetCombinedSamplerSuffix();
                        for (Uint32 i = 0; i < Resources.GetNumSepSmplrs() && !SamplerFound; ++i)
                        {
                            const auto& SepSmpl = Resources.GetSepSmplr(i);
                            SamplerFound        = StreqSuff(SepSmpl.Name, ImtblSamDesc.SamplerOrTextureName, CombinedSamplerSuffix);
                        }
                    }
                }
            }

            if (!SamplerFound)
            {
                LOG_WARNING_MESSAGE("Immutable sampler '", ImtblSamDesc.SamplerOrTextureName,
                                    "' is not found in any of the designated shader stages: ",
                                    GetAllowedShadersString(ImtblSamDesc.ShaderStages));
            }
        }
    }
}
#endif

void ShaderResourceLayoutVk::Initialize(IRenderDevice*                    pRenderDevice,
                                        TShaderStages&                    ShaderStages,
                                        ShaderResourceLayoutVk            Layouts[],
                                        IMemoryAllocator&                 LayoutDataAllocator,
                                        const PipelineResourceLayoutDesc& ResourceLayoutDesc,
                                        class PipelineLayout&             PipelineLayout,
                                        bool                              VerifyVariables,
                                        bool                              VerifyImmutableSamplers)
{
#ifdef DILIGENT_DEVELOPMENT
    dvpVerifyResourceLayoutDesc(ShaderStages, ResourceLayoutDesc, VerifyVariables, VerifyImmutableSamplers);
#endif

    // Mappings from resource name to its index, for every shader stage
    std::array<ResourceNameToIndex_t, MAX_SHADERS_IN_PIPELINE> ResourceNameToIndexArray;

    constexpr bool AllocateImmutableSamplers = true;

    std::vector<StringPool> stringPools;
    stringPools.reserve(ShaderStages.size());
    for (size_t s = 0; s < ShaderStages.size(); ++s)
    {
        stringPools.emplace_back(
            Layouts[s].AllocateMemory(ShaderStages[s].Shaders, LayoutDataAllocator, ResourceLayoutDesc,
                                      nullptr, 0, ResourceNameToIndexArray[s], AllocateImmutableSamplers));
    }

    // Current resource index, for every variable type in every shader stage
    std::array<std::array<Uint32, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES>, MAX_SHADERS_IN_PIPELINE> CurrResInd = {};
    // Current immutable sampler index, for every shader stage
    std::array<Uint32, MAX_SHADERS_IN_PIPELINE> CurrImmutableSamplerInd = {};

#ifdef DILIGENT_DEBUG
    std::unordered_map<Uint32, std::pair<Uint32, Uint32>> dbgBindings_CacheOffsets;
#endif

    auto AddResource = [&](const Uint32                      ShaderStageInd,
                           const SPIRVShaderResources&       Resources,
                           const SPIRVShaderResourceAttribs& Attribs,
                           std::vector<uint32_t>&            SPIRV) //
    {
        auto& ResourceNameToIndex = ResourceNameToIndexArray[ShaderStageInd];

        auto ResIter = ResourceNameToIndex.find(HashMapStringKey{Attribs.Name});
        VERIFY(ResIter != ResourceNameToIndex.end(), "Resource '", Attribs.Name,
               "' is not found in ResourceNameToIndex map. This is a bug as the resource must have been processed by AllocateMemory and added to the map.");

        const auto ShaderType = Resources.GetShaderType();
        const auto VarType    = FindShaderVariableType(ShaderType, Attribs, ResourceLayoutDesc, Resources.GetCombinedSamplerSuffix());

        auto& ResLayout = Layouts[ShaderStageInd];

        const VkResource* pResource = nullptr;
        if (ResIter->second == InvalidResourceIndex)
        {
            // add new resource
            Uint32 Binding       = 0;
            Uint32 DescriptorSet = 0;
            Uint32 CacheOffset   = 0;
            Uint32 SamplerInd    = VkResource::InvalidSamplerInd;

            if (Attribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage)
            {
                // Separate samplers are enumerated before separate images, so the sampler
                // assigned to this separate image must have already been created.
                SamplerInd = FindAssignedSampler(ResLayout, Resources, Attribs, CurrResInd[ShaderStageInd][VarType], VarType);
            }

            VkSampler vkImmutableSampler = VK_NULL_HANDLE;
            if (Attribs.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage ||
                Attribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler)
            {
                // Only search for the immutable sampler for combined image samplers and separate samplers
                Int32 SrcImmutableSamplerInd = FindImmutableSampler(ShaderType, ResourceLayoutDesc, Attribs, Resources.GetCombinedSamplerSuffix());
                if (SrcImmutableSamplerInd >= 0)
                {
                    // NB: for immutable separate samplers we still allocate VkResource instances, but they are never exposed to the app

                    // We reserve enough space for the maximum number of immutable samplers that may be used in the stage,
                    // but not all of them will necessarily be initialized.
                    auto& ImmutableSampler = ResLayout.GetImmutableSampler(CurrImmutableSamplerInd[ShaderStageInd]++);
                    VERIFY(!ImmutableSampler, "Immutable sampler has already been initialized. This is unexpected "
                                              "as all resources are deduplicated and should only be initialized once.");
                    const auto& ImmutableSamplerDesc = ResourceLayoutDesc.ImmutableSamplers[SrcImmutableSamplerInd].Desc;
                    pRenderDevice->CreateSampler(ImmutableSamplerDesc, &ImmutableSampler);
                    vkImmutableSampler = ImmutableSampler.RawPtr<SamplerVkImpl>()->GetVkSampler();
                }
            }

            PipelineLayout.AllocateResourceSlot(Attribs, VarType, vkImmutableSampler, Resources.GetShaderType(), DescriptorSet, Binding, CacheOffset);
            VERIFY(DescriptorSet <= std::numeric_limits<decltype(VkResource::DescriptorSet)>::max(), "Descriptor set (", DescriptorSet, ") excceeds maximum representable value");
            VERIFY(Binding <= std::numeric_limits<decltype(VkResource::Binding)>::max(), "Binding (", Binding, ") excceeds maximum representable value");

#ifdef DILIGENT_DEBUG
            // Verify that bindings and cache offsets monotonically increase in every descriptor set
            auto Binding_OffsetIt = dbgBindings_CacheOffsets.find(DescriptorSet);
            if (Binding_OffsetIt != dbgBindings_CacheOffsets.end())
            {
                VERIFY(Binding > Binding_OffsetIt->second.first, "Binding for descriptor set ", DescriptorSet, " is not strictly monotonic");
                VERIFY(CacheOffset > Binding_OffsetIt->second.second, "Cache offset for descriptor set ", DescriptorSet, " is not strictly monotonic");
            }
            dbgBindings_CacheOffsets[DescriptorSet] = std::make_pair(Binding, CacheOffset);
#endif

            auto& ResInd    = CurrResInd[ShaderStageInd][VarType];
            ResIter->second = ResInd;

            pResource = ::new (&ResLayout.GetResource(VarType, ResInd++)) VkResource //
                {
                    ResLayout,
                    stringPools[ShaderStageInd].CopyString(Attribs.Name),
                    Attribs,
                    VarType,
                    Binding,
                    DescriptorSet,
                    CacheOffset,
                    SamplerInd,
                    vkImmutableSampler != VK_NULL_HANDLE //
                };
        }
        else
        {
            // merge with existing
            pResource = &ResLayout.GetResource(VarType, ResIter->second);
            VerifyResourceMerge(*pResource, Attribs, VarType);
        }
        VERIFY_EXPR(pResource != nullptr);
        SPIRV[Attribs.BindingDecorationOffset]       = pResource->Binding;
        SPIRV[Attribs.DescriptorSetDecorationOffset] = pResource->DescriptorSet;
    };

    // First process uniform buffers for ALL shader stages to make sure all UBs go first in every descriptor set
    for (size_t s = 0; s < ShaderStages.size(); ++s)
    {
        auto& Shaders = ShaderStages[s].Shaders;
        for (size_t i = 0; i < Shaders.size(); ++i)
        {
            auto& SPIRV     = ShaderStages[s].SPIRVs[i];
            auto& Resources = *Shaders[i]->GetShaderResources();
            for (Uint32 n = 0; n < Resources.GetNumUBs(); ++n)
            {
                const auto& UB = Resources.GetUB(n);
                AddResource(static_cast<Uint32>(s), Resources, UB, SPIRV);
            }
        }
    }

    // Second, process all storage buffers in all shader stages
    for (size_t s = 0; s < ShaderStages.size(); ++s)
    {
        auto& Shaders = ShaderStages[s].Shaders;
        for (size_t i = 0; i < Shaders.size(); ++i)
        {
            auto& Resources = *Shaders[i]->GetShaderResources();
            auto& SPIRV     = ShaderStages[s].SPIRVs[i];
            for (Uint32 n = 0; n < Resources.GetNumSBs(); ++n)
            {
                const auto& SB = Resources.GetSB(n);
                AddResource(static_cast<Uint32>(s), Resources, SB, SPIRV);
            }
        }
    }

    // Finally, process all other resource types
    for (size_t s = 0; s < ShaderStages.size(); ++s)
    {
        auto& Shaders = ShaderStages[s].Shaders;
        for (size_t i = 0; i < Shaders.size(); ++i)
        {
            auto& Resources = *Shaders[i]->GetShaderResources();
            auto& SPIRV     = ShaderStages[s].SPIRVs[i];
            // clang-format off
            Resources.ProcessResources(
                [&](const SPIRVShaderResourceAttribs& UB, Uint32)
                {
                    VERIFY_EXPR(UB.Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer);
                    // Skip
                },
                [&](const SPIRVShaderResourceAttribs& SB, Uint32)
                {
                    VERIFY_EXPR(SB.Type == SPIRVShaderResourceAttribs::ResourceType::ROStorageBuffer || SB.Type == SPIRVShaderResourceAttribs::ResourceType::RWStorageBuffer);
                    // Skip
                },
                [&](const SPIRVShaderResourceAttribs& Img, Uint32)
                {
                    VERIFY_EXPR(Img.Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage || Img.Type == SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer);
                    AddResource(static_cast<Uint32>(s), Resources, Img, SPIRV);
                },
                [&](const SPIRVShaderResourceAttribs& SmplImg, Uint32)
                {
                    VERIFY_EXPR(SmplImg.Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage || SmplImg.Type == SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer);
                    AddResource(static_cast<Uint32>(s), Resources, SmplImg, SPIRV);
                },
                [&](const SPIRVShaderResourceAttribs& AC, Uint32)
                {
                    VERIFY_EXPR(AC.Type == SPIRVShaderResourceAttribs::ResourceType::AtomicCounter);
                    AddResource(static_cast<Uint32>(s), Resources, AC, SPIRV);
                },
                [&](const SPIRVShaderResourceAttribs& SepSmpl, Uint32)
                {
                    VERIFY_EXPR(SepSmpl.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler);
                    AddResource(static_cast<Uint32>(s), Resources, SepSmpl, SPIRV);
                },
                [&](const SPIRVShaderResourceAttribs& SepImg, Uint32)
                {
                    VERIFY_EXPR(SepImg.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage || SepImg.Type == SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer);
                    AddResource(static_cast<Uint32>(s), Resources, SepImg, SPIRV);
                },
                [&](const SPIRVShaderResourceAttribs& InputAtt, Uint32)
                {
                    VERIFY_EXPR(InputAtt.Type == SPIRVShaderResourceAttribs::ResourceType::InputAttachment);
                    AddResource(static_cast<Uint32>(s), Resources, InputAtt, SPIRV);
                },
                [&](const SPIRVShaderResourceAttribs& AccelStruct, Uint32)
                {
                    VERIFY_EXPR(AccelStruct.Type == SPIRVShaderResourceAttribs::ResourceType::AccelerationStructure);
                    AddResource(static_cast<Uint32>(s), Resources, AccelStruct, SPIRV);
                }
            );
            // clang-format on
        }
    }

#ifdef DILIGENT_DEBUG
    for (size_t s = 0; s < ShaderStages.size(); ++s)
    {
        auto& Layout = Layouts[s];
        for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC; VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
        {
            VERIFY(CurrResInd[s][VarType] == Layout.m_NumResources[VarType], "Not all resources have been initialized, which will cause a crash when dtor is called. This is a bug.");
        }
        // Some immutable samplers may never be initialized if they are not present in shaders
        VERIFY_EXPR(CurrImmutableSamplerInd[s] <= Layout.m_NumImmutableSamplers);

        VERIFY_EXPR(stringPools[s].GetRemainingSize() == 0);
    }
#endif
}


void ShaderResourceLayoutVk::VkResource::UpdateDescriptorHandle(VkDescriptorSet                                     vkDescrSet,
                                                                uint32_t                                            ArrayElement,
                                                                const VkDescriptorImageInfo*                        pImageInfo,
                                                                const VkDescriptorBufferInfo*                       pBufferInfo,
                                                                const VkBufferView*                                 pTexelBufferView,
                                                                const VkWriteDescriptorSetAccelerationStructureKHR* pAccelStructInfo) const
{
    VERIFY_EXPR(vkDescrSet != VK_NULL_HANDLE);

    VkWriteDescriptorSet WriteDescrSet;
    WriteDescrSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    WriteDescrSet.pNext           = pAccelStructInfo;
    WriteDescrSet.dstSet          = vkDescrSet;
    WriteDescrSet.dstBinding      = Binding;
    WriteDescrSet.dstArrayElement = ArrayElement;
    WriteDescrSet.descriptorCount = 1;
    // descriptorType must be the same type as that specified in VkDescriptorSetLayoutBinding for dstSet at dstBinding.
    // The type of the descriptor also controls which array the descriptors are taken from. (13.2.4)
    WriteDescrSet.descriptorType   = PipelineLayout::GetVkDescriptorType(Type);
    WriteDescrSet.pImageInfo       = pImageInfo;
    WriteDescrSet.pBufferInfo      = pBufferInfo;
    WriteDescrSet.pTexelBufferView = pTexelBufferView;

    ParentResLayout.m_LogicalDevice.UpdateDescriptorSets(1, &WriteDescrSet, 0, nullptr);
}

template <typename ObjectType, typename TPreUpdateObject>
bool ShaderResourceLayoutVk::VkResource::UpdateCachedResource(ShaderResourceCacheVk::Resource& DstRes,
                                                              RefCntAutoPtr<ObjectType>&&      pObject,
                                                              TPreUpdateObject                 PreUpdateObject) const
{
    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    if (pObject)
    {
        if (GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as writing descriptors while they are used by the GPU is an undefined behavior
            return false;
        }

        PreUpdateObject(DstRes.pObject.template RawPtr<const ObjectType>(), pObject.template RawPtr<const ObjectType>());
        DstRes.pObject.Attach(pObject.Detach());
        return true;
    }
    else
    {
        return false;
    }
}

void ShaderResourceLayoutVk::VkResource::CacheUniformBuffer(IDeviceObject*                   pBuffer,
                                                            ShaderResourceCacheVk::Resource& DstRes,
                                                            VkDescriptorSet                  vkDescrSet,
                                                            Uint32                           ArrayInd,
                                                            Uint16&                          DynamicBuffersCounter) const
{
    VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::UniformBuffer, "Uniform buffer resource is expected");
    RefCntAutoPtr<BufferVkImpl> pBufferVk{pBuffer, IID_BufferVk};
#ifdef DILIGENT_DEVELOPMENT
    VerifyConstantBufferBinding(*this, GetVariableType(), ArrayInd, pBuffer, pBufferVk.RawPtr(), DstRes.pObject.RawPtr(), ParentResLayout.GetShaderName());

    if (pBufferVk->GetDesc().uiSizeInBytes < BufferStaticSize)
    {
        // It is OK if robustBufferAccess feature is enabled, otherwise access outside of buffer range may lead to crash or undefined behavior.
        LOG_WARNING_MESSAGE("Error binding uniform buffer '", pBufferVk->GetDesc().Name, "' to shader variable '",
                            Name, "' in shader '", ParentResLayout.GetShaderName(), "': buffer size in the shader (",
                            BufferStaticSize, ") is incompatible with the actual buffer size (", pBufferVk->GetDesc().uiSizeInBytes, ").");
    }
#endif

    auto UpdateDynamicBuffersCounter = [&DynamicBuffersCounter](const BufferVkImpl* pOldBuffer, const BufferVkImpl* pNewBuffer) {
        if (pOldBuffer != nullptr && pOldBuffer->GetDesc().Usage == USAGE_DYNAMIC)
        {
            VERIFY(DynamicBuffersCounter > 0, "Dynamic buffers counter must be greater than zero when there is at least one dynamic buffer bound in the resource cache");
            --DynamicBuffersCounter;
        }
        if (pNewBuffer != nullptr && pNewBuffer->GetDesc().Usage == USAGE_DYNAMIC)
            ++DynamicBuffersCounter;
    };
    if (UpdateCachedResource(DstRes, std::move(pBufferVk), UpdateDynamicBuffersCounter))
    {
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC descriptor type require
        // buffer to be created with VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT

        // Do not update descriptor for a dynamic uniform buffer. All dynamic resource
        // descriptors are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorBufferInfo DescrBuffInfo = DstRes.GetUniformBufferDescriptorWriteInfo();
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, nullptr, &DescrBuffInfo, nullptr);
        }
    }
}

void ShaderResourceLayoutVk::VkResource::CacheStorageBuffer(IDeviceObject*                   pBufferView,
                                                            ShaderResourceCacheVk::Resource& DstRes,
                                                            VkDescriptorSet                  vkDescrSet,
                                                            Uint32                           ArrayInd,
                                                            Uint16&                          DynamicBuffersCounter) const
{
    // clang-format off
    VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::ROStorageBuffer || 
           Type == SPIRVShaderResourceAttribs::ResourceType::RWStorageBuffer,
           "Storage buffer resource is expected");
    // clang-format on

    RefCntAutoPtr<BufferViewVkImpl> pBufferViewVk{pBufferView, IID_BufferViewVk};
#ifdef DILIGENT_DEVELOPMENT
    {
        // HLSL buffer SRVs are mapped to storge buffers in GLSL
        auto RequiredViewType = Type == SPIRVShaderResourceAttribs::ResourceType::ROStorageBuffer ? BUFFER_VIEW_SHADER_RESOURCE : BUFFER_VIEW_UNORDERED_ACCESS;
        VerifyResourceViewBinding(*this, GetVariableType(), ArrayInd, pBufferView, pBufferViewVk.RawPtr(), {RequiredViewType}, DstRes.pObject.RawPtr(), ParentResLayout.GetShaderName());
        if (pBufferViewVk != nullptr)
        {
            const auto& ViewDesc = pBufferViewVk->GetDesc();
            const auto& BuffDesc = pBufferViewVk->GetBuffer()->GetDesc();
            if (BuffDesc.Mode != BUFFER_MODE_STRUCTURED && BuffDesc.Mode != BUFFER_MODE_RAW)
            {
                LOG_ERROR_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                  Name, "' in shader '", ParentResLayout.GetShaderName(), "': structured buffer view is expected.");
            }

            if (BufferStride == 0 && ViewDesc.ByteWidth < BufferStaticSize)
            {
                // It is OK if robustBufferAccess feature is enabled, otherwise access outside of buffer range may lead to crash or undefined behavior.
                LOG_WARNING_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                    Name, "' in shader '", ParentResLayout.GetShaderName(), "': buffer size in the shader (",
                                    BufferStaticSize, ") is incompatible with the actual buffer view size (", ViewDesc.ByteWidth, ").");
            }

            if (BufferStride > 0 && (ViewDesc.ByteWidth < BufferStaticSize || (ViewDesc.ByteWidth - BufferStaticSize) % BufferStride != 0))
            {
                // For buffers with dynamic arrays we know only static part size and array element stride.
                // Element stride in the shader may be differ than in the code. Here we check that the buffer size is exactly the same as the array with N elements.
                LOG_WARNING_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                    Name, "' in shader '", ParentResLayout.GetShaderName(), "': static buffer size in the shader (",
                                    BufferStaticSize, ") and array element stride (", BufferStride, ") are incompatible with the actual buffer view size (", ViewDesc.ByteWidth, "),",
                                    " this may be the result of the array element size mismatch.");
            }
        }
    }
#endif

    auto UpdateDynamicBuffersCounter = [&DynamicBuffersCounter](const BufferViewVkImpl* pOldBufferView, const BufferViewVkImpl* pNewBufferView) {
        if (pOldBufferView != nullptr && pOldBufferView->GetBuffer<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC)
        {
            VERIFY(DynamicBuffersCounter > 0, "Dynamic buffers counter must be greater than zero when there is at least one dynamic buffer bound in the resource cache");
            --DynamicBuffersCounter;
        }
        if (pNewBufferView != nullptr && pNewBufferView->GetBuffer<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC)
            ++DynamicBuffersCounter;
    };

    if (UpdateCachedResource(DstRes, std::move(pBufferViewVk), UpdateDynamicBuffersCounter))
    {
        // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC descriptor type
        // require buffer to be created with VK_BUFFER_USAGE_STORAGE_BUFFER_BIT (13.2.4)

        // Do not update descriptor for a dynamic storage buffer. All dynamic resource
        // descriptors are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorBufferInfo DescrBuffInfo = DstRes.GetStorageBufferDescriptorWriteInfo();
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, nullptr, &DescrBuffInfo, nullptr);
        }
    }
}

void ShaderResourceLayoutVk::VkResource::CacheTexelBuffer(IDeviceObject*                   pBufferView,
                                                          ShaderResourceCacheVk::Resource& DstRes,
                                                          VkDescriptorSet                  vkDescrSet,
                                                          Uint32                           ArrayInd,
                                                          Uint16&                          DynamicBuffersCounter) const
{
    // clang-format off
    VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer || 
           Type == SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer,
           "Uniform or storage buffer resource is expected");
    // clang-format on

    RefCntAutoPtr<BufferViewVkImpl> pBufferViewVk{pBufferView, IID_BufferViewVk};
#ifdef DILIGENT_DEVELOPMENT
    {
        // HLSL buffer SRVs are mapped to storge buffers in GLSL
        auto RequiredViewType = Type == SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer ? BUFFER_VIEW_UNORDERED_ACCESS : BUFFER_VIEW_SHADER_RESOURCE;
        VerifyResourceViewBinding(*this, GetVariableType(), ArrayInd, pBufferView, pBufferViewVk.RawPtr(), {RequiredViewType}, DstRes.pObject.RawPtr(), ParentResLayout.GetShaderName());
        if (pBufferViewVk != nullptr)
        {
            const auto& ViewDesc = pBufferViewVk->GetDesc();
            const auto& BuffDesc = pBufferViewVk->GetBuffer()->GetDesc();
            if (!((BuffDesc.Mode == BUFFER_MODE_FORMATTED && ViewDesc.Format.ValueType != VT_UNDEFINED) || BuffDesc.Mode == BUFFER_MODE_RAW))
            {
                LOG_ERROR_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                  Name, "' in shader '", ParentResLayout.GetShaderName(), "': formatted buffer view is expected.");
            }
        }
    }
#endif

    auto UpdateDynamicBuffersCounter = [&DynamicBuffersCounter](const BufferViewVkImpl* pOldBufferView, const BufferViewVkImpl* pNewBufferView) {
        if (pOldBufferView != nullptr && pOldBufferView->GetBuffer<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC)
        {
            VERIFY(DynamicBuffersCounter > 0, "Dynamic buffers counter must be greater than zero when there is at least one dynamic buffer bound in the resource cache");
            --DynamicBuffersCounter;
        }
        if (pNewBufferView != nullptr && pNewBufferView->GetBuffer<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC)
            ++DynamicBuffersCounter;
    };

    if (UpdateCachedResource(DstRes, std::move(pBufferViewVk), UpdateDynamicBuffersCounter))
    {
        // The following bits must have been set at buffer creation time:
        //  * VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER  ->  VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
        //  * VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER  ->  VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT

        // Do not update descriptor for a dynamic texel buffer. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkBufferView BuffView = DstRes.pObject.RawPtr<BufferViewVkImpl>()->GetVkBufferView();
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, nullptr, nullptr, &BuffView);
        }
    }
}

template <typename TCacheSampler>
void ShaderResourceLayoutVk::VkResource::CacheImage(IDeviceObject*                   pTexView,
                                                    ShaderResourceCacheVk::Resource& DstRes,
                                                    VkDescriptorSet                  vkDescrSet,
                                                    Uint32                           ArrayInd,
                                                    TCacheSampler                    CacheSampler) const
{
    // clang-format off
    VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage  || 
           Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage ||
           Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage,
           "Storage image, separate image or sampled image resource is expected");
    // clang-format on

    RefCntAutoPtr<TextureViewVkImpl> pTexViewVk0{pTexView, IID_TextureViewVk};
#ifdef DILIGENT_DEVELOPMENT
    {
        // HLSL buffer SRVs are mapped to storge buffers in GLSL
        auto RequiredViewType = Type == SPIRVShaderResourceAttribs::ResourceType::StorageImage ? TEXTURE_VIEW_UNORDERED_ACCESS : TEXTURE_VIEW_SHADER_RESOURCE;
        VerifyResourceViewBinding(*this, GetVariableType(), ArrayInd, pTexView, pTexViewVk0.RawPtr(), {RequiredViewType}, DstRes.pObject.RawPtr(), ParentResLayout.GetShaderName());
    }
#endif
    if (UpdateCachedResource(DstRes, std::move(pTexViewVk0), [](const TextureViewVkImpl*, const TextureViewVkImpl*) {}))
    {
        // We can do RawPtr here safely since UpdateCachedResource() returned true
        auto* pTexViewVk = DstRes.pObject.RawPtr<TextureViewVkImpl>();
#ifdef DILIGENT_DEVELOPMENT
        if (Type == SPIRVShaderResourceAttribs::ResourceType::SampledImage && !IsImmutableSamplerAssigned())
        {
            if (pTexViewVk->GetSampler() == nullptr)
            {
                LOG_ERROR_MESSAGE("Error binding texture view '", pTexViewVk->GetDesc().Name, "' to variable '", GetPrintName(ArrayInd),
                                  "' in shader '", ParentResLayout.GetShaderName(), "'. No sampler is assigned to the view");
            }
        }
#endif

        // Do not update descriptor for a dynamic image. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorImageInfo DescrImgInfo = DstRes.GetImageDescriptorWriteInfo(IsImmutableSamplerAssigned());
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, &DescrImgInfo, nullptr, nullptr);
        }

        if (SamplerInd != InvalidSamplerInd)
        {
            VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage,
                   "Only separate images can be assigned separate samplers when using HLSL-style combined samplers.");
            VERIFY(!IsImmutableSamplerAssigned(), "Separate image can't be assigned an immutable sampler.");
            const auto& SamplerAttribs = ParentResLayout.GetResource(GetVariableType(), SamplerInd);
            VERIFY_EXPR(SamplerAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler);
            if (!SamplerAttribs.IsImmutableSamplerAssigned())
            {
                auto* pSampler = pTexViewVk->GetSampler();
                if (pSampler != nullptr)
                {
                    CacheSampler(SamplerAttribs, pSampler);
                }
                else
                {
                    LOG_ERROR_MESSAGE("Failed to bind sampler to sampler variable '", SamplerAttribs.Name,
                                      "' assigned to separate image '", GetPrintName(ArrayInd), "' in shader '",
                                      ParentResLayout.GetShaderName(), "': no sampler is set in texture view '", pTexViewVk->GetDesc().Name, '\'');
                }
            }
        }
    }
}

void ShaderResourceLayoutVk::VkResource::CacheSeparateSampler(IDeviceObject*                   pSampler,
                                                              ShaderResourceCacheVk::Resource& DstRes,
                                                              VkDescriptorSet                  vkDescrSet,
                                                              Uint32                           ArrayInd) const
{
    VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler, "Separate sampler resource is expected");
    VERIFY(!IsImmutableSamplerAssigned(), "This separate sampler is assigned an immutable sampler");

    RefCntAutoPtr<SamplerVkImpl> pSamplerVk{pSampler, IID_Sampler};
#ifdef DILIGENT_DEVELOPMENT
    if (pSampler != nullptr && pSamplerVk == nullptr)
    {
        LOG_ERROR_MESSAGE("Failed to bind object '", pSampler->GetDesc().Name, "' to variable '", GetPrintName(ArrayInd),
                          "' in shader '", ParentResLayout.GetShaderName(), "'. Unexpected object type: sampler is expected");
    }
    if (GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && DstRes.pObject != nullptr && DstRes.pObject != pSamplerVk)
    {
        auto VarTypeStr = GetShaderVariableTypeLiteralName(GetVariableType());
        LOG_ERROR_MESSAGE("Non-null sampler is already bound to ", VarTypeStr, " shader variable '", GetPrintName(ArrayInd),
                          "' in shader '", ParentResLayout.GetShaderName(),
                          "'. Attempting to bind another sampler or null is an error and may "
                          "cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic.");
    }
#endif
    if (UpdateCachedResource(DstRes, std::move(pSamplerVk), [](const SamplerVkImpl*, const SamplerVkImpl*) {}))
    {
        // Do not update descriptor for a dynamic sampler. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorImageInfo DescrImgInfo = DstRes.GetSamplerDescriptorWriteInfo();
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, &DescrImgInfo, nullptr, nullptr);
        }
    }
}

void ShaderResourceLayoutVk::VkResource::CacheInputAttachment(IDeviceObject*                   pTexView,
                                                              ShaderResourceCacheVk::Resource& DstRes,
                                                              VkDescriptorSet                  vkDescrSet,
                                                              Uint32                           ArrayInd) const
{
    VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::InputAttachment, "Input attachment resource is expected");
    RefCntAutoPtr<TextureViewVkImpl> pTexViewVk0{pTexView, IID_TextureViewVk};
#ifdef DILIGENT_DEVELOPMENT
    VerifyResourceViewBinding(*this, GetVariableType(), ArrayInd, pTexView, pTexViewVk0.RawPtr(), {TEXTURE_VIEW_SHADER_RESOURCE}, DstRes.pObject.RawPtr(), ParentResLayout.GetShaderName());
#endif
    if (UpdateCachedResource(DstRes, std::move(pTexViewVk0), [](const TextureViewVkImpl*, const TextureViewVkImpl*) {}))
    {
        // Do not update descriptor for a dynamic image. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorImageInfo DescrImgInfo = DstRes.GetInputAttachmentDescriptorWriteInfo();
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, &DescrImgInfo, nullptr, nullptr);
        }
        //
    }
}

void ShaderResourceLayoutVk::VkResource::CacheAccelerationStructure(IDeviceObject*                   pTLAS,
                                                                    ShaderResourceCacheVk::Resource& DstRes,
                                                                    VkDescriptorSet                  vkDescrSet,
                                                                    Uint32                           ArrayInd) const
{
    VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::AccelerationStructure, "Acceleration Structure resource is expected");
    RefCntAutoPtr<TopLevelASVkImpl> pTLASVk{pTLAS, IID_TopLevelASVk};
#ifdef DILIGENT_DEVELOPMENT
    VerifyTLASResourceBinding(*this, GetVariableType(), ArrayInd, pTLASVk.RawPtr(), DstRes.pObject.RawPtr(), ParentResLayout.GetShaderName());
#endif
    if (UpdateCachedResource(DstRes, std::move(pTLASVk), [](const TopLevelASVkImpl*, const TopLevelASVkImpl*) {}))
    {
        // Do not update descriptor for a dynamic TLAS. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (vkDescrSet != VK_NULL_HANDLE && GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkWriteDescriptorSetAccelerationStructureKHR DescrASInfo = DstRes.GetAccelerationStructureWriteInfo();
            UpdateDescriptorHandle(vkDescrSet, ArrayInd, nullptr, nullptr, nullptr, &DescrASInfo);
        }
        //
    }
}

void ShaderResourceLayoutVk::VkResource::BindResource(IDeviceObject* pObj, Uint32 ArrayIndex, ShaderResourceCacheVk& ResourceCache) const
{
    VERIFY_EXPR(ArrayIndex < ArraySize);

    auto& DstDescrSet = ResourceCache.GetDescriptorSet(DescriptorSet);
    auto  vkDescrSet  = DstDescrSet.GetVkDescriptorSet();
#ifdef DILIGENT_DEBUG
    if (ResourceCache.DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::SRBResources)
    {
        if (VariableType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC || VariableType == SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
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
    auto& DstRes = DstDescrSet.GetResource(CacheOffset + ArrayIndex);
    VERIFY(DstRes.Type == Type, "Inconsistent types");

    if (pObj)
    {
        static_assert(Uint32{SPIRVShaderResourceAttribs::ResourceType::NumResourceTypes} == 12, "Please handle the new resource type below");
        switch (Type)
        {
            case SPIRVShaderResourceAttribs::ResourceType::UniformBuffer:
                CacheUniformBuffer(pObj, DstRes, vkDescrSet, ArrayIndex, ResourceCache.GetDynamicBuffersCounter());
                break;

            case SPIRVShaderResourceAttribs::ResourceType::ROStorageBuffer:
            case SPIRVShaderResourceAttribs::ResourceType::RWStorageBuffer:
                CacheStorageBuffer(pObj, DstRes, vkDescrSet, ArrayIndex, ResourceCache.GetDynamicBuffersCounter());
                break;

            case SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer:
            case SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer:
                CacheTexelBuffer(pObj, DstRes, vkDescrSet, ArrayIndex, ResourceCache.GetDynamicBuffersCounter());
                break;

            case SPIRVShaderResourceAttribs::ResourceType::StorageImage:
            case SPIRVShaderResourceAttribs::ResourceType::SeparateImage:
            case SPIRVShaderResourceAttribs::ResourceType::SampledImage:
                CacheImage(pObj, DstRes, vkDescrSet, ArrayIndex,
                           [&](const VkResource& SeparateSampler, ISampler* pSampler) {
                               VERIFY(!SeparateSampler.IsImmutableSamplerAssigned(), "Separate sampler '", SeparateSampler.Name, "' is assigned an immutable sampler");
                               VERIFY_EXPR(Type == SPIRVShaderResourceAttribs::ResourceType::SeparateImage);
                               DEV_CHECK_ERR(SeparateSampler.ArraySize == 1 || SeparateSampler.ArraySize == ArraySize,
                                             "Array size (", SeparateSampler.ArraySize,
                                             ") of separate sampler variable '",
                                             SeparateSampler.Name,
                                             "' must be one or the same as the array size (", ArraySize,
                                             ") of separate image variable '", Name, "' it is assigned to");
                               Uint32 SamplerArrInd = SeparateSampler.ArraySize == 1 ? 0 : ArrayIndex;
                               SeparateSampler.BindResource(pSampler, SamplerArrInd, ResourceCache);
                           });
                break;

            case SPIRVShaderResourceAttribs::ResourceType::SeparateSampler:
                if (!IsImmutableSamplerAssigned())
                {
                    CacheSeparateSampler(pObj, DstRes, vkDescrSet, ArrayIndex);
                }
                else
                {
                    // Immutable samplers are permanently bound into the set layout; later binding a sampler
                    // into an immutable sampler slot in a descriptor set is not allowed (13.2.1)
                    LOG_ERROR_MESSAGE("Attempting to assign a sampler to an immutable sampler '", Name, '\'');
                }
                break;

            case SPIRVShaderResourceAttribs::ResourceType::InputAttachment:
                CacheInputAttachment(pObj, DstRes, vkDescrSet, ArrayIndex);
                break;

            case SPIRVShaderResourceAttribs::ResourceType::AccelerationStructure:
                CacheAccelerationStructure(pObj, DstRes, vkDescrSet, ArrayIndex);
                break;

            default: UNEXPECTED("Unknown resource type ", static_cast<Int32>(Type));
        }
    }
    else
    {
        if (DstRes.pObject && GetVariableType() != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            LOG_ERROR_MESSAGE("Shader variable '", Name, "' in shader '", ParentResLayout.GetShaderName(),
                              "' is not dynamic but being unbound. This is an error and may cause unpredicted behavior. "
                              "Use another shader resource binding instance or label shader variable as dynamic if you need to bind another resource.");
        }

        DstRes.pObject.Release();
    }
}

bool ShaderResourceLayoutVk::VkResource::IsBound(Uint32 ArrayIndex, const ShaderResourceCacheVk& ResourceCache) const
{
    VERIFY_EXPR(ArrayIndex < ArraySize);

    if (DescriptorSet < ResourceCache.GetNumDescriptorSets())
    {
        const auto& Set = ResourceCache.GetDescriptorSet(DescriptorSet);
        if (CacheOffset + ArrayIndex < Set.GetSize())
        {
            const auto& CachedRes = Set.GetResource(CacheOffset + ArrayIndex);
            return CachedRes.pObject != nullptr;
        }
    }

    return false;
}


void ShaderResourceLayoutVk::InitializeStaticResources(const ShaderResourceLayoutVk& SrcLayout,
                                                       const ShaderResourceCacheVk&  SrcResourceCache,
                                                       ShaderResourceCacheVk&        DstResourceCache) const
{
    auto NumStaticResources = m_NumResources[SHADER_RESOURCE_VARIABLE_TYPE_STATIC];
    VERIFY(NumStaticResources == SrcLayout.m_NumResources[SHADER_RESOURCE_VARIABLE_TYPE_STATIC], "Inconsistent number of static resources");
    VERIFY(SrcLayout.GetShaderType() == GetShaderType(), "Incosistent shader types");

    // Static shader resources are stored in one large continuous descriptor set
    for (Uint32 r = 0; r < NumStaticResources; ++r)
    {
        // Get resource attributes. Resources have the same index in both layouts.
        const auto& DstRes = GetResource(SHADER_RESOURCE_VARIABLE_TYPE_STATIC, r);
        const auto& SrcRes = SrcLayout.GetResource(SHADER_RESOURCE_VARIABLE_TYPE_STATIC, r);
        VERIFY(strcmp(SrcRes.Name, DstRes.Name) == 0, "Src resource name ('", SrcRes.Name, "') does match the dst resource name '(", DstRes.Name, "'). This is a bug.");
        VERIFY(SrcRes.Type == DstRes.Type, "Src and dst resource types are incompatible. This is a bug.");
        VERIFY(SrcRes.ResourceDim == DstRes.ResourceDim, "Src and dst resource dimensions are incompatible. This is a bug.");
        VERIFY(SrcRes.Binding == Uint32{SrcRes.Type}, "Unexpected binding");
        VERIFY(SrcRes.ArraySize == DstRes.ArraySize, "Src and dst resource array sizes are not identical. This is a bug.");
        VERIFY(SrcRes.IsImmutableSamplerAssigned() == DstRes.IsImmutableSamplerAssigned(), "Src and dst resource immutable sampler flags are not identical. This is a bug.");

        if (DstRes.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler &&
            DstRes.IsImmutableSamplerAssigned())
            continue; // Skip immutable separate samplers

        for (Uint32 ArrInd = 0; ArrInd < DstRes.ArraySize; ++ArrInd)
        {
            auto           SrcOffset    = SrcRes.CacheOffset + ArrInd;
            const auto&    SrcCachedSet = SrcResourceCache.GetDescriptorSet(SrcRes.DescriptorSet);
            const auto&    SrcCachedRes = SrcCachedSet.GetResource(SrcOffset);
            IDeviceObject* pObject      = SrcCachedRes.pObject.RawPtr<IDeviceObject>();
            if (!pObject)
                LOG_ERROR_MESSAGE("No resource is assigned to static shader variable '", SrcRes.GetPrintName(ArrInd), "' in shader '", GetShaderName(), "'.");

            auto           DstOffset       = DstRes.CacheOffset + ArrInd;
            IDeviceObject* pCachedResource = DstResourceCache.GetDescriptorSet(DstRes.DescriptorSet).GetResource(DstOffset).pObject;
            if (pCachedResource != pObject)
            {
                VERIFY(pCachedResource == nullptr, "Static resource has already been initialized, and the resource to be assigned from the shader does not match previously assigned resource");
                DstRes.BindResource(pObject, ArrInd, DstResourceCache);
            }
        }
    }
}


#ifdef DILIGENT_DEVELOPMENT
bool ShaderResourceLayoutVk::dvpVerifyBindings(const ShaderResourceCacheVk& ResourceCache) const
{
    bool BindingsOK = true;
    for (SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC; VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
    {
        for (Uint32 r = 0; r < m_NumResources[VarType]; ++r)
        {
            const auto& Res = GetResource(VarType, r);
            VERIFY(Res.GetVariableType() == VarType, "Unexpected variable type");
            for (Uint32 ArrInd = 0; ArrInd < Res.ArraySize; ++ArrInd)
            {
                const auto& CachedDescrSet = ResourceCache.GetDescriptorSet(Res.DescriptorSet);
                const auto& CachedRes      = CachedDescrSet.GetResource(Res.CacheOffset + ArrInd);
                VERIFY(CachedRes.Type == Res.Type, "Inconsistent types");
                if (CachedRes.pObject == nullptr &&
                    !(Res.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler && Res.IsImmutableSamplerAssigned()))
                {
                    LOG_ERROR_MESSAGE("No resource is bound to ", GetShaderVariableTypeLiteralName(Res.GetVariableType()), " variable '", Res.GetPrintName(ArrInd), "' in shader '", GetShaderName(), "'");
                    BindingsOK = false;
                }
#    ifdef DILIGENT_DEBUG
                auto vkDescSet           = CachedDescrSet.GetVkDescriptorSet();
                auto dbgCacheContentType = ResourceCache.DbgGetContentType();
                if (dbgCacheContentType == ShaderResourceCacheVk::DbgCacheContentType::StaticShaderResources)
                    VERIFY(vkDescSet == VK_NULL_HANDLE, "Static resource cache should never have vulkan descriptor set");
                else if (dbgCacheContentType == ShaderResourceCacheVk::DbgCacheContentType::SRBResources)
                {
                    if (VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC || VarType == SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
                    {
                        VERIFY(vkDescSet != VK_NULL_HANDLE, "Static and mutable variables must have valid vulkan descriptor set assigned");
                    }
                    else if (VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                    {
                        VERIFY(vkDescSet == VK_NULL_HANDLE, "Dynamic variables must not be assigned a vulkan descriptor set");
                    }
                }
                else
                    UNEXPECTED("Unexpected cache content type");
#    endif
            }
        }
    }
    return BindingsOK;
}
#endif

void ShaderResourceLayoutVk::InitializeResourceMemoryInCache(ShaderResourceCacheVk& ResourceCache) const
{
    auto TotalResources = GetTotalResourceCount();
    for (Uint32 r = 0; r < TotalResources; ++r)
    {
        const auto& Res = GetResource(r);
        ResourceCache.InitializeResources(Res.DescriptorSet, Res.CacheOffset, Res.ArraySize, Res.Type);
    }
}

void ShaderResourceLayoutVk::CommitDynamicResources(const ShaderResourceCacheVk& ResourceCache,
                                                    VkDescriptorSet              vkDynamicDescriptorSet) const
{
    Uint32 NumDynamicResources = m_NumResources[SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC];
    VERIFY(NumDynamicResources != 0, "This shader resource layout does not contain dynamic resources");
    VERIFY_EXPR(vkDynamicDescriptorSet != VK_NULL_HANDLE);

#ifdef DILIGENT_DEBUG
    static constexpr size_t ImgUpdateBatchSize          = 4;
    static constexpr size_t BuffUpdateBatchSize         = 2;
    static constexpr size_t TexelBuffUpdateBatchSize    = 2;
    static constexpr size_t AccelStructBatchSize        = 2;
    static constexpr size_t WriteDescriptorSetBatchSize = 2;
#else
    static constexpr size_t ImgUpdateBatchSize          = 128;
    static constexpr size_t BuffUpdateBatchSize         = 64;
    static constexpr size_t TexelBuffUpdateBatchSize    = 32;
    static constexpr size_t AccelStructBatchSize        = 32;
    static constexpr size_t WriteDescriptorSetBatchSize = 32;
#endif

    // Do not zero-initiaize arrays!
    std::array<VkDescriptorImageInfo, ImgUpdateBatchSize>                          DescrImgInfoArr;
    std::array<VkDescriptorBufferInfo, BuffUpdateBatchSize>                        DescrBuffInfoArr;
    std::array<VkBufferView, TexelBuffUpdateBatchSize>                             DescrBuffViewArr;
    std::array<VkWriteDescriptorSetAccelerationStructureKHR, AccelStructBatchSize> DescrAccelStructArr;
    std::array<VkWriteDescriptorSet, WriteDescriptorSetBatchSize>                  WriteDescrSetArr;

    Uint32 ResNum = 0, ArrElem = 0;
    auto   DescrImgIt      = DescrImgInfoArr.begin();
    auto   DescrBuffIt     = DescrBuffInfoArr.begin();
    auto   BuffViewIt      = DescrBuffViewArr.begin();
    auto   AccelStructIt   = DescrAccelStructArr.begin();
    auto   WriteDescrSetIt = WriteDescrSetArr.begin();

#ifdef DILIGENT_DEBUG
    Int32 DynamicDescrSetIndex = -1;
#endif

    while (ResNum < NumDynamicResources)
    {
        const auto& Res = GetResource(SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, ResNum);
        VERIFY_EXPR(Res.GetVariableType() == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);
#ifdef DILIGENT_DEBUG
        if (DynamicDescrSetIndex < 0)
            DynamicDescrSetIndex = Res.DescriptorSet;
        else
            VERIFY(DynamicDescrSetIndex == Res.DescriptorSet, "Inconsistent dynamic resource desriptor set index");
#endif
        const auto& SetResources = ResourceCache.GetDescriptorSet(Res.DescriptorSet);
        WriteDescrSetIt->sType   = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        WriteDescrSetIt->pNext   = nullptr;
        VERIFY(SetResources.GetVkDescriptorSet() == VK_NULL_HANDLE, "Dynamic descriptor set must not be assigned to the resource cache");
        WriteDescrSetIt->dstSet = vkDynamicDescriptorSet;
        VERIFY(WriteDescrSetIt->dstSet != VK_NULL_HANDLE, "Vulkan descriptor set must not be null");
        WriteDescrSetIt->dstBinding      = Res.Binding;
        WriteDescrSetIt->dstArrayElement = ArrElem;
        // descriptorType must be the same type as that specified in VkDescriptorSetLayoutBinding for dstSet at dstBinding.
        // The type of the descriptor also controls which array the descriptors are taken from. (13.2.4)
        WriteDescrSetIt->descriptorType = PipelineLayout::GetVkDescriptorType(Res.Type);

        // For every resource type, try to batch as many descriptor updates as we can
        static_assert(Uint32{SPIRVShaderResourceAttribs::ResourceType::NumResourceTypes} == 12, "Please handle the new resource type below");
        switch (Res.Type)
        {
            case SPIRVShaderResourceAttribs::ResourceType::UniformBuffer:
                WriteDescrSetIt->pBufferInfo = &(*DescrBuffIt);
                while (ArrElem < Res.ArraySize && DescrBuffIt != DescrBuffInfoArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + ArrElem);
                    *DescrBuffIt          = CachedRes.GetUniformBufferDescriptorWriteInfo();
                    ++DescrBuffIt;
                    ++ArrElem;
                }
                break;

            case SPIRVShaderResourceAttribs::ResourceType::ROStorageBuffer:
            case SPIRVShaderResourceAttribs::ResourceType::RWStorageBuffer:
                WriteDescrSetIt->pBufferInfo = &(*DescrBuffIt);
                while (ArrElem < Res.ArraySize && DescrBuffIt != DescrBuffInfoArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + ArrElem);
                    *DescrBuffIt          = CachedRes.GetStorageBufferDescriptorWriteInfo();
                    ++DescrBuffIt;
                    ++ArrElem;
                }
                break;

            case SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer:
            case SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer:
                WriteDescrSetIt->pTexelBufferView = &(*BuffViewIt);
                while (ArrElem < Res.ArraySize && BuffViewIt != DescrBuffViewArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + ArrElem);
                    *BuffViewIt           = CachedRes.GetBufferViewWriteInfo();
                    ++BuffViewIt;
                    ++ArrElem;
                }
                break;

            case SPIRVShaderResourceAttribs::ResourceType::SeparateImage:
            case SPIRVShaderResourceAttribs::ResourceType::StorageImage:
            case SPIRVShaderResourceAttribs::ResourceType::SampledImage:
                WriteDescrSetIt->pImageInfo = &(*DescrImgIt);
                while (ArrElem < Res.ArraySize && DescrImgIt != DescrImgInfoArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + ArrElem);
                    *DescrImgIt           = CachedRes.GetImageDescriptorWriteInfo(Res.IsImmutableSamplerAssigned());
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
                if (!Res.IsImmutableSamplerAssigned())
                {
                    WriteDescrSetIt->pImageInfo = &(*DescrImgIt);
                    while (ArrElem < Res.ArraySize && DescrImgIt != DescrImgInfoArr.end())
                    {
                        const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + ArrElem);
                        *DescrImgIt           = CachedRes.GetSamplerDescriptorWriteInfo();
                        ++DescrImgIt;
                        ++ArrElem;
                    }
                }
                else
                {
                    ArrElem                          = Res.ArraySize;
                    WriteDescrSetIt->dstArrayElement = Res.ArraySize;
                }
                break;

            case SPIRVShaderResourceAttribs::ResourceType::AccelerationStructure:
                WriteDescrSetIt->pNext = &(*AccelStructIt);
                while (ArrElem < Res.ArraySize && AccelStructIt != DescrAccelStructArr.end())
                {
                    const auto& CachedRes = SetResources.GetResource(Res.CacheOffset + ArrElem);
                    *AccelStructIt        = CachedRes.GetAccelerationStructureWriteInfo();
                    ++AccelStructIt;
                    ++ArrElem;
                }
                break;

            default:
                UNEXPECTED("Unexpected resource type");
        }

        WriteDescrSetIt->descriptorCount = ArrElem - WriteDescrSetIt->dstArrayElement;
        if (ArrElem == Res.ArraySize)
        {
            ArrElem = 0;
            ++ResNum;
        }
        // descriptorCount == 0 for immutable separate samplers
        if (WriteDescrSetIt->descriptorCount > 0)
            ++WriteDescrSetIt;

        // If we ran out of space in any of the arrays or if we processed all resources,
        // flush pending updates and reset iterators
        if (ResNum == NumDynamicResources ||
            DescrImgIt == DescrImgInfoArr.end() ||
            DescrBuffIt == DescrBuffInfoArr.end() ||
            BuffViewIt == DescrBuffViewArr.end() ||
            AccelStructIt == DescrAccelStructArr.end() ||
            WriteDescrSetIt == WriteDescrSetArr.end())
        {
            auto DescrWriteCount = static_cast<Uint32>(std::distance(WriteDescrSetArr.begin(), WriteDescrSetIt));
            if (DescrWriteCount > 0)
                m_LogicalDevice.UpdateDescriptorSets(DescrWriteCount, WriteDescrSetArr.data(), 0, nullptr);

            DescrImgIt      = DescrImgInfoArr.begin();
            DescrBuffIt     = DescrBuffInfoArr.begin();
            BuffViewIt      = DescrBuffViewArr.begin();
            AccelStructIt   = DescrAccelStructArr.begin();
            WriteDescrSetIt = WriteDescrSetArr.begin();
        }
    }
}

bool ShaderResourceLayoutVk::IsCompatibleWith(const ShaderResourceLayoutVk& ResLayout) const
{
    if (m_NumResources != ResLayout.m_NumResources)
        return false;

    for (Uint32 i = 0, Cnt = GetTotalResourceCount(); i < Cnt; ++i)
    {
        const auto& lhs = this->GetResource(i);
        const auto& rhs = ResLayout.GetResource(i);

        if (!lhs.IsCompatibleWith(rhs))
            return false;
    }

    return true;
}

} // namespace Diligent
