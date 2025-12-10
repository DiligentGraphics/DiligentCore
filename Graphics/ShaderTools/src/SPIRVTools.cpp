/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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

#include "SPIRVTools.hpp"
#include "DebugUtilities.hpp"

// Temporarily disable warning C4127: conditional expression is constant
// This warning is triggered by SPIRV-Tools headers in ThirdParty
#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4127)
#endif

#include "spirv-tools/optimizer.hpp"

// SPIRV-Tools internal headers for custom pass implementation
#include "source/opt/pass.h"
#include "source/opt/ir_context.h"
#include "source/opt/type_manager.h"
#include "source/opt/decoration_manager.h"

// Restore warning settings
#ifdef _MSC_VER
#    pragma warning(pop)
#endif

namespace Diligent
{

namespace
{

void SpvOptimizerMessageConsumer(
    spv_message_level_t level,
    const char* /* source */,
    const spv_position_t& /* position */,
    const char* message)
{
    const char*            LevelText   = "message";
    DEBUG_MESSAGE_SEVERITY MsgSeverity = DEBUG_MESSAGE_SEVERITY_INFO;
    switch (level)
    {
        case SPV_MSG_FATAL:
            // Unrecoverable error due to environment (e.g. out of memory)
            LevelText   = "fatal error";
            MsgSeverity = DEBUG_MESSAGE_SEVERITY_FATAL_ERROR;
            break;

        case SPV_MSG_INTERNAL_ERROR:
            // Unrecoverable error due to SPIRV-Tools internals (e.g. unimplemented feature)
            LevelText   = "internal error";
            MsgSeverity = DEBUG_MESSAGE_SEVERITY_ERROR;
            break;

        case SPV_MSG_ERROR:
            // Normal error due to user input.
            LevelText   = "error";
            MsgSeverity = DEBUG_MESSAGE_SEVERITY_ERROR;
            break;

        case SPV_MSG_WARNING:
            LevelText   = "warning";
            MsgSeverity = DEBUG_MESSAGE_SEVERITY_WARNING;
            break;

        case SPV_MSG_INFO:
            LevelText   = "info";
            MsgSeverity = DEBUG_MESSAGE_SEVERITY_INFO;
            break;

        case SPV_MSG_DEBUG:
            LevelText   = "debug";
            MsgSeverity = DEBUG_MESSAGE_SEVERITY_INFO;
            break;
    }

    if (level == SPV_MSG_FATAL || level == SPV_MSG_INTERNAL_ERROR || level == SPV_MSG_ERROR || level == SPV_MSG_WARNING)
        LOG_DEBUG_MESSAGE(MsgSeverity, "Spirv optimizer ", LevelText, ": ", message);
}

#define SPV_SPIRV_VERSION_WORD(MAJOR, MINOR) ((uint32_t(uint8_t(MAJOR)) << 16) | (uint32_t(uint8_t(MINOR)) << 8))

spv_target_env SpvTargetEnvFromSPIRV(const std::vector<uint32_t>& SPIRV)
{
    if (SPIRV.size() < 2)
    {
        // Invalid SPIRV
        return SPV_ENV_VULKAN_1_0;
    }

    switch (SPIRV[1])
    {
        case SPV_SPIRV_VERSION_WORD(1, 0): return SPV_ENV_VULKAN_1_0;
        case SPV_SPIRV_VERSION_WORD(1, 1): return SPV_ENV_VULKAN_1_0;
        case SPV_SPIRV_VERSION_WORD(1, 2): return SPV_ENV_VULKAN_1_0;
        case SPV_SPIRV_VERSION_WORD(1, 3): return SPV_ENV_VULKAN_1_1;
        case SPV_SPIRV_VERSION_WORD(1, 4): return SPV_ENV_VULKAN_1_1_SPIRV_1_4;
        case SPV_SPIRV_VERSION_WORD(1, 5): return SPV_ENV_VULKAN_1_2;
        case SPV_SPIRV_VERSION_WORD(1, 6): return SPV_ENV_VULKAN_1_3;
        default: return SPV_ENV_VULKAN_1_3;
    }
}

#undef SPV_SPIRV_VERSION_WORD

// A pass that converts a uniform buffer variable to a push constant.
// This pass:
// 1. Finds the variable with the specified block name
// 2. Changes its storage class from Uniform to PushConstant
// 3. Updates all pointer types that reference this variable
// 4. Removes Binding and DescriptorSet decorations
class ConvertUBOToPushConstantPass : public spvtools::opt::Pass
{
public:
    explicit ConvertUBOToPushConstantPass(const std::string& block_name) :
        m_BlockName{block_name}
    {}

