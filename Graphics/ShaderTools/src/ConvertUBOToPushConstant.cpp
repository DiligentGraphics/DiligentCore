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

namespace SPIRVToolsInternal
{

//Forward declarations

void SpvOptimizerMessageConsumer(
    spv_message_level_t level,
    const char* /* source */,
    const spv_position_t& /* position */,
    const char* message);

spv_target_env SpvTargetEnvFromSPIRV(const std::vector<uint32_t>& SPIRV);

} // namespace SPIRVToolsInternal

namespace
{

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

        // Collect all IDs that match the block name by searching OpName instructions.
        // Multiple OpName instructions may have the same name, so we need to check all of them
        // to find the one that refers to a UniformBuffer (Uniform storage class + Block decoration).
        std::vector<uint32_t> candidate_ids;
        for (auto& debug_inst : context()->module()->debugs2())
        {
            if (debug_inst.opcode() == spv::Op::OpName &&
                debug_inst.GetOperand(1).AsString() == m_BlockName)
            {
                candidate_ids.push_back(debug_inst.GetOperand(0).AsId());
            }
        }

        if (candidate_ids.empty())
        {
            // Block name not found
            return Status::Failure;
        }

        // Try each candidate ID to find a UniformBuffer
        spvtools::opt::Instruction* target_var = nullptr;
        for (uint32_t named_id : candidate_ids)
        {
            spvtools::opt::Instruction* named_inst = get_def_use_mgr()->GetDef(named_id);
            if (named_inst == nullptr)
            {
                continue;
            }

            if (named_inst->opcode() == spv::Op::OpVariable)
            {
                // The name refers directly to a variable - check if it's a UniformBuffer
                spvtools::opt::Instruction* var_inst = named_inst;

                // Get the pointer type of the variable
                spvtools::opt::Instruction* ptr_type_inst = get_def_use_mgr()->GetDef(var_inst->type_id());
                if (ptr_type_inst == nullptr || ptr_type_inst->opcode() != spv::Op::OpTypePointer)
                {
                    continue;
                }

                // Check if the storage class is Uniform
                spv::StorageClass storage_class =
                    static_cast<spv::StorageClass>(ptr_type_inst->GetSingleWordInOperand(0));
                if (storage_class != spv::StorageClass::Uniform)
                {
                    continue;
                }

                // Get the pointee type ID and verify it has Block decoration
                uint32_t pointee_type_id = ptr_type_inst->GetSingleWordInOperand(1);
                if (HasBlockDecoration(pointee_type_id))
                {
                    // Found a UniformBuffer!
                    target_var = var_inst;
                    break;
                }
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
                        // Verify it has Block decoration
                        if (HasBlockDecoration(pointee_type_id))
                        {
                            // Found a UniformBuffer!
                            target_var = &inst;
                            break;
                        }
                    }
                }

                if (target_var != nullptr)
                {
                    break;
                }
            }
        }

        if (target_var == nullptr)
        {
            // No UniformBuffer found with the given block name
            return Status::Failure;
        }

        uint32_t target_var_id = target_var->result_id();

        // Get the pointer type of the variable (we already verified it above, but get it again for consistency)
        spvtools::opt::Instruction* ptr_type_inst = get_def_use_mgr()->GetDef(target_var->type_id());
        if (ptr_type_inst == nullptr || ptr_type_inst->opcode() != spv::Op::OpTypePointer)
        {
            return Status::Failure;
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

        // Note: We don't need to manually reorder the pointer type instruction.
        // SPIR-V does not require types to appear in any specific relative order,
        // as long as they are valid type declarations. FindPointerToType handles
        // type creation correctly, and the SPIRV-Tools framework manages instruction
        // ordering appropriately.

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
        // This switch covers the common pointer-producing opcodes.
        // Reference: SPIRV-Tools fix_storage_class.cpp
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

            case spv::Op::OpFunctionCall:
                // We cannot be sure of the actual connection between the storage class
                // of the parameter and the storage class of the result, so we should not
                // do anything. If the result type needs to be fixed, the function call
                // should be inlined first.
                return false;

            case spv::Op::OpLoad:
            case spv::Op::OpStore:
            case spv::Op::OpCopyMemory:
            case spv::Op::OpCopyMemorySized:
            case spv::Op::OpImageTexelPointer:
            case spv::Op::OpBitcast:
            case spv::Op::OpVariable:
                // These don't produce pointer results that need updating,
                // or the result type is independent of the operand's storage class.
                return false;

            default:
                // Unexpected pointer-producing instruction. This may indicate
                // a new SPIR-V extension or pattern not yet handled.
                UNEXPECTED("Unexpected instruction with pointer result type: opcode ", static_cast<uint32_t>(inst->opcode()));
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

    // Checks if a type has the Block decoration, which identifies it as a UBO struct type.
    bool HasBlockDecoration(uint32_t type_id)
    {
        bool has_block = false;
        get_decoration_mgr()->ForEachDecoration(
            type_id, static_cast<uint32_t>(spv::Decoration::Block),
            [&has_block](const spvtools::opt::Instruction&) {
                has_block = true;
            });
        return has_block;
    }

    std::string m_BlockName;
};

} // namespace

std::vector<uint32_t> ConvertUBOToPushConstants(
    const std::vector<uint32_t>& SPIRV,
    const std::string&           BlockName)
{
    using namespace SPIRVToolsInternal;

    spv_target_env TargetEnv = SpvTargetEnvFromSPIRV(SPIRV);

    spvtools::Optimizer optimizer(TargetEnv);

    optimizer.SetMessageConsumer(SpvOptimizerMessageConsumer);

    // Register the pass to convert UBO to push constant using custom out-of-tree pass
    optimizer.RegisterPass(spvtools::Optimizer::PassToken(
        std::make_unique<ConvertUBOToPushConstantPass>(BlockName)));

    spvtools::OptimizerOptions options;
#ifdef DILIGENT_DEVELOPMENT
    // Run validator in debug build
    options.set_run_validator(true);
#else
    // Do not run validator in release build
    options.set_run_validator(false);
#endif

    std::vector<uint32_t> result;
    if (!optimizer.Run(SPIRV.data(), SPIRV.size(), &result, options))
    {
        return {};
    }
    return result;
}

} // namespace Diligent
