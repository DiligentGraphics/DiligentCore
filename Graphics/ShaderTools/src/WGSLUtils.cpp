/*
 *  Copyright 2024-2026 Diligent Graphics LLC
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

#include <algorithm>
#include <limits>
#include <unordered_set>

#include "WGSLUtils.hpp"
#include "WGSLShaderReflection.hpp"
#include "DebugUtilities.hpp"
#include "ParsingTools.hpp"
#include "ShaderToolsCommon.hpp"
#if defined(USE_SPIRV_TOOLS) && TINT_BUILD_SPV_READER
#    define SPIRV_TO_WGSL_SUPPORTED 1
#    include "SPIRVTools.hpp"
#    include "SPIRVShaderResources.hpp"
#    include "EngineMemory.h"
#    include "spirv/unified1/spirv.hpp"
#    include "spirv-tools/libspirv.h"
#else
#    define SPIRV_TO_WGSL_SUPPORTED 0
#endif

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4324) //  warning C4324: structure was padded due to alignment specifier
#endif

#include <tint/tint.h>
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/core_builtin_call.h"
#include "src/tint/lang/core/ir/let.h"
#include "src/tint/lang/core/ir/load.h"
#include "src/tint/lang/core/ir/var.h"
#include "src/tint/lang/core/ir/transform/direct_variable_access.h"
#include "src/tint/lang/core/type/storage_texture.h"
#include "src/tint/lang/core/ir/transform/binding_remapper.h"
#include "src/tint/lang/core/ir/transform/single_entry_point.h"
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/wgsl/ast/function.h"
#include "src/tint/lang/wgsl/ast/identifier_expression.h"
#include "src/tint/lang/wgsl/ast/identifier.h"
#include "src/tint/lang/wgsl/sem/variable.h"
#include "src/tint/lang/core/type/atomic.h"
#include "src/tint/lang/core/type/array.h"
#include "src/tint/lang/core/type/struct.h"


#ifdef _MSC_VER
#    pragma warning(pop)
#endif

namespace Diligent
{

WGSLEmulatedResourceArrayElement GetWGSLEmulatedArrayElement(const std::string& Name, const std::string& Suffix)
{
    if (Name.empty() || Suffix.empty())
        return {Name, -1};

    size_t SuffixPos = Name.find(Suffix);
    // g_Tex2DArr_15
    //  ^
    if (SuffixPos != std::string::npos)
    {
        // Properly handle self-overlapping suffixes, e.g. "g_Tex2Dxxx24", "xx" is the suffix
        size_t NextSuffixPos = Name.find(Suffix, SuffixPos + 1);
        while (NextSuffixPos != std::string::npos)
        {
            SuffixPos     = NextSuffixPos;
            NextSuffixPos = Name.find(Suffix, SuffixPos + 1);
        }
    }

    // g_Tex2DArr_15
    //           ^
    if (SuffixPos != std::string::npos && SuffixPos + Suffix.length() < Name.length())
    {
        int Index = 0;
        if (Parsing::IsDigit(Name[SuffixPos + Suffix.length()]) &&
            Parsing::ParseInteger(Name.begin() + SuffixPos + Suffix.length(), Name.end(), Index) == Name.end())
        {
            // g_Tex2DArr_15
            //            ^
            VERIFY_EXPR(Index >= 0);
            return {Name.substr(0, SuffixPos), Index};
        }
    }

    return {Name, -1};
}

#if SPIRV_TO_WGSL_SUPPORTED
// The legacy Tint SPIR-V reader inferred image access from shader operations.
// The IR reader defaults storage images without access decorations to read_write,
// even when they are only loaded or stored. Using these types directly changes
// WGSL resource reflection and the bind-group layouts expected by Diligent.
//
// Restore the legacy mapping per resource: load-only images become sampled textures,
// store-only images become write-only storage textures, and images used both ways
// remain read_write. Queries alone do not infer access: keep the declared access,
// mapping explicitly read-only images to sampled textures. Unknown uses retain
// their original types. Only the temporary Tint IR is changed, not the source SPIR-V.
static bool RestoreTextureAccess(tint::core::ir::Module& IR)
{
    namespace ir    = tint::core::ir;
    namespace type  = tint::core::type;
    using BuiltinFn = tint::core::BuiltinFn;

    // Resolve texture parameters to global resources, including through nested calls.
    // Helpers called with different resources need separate versions so a shared
    // parameter does not force all arguments to have the same texture access mode.
    // Run this only when needed: full handle specialization can duplicate helpers
    // for distinct resources and also specializes other texture/sampler parameters.
    bool HasTextureParameters = false;
    for (const auto* Function : IR.functions)
    {
        for (const auto* Param : Function->Params())
            HasTextureParameters |= Param->Type()->UnwrapPtr()->Is<type::StorageTexture>();
    }
    if (HasTextureParameters)
    {
        ir::transform::DirectVariableAccessConfig Config;
        Config.transform_handle = ir::transform::HandleTransformLevel::kFull;
        auto Result             = ir::transform::DirectVariableAccess(IR, Config);
        if (Result != tint::Success)
        {
            LOG_ERROR_MESSAGE("Failed to specialize texture parameters:\n", Result.Failure().reason);
            return false;
        }
    }

    ir::Builder Builder{IR};
    auto&       Types = IR.Types();
    for (auto* Inst : *IR.root_block)
    {
        auto* Var = Inst->As<ir::Var>();
        if (Var == nullptr)
            continue;
        const auto* Texture = Var->Result()->Type()->UnwrapPtr()->As<type::StorageTexture>();
        if (Texture == nullptr)
            continue;

        // Follow loads and aliases to find this resource's texture operations.
        // Keep the alias values so their types can be updated together with the variable.
        std::vector<ir::Value*>           Values{Var->Result()};
        std::unordered_set<ir::Value*>    Visited{Var->Result()};
        std::vector<ir::CoreBuiltinCall*> Loads;

        bool HasReads       = false;
        bool HasWrites      = false;
        bool HasUnknownUses = false;
        for (size_t i = 0; i < Values.size(); ++i)
        {
            for (const auto& Use : Values[i]->UsagesUnsorted())
            {
                if (Use->instruction->Is<ir::Load>() || Use->instruction->Is<ir::Let>())
                {
                    auto* Result = Use->instruction->Result();
                    if (Visited.insert(Result).second)
                        Values.push_back(Result);
                }
                else if (auto* Call = Use->instruction->As<ir::CoreBuiltinCall>())
                {
                    switch (Call->Func())
                    {
                        case BuiltinFn::kTextureLoad:
                            HasReads = true;
                            Loads.push_back(Call);
                            break;
                        case BuiltinFn::kTextureStore:
                            HasWrites = true;
                            break;
                        case BuiltinFn::kTextureDimensions:
                        case BuiltinFn::kTextureNumLayers:
                            // Metadata queries do not read or write texels.
                            break;
                        default:
                            HasUnknownUses = true;
                            break;
                    }
                }
                else
                {
                    // Retain the declared access for operations we cannot safely narrow.
                    HasUnknownUses = true;
                }
            }
        }
        // Preserve the declared type when narrowing is unsafe or both access modes are needed.
        if (HasUnknownUses || (HasReads && HasWrites))
            continue;

        const type::Type* NewTextureType = nullptr;
        if (HasWrites)
        {
            NewTextureType = Types.storage_texture(Texture->Dim(), Texture->TexelFormat(), tint::core::Access::kWrite);
        }
        else if (HasReads || Texture->Access() == tint::core::Access::kRead)
        {
            // The legacy reader used sampled bindings for load-only images and for
            // query-only images explicitly declared read-only (SPIR-V NonWritable).
            NewTextureType = Types.sampled_texture(Texture->Dim(), Texture->Type());
            for (auto* Load : Loads)
                Load->AppendArg(Builder.Constant(tint::core::i32{0})); // Sampled texture loads require a mip level.
        }
        if (NewTextureType == nullptr)
            continue;

        // Tint types are shared. Assign replacement types to this resource and its
        // aliases instead of mutating a type that may also describe another resource.
        for (auto* Value : Values)
        {
            if (const auto* Ptr = Value->Type()->As<type::Pointer>())
                Value->SetType(Types.ptr(Ptr->AddressSpace(), NewTextureType, Ptr->Access()));
            else
                Value->SetType(NewTextureType);
        }
    }
    return true;
}

static WGSLShaderReflection CaptureSPIRVReflection(const std::vector<uint32_t>&  SPIRV,
                                                   const tint::core::ir::Module& IR,
                                                   bool                          LoadUniformBufferReflection)
{
    WGSLShaderReflection Reflection;
    // SPIRVShaderResources uses OpSource to interpret HLSL matrix layouts and buffer
    // names. Keep the legacy WGSL fallback for binaries stripped of source metadata.
    bool HasSource = false;
    for (size_t i = 5; i < SPIRV.size();)
    {
        const Uint32 WordCount = SPIRV[i] >> 16;
        if (WordCount == 0 || WordCount > SPIRV.size() - i)
            return Reflection;
        if ((SPIRV[i] & 0xffffu) == static_cast<Uint32>(spv::Op::OpSource))
            HasSource = true;
        i += WordCount;
    }
    if (!HasSource)
        return Reflection;

    SPIRVShaderResources::CreateInfo ResCI;
    ResCI.Name                        = "SPIR-V to WGSL conversion";
    ResCI.LoadUniformBufferReflection = LoadUniformBufferReflection;
    for (const auto* Function : IR.functions)
    {
        if (!Function->IsEntryPoint())
            continue;
        // Conversion normally receives a single shader. Do not infer reflection
        // from one entry point when supplied bytecode contains several stages.
        if (ResCI.ShaderType != SHADER_TYPE_UNKNOWN)
            return Reflection;

        if (Function->IsVertex())
            ResCI.ShaderType = SHADER_TYPE_VERTEX;
        else if (Function->IsFragment())
            ResCI.ShaderType = SHADER_TYPE_PIXEL;
        else
            ResCI.ShaderType = SHADER_TYPE_COMPUTE;
    }
    if (ResCI.ShaderType == SHADER_TYPE_UNKNOWN)
        return Reflection;

    const SPIRVShaderResources Resources{GetRawAllocator(), SPIRV, ResCI};
    for (Uint32 i = 0; i < Resources.GetTotalResources(); ++i)
    {
        const auto& Res = Resources.GetResource(i);
        if (Res.Type == SPIRVShaderResourceAttribs::ResourceType::PushConstant ||
            Res.Name == nullptr || Res.Name[0] == '\0')
            continue;
        const WGSLShaderReflection::BindingPoint Binding{
            SPIRV[Res.DescriptorSetDecorationOffset],
            SPIRV[Res.BindingDecorationOffset],
        };
        auto& Dst = Reflection.Resources[Binding];
        Dst.Name  = Res.Name;
        if (LoadUniformBufferReflection && i < Resources.GetNumUBs())
        {
            const ShaderCodeBufferDesc& Buffer = *Resources.GetUniformBufferDesc(i);

            Dst.UniformBuffer       = std::make_unique<ShaderCodeBufferDescX>();
            Dst.UniformBuffer->Size = Buffer.Size;
            for (Uint32 v = 0; v < Buffer.NumVariables; ++v)
                Dst.UniformBuffer->AddVariable(Buffer.pVariables[v]);
        }
    }
    return Reflection;
}
#endif

std::string ConvertSPIRVtoWGSL(const std::vector<uint32_t>& SPIRV, bool LoadUniformBufferReflection)
{
#if SPIRV_TO_WGSL_SUPPORTED
    // Tint does not support HLSL reflection extensions. Strip them from a copy
    // so the original SPIR-V remains available for reflection.
    // Match the validation environment used by Tint, including relaxed block layouts.
    auto ConversionSPIRV = OptimizeSPIRV(SPIRV, SPV_ENV_VULKAN_1_1, SPIRV_OPTIMIZATION_FLAG_STRIP_REFLECTION);
    if (ConversionSPIRV.empty())
    {
        LOG_ERROR_MESSAGE("Failed to prepare SPIR-V for Tint conversion");
        return {};
    }

    auto IR = tint::spirv::reader::ReadIR(ConversionSPIRV);
    if (IR != tint::Success)
    {
        LOG_ERROR_MESSAGE("Tint SPIR-V reader failure:\n", IR.Failure().reason, "\n");
        return {};
    }

    // Capture source reflection from the unmodified SPIR-V, before Tint raises
    // buffer layouts or renames resources for WGSL.
    WGSLShaderReflection Reflection = CaptureSPIRVReflection(SPIRV, IR.Get(), LoadUniformBufferReflection);

    if (!RestoreTextureAccess(IR.Get()))
        return {};

    tint::wgsl::writer::Options WGSLWriterOptions;
    WGSLWriterOptions.allow_non_uniform_derivatives = true;
    WGSLWriterOptions.allowed_features              = tint::wgsl::AllowedFeatures::Everything();

    auto GenerationResult = tint::wgsl::writer::WgslFromIR(IR.Get(), WGSLWriterOptions);
    if (GenerationResult != tint::Success)
    {
        LOG_ERROR_MESSAGE("Tint WGSL writer failure:\n", GenerationResult.Failure().reason, "\n");
        return {};
    }

    std::string WGSL = std::move(GenerationResult->wgsl);
    AppendWGSLShaderReflection(WGSL, Reflection);
    return WGSL;
#else
    (void)SPIRV;
    (void)LoadUniformBufferReflection;
    LOG_ERROR_MESSAGE("SPIR-V to WGSL conversion is not supported in this build; provide WGSL shaders instead.");
    return {};
#endif
}

static bool IsAtomic(const tint::core::type::Type* WGSLType)
{
    if (WGSLType == nullptr)
        return false;

    if (WGSLType->Is<tint::core::type::Struct>())
    {
        const tint::core::type::Struct* WGSLStruct = WGSLType->As<tint::core::type::Struct>();
        if (WGSLStruct == nullptr || WGSLStruct->Members().IsEmpty())
            return false;

        for (const tint::core::type::StructMember* WGSLMember : WGSLStruct->Members())
        {
            const tint::core::type::Type* WGSLMemberType = WGSLMember->Type();
            if (WGSLMemberType == nullptr)
                return false;

            if (WGSLMemberType->Is<tint::core::type::Array>())
            {
                const tint::core::type::Array* WGSLArray = WGSLMemberType->As<tint::core::type::Array>();
                if (WGSLArray == nullptr)
                    return false;

                const tint::core::type::Type* WGSLArrayElemType = WGSLArray->ElemType();
                if (WGSLArrayElemType == nullptr)
                    return false;

                if (IsAtomic(WGSLArrayElemType))
                    return true;
            }
            else
            {
                if (IsAtomic(WGSLMemberType))
                    return true;
            }
        }

        return false;
    }
    else
    {
        return WGSLType->Is<tint::core::type::Atomic>();
    }
}

std::string GetWGSLResourceAlternativeName(const tint::Program& Program, const tint::inspector::ResourceBinding& Binding)
{
    if (Binding.resource_type != tint::inspector::ResourceBinding::ResourceType::kUniformBuffer &&
        Binding.resource_type != tint::inspector::ResourceBinding::ResourceType::kStorageBuffer &&
        Binding.resource_type != tint::inspector::ResourceBinding::ResourceType::kReadOnlyStorageBuffer)
    {
        return {};
    }

    const tint::ast::Variable* Variable = nullptr;
    for (const tint::ast::Variable* Var : Program.AST().GlobalVariables())
    {
        if (Var->name->symbol.Name() == Binding.variable_name)
        {
            Variable = Var;
            break;
        }
    }

    if (Variable == nullptr)
    {
        return {};
    }

    const tint::sem::GlobalVariable* SemVariable = Program.Sem().Get(Variable)->As<tint::sem::GlobalVariable>();
    VERIFY_EXPR(SemVariable->Attributes().binding_point->group == Binding.bind_group &&
                SemVariable->Attributes().binding_point->binding == Binding.binding);

    std::string TypeName = SemVariable->Declaration()->type->identifier->symbol.Name();
    if (Binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kUniformBuffer)
    {
        //   HLSL:
        //      cbuffer CB0
        //      {
        //          float4 g_Data0;
        //      }
        //   WGSL:
        //      struct CB0 {
        //        g_Data0 : vec4f,
        //      }
        //      @group(0) @binding(0) var<uniform> x_13 : CB0;
        return TypeName;
    }
    else
    {
        VERIFY_EXPR(Binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kStorageBuffer ||
                    Binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kReadOnlyStorageBuffer);
        //   HLSL:
        //      struct BufferData0
        //      {
        //          float4 data;
        //      };
        //      StructuredBuffer<BufferData0> g_Buff0;
        //      StructuredBuffer<BufferData0> g_Buff1;
        //      StructuredBuffer<int>         g_AtomicBuff0; // Used in atomic operations
        //      StructuredBuffer<int>         g_AtomicBuff1; // Used in atomic operations
        //   WGSL:
        //      struct g_Buff0 {
        //        x_data : RTArr,
        //      }
        //      @group(0) @binding(0) var<storage, read> g_Buff0_1       : g_Buff0;
        //      @group(0) @binding(1) var<storage, read> g_Buff1         : g_Buff0;
        //      @group(0) @binding(2) var<storage, read> g_AtomicBuff0_1 : g_AtomicBuff0_atomic;
        //      @group(0) @binding(3) var<storage, read> g_AtomicBuff1   : g_AtomicBuff0_atomic;

        if (IsAtomic(Program.TypeOf(Variable->type)))
        {
            // Remove "_atomic" postfix from the type name
            const std::string_view AtomicPostfix = "_atomic";
            if (TypeName.length() > AtomicPostfix.length() && TypeName.compare(TypeName.length() - AtomicPostfix.length(), AtomicPostfix.length(), AtomicPostfix) == 0)
            {
                TypeName.erase(TypeName.length() - AtomicPostfix.length());
            }
        }

        if (strncmp(Binding.variable_name.c_str(), TypeName.c_str(), TypeName.length()) == 0)
        {
            //      @group(0) @binding(0) var<storage, read> g_Buff0_1 : g_Buff0;
            return TypeName;
        }
        else
        {
            //      @group(0) @binding(1) var<storage, read> g_Buff1   : g_Buff0;
            return {};
        }
    }
}

static WGSLResourceMapping::const_iterator FindResourceAsArrayElement(const WGSLResourceMapping& ResMapping,
                                                                      const std::string&         EmulatedArrayIndexSuffix,
                                                                      const std::string&         Name,
                                                                      Uint32&                    ArrayIndex)
{
    if (EmulatedArrayIndexSuffix.empty())
        return ResMapping.end();

    if (WGSLEmulatedResourceArrayElement ArrayElem = GetWGSLEmulatedArrayElement(Name, EmulatedArrayIndexSuffix))
    {
        auto BindigIt = ResMapping.find(ArrayElem.Name);
        if (BindigIt == ResMapping.end())
            return ResMapping.end();

        if (static_cast<Uint32>(ArrayElem.Index) < BindigIt->second.ArraySize)
        {
            ArrayIndex = static_cast<Uint32>(ArrayElem.Index);
            return BindigIt;
        }
    }

    return ResMapping.end();
}

std::string RemapWGSLResourceBindings(const std::string&         WGSL,
                                      const WGSLResourceMapping& ResMapping,
                                      const char*                EmulatedArrayIndexSuffix,
                                      const char*                EntryPoint,
                                      std::string*               pRemappedEntryPoint)
{
    tint::Source::File srcFile("", WGSL);
    tint::Program      Program = tint::wgsl::reader::Parse(&srcFile, {tint::wgsl::AllowedFeatures::Everything()});

    if (!Program.IsValid())
    {
        LOG_ERROR_MESSAGE("Tint WGSL reader failure:\nParser: ", Program.Diagnostics().Str(), "\n");
        return {};
    }

    WGSLShaderReflection Reflection;
    if (!ParseWGSLShaderReflection(WGSL, Reflection))
    {
        LOG_ERROR_MESSAGE("Invalid WGSL reflection metadata");
        return {};
    }

    std::unordered_map<tint::BindingPoint, tint::BindingPoint> BindingPoints;
    WGSLShaderReflection                                       RemappedReflection;

    tint::inspector::Inspector Inspector{Program};
    const auto                 EntryPoints = Inspector.GetEntryPoints();
    if (EntryPoint != nullptr && std::none_of(EntryPoints.begin(), EntryPoints.end(), [EntryPoint](const auto& EP) { return EP.name == EntryPoint; }))
    {
        LOG_ERROR_MESSAGE("WGSL entry point '", EntryPoint, "' was not found");
        return {};
    }
    for (const tint::inspector::EntryPoint& EP : EntryPoints)
    {
        if (EntryPoint != nullptr && EP.name != EntryPoint)
            continue;

        // Capture names before Tint's IR and WGSL writers can rename them. IDs
        // remain stable, including implicitly assigned IDs and workgroup-size overrides.
        for (const auto& Override : EP.overrides)
        {
            const auto It = Reflection.SpecializationConstants.find(Override.id.value);
            RemappedReflection.SpecializationConstants.emplace(Override.id.value,
                                                               It != Reflection.SpecializationConstants.end() ? It->second : Override.name);
        }

        // Bindings may be shared by resources in separate entry points, but
        // resources used by the same entry point need distinct destinations.
        std::unordered_map<tint::BindingPoint, tint::BindingPoint> Destinations;
        for (const tint::inspector::ResourceBinding& Binding : Inspector.GetResourceBindings(EP.name))
        {
            Uint32 ArrayIndex = 0;

            // Original names take precedence over generated identifiers. Fall back
            // to the legacy name rules for native WGSL and older shader archives.
            const auto* SourceResource = Reflection.Find(Binding.bind_group, Binding.binding);
            std::string ResourceName   = SourceResource != nullptr ? SourceResource->Name : Binding.variable_name;
            auto        DstBindigIt    = ResMapping.find(ResourceName);
            if (EmulatedArrayIndexSuffix != nullptr && DstBindigIt == ResMapping.end())
            {
                DstBindigIt = FindResourceAsArrayElement(ResMapping, EmulatedArrayIndexSuffix, ResourceName, ArrayIndex);
            }

            if (DstBindigIt == ResMapping.end() && SourceResource == nullptr)
            {
                const std::string AltName = GetWGSLResourceAlternativeName(Program, Binding);
                if (!AltName.empty())
                {
                    DstBindigIt = ResMapping.find(AltName);
                    if (EmulatedArrayIndexSuffix != nullptr && DstBindigIt == ResMapping.end())
                    {
                        DstBindigIt = FindResourceAsArrayElement(ResMapping, EmulatedArrayIndexSuffix, AltName, ArrayIndex);
                    }
                    if (DstBindigIt != ResMapping.end())
                        ResourceName = AltName;
                }
            }

            if (DstBindigIt != ResMapping.end())
            {
                const WGSLResourceBindingInfo& DstBindig = DstBindigIt->second;
                if (DstBindig.ArraySize == 0 || ArrayIndex >= DstBindig.ArraySize)
                {
                    LOG_ERROR_MESSAGE("Invalid resource array size for variable '", ResourceName, "'");
                    return {};
                }
                if (ArrayIndex > (std::numeric_limits<Uint32>::max)() - DstBindig.Index)
                {
                    LOG_ERROR_MESSAGE("Binding index overflows for variable '", ResourceName, "'");
                    return {};
                }

                const tint::BindingPoint SrcPoint{Binding.bind_group, Binding.binding};
                const tint::BindingPoint DstPoint{DstBindig.Group, DstBindig.Index + ArrayIndex};
                const auto               SrcInsertion = BindingPoints.emplace(SrcPoint, DstPoint);
                if (!SrcInsertion.second && !(SrcInsertion.first->second == DstPoint))
                {
                    LOG_ERROR_MESSAGE("Conflicting remappings for binding ", SrcPoint.binding, " in group ", SrcPoint.group);
                    return {};
                }
                const auto DstInsertion = Destinations.emplace(DstPoint, SrcPoint);
                if (!DstInsertion.second && !(DstInsertion.first->second == SrcPoint))
                {
                    LOG_ERROR_MESSAGE("Resource binding collision for variable '", ResourceName, "' in entry point '", EP.name, "'");
                    return {};
                }

                // Native WGSL has no input metadata. Save the name used by the
                // application, including an emulated array element's full suffix.
                auto NameInsertion = RemappedReflection.Resources.emplace(
                    WGSLShaderReflection::BindingPoint{DstPoint.group, DstPoint.binding}, WGSLShaderReflection::Resource{});
                if (!NameInsertion.second && NameInsertion.first->second.Name != ResourceName)
                {
                    LOG_ERROR_MESSAGE("Conflicting resource names at remapped binding ", DstPoint.binding, " in group ", DstPoint.group);
                    return {};
                }
                NameInsertion.first->second.Name = std::move(ResourceName);
            }
            else
            {
                LOG_ERROR_MESSAGE("Binding for variable '", ResourceName, "' is not found in the remap indices");
                return {};
            }
        }
    }

    auto IR = tint::wgsl::reader::ProgramToLoweredIR(Program);
    if (IR != tint::Success)
    {
        LOG_ERROR_MESSAGE("Tint WGSL to IR conversion failure:\n", IR.Failure().reason, "\n");
        return {};
    }

    if (EntryPoint != nullptr)
    {
        // Reflection and the pipeline signature describe only the selected entry
        // point. Remove the other resources before changing their shared bindings.
        auto SelectionResult = tint::core::ir::transform::SingleEntryPoint(IR.Get(), EntryPoint);
        if (SelectionResult != tint::Success)
        {
            LOG_ERROR_MESSAGE("Tint entry point selection failure:\n", SelectionResult.Failure().reason, "\n");
            return {};
        }
    }

    auto RemapResult = tint::core::ir::transform::BindingRemapper(IR.Get(), BindingPoints);
    if (RemapResult != tint::Success)
    {
        LOG_ERROR_MESSAGE("Tint binding remapper failure:\n", RemapResult.Failure().reason, "\n");
        return {};
    }

    tint::wgsl::writer::Options WGSLWriterOptions;
    WGSLWriterOptions.allow_non_uniform_derivatives = true;
    WGSLWriterOptions.allowed_features              = tint::wgsl::AllowedFeatures::Everything();
    // WgslFromIR performs these same two steps. Keep the generated program so
    // we can read the final name: Tint may rename an entry point both in IR and
    // while creating the WGSL program, without updating the IR name in the latter case.
    auto RemappedProgram = tint::wgsl::writer::ProgramFromIR(IR.Get(), WGSLWriterOptions);
    if (RemappedProgram != tint::Success)
    {
        LOG_ERROR_MESSAGE("Tint WGSL writer failure:\nProgram: ", RemappedProgram.Failure().reason, "\n");
        return {};
    }

    std::string RemappedEntryPoint;
    if (EntryPoint != nullptr)
    {
        for (const auto* Function : RemappedProgram->AST().Functions())
        {
            if (!Function->IsEntryPoint())
                continue;
            if (!RemappedEntryPoint.empty())
            {
                LOG_ERROR_MESSAGE("Remapped WGSL contains more than one entry point");
                return {};
            }
            RemappedEntryPoint = Function->name->symbol.Name();
        }
        if (RemappedEntryPoint.empty())
        {
            LOG_ERROR_MESSAGE("Remapped WGSL contains no entry point");
            return {};
        }
    }

    auto GenerationResult = tint::wgsl::writer::Generate(RemappedProgram.Get(), WGSLWriterOptions);
    if (GenerationResult != tint::Success)
    {
        LOG_ERROR_MESSAGE("Tint WGSL writer failure:\nGenerate: ", GenerationResult.Failure().reason, "\n");
        return {};
    }

    std::string PatchedWGSL = std::move(GenerationResult->wgsl);

    // Tint does not preserve comments. Carry reflection across the rewrite using
    // the same binding-point mapping as the shader, including emulated arrays.
    for (auto& Entry : Reflection.Resources)
    {
        auto It = BindingPoints.find(tint::BindingPoint{Entry.first.Group, Entry.first.Binding});
        if (It != BindingPoints.end())
            RemappedReflection.Resources[{It->second.group, It->second.binding}] = std::move(Entry.second);
    }
    AppendWGSLShaderReflection(PatchedWGSL, RemappedReflection);

    // If original WGSL contains shader source language definition, append it to the patched WGSL
    SHADER_SOURCE_LANGUAGE SrcLang = ParseShaderSourceLanguageDefinition(WGSL);
    if (SrcLang != SHADER_SOURCE_LANGUAGE_DEFAULT)
        AppendShaderSourceLanguageDefinition(PatchedWGSL, SrcLang);

    if (pRemappedEntryPoint != nullptr)
        *pRemappedEntryPoint = std::move(RemappedEntryPoint);

    return PatchedWGSL;
}

} // namespace Diligent