    const char* name() const override { return "convert-ubo-to-push-constant"; }

    Status Process() override
    {
        bool modified = false;

        // Find the ID that matches the block name by searching OpName instructions
        // This could be either a variable ID or a type ID (struct type)
        uint32_t named_id = 0;
        for (auto& debug_inst : context()->module()->debugs2())
        {
            if (debug_inst.opcode() == spv::Op::OpName &&
                debug_inst.GetOperand(1).AsString() == m_BlockName)
            {
                named_id = debug_inst.GetOperand(0).AsId();
                break;
            }
        }

        if (named_id == 0)
        {
            // Block name not found
            return Status::SuccessWithoutChange;
        }

        // Check if the named_id is a variable or a type
        spvtools::opt::Instruction* target_var = nullptr;
        spvtools::opt::Instruction* named_inst = get_def_use_mgr()->GetDef(named_id);

        if (named_inst == nullptr)
        {
            return Status::SuccessWithoutChange;
        }

        if (named_inst->opcode() == spv::Op::OpVariable)
        {
            // The name refers directly to a variable
            target_var = named_inst;
        }
        else if (named_inst->opcode() == spv::Op::OpTypeStruct)
        {
            // The name refers to a struct type, we need to find the variable
            // that uses a pointer to this struct type with Uniform storage class
            uint32_t struct_type_id = named_id;

            // Search for a variable that points to this struct type with Uniform storage class
            for (auto& inst : context()->types_values())
            {
                if (inst.opcode() != spv::Op::OpVariable)
                {
                    continue;
                }

                // Get the pointer type of this variable
                spvtools::opt::Instruction* ptr_type = get_def_use_mgr()->GetDef(inst.type_id());
                if (ptr_type == nullptr || ptr_type->opcode() != spv::Op::OpTypePointer)
                {
                    continue;
                }

                // Check storage class is Uniform
                spv::StorageClass sc = static_cast<spv::StorageClass>(
                    ptr_type->GetSingleWordInOperand(0));
                if (sc != spv::StorageClass::Uniform)
                {
                    continue;
                }

                // Check if the pointee type is our struct type
                uint32_t pointee_type_id = ptr_type->GetSingleWordInOperand(1);
                if (pointee_type_id == struct_type_id)
                {
                    target_var = &inst;
                    break;
                }
            }
        }

        if (target_var == nullptr)
        {
            // Variable not found
            return Status::SuccessWithoutChange;
        }

        uint32_t target_var_id = target_var->result_id();

        // Get the pointer type of the variable
        spvtools::opt::Instruction* ptr_type_inst = get_def_use_mgr()->GetDef(target_var->type_id());
        if (ptr_type_inst == nullptr || ptr_type_inst->opcode() != spv::Op::OpTypePointer)
        {
            return Status::SuccessWithoutChange;
        }

        // Check if the storage class is Uniform
        spv::StorageClass storage_class =
            static_cast<spv::StorageClass>(ptr_type_inst->GetSingleWordInOperand(0));
        if (storage_class != spv::StorageClass::Uniform)
        {
            // Not a uniform buffer, nothing to do
            return Status::SuccessWithoutChange;
        }

        // Get the pointee type ID
        uint32_t pointee_type_id = ptr_type_inst->GetSingleWordInOperand(1);

        // Create or find a pointer type with PushConstant storage class
        spvtools::opt::analysis::TypeManager* type_mgr = context()->get_type_mgr();
        uint32_t                              new_ptr_type_id =
            type_mgr->FindPointerToType(pointee_type_id, spv::StorageClass::PushConstant);

        if (new_ptr_type_id == 0)
        {
            // Failed to create new pointer type
            return Status::Failure;
        }

        // Ensure the new pointer type is defined before the variable
        // FindPointerToType may have created it at the end, we need to move it
        spvtools::opt::Instruction* new_ptr_type_inst = get_def_use_mgr()->GetDef(new_ptr_type_id);
        if (new_ptr_type_inst != nullptr)
        {
            // Find the pointee type instruction to insert after it
            spvtools::opt::Instruction* pointee_type_inst = get_def_use_mgr()->GetDef(pointee_type_id);

            // Check if new_ptr_type_inst is after target_var in the types_values list
            bool needs_move = false;
            for (auto& inst : context()->types_values())
            {
                if (&inst == target_var)
                {
                    // Found target_var first, so new_ptr_type_inst is after it
                    needs_move = true;
                    break;
                }
                if (&inst == new_ptr_type_inst)
                {
                    // Found new_ptr_type_inst first, it's in the right position
                    needs_move = false;
                    break;
                }
            }

            if (needs_move && pointee_type_inst != nullptr)
            {
                // Move the new pointer type to right after the pointee type
                // InsertAfter will automatically remove it from its current position
                new_ptr_type_inst->InsertAfter(pointee_type_inst);
            }
        }

        // Update the variable's type to the new pointer type
        target_var->SetResultType(new_ptr_type_id);

        // Also update the storage class operand of OpVariable itself
        // OpVariable has the storage class as the first operand (index 0)
        target_var->SetInOperand(0, {static_cast<uint32_t>(spv::StorageClass::PushConstant)});

        context()->UpdateDefUse(target_var);
        modified = true;

        // Propagate storage class change to all users of this variable
        std::set<uint32_t>                       seen;
        std::vector<spvtools::opt::Instruction*> users;
        get_def_use_mgr()->ForEachUser(target_var, [&users](spvtools::opt::Instruction* user) {
            users.push_back(user);
        });

        for (spvtools::opt::Instruction* user : users)
        {
            modified |= PropagateStorageClass(user, &seen);
        }

        // Remove Binding and DescriptorSet decorations from the variable
        auto* deco_mgr = context()->get_decoration_mgr();
        deco_mgr->RemoveDecorationsFrom(target_var_id, [](const spvtools::opt::Instruction& inst) {
            if (inst.opcode() != spv::Op::OpDecorate)
            {
                return false;
            }
            spv::Decoration decoration =
                static_cast<spv::Decoration>(inst.GetSingleWordInOperand(1));
            return decoration == spv::Decoration::Binding ||
                decoration == spv::Decoration::DescriptorSet;
        });

        return modified ? Status::SuccessWithChange : Status::SuccessWithoutChange;
    }

