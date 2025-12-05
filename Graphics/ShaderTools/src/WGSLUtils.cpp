/*
 *  Copyright 2024-2025 Diligent Graphics LLC
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

#include "WGSLUtils.hpp"
#include "DebugUtilities.hpp"
#include "ParsingTools.hpp"
#include "ShaderToolsCommon.hpp"

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4324) //  warning C4324: structure was padded due to alignment specifier
#endif

#define TINT_BUILD_SPV_READER  1
#define TINT_BUILD_WGSL_WRITER 1
#include <tint/tint.h>
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/wgsl/ast/identifier_expression.h"
#include "src/tint/lang/wgsl/ast/identifier.h"
#include "src/tint/lang/wgsl/sem/variable.h"
#include "src/tint/lang/core/type/atomic.h"
#include "src/tint/lang/core/type/array.h"
#include "src/tint/lang/core/type/struct.h"
#include "src/tint/lang/core/ir/transform/binding_remapper.h"
#include "src/tint/lang/wgsl/inspector/inspector.h"
#include "src/tint/lang/wgsl/writer/ir_to_program/ir_to_program.h"
#include "src/tint/lang/wgsl/reader/program_to_ir/program_to_ir.h"

#ifdef _MSC_VER
#    pragma warning(pop)
#endif
#include <SPIRVTools.hpp>

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
        if (Parsing::ParseInteger(Name.begin() + SuffixPos + Suffix.length(), Name.end(), Index) == Name.end())
        {
            // g_Tex2DArr_15
            //            ^
            VERIFY_EXPR(Index >= 0);
            return {Name.substr(0, SuffixPos), Index};
        }
    }

    return {Name, -1};
}

std::string DecodeSpirvLiteralString(const std::vector<uint32_t>& SPIRV,
                                     size_t                       StartWord,
                                     uint16_t                     WordCount)
{
    std::string Result;
    Result.reserve(static_cast<size_t>(WordCount) * 4);

    const size_t EndWord = StartWord + WordCount;
    for (size_t Idx = StartWord; Idx < EndWord; ++Idx)
    {
        uint32_t Word = SPIRV[Idx];

        for (int32_t ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
        {
            char c = static_cast<char>((Word >> (8 * ByteIndex)) & 0xFF);
            if (c == '\0')
                return Result;
            Result.push_back(c);
        }
    }

    return Result;
}

void StripGoogleHlslFunctionality(std::vector<uint32_t>& SPIRV)
{
    if (SPIRV.size() <= 5)
        return;

    constexpr uint16_t OpExtension                  = 10;
    constexpr uint16_t OpDecorateStringGOOGLE       = 5632;
    constexpr uint16_t OpMemberDecorateStringGOOGLE = 5633;

    size_t InstructionPointer = 5; // Skip SPIR-V header
    while (InstructionPointer < SPIRV.size())
    {
        uint32_t Instruction = SPIRV[InstructionPointer];
        uint16_t WordCount   = Instruction >> 16;
        uint16_t OpCode      = Instruction & 0xFFFF;

        if (WordCount == 0 || InstructionPointer + WordCount > SPIRV.size())
            break;

        auto EraseCurrent = [&](size_t count) {
            SPIRV.erase(SPIRV.begin() + InstructionPointer,
                        SPIRV.begin() + InstructionPointer + count);
        };

        if (OpCode == OpExtension)
        {
            std::string Extension = DecodeSpirvLiteralString(SPIRV, InstructionPointer + 1, static_cast<uint16_t>(WordCount - 1));

            if (Extension == "SPV_GOOGLE_hlsl_functionality1")
            {
                EraseCurrent(WordCount);
                continue;
            }
        }
        else if (OpCode == OpDecorateStringGOOGLE || OpCode == OpMemberDecorateStringGOOGLE)
        {
            EraseCurrent(WordCount);
            continue;
        }
        InstructionPointer += WordCount;
    }
}

std::string ConvertSPIRVtoWGSL(const std::vector<uint32_t>& SPIRV)
{
    tint::wgsl::writer::Options WGSLWriterOptions{};
    WGSLWriterOptions.allow_non_uniform_derivatives = true;
    WGSLWriterOptions.allowed_features              = tint::wgsl::AllowedFeatures::Everything();
    WGSLWriterOptions.minify                        = false;

    auto Result = tint::SpirvToWgsl(SPIRV, WGSLWriterOptions);
    if (Result != tint::Success)
    {
        LOG_ERROR_MESSAGE("Tint SPIR-V -> WGSL failed:\n" + Result.Failure().reason + "\n");
        return {};
    }

    return Result.Get();
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

static std::string StripNumericSuffix(std::string Name)
{
    const auto UnderscorePos = Name.find_last_of('_');
    if (UnderscorePos == std::string::npos || UnderscorePos + 1 >= Name.length())
        return Name;

    const bool AllDigits =
        std::all_of(Name.begin() + static_cast<std::string::difference_type>(UnderscorePos + 1),
                    Name.end(),
                    [](char c) { return c >= '0' && c <= '9'; });

    if (!AllDigits)
        return Name;

    Name.erase(UnderscorePos);
    return Name;
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
        return StripNumericSuffix(std::move(TypeName));
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
        auto BindingIt = ResMapping.find(ArrayElem.Name);
        if (BindingIt == ResMapping.end())
            return ResMapping.end();

        if (ArrayElem.Index < static_cast<int>(BindingIt->second.ArraySize))
        {
            ArrayIndex = static_cast<Uint32>(ArrayElem.Index);
            return BindingIt;
        }
    }

    return ResMapping.end();
}

std::string RemapWGSLResourceBindings(const std::string&         WGSL,
                                      const WGSLResourceMapping& ResMapping,
                                      const char*                EmulatedArrayIndexSuffix)
{
    tint::Source::File SrcFile("", WGSL);

    tint::wgsl::reader::Options WGSLReaderOptions{};
    WGSLReaderOptions.allowed_features = tint::wgsl::AllowedFeatures::Everything();

    tint::Program Program = tint::wgsl::reader::Parse(&SrcFile, WGSLReaderOptions);

    if (!Program.IsValid())
    {
        LOG_ERROR_MESSAGE("Tint WGSL parse failed:\n", Program.Diagnostics().Str(), "\n");
        return {};
    }

    std::unordered_map<tint::BindingPoint, tint::BindingPoint> BindingPoints;
    tint::inspector::Inspector                                 Inspector{Program};
    for (tint::inspector::EntryPoint& EntryPoint : Inspector.GetEntryPoints())
    {
        for (tint::inspector::ResourceBinding& Binding : Inspector.GetResourceBindings(EntryPoint.name))
        {
            Uint32 ArrayIndex = 0;

            auto DstBindingIt = ResMapping.find(Binding.variable_name);
            if (EmulatedArrayIndexSuffix != nullptr && DstBindingIt == ResMapping.end())
            {
                DstBindingIt = FindResourceAsArrayElement(ResMapping, EmulatedArrayIndexSuffix, Binding.variable_name, ArrayIndex);
            }

            if (DstBindingIt == ResMapping.end())
            {
                const std::string AltName = GetWGSLResourceAlternativeName(Program, Binding);
                if (!AltName.empty())
                {
                    DstBindingIt = ResMapping.find(AltName);
                    if (EmulatedArrayIndexSuffix != nullptr && DstBindingIt == ResMapping.end())
                    {
                        DstBindingIt = FindResourceAsArrayElement(ResMapping, EmulatedArrayIndexSuffix, AltName, ArrayIndex);
                    }
                }
            }

            if (DstBindingIt != ResMapping.end())
            {
                const WGSLResourceBindingInfo& DstBinding = DstBindingIt->second;
                BindingPoints.emplace(tint::BindingPoint{Binding.bind_group, Binding.binding}, tint::BindingPoint{DstBinding.Group, DstBinding.Index + ArrayIndex});
            }
            else
            {
                LOG_ERROR_MESSAGE("Binding for variable '", Binding.variable_name, "' is not found in the remap indices");
            }
        }
    }

    auto Module = tint::wgsl::reader::ProgramToIR(Program);
    if (Module != tint::Success)
    {
        LOG_ERROR_MESSAGE("Tint WGSL → IR failed:\n", Module.Failure().reason, "\n");
        return {};
    }

    if (auto Result = tint::core::ir::transform::BindingRemapper(Module.Get(), BindingPoints); Result != tint::Success)
    {
        LOG_ERROR_MESSAGE("Tint binding remap failed:\n", Result.Failure().reason, "\n");
        return {};
    }

    tint::wgsl::writer::Options WGSLWriterOptions{};
    WGSLWriterOptions.allow_non_uniform_derivatives = true;
    WGSLWriterOptions.allowed_features              = WGSLReaderOptions.allowed_features;
    WGSLWriterOptions.minify                        = false;

    auto Result = tint::wgsl::writer::WgslFromIR(Module.Get(), WGSLWriterOptions);
    if (Result != tint::Success)
    {
        LOG_ERROR_MESSAGE("Tint IR -> WGSL failed:\n", Result.Failure().reason, "\n");
        return {};
    }

    std::string PatchedWGSL = std::move(Result->wgsl);

    // If original WGSL contains shader source language definition, append it to the patched WGSL
    SHADER_SOURCE_LANGUAGE SrcLang = ParseShaderSourceLanguageDefinition(WGSL);
    if (SrcLang != SHADER_SOURCE_LANGUAGE_DEFAULT)
        AppendShaderSourceLanguageDefinition(PatchedWGSL, SrcLang);

    return PatchedWGSL;
}

} // namespace Diligent
