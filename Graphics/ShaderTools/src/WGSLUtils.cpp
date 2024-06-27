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

#include "WGSLUtils.hpp"
#include "DebugUtilities.hpp"

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

#ifdef _MSC_VER
#    pragma warning(pop)
#endif

namespace Diligent
{

std::string ConvertSPIRVtoWGSL(const std::vector<uint32_t>& SPIRV)
{
    tint::spirv::reader::Options SPIRVReaderOptions{true};
    tint::Program                Program = Read(SPIRV, SPIRVReaderOptions);

    if (!Program.IsValid())
    {
        LOG_ERROR_MESSAGE("Tint SPIR-V reader failure:\nParser: " + Program.Diagnostics().Str() + "\n");
        return {};
    }

    auto GenerationResult = tint::wgsl::writer::Generate(Program, {});
    if (GenerationResult != tint::Success)
    {
        LOG_ERROR_MESSAGE("Tint WGSL writer failure:\nGeneate: " + GenerationResult.Failure().reason.Str() + "\n");
        return {};
    }

    return GenerationResult->wgsl;
}

std::string RamapWGSLResourceBindings(const std::string& WGSL, const WGSLResourceMapping& ResMapping)
{
    tint::Source::File srcFile("", WGSL);
    tint::Program      Program = tint::wgsl::reader::Parse(&srcFile, {tint::wgsl::AllowedFeatures::Everything()});

    if (!Program.IsValid())
    {
        LOG_ERROR_MESSAGE("Tint WGSL reader failure:\nParser: ", Program.Diagnostics().Str(), "\n");
        return {};
    }

    tint::ast::transform::BindingRemapper::BindingPoints BindingPoints;

    // HLSL constant buffer names may be transformed to types, for example:
    //
    // HLSL:
    //      cbuffer CB0
    //      {
    //          float4 g_Data0;
    //      }
    // WGSL:
    //      struct CB0 {
    //        g_Data0 : vec4f,
    //      }
    //      @group(0) @binding(0) var<uniform> x_13 : CB0;

    auto FindAlternativeName = [](const tint::Program& Program, const tint::inspector::ResourceBinding& Binding) -> std::optional<std::string> {
        if (Binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kUniformBuffer ||
            Binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kStorageBuffer ||
            Binding.resource_type == tint::inspector::ResourceBinding::ResourceType::kReadOnlyStorageBuffer)
        {
            for (const auto* Variable : Program.AST().GlobalVariables())
            {
                if (Variable->HasBindingPoint())
                {
                    const auto* SemVariable = Program.Sem().Get(Variable)->As<tint::sem::GlobalVariable>();
                    if (SemVariable->Attributes().binding_point->group == Binding.bind_group &&
                        SemVariable->Attributes().binding_point->binding == Binding.binding)
                        return SemVariable->Declaration()->type->identifier->symbol.Name();
                }
            }
        }
        return std::nullopt;
    };

    tint::inspector::Inspector Inspector{Program};
    for (auto& EntryPoint : Inspector.GetEntryPoints())
    {
        for (auto& Binding : Inspector.GetResourceBindings(EntryPoint.name))
        {
            const auto& DstBindigIt = ResMapping.find(Binding.variable_name);
            if (DstBindigIt != ResMapping.end())
            {
                const auto& DstBindig = DstBindigIt->second;
                BindingPoints.emplace(tint::ast::transform::BindingPoint{Binding.bind_group, Binding.binding}, tint::ast::transform::BindingPoint{DstBindig.Group, DstBindig.Index});
            }
            else if (auto VariableName = FindAlternativeName(Program, Binding))
            {
                const auto& AltDstBindigIt = ResMapping.find(VariableName.value());
                if (AltDstBindigIt != ResMapping.end())
                {
                    const auto& DstBindig = AltDstBindigIt->second;
                    BindingPoints.emplace(tint::ast::transform::BindingPoint{Binding.bind_group, Binding.binding}, tint::ast::transform::BindingPoint{DstBindig.Group, DstBindig.Index});
                }
                else
                {
                    LOG_ERROR_MESSAGE("Binding for variable '", Binding.variable_name, "' is not found in the remap indices");
                }
            }
            else
            {
                LOG_ERROR_MESSAGE("Binding for variable '", Binding.variable_name, "' is not found in the remap indices");
            }
        }
    }

    tint::ast::transform::Manager Manager;
    tint::ast::transform::DataMap Inputs;
    tint::ast::transform::DataMap Outputs;

    Inputs.Add<tint::ast::transform::BindingRemapper::Remappings>(BindingPoints, tint::ast::transform::BindingRemapper::AccessControls{}, false);
    Manager.Add<tint::ast::transform::BindingRemapper>();
    tint::ast::transform::Output TransformResult = Manager.Run(Program, Inputs, Outputs);

    auto GenerationResult = tint::wgsl::writer::Generate(TransformResult.program, {});

    if (GenerationResult != tint::Success)
    {
        LOG_ERROR_MESSAGE("Tint WGSL writer failure:\nGeneate: ", GenerationResult.Failure().reason.Str(), "\n");
        return {};
    }

    return GenerationResult->wgsl;
}

} // namespace Diligent