    spvtools::opt::IRContext::Analysis GetPreservedAnalyses() override
    {
        // This pass modifies types and decorations
        return spvtools::opt::IRContext::kAnalysisNone;
    }

private:
    // Recursively updates the storage class of pointer types used by instructions
    // that reference the target variable.
    bool PropagateStorageClass(spvtools::opt::Instruction* inst, std::set<uint32_t>* seen)
    {
        if (!IsPointerResultType(inst))
        {
            return false;
        }

        // Already has the correct storage class
        if (IsPointerToStorageClass(inst, spv::StorageClass::PushConstant))
        {
            if (inst->opcode() == spv::Op::OpPhi)
            {
                if (!seen->insert(inst->result_id()).second)
                {
                    return false;
                }
            }

            bool                                     modified = false;
            std::vector<spvtools::opt::Instruction*> users;
            get_def_use_mgr()->ForEachUser(inst, [&users](spvtools::opt::Instruction* user) {
                users.push_back(user);
            });
            for (spvtools::opt::Instruction* user : users)
            {
                modified |= PropagateStorageClass(user, seen);
            }

            if (inst->opcode() == spv::Op::OpPhi)
            {
                seen->erase(inst->result_id());
            }
            return modified;
        }

        // Handle instructions that produce pointer results
        switch (inst->opcode())
        {
            case spv::Op::OpAccessChain:
            case spv::Op::OpPtrAccessChain:
            case spv::Op::OpInBoundsAccessChain:
            case spv::Op::OpInBoundsPtrAccessChain:
            case spv::Op::OpCopyObject:
            case spv::Op::OpPhi:
            case spv::Op::OpSelect:
                ChangeResultStorageClass(inst);
                {
                    std::vector<spvtools::opt::Instruction*> users;
                    get_def_use_mgr()->ForEachUser(inst, [&users](spvtools::opt::Instruction* user) {
                        users.push_back(user);
                    });
                    for (spvtools::opt::Instruction* user : users)
                    {
                        PropagateStorageClass(user, seen);
                    }
                }
                return true;

            case spv::Op::OpLoad:
            case spv::Op::OpStore:
            case spv::Op::OpCopyMemory:
            case spv::Op::OpCopyMemorySized:
                // These don't produce pointer results that need updating
                return false;

            default:
                return false;
        }
    }

    // Changes the result type of an instruction to use the new storage class.
    void ChangeResultStorageClass(spvtools::opt::Instruction* inst)
    {
        spvtools::opt::analysis::TypeManager* type_mgr         = context()->get_type_mgr();
        spvtools::opt::Instruction*           result_type_inst = get_def_use_mgr()->GetDef(inst->type_id());

        if (result_type_inst->opcode() != spv::Op::OpTypePointer)
        {
            return;
        }

        uint32_t pointee_type_id = result_type_inst->GetSingleWordInOperand(1);
        uint32_t new_result_type_id =
            type_mgr->FindPointerToType(pointee_type_id, spv::StorageClass::PushConstant);

        inst->SetResultType(new_result_type_id);
        context()->UpdateDefUse(inst);
    }

