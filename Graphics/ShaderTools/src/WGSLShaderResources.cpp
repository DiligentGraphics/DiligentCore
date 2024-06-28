/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "WGSLShaderResources.hpp"
#include "Align.hpp"
#include "StringPool.hpp"
#include "WGSLUtils.hpp"

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4324) //  warning C4324: structure was padded due to alignment specifier
#endif

#include <tint/tint.h>
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/wgsl/ast/identifier_expression.h"
#include "src/tint/lang/wgsl/ast/identifier.h"
#include "src/tint/lang/wgsl/sem/variable.h"

#ifdef _MSC_VER
#    pragma warning(pop)
#endif

namespace Diligent
{

SHADER_TYPE TintPipelineStageToShaderType(tint::inspector::PipelineStage Stage)
{
    switch (Stage)
    {
        case tint::inspector::PipelineStage::kVertex:
            return SHADER_TYPE_VERTEX;

        case tint::inspector::PipelineStage::kFragment:
            return SHADER_TYPE_PIXEL;

        case tint::inspector::PipelineStage::kCompute:
            return SHADER_TYPE_COMPUTE;

        default:
            UNEXPECTED("Unexpected pipeline stage");
            return SHADER_TYPE_UNKNOWN;
    }
}

WGSLShaderResourceAttribs::ResourceType TintResourceTypeToWGSLShaderAttribsResourceType(tint::inspector::ResourceBinding::ResourceType TintResType)
{
    using TintResourceType = tint::inspector::ResourceBinding::ResourceType;
    switch (TintResType)
    {
        case TintResourceType::kUniformBuffer:
            return WGSLShaderResourceAttribs::ResourceType::UniformBuffer;

        case TintResourceType::kStorageBuffer:
            return WGSLShaderResourceAttribs::ResourceType::RWStorageBuffer;

        case TintResourceType::kReadOnlyStorageBuffer:
            return WGSLShaderResourceAttribs::ResourceType::ROStorageBuffer;

        case TintResourceType::kSampler:
            return WGSLShaderResourceAttribs::ResourceType::Sampler;

        case TintResourceType::kComparisonSampler:
            return WGSLShaderResourceAttribs::ResourceType::ComparisonSampler;

        case TintResourceType::kSampledTexture:
            return WGSLShaderResourceAttribs::ResourceType::Texture;

        case TintResourceType::kMultisampledTexture:
            return WGSLShaderResourceAttribs::ResourceType::TextureMS;

        case TintResourceType::kWriteOnlyStorageTexture:
            return WGSLShaderResourceAttribs::ResourceType::WOStorageTexture;

        case TintResourceType::kReadOnlyStorageTexture:
            return WGSLShaderResourceAttribs::ResourceType::ROStorageTexture;

        case TintResourceType::kReadWriteStorageTexture:
            return WGSLShaderResourceAttribs::ResourceType::RWStorageTexture;

        case TintResourceType::kDepthTexture:
            return WGSLShaderResourceAttribs::ResourceType::DepthTexture;

        case TintResourceType::kDepthMultisampledTexture:
            return WGSLShaderResourceAttribs::ResourceType::DepthTextureMS;

        case TintResourceType::kExternalTexture:
            return WGSLShaderResourceAttribs::ResourceType::ExternalTexture;

        case TintResourceType::kInputAttachment:
            UNEXPECTED("Input attachments are not currently supported");
            return WGSLShaderResourceAttribs::ResourceType::NumResourceTypes;

        default:
            UNEXPECTED("Unexpected resource type");
            return WGSLShaderResourceAttribs::ResourceType::NumResourceTypes;
    }
}

WGSLShaderResourceAttribs::TextureSampleType TintSampleKindToWGSLShaderAttribsSampleType(const tint::inspector::ResourceBinding& TintBinding)
{
    using TintResourceType = tint::inspector::ResourceBinding::ResourceType;
    using TintSampledKind  = tint::inspector::ResourceBinding::SampledKind;

    if (TintBinding.resource_type != TintResourceType::kSampledTexture &&
        TintBinding.resource_type != TintResourceType::kMultisampledTexture &&
        TintBinding.resource_type != TintResourceType::kWriteOnlyStorageTexture &&
        TintBinding.resource_type != TintResourceType::kReadOnlyStorageTexture &&
        TintBinding.resource_type != TintResourceType::kReadWriteStorageTexture &&
        TintBinding.resource_type != TintResourceType::kExternalTexture)
        return WGSLShaderResourceAttribs::TextureSampleType::Unknown;

    switch (TintBinding.sampled_kind)
    {
        case TintSampledKind::kFloat:
            return WGSLShaderResourceAttribs::TextureSampleType::Float;

        case TintSampledKind::kSInt:
            return WGSLShaderResourceAttribs::TextureSampleType::SInt;

        case TintSampledKind::kUInt:
            return WGSLShaderResourceAttribs::TextureSampleType::UInt;

        case TintSampledKind::kUnknown:
            return WGSLShaderResourceAttribs::TextureSampleType::Unknown;

        default:
            UNEXPECTED("Unexpected sample kind");
            return WGSLShaderResourceAttribs::TextureSampleType::Unknown;
    }
}

RESOURCE_DIMENSION TintTextureDimensionToResourceDimension(tint::inspector::ResourceBinding::TextureDimension TintTexDim)
{
    using TintTextureDim = tint::inspector::ResourceBinding::TextureDimension;
    switch (TintTexDim)
    {
        case TintTextureDim::k1d:
            return RESOURCE_DIM_TEX_1D;

        case TintTextureDim::k2d:
            return RESOURCE_DIM_TEX_2D;

        case TintTextureDim::k2dArray:
            return RESOURCE_DIM_TEX_2D_ARRAY;

        case TintTextureDim::k3d:
            return RESOURCE_DIM_TEX_3D;

        case TintTextureDim::kCube:
            return RESOURCE_DIM_TEX_CUBE;

        case TintTextureDim::kCubeArray:
            return RESOURCE_DIM_TEX_CUBE_ARRAY;

        case TintTextureDim::kNone:
            return RESOURCE_DIM_UNDEFINED;

        default:
            UNEXPECTED("Unexpected texture dimension");
            return RESOURCE_DIM_UNDEFINED;
    }
}

RESOURCE_DIMENSION TintBindingToResourceDimension(const tint::inspector::ResourceBinding& TintBinding)
{
    using TintResourceType = tint::inspector::ResourceBinding::ResourceType;
    switch (TintBinding.resource_type)
    {
        case TintResourceType::kUniformBuffer:
        case TintResourceType::kStorageBuffer:
        case TintResourceType::kReadOnlyStorageBuffer:
            return RESOURCE_DIM_BUFFER;

        case TintResourceType::kSampler:
        case TintResourceType::kComparisonSampler:
            return RESOURCE_DIM_UNDEFINED;

        case TintResourceType::kSampledTexture:
        case TintResourceType::kMultisampledTexture:
        case TintResourceType::kWriteOnlyStorageTexture:
        case TintResourceType::kReadOnlyStorageTexture:
        case TintResourceType::kReadWriteStorageTexture:
        case TintResourceType::kDepthTexture:
        case TintResourceType::kDepthMultisampledTexture:
        case TintResourceType::kExternalTexture:
            return TintTextureDimensionToResourceDimension(TintBinding.dim);

        case TintResourceType::kInputAttachment:
            return RESOURCE_DIM_UNDEFINED;

        default:
            UNEXPECTED("Unexpected resource type");
            return RESOURCE_DIM_UNDEFINED;
    }
}

TEXTURE_FORMAT TintTexelFormatToTextureFormat(const tint::inspector::ResourceBinding& TintBinding)
{
    using TintResourceType = tint::inspector::ResourceBinding::ResourceType;
    using TintTexelFormat  = tint::inspector::ResourceBinding::TexelFormat;

    if (TintBinding.resource_type != TintResourceType::kWriteOnlyStorageTexture &&
        TintBinding.resource_type != TintResourceType::kReadOnlyStorageTexture &&
        TintBinding.resource_type != TintResourceType::kReadWriteStorageTexture)
    {
        // Format is only defined for storage textures
        return TEX_FORMAT_UNKNOWN;
    }

    switch (TintBinding.image_format)
    {
            // clang-format off
        case TintTexelFormat::kBgra8Unorm:  return TEX_FORMAT_BGRA8_UNORM;
        case TintTexelFormat::kRgba8Unorm:  return TEX_FORMAT_RGBA8_UNORM;
        case TintTexelFormat::kRgba8Snorm:  return TEX_FORMAT_RGBA8_SNORM;
        case TintTexelFormat::kRgba8Uint:   return TEX_FORMAT_RGBA8_UINT;
        case TintTexelFormat::kRgba8Sint:   return TEX_FORMAT_RGBA8_SINT;
        case TintTexelFormat::kRgba16Uint:  return TEX_FORMAT_RGBA16_UINT;
        case TintTexelFormat::kRgba16Sint:  return TEX_FORMAT_RGBA16_SINT;
        case TintTexelFormat::kRgba16Float: return TEX_FORMAT_RGBA16_FLOAT;
        case TintTexelFormat::kR32Uint:     return TEX_FORMAT_R32_UINT;
        case TintTexelFormat::kR32Sint:     return TEX_FORMAT_R32_SINT;
        case TintTexelFormat::kR32Float:    return TEX_FORMAT_R32_FLOAT;
        case TintTexelFormat::kRg32Uint:    return TEX_FORMAT_RG32_UINT;
        case TintTexelFormat::kRg32Sint:    return TEX_FORMAT_RG32_SINT;
        case TintTexelFormat::kRg32Float:   return TEX_FORMAT_RG32_FLOAT;
        case TintTexelFormat::kRgba32Uint:  return TEX_FORMAT_RGBA32_UINT;
        case TintTexelFormat::kRgba32Sint:  return TEX_FORMAT_RGBA32_SINT;
        case TintTexelFormat::kRgba32Float: return TEX_FORMAT_RGBA32_FLOAT;
        case TintTexelFormat::kR8Unorm:     return TEX_FORMAT_R8_UNORM;
        case TintTexelFormat::kNone:        return TEX_FORMAT_UNKNOWN;
        // clang-format on
        default:
            UNEXPECTED("Unexpected texel format");
            return TEX_FORMAT_UNKNOWN;
    }
}

WGSLShaderResourceAttribs::WGSLShaderResourceAttribs(const char*                             _Name,
                                                     const tint::inspector::ResourceBinding& TintBinding) noexcept :
    // clang-format off
    Name             {_Name},
    ArraySize        {1},
    Type             {TintResourceTypeToWGSLShaderAttribsResourceType(TintBinding.resource_type)},
    ResourceDim      {TintBindingToResourceDimension(TintBinding)},
    Format			 {TintTexelFormatToTextureFormat(TintBinding)},
    BindGroup        {static_cast<Uint16>(TintBinding.bind_group)},
    BindIndex        {static_cast<Uint16>(TintBinding.binding)},
    SampleType       {TintSampleKindToWGSLShaderAttribsSampleType(TintBinding)},
    BufferStaticSize {TintBinding.resource_type == tint::inspector::ResourceBinding::ResourceType::kUniformBuffer ? static_cast<Uint32>(TintBinding.size) : 0}
// clang-format on
{}

WGSLShaderResourceAttribs::WGSLShaderResourceAttribs(const char*        _Name,
                                                     ResourceType       _Type,
                                                     Uint16             _ArraySize,
                                                     RESOURCE_DIMENSION _ResourceDim,
                                                     TEXTURE_FORMAT     _Format,
                                                     TextureSampleType  _SampleType,
                                                     uint16_t           _BindGroup,
                                                     uint16_t           _BindIndex,
                                                     Uint32             _BufferStaticSize) noexcept :
    Name{_Name},
    ArraySize{_ArraySize},
    Type{_Type},
    ResourceDim{_ResourceDim},
    Format{_Format},
    BindGroup{_BindGroup},
    BindIndex{_BindIndex},
    SampleType{_SampleType},
    BufferStaticSize{_BufferStaticSize}
{
}

SHADER_RESOURCE_TYPE WGSLShaderResourceAttribs::GetShaderResourceType(ResourceType Type)
{
    static_assert(Uint32{WGSLShaderResourceAttribs::ResourceType::NumResourceTypes} == 13, "Please handle the new resource type below");
    switch (Type)
    {
        case WGSLShaderResourceAttribs::ResourceType::UniformBuffer:
            return SHADER_RESOURCE_TYPE_CONSTANT_BUFFER;

        case WGSLShaderResourceAttribs::ResourceType::ROStorageBuffer:
            return SHADER_RESOURCE_TYPE_BUFFER_SRV;

        case WGSLShaderResourceAttribs::ResourceType::RWStorageBuffer:
            return SHADER_RESOURCE_TYPE_BUFFER_UAV;

        case WGSLShaderResourceAttribs::ResourceType::Sampler:
        case WGSLShaderResourceAttribs::ResourceType::ComparisonSampler:
            return SHADER_RESOURCE_TYPE_SAMPLER;

        case WGSLShaderResourceAttribs::ResourceType::Texture:
        case WGSLShaderResourceAttribs::ResourceType::TextureMS:
        case WGSLShaderResourceAttribs::ResourceType::DepthTexture:
        case WGSLShaderResourceAttribs::ResourceType::DepthTextureMS:
            return SHADER_RESOURCE_TYPE_TEXTURE_SRV;

        case WGSLShaderResourceAttribs::ResourceType::WOStorageTexture:
        case WGSLShaderResourceAttribs::ResourceType::ROStorageTexture:
        case WGSLShaderResourceAttribs::ResourceType::RWStorageTexture:
            return SHADER_RESOURCE_TYPE_TEXTURE_UAV;

        case WGSLShaderResourceAttribs::ResourceType::ExternalTexture:
            LOG_WARNING_MESSAGE("External textures are not currently supported");
            return SHADER_RESOURCE_TYPE_UNKNOWN;

        default:
            UNEXPECTED("Unknown WGSL resource type");
            return SHADER_RESOURCE_TYPE_UNKNOWN;
    }
}

PIPELINE_RESOURCE_FLAGS WGSLShaderResourceAttribs::GetPipelineResourceFlags(ResourceType Type)
{
    return PIPELINE_RESOURCE_FLAG_NONE;
}

WGSLShaderResources::WGSLShaderResources(IMemoryAllocator&      Allocator,
                                         const std::string&     WGSL,
                                         SHADER_SOURCE_LANGUAGE SourceLanguage,
                                         const char*            ShaderName,
                                         const char*            CombinedSamplerSuffix,
                                         const char*            EntryPoint,
                                         bool                   LoadUniformBufferReflection) noexcept(false)
{
    VERIFY_EXPR(ShaderName != nullptr);

    tint::Source::File SrcFile("", WGSL);
    tint::Program      Program = tint::wgsl::reader::Parse(&SrcFile, {tint::wgsl::AllowedFeatures::Everything()});
    if (!Program.IsValid())
    {
        LOG_ERROR_AND_THROW("Failed to parse shader '", ShaderName, "':\n",
                            Program.Diagnostics().Str(), "\n");
    }

    tint::inspector::Inspector Inspector{Program};

    const auto EntryPoints = Inspector.GetEntryPoints();
    if (EntryPoints.empty())
    {
        LOG_ERROR_AND_THROW("The program does not contain any entry points");
    }

    size_t EntryPointIdx = 0;
    if (EntryPoint == nullptr)
    {
        VERIFY(EntryPoints.size() == 1, "The program contains more than one entry point. Please specify the entry point name.");
        EntryPoint    = EntryPoints[0].name.c_str();
        EntryPointIdx = 0;
    }
    else
    {
        for (EntryPointIdx = 0; EntryPointIdx < EntryPoints.size(); ++EntryPointIdx)
        {
            if (EntryPoints[EntryPointIdx].name == EntryPoint)
                break;
        }
        if (EntryPointIdx == EntryPoints.size())
        {
            LOG_ERROR_AND_THROW("Entry point '", EntryPoint, "' not found in the shader '", ShaderName, "'");
        }
    }
    m_ShaderType = TintPipelineStageToShaderType(EntryPoints[EntryPointIdx].stage);

    const auto ResourceBindings = Inspector.GetResourceBindings(EntryPoint);

    using TintResourceType = tint::inspector::ResourceBinding::ResourceType;

    auto GetResourceName = [SourceLanguage](const tint::Program& Program, const tint::inspector::ResourceBinding& Binding) {
        if (SourceLanguage == SHADER_SOURCE_LANGUAGE_WGSL)
        {
            return Binding.variable_name;
        }
        else
        {
            //   HLSL:
            //      struct BufferData0
            //      {
            //          float4 data;
            //      };
            //      StructuredBuffer<BufferData0> g_Buff0;
            //      StructuredBuffer<BufferData0> g_Buff1;
            //   WGSL:
            //      struct g_Buff0 {
            //        x_data : RTArr,
            //      }
            //      @group(0) @binding(0) var<storage, read> g_Buff0_1 : g_Buff0;
            //      @group(0) @binding(1) var<storage, read> g_Buff1   : g_Buff0;
            auto AltName = GetWGSLResourceAlternativeName(Program, Binding);
            return !AltName.empty() ? AltName : Binding.variable_name;
        }
    };

    // Count resources
    ResourceCounters ResCounters;
    size_t           ResourceNamesPoolSize = 0;
    for (const auto& Binding : ResourceBindings)
    {
        switch (Binding.resource_type)
        {
            case TintResourceType::kUniformBuffer:
                ++ResCounters.NumUBs;
                break;

            case TintResourceType::kStorageBuffer:
            case TintResourceType::kReadOnlyStorageBuffer:
                ++ResCounters.NumSBs;
                break;

            case TintResourceType::kSampler:
            case TintResourceType::kComparisonSampler:
                ++ResCounters.NumSamplers;
                break;

            case TintResourceType::kSampledTexture:
            case TintResourceType::kMultisampledTexture:
            case TintResourceType::kDepthTexture:
            case TintResourceType::kDepthMultisampledTexture:
                ++ResCounters.NumTextures;
                break;

            case TintResourceType::kWriteOnlyStorageTexture:
            case TintResourceType::kReadOnlyStorageTexture:
            case TintResourceType::kReadWriteStorageTexture:
                ++ResCounters.NumStTextures;
                break;

            case TintResourceType::kExternalTexture:
                ++ResCounters.NumExtTextures;
                break;

            case TintResourceType::kInputAttachment:
                UNSUPPORTED("Input attachments are not currently supported");
                break;

            default:
                UNEXPECTED("Unexpected resource type");
        }

        const std::string ResourceName = GetResourceName(Program, Binding);
        ResourceNamesPoolSize += ResourceName.length() + 1;
    }

    if (CombinedSamplerSuffix != nullptr)
    {
        ResourceNamesPoolSize += strlen(CombinedSamplerSuffix) + 1;
    }

    ResourceNamesPoolSize += strlen(ShaderName) + 1;
    ResourceNamesPoolSize += strlen(EntryPoint) + 1;

    // Resource names pool is only needed to facilitate string allocation.
    StringPool ResourceNamesPool;
    Initialize(Allocator, ResCounters, ResourceNamesPoolSize, ResourceNamesPool);

    // Allocate resources
    ResourceCounters CurrRes;
    for (const auto& Binding : ResourceBindings)
    {
        const std::string ResourceName = GetResourceName(Program, Binding);
        const char*       PooledName   = ResourceNamesPool.CopyString(ResourceName);
        switch (Binding.resource_type)
        {
            case TintResourceType::kUniformBuffer:
            {
                new (&GetUB(CurrRes.NumUBs++)) WGSLShaderResourceAttribs{PooledName, Binding};
            }
            break;

            case TintResourceType::kStorageBuffer:
            case TintResourceType::kReadOnlyStorageBuffer:
            {
                new (&GetSB(CurrRes.NumSBs++)) WGSLShaderResourceAttribs{PooledName, Binding};
            }
            break;

            case TintResourceType::kSampler:
            case TintResourceType::kComparisonSampler:
            {
                new (&GetSampler(CurrRes.NumSamplers++)) WGSLShaderResourceAttribs{PooledName, Binding};
            }
            break;

            case TintResourceType::kSampledTexture:
            case TintResourceType::kMultisampledTexture:
            case TintResourceType::kDepthTexture:
            case TintResourceType::kDepthMultisampledTexture:
            {
                new (&GetTexture(CurrRes.NumTextures++)) WGSLShaderResourceAttribs{PooledName, Binding};
            }
            break;

            case TintResourceType::kWriteOnlyStorageTexture:
            case TintResourceType::kReadOnlyStorageTexture:
            case TintResourceType::kReadWriteStorageTexture:
            {
                new (&GetStTexture(CurrRes.NumStTextures++)) WGSLShaderResourceAttribs{PooledName, Binding};
            }
            break;

            case TintResourceType::kExternalTexture:
            {
                new (&GetExtTexture(CurrRes.NumExtTextures++)) WGSLShaderResourceAttribs{PooledName, Binding};
            }
            break;

            case TintResourceType::kInputAttachment:
                UNSUPPORTED("Input attachments are not currently supported");
                break;

            default:
                UNEXPECTED("Unexpected resource type");
        }
    }

    VERIFY_EXPR(CurrRes.NumUBs == GetNumUBs());
    VERIFY_EXPR(CurrRes.NumSBs == GetNumSBs());
    VERIFY_EXPR(CurrRes.NumTextures == GetNumTextures());
    VERIFY_EXPR(CurrRes.NumStTextures == GetNumStTextures());
    VERIFY_EXPR(CurrRes.NumSamplers == GetNumSamplers());
    VERIFY_EXPR(CurrRes.NumExtTextures == GetNumExtTextures());

    if (CombinedSamplerSuffix != nullptr)
    {
        m_CombinedSamplerSuffix = ResourceNamesPool.CopyString(CombinedSamplerSuffix);
    }

    m_ShaderName = ResourceNamesPool.CopyString(ShaderName);
    m_EntryPoint = ResourceNamesPool.CopyString(EntryPoint);
    VERIFY(ResourceNamesPool.GetRemainingSize() == 0, "Names pool must be empty");
}

void WGSLShaderResources::Initialize(IMemoryAllocator&       Allocator,
                                     const ResourceCounters& Counters,
                                     size_t                  ResourceNamesPoolSize,
                                     StringPool&             ResourceNamesPool)
{
    Uint32           CurrentOffset = 0;
    constexpr Uint32 MaxOffset     = std::numeric_limits<OffsetType>::max();
    auto             AdvanceOffset = [&CurrentOffset, MaxOffset](Uint32 NumResources) {
        VERIFY(CurrentOffset <= MaxOffset, "Current offset (", CurrentOffset, ") exceeds max allowed value (", MaxOffset, ")");
        (void)MaxOffset;
        auto Offset = static_cast<OffsetType>(CurrentOffset);
        CurrentOffset += NumResources;
        return Offset;
    };

    auto UniformBufferOffset = AdvanceOffset(Counters.NumUBs);
    (void)UniformBufferOffset;
    m_StorageBufferOffset   = AdvanceOffset(Counters.NumSBs);
    m_TextureOffset         = AdvanceOffset(Counters.NumTextures);
    m_StorageTextureOffset  = AdvanceOffset(Counters.NumStTextures);
    m_SamplerOffset         = AdvanceOffset(Counters.NumSamplers);
    m_ExternalTextureOffset = AdvanceOffset(Counters.NumExtTextures);
    m_TotalResources        = AdvanceOffset(0);
    static_assert(Uint32{WGSLShaderResourceAttribs::ResourceType::NumResourceTypes} == 13, "Please update the new resource type offset");

    auto AlignedResourceNamesPoolSize = AlignUp(ResourceNamesPoolSize, sizeof(void*));

    static_assert(sizeof(WGSLShaderResourceAttribs) % sizeof(void*) == 0, "Size of WGSLShaderResourceAttribs struct must be multiple of sizeof(void*)");
    // clang-format off
    auto MemorySize = m_TotalResources             * sizeof(WGSLShaderResourceAttribs) +
                      AlignedResourceNamesPoolSize * sizeof(char);

    VERIFY_EXPR(GetNumUBs()         == Counters.NumUBs);
    VERIFY_EXPR(GetNumSBs()         == Counters.NumSBs);
    VERIFY_EXPR(GetNumTextures()    == Counters.NumTextures);
    VERIFY_EXPR(GetNumStTextures()  == Counters.NumStTextures);
    VERIFY_EXPR(GetNumSamplers()    == Counters.NumSamplers);
    VERIFY_EXPR(GetNumExtTextures() == Counters.NumExtTextures);
    static_assert(Uint32{WGSLShaderResourceAttribs::ResourceType::NumResourceTypes} == 13, "Please update the new resource count verification");
    // clang-format on

    if (MemorySize)
    {
        auto* pRawMem   = Allocator.Allocate(MemorySize, "Memory for shader resources", __FILE__, __LINE__);
        m_MemoryBuffer  = std::unique_ptr<void, STDDeleterRawMem<void>>(pRawMem, Allocator);
        char* NamesPool = reinterpret_cast<char*>(m_MemoryBuffer.get()) +
            m_TotalResources * sizeof(WGSLShaderResourceAttribs);
        ResourceNamesPool.AssignMemory(NamesPool, ResourceNamesPoolSize);
    }
}

WGSLShaderResources::~WGSLShaderResources()
{
    for (Uint32 n = 0; n < GetTotalResources(); ++n)
        GetResource(n).~WGSLShaderResourceAttribs();
}

std::string WGSLShaderResources::DumpResources()
{
    std::stringstream ss;
    ss << "Shader '" << m_ShaderName << "' resource stats: total resources: " << GetTotalResources() << ":" << std::endl
       << "UBs: " << GetNumUBs() << "; SBs: " << GetNumSBs() << "; Textures: " << GetNumTextures() << "; St Textures: " << GetNumStTextures()
       << "; Samplers: " << GetNumSamplers() << "; Ext Textures: " << GetNumExtTextures() << '.' << std::endl
       << "Resources:";

    Uint32 ResNum       = 0;
    auto   DumpResource = [&ss, &ResNum](const WGSLShaderResourceAttribs& Res) {
        std::stringstream FullResNameSS;
        FullResNameSS << '\'' << Res.Name;
        if (Res.ArraySize > 1)
            FullResNameSS << '[' << Res.ArraySize << ']';
        FullResNameSS << '\'';
        ss << std::setw(32) << FullResNameSS.str();
        ++ResNum;
    };

    ProcessResources(
        [&](const WGSLShaderResourceAttribs& UB, Uint32) //
        {
            VERIFY(UB.Type == WGSLShaderResourceAttribs::ResourceType::UniformBuffer, "Unexpected resource type");
            ss << std::endl
               << std::setw(3) << ResNum << " Uniform Buffer     ";
            DumpResource(UB);
        },
        [&](const WGSLShaderResourceAttribs& SB, Uint32) //
        {
            VERIFY((SB.Type == WGSLShaderResourceAttribs::ResourceType::ROStorageBuffer ||
                    SB.Type == WGSLShaderResourceAttribs::ResourceType::RWStorageBuffer),
                   "Unexpected resource type");
            ss << std::endl
               << std::setw(3) << ResNum
               << (SB.Type == WGSLShaderResourceAttribs::ResourceType::ROStorageBuffer ? " RO Storage Buffer  " : " RW Storage Buffer  ");
            DumpResource(SB);
        },
        [&](const WGSLShaderResourceAttribs& Tex, Uint32) //
        {
            if (Tex.Type == WGSLShaderResourceAttribs::ResourceType::Texture)
            {
                ss << std::endl
                   << std::setw(3) << ResNum << " Texture          ";
            }
            else if (Tex.Type == WGSLShaderResourceAttribs::ResourceType::TextureMS)
            {
                ss << std::endl
                   << std::setw(3) << ResNum << " TextureMS        ";
            }
            else if (Tex.Type == WGSLShaderResourceAttribs::ResourceType::DepthTexture)
            {
                ss << std::endl
                   << std::setw(3) << ResNum << " Depth Texture    ";
            }
            else if (Tex.Type == WGSLShaderResourceAttribs::ResourceType::DepthTextureMS)
            {
                ss << std::endl
                   << std::setw(3) << ResNum << " Depth TextureMS  ";
            }
            else
            {
                UNEXPECTED("Unexpected resource type");
            }
            DumpResource(Tex);
        },
        [&](const WGSLShaderResourceAttribs& StTex, Uint32) //
        {
            if (StTex.Type == WGSLShaderResourceAttribs::ResourceType::WOStorageTexture)
            {
                ss << std::endl
                   << std::setw(3) << ResNum << " WO Storage Tex   ";
            }
            else if (StTex.Type == WGSLShaderResourceAttribs::ResourceType::ROStorageTexture)
            {
                ss << std::endl
                   << std::setw(3) << ResNum << " RO Storage Tex   ";
            }
            else if (StTex.Type == WGSLShaderResourceAttribs::ResourceType::RWStorageTexture)
            {
                ss << std::endl
                   << std::setw(3) << ResNum << " RW Storage Tex   ";
            }
            else
            {
                UNEXPECTED("Unexpected resource type");
            }
            DumpResource(StTex);
        },
        [&](const WGSLShaderResourceAttribs& Sam, Uint32) //
        {
            if (Sam.Type == WGSLShaderResourceAttribs::ResourceType::Sampler)
            {
                ss << std::endl
                   << std::setw(3) << ResNum << " Sampler          ";
            }
            else if (Sam.Type == WGSLShaderResourceAttribs::ResourceType::ComparisonSampler)
            {
                ss << std::endl
                   << std::setw(3) << ResNum << " Sampler Cmp      ";
            }
            else
            {
                UNEXPECTED("Unexpected resource type");
            }
            DumpResource(Sam);
        },
        [&](const WGSLShaderResourceAttribs& ExtTex, Uint32) //
        {
            VERIFY(ExtTex.Type == WGSLShaderResourceAttribs::ResourceType::ExternalTexture, "Unexpected resource type");
            ss << std::endl
               << std::setw(3) << ResNum << " Ext Texture     ";
            DumpResource(ExtTex);
        } //
    );
    VERIFY_EXPR(ResNum == GetTotalResources());

    return ss.str();
}

} // namespace Diligent