    // Checks if the instruction result type is a pointer.
    bool IsPointerResultType(spvtools::opt::Instruction* inst)
    {
        if (inst->type_id() == 0)
        {
            return false;
        }

        spvtools::opt::Instruction* type_def = get_def_use_mgr()->GetDef(inst->type_id());
        return type_def != nullptr && type_def->opcode() == spv::Op::OpTypePointer;
    }

    // Checks if the instruction result type is a pointer to the specified storage class.
    bool IsPointerToStorageClass(spvtools::opt::Instruction* inst, spv::StorageClass storage_class)
    {
        if (inst->type_id() == 0)
        {
            return false;
        }

        spvtools::opt::Instruction* type_def = get_def_use_mgr()->GetDef(inst->type_id());
        if (type_def == nullptr || type_def->opcode() != spv::Op::OpTypePointer)
        {
            return false;
        }

        spv::StorageClass pointer_storage_class =
            static_cast<spv::StorageClass>(type_def->GetSingleWordInOperand(0));
        return pointer_storage_class == storage_class;
    }

    std::string m_BlockName;
};

} // namespace

std::vector<uint32_t> OptimizeSPIRV(const std::vector<uint32_t>& SrcSPIRV, spv_target_env TargetEnv, SPIRV_OPTIMIZATION_FLAGS Passes)
{
    VERIFY_EXPR(Passes != SPIRV_OPTIMIZATION_FLAG_NONE);

    if (TargetEnv == SPV_ENV_MAX)
        TargetEnv = SpvTargetEnvFromSPIRV(SrcSPIRV);

    spvtools::Optimizer SpirvOptimizer(TargetEnv);
    SpirvOptimizer.SetMessageConsumer(SpvOptimizerMessageConsumer);

    spvtools::OptimizerOptions Options;
#ifndef DILIGENT_DEVELOPMENT
    // Do not run validator in release build
    Options.set_run_validator(false);
#endif

    // SPIR-V bytecode generated from HLSL must be legalized to
    // turn it into a valid vulkan SPIR-V shader.
    if (Passes & SPIRV_OPTIMIZATION_FLAG_LEGALIZATION)
    {
        SpirvOptimizer.RegisterLegalizationPasses();

        spvtools::ValidatorOptions ValidatorOptions;
        ValidatorOptions.SetBeforeHlslLegalization(true);
        Options.set_validator_options(ValidatorOptions);
    }

    if (Passes & SPIRV_OPTIMIZATION_FLAG_PERFORMANCE)
    {
        SpirvOptimizer.RegisterPerformancePasses();
    }

    if (Passes & SPIRV_OPTIMIZATION_FLAG_STRIP_REFLECTION)
    {
        // Decorations defined in SPV_GOOGLE_hlsl_functionality1 are the only instructions
        // removed by strip-reflect-info pass. SPIRV offsets become INVALID after this operation.
        SpirvOptimizer.RegisterPass(spvtools::CreateStripReflectInfoPass());
    }

    std::vector<uint32_t> OptimizedSPIRV;
    if (!SpirvOptimizer.Run(SrcSPIRV.data(), SrcSPIRV.size(), &OptimizedSPIRV, Options))
        OptimizedSPIRV.clear();

    return OptimizedSPIRV;
}

std::vector<uint32_t> PatchSPIRVConvertUniformBufferToPushConstant(
    const std::vector<uint32_t>& SPIRV,
    const std::string&           BlockName)
{
    spv_target_env TargetEnv = SpvTargetEnvFromSPIRV(SPIRV);

    spvtools::Optimizer optimizer(TargetEnv);

    optimizer.SetMessageConsumer(SpvOptimizerMessageConsumer);

    // Register the pass to convert UBO to push constant using custom out-of-tree pass
    optimizer.RegisterPass(spvtools::Optimizer::PassToken(
        std::make_unique<ConvertUBOToPushConstantPass>(BlockName)));

    spvtools::OptimizerOptions options;
#ifndef DILIGENT_DEVELOPMENT
    // Do not run validator in release build
    options.set_run_validator(false);
#else
    // Run validator in debug build
    options.set_run_validator(true);
#endif

    std::vector<uint32_t> result;
    if (!optimizer.Run(SPIRV.data(), SPIRV.size(), &result, options))
    {
        return {};
    }
    return result;
}

} // namespace Diligent
