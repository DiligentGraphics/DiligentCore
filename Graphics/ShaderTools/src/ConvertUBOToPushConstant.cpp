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

#include <vector>
#include <unordered_set>

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

    static spvtools::Optimizer::PassToken Create(std::string BlockName)
    {
        return spvtools::Optimizer::PassToken{
            std::make_unique<ConvertUBOToPushConstantPass>(BlockName)};
    }

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
            LOG_ERROR_MESSAGE("Failed to convert UBO block '", m_BlockName, "': no OpName found.");
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
                if (IsUBOBlockType(pointee_type_id))
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
                        if (IsUBOBlockType(pointee_type_id))
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
            LOG_ERROR_MESSAGE("Failed to convert UBO block '", m_BlockName, "': no matching UniformBuffer found.");
            return Status::Failure;
        }

        uint32_t target_var_id = target_var->result_id();

        // Get the pointer type of the variable (we already verified it above, but get it again for consistency)
        spvtools::opt::Instruction* ptr_type_inst = get_def_use_mgr()->GetDef(target_var->type_id());
        if (ptr_type_inst == nullptr || ptr_type_inst->opcode() != spv::Op::OpTypePointer)
        {
            LOG_ERROR_MESSAGE("Failed to convert UBO block '", m_BlockName, "': target variable has unexpected type.");
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
            LOG_ERROR_MESSAGE("Failed to convert UBO block '", m_BlockName, "': could not create PushConstant pointer type.");
            return Status::Failure;
        }

        // IMPORTANT: FindPointerToType() may create a new type instruction at the end of
        // the types_values section, or it may return an existing type. In either case,
        // SPIR-V requires all IDs to be defined before use (SSA form).
        //
        // We must ensure the pointer type instruction appears BEFORE the OpVariable
        // that will reference it. However, we must NOT move an existing type that is
        // already correctly positioned, as that could break other instructions that
        // depend on it being defined before their use.
        spvtools::opt::Instruction* new_ptr_type_inst = get_def_use_mgr()->GetDef(new_ptr_type_id);
        EnsureTypeBeforeUseInTypesValues(new_ptr_type_inst, target_var);

        // Update the variable's type to the new pointer type
        target_var->SetResultType(new_ptr_type_id);

        // Also update the storage class operand of OpVariable itself
        // OpVariable has the storage class as the first operand (index 0)
        target_var->SetInOperand(0, {static_cast<uint32_t>(spv::StorageClass::PushConstant)});

        context()->UpdateDefUse(target_var);
        modified = true;

        // IMPORTANT: We changed pointer types + storage class; TypeManager and other analyses may be stale.
        // Invalidate analyses that can cache type information and def-use.
        context()->InvalidateAnalyses(spvtools::opt::IRContext::kAnalysisTypes |
                                      spvtools::opt::IRContext::kAnalysisDefUse |
                                      spvtools::opt::IRContext::kAnalysisDecorations);

        // Propagate storage class change to all users of this variable
        std::vector<spvtools::opt::Instruction*> users;
        get_def_use_mgr()->ForEachUser(target_var, [&users](spvtools::opt::Instruction* user) {
            users.push_back(user);
        });

        std::unordered_set<uint32_t> visited;
        for (spvtools::opt::Instruction* user : users)
        {
            modified |= PropagateStorageClass(*user, visited);
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

        // Invalidate decoration analysis since we modified decorations
        context()->InvalidateAnalyses(spvtools::opt::IRContext::kAnalysisDecorations);

        return modified ? Status::SuccessWithChange : Status::SuccessWithoutChange;
    }

    spvtools::opt::IRContext::Analysis GetPreservedAnalyses() override
    {
        // This pass modifies types and decorations
        return spvtools::opt::IRContext::kAnalysisNone;
    }

private:
    // Returns true if instruction 'a' appears before instruction 'b' in module->types_values().
    // This is used to check if a type definition comes before its use in the SPIR-V module.
    bool ComesBeforeInTypesValues(const spvtools::opt::Instruction* a,
                                  const spvtools::opt::Instruction* b) const
    {
        if (a == nullptr || b == nullptr)
            return false;
        if (a == b)
            return true; // Same instruction, no reordering needed

        bool seen_a = false;
        for (auto& inst : context()->module()->types_values())
        {
            if (&inst == a)
            {
                seen_a = true;
            }
            else if (&inst == b)
            {
                return seen_a;
            }
        }
        // If either is not in types_values(), be conservative and return false.
        return false;
    }

    // Ensures 'type_inst' is placed before 'use_inst' in the types_values section.
    // Only moves the instruction when type_inst currently appears after use_inst.
    // This is necessary because SPIR-V requires all IDs to be defined before use (SSA form).
    //
    // IMPORTANT: This function only moves type_inst forward (earlier in the list), never backward.
    // If type_inst is already before use_inst, no action is taken to avoid breaking existing uses.
    //
    // If use_inst is not in types_values (e.g., it's in a function body), no action is taken
    // because the entire types_values section appears before any function in the module.
    void EnsureTypeBeforeUseInTypesValues(spvtools::opt::Instruction* type_inst,
                                          spvtools::opt::Instruction* use_inst)
    {
        if (type_inst == nullptr || use_inst == nullptr)
            return;

        // Check if use_inst is in types_values section.
        // If it's not (e.g., it's in a function body like OpAccessChain),
        // we don't need to move type_inst because the entire types_values section
        // appears before any function in the module.
        bool use_in_types_values = false;
        for (auto& inst : context()->module()->types_values())
        {
            if (&inst == use_inst)
            {
                use_in_types_values = true;
                break;
            }
        }

        if (!use_in_types_values)
            return;

        // If type_inst already comes before use_inst, do nothing.
        // This is critical: moving an existing type that's already correctly positioned
        // could break other instructions that depend on it.
        if (ComesBeforeInTypesValues(type_inst, use_inst))
            return;

        // type_inst appears after use_inst (or not found), so we need to move it.
        // Insert it immediately before the use_inst to satisfy the SSA requirement.
        type_inst->RemoveFromList();
        type_inst->InsertBefore(use_inst);
    }

    // Recursively updates the storage class of pointer types used by instructions
    // that reference the target variable.
    bool PropagateStorageClass(spvtools::opt::Instruction& inst, std::unordered_set<uint32_t>& visited)
    {
        if (!IsPointerResultType(inst))
        {
            return false;
        }

        // Use a "visited" set keyed by result_id for ANY pointer-producing instruction.
        // This avoids infinite recursion in pointer SSA loops.
        if (inst.result_id() != 0)
        {
            if (!visited.insert(inst.result_id()).second)
                return false;
        }

        // Already has the correct storage class
        if (IsPointerToStorageClass(inst, spv::StorageClass::PushConstant))
        {
            std::vector<spvtools::opt::Instruction*> users;
            get_def_use_mgr()->ForEachUser(&inst, [&users](spvtools::opt::Instruction* user) {
                users.push_back(user);
            });

            bool modified = false;
            for (spvtools::opt::Instruction* user : users)
            {
                if (PropagateStorageClass(*user, visited))
                    modified = true;
            }

            return modified;
        }

        // Handle instructions that produce pointer results
        // This switch covers the common pointer-producing opcodes.
        // Reference: SPIRV-Tools fix_storage_class.cpp
        switch (inst.opcode())
        {
            case spv::Op::OpAccessChain:
            case spv::Op::OpPtrAccessChain:
            case spv::Op::OpInBoundsAccessChain:
            case spv::Op::OpInBoundsPtrAccessChain:
            case spv::Op::OpCopyObject:
            case spv::Op::OpPhi:
            case spv::Op::OpSelect:
            case spv::Op::OpBitcast:
            case spv::Op::OpUndef:
            case spv::Op::OpConstantNull:
                ChangeResultStorageClass(inst);
                {
                    std::vector<spvtools::opt::Instruction*> users;
                    get_def_use_mgr()->ForEachUser(&inst, [&users](spvtools::opt::Instruction* user) {
                        users.push_back(user);
                    });
                    for (spvtools::opt::Instruction* user : users)
                    {
                        PropagateStorageClass(*user, visited);
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
            case spv::Op::OpVariable:
                // These don't produce pointer results that need updating,
                // or the result type is independent of the operand's storage class.
                return false;

            default:
                // Unexpected pointer-producing instruction. This may indicate
                // a new SPIR-V extension or pattern not yet handled.
                UNEXPECTED("Unexpected instruction with pointer result type: opcode ", static_cast<uint32_t>(inst.opcode()));
                return false;
        }
    }

    // Changes the result type of an instruction to use the new storage class.
    void ChangeResultStorageClass(spvtools::opt::Instruction& inst)
    {
        spvtools::opt::analysis::TypeManager* type_mgr         = context()->get_type_mgr();
        spvtools::opt::Instruction*           result_type_inst = get_def_use_mgr()->GetDef(inst.type_id());

        if (result_type_inst == nullptr || result_type_inst->opcode() != spv::Op::OpTypePointer)
            return;

        uint32_t pointee_type_id = result_type_inst->GetSingleWordInOperand(1);
        uint32_t new_result_type_id =
            type_mgr->FindPointerToType(pointee_type_id, spv::StorageClass::PushConstant);

        if (new_result_type_id == 0)
            return;

        // Ensure the pointer type is properly positioned in the types section.
        // FindPointerToType may return an existing type or create a new one at the end.
        // If inst is in types_values (e.g., OpConstantNull), we need to ensure the type
        // is defined before inst. If inst is in a function body (e.g., OpAccessChain),
        // no reordering is needed since types_values always precedes functions.
        spvtools::opt::Instruction* new_type_inst = get_def_use_mgr()->GetDef(new_result_type_id);
        EnsureTypeBeforeUseInTypesValues(new_type_inst, &inst);

        inst.SetResultType(new_result_type_id);
        context()->UpdateDefUse(&inst);
    }

    // Checks if the instruction result type is a pointer.
    bool IsPointerResultType(const spvtools::opt::Instruction& inst) const
    {
        if (inst.type_id() == 0)
        {
            return false;
        }

        spvtools::opt::Instruction* type_def = get_def_use_mgr()->GetDef(inst.type_id());
        return type_def != nullptr && type_def->opcode() == spv::Op::OpTypePointer;
    }

    // Checks if the instruction result type is a pointer to the specified storage class.
    bool IsPointerToStorageClass(const spvtools::opt::Instruction& inst, spv::StorageClass storage_class) const
    {
        if (inst.type_id() == 0)
        {
            return false;
        }

        spvtools::opt::Instruction* type_def = get_def_use_mgr()->GetDef(inst.type_id());
        if (type_def == nullptr || type_def->opcode() != spv::Op::OpTypePointer)
        {
            return false;
        }

        spv::StorageClass pointer_storage_class =
            static_cast<spv::StorageClass>(type_def->GetSingleWordInOperand(0));
        return pointer_storage_class == storage_class;
    }

    bool HasDecoration(uint32_t id, spv::Decoration deco) const
    {
        bool found = false;
        get_decoration_mgr()->ForEachDecoration(
            id, static_cast<uint32_t>(deco),
            [&found](const spvtools::opt::Instruction&) {
                found = true;
            });
        return found;
    }

    // Checks if a type has the Block decoration (but not the BufferBlock),
    // which identifies it as a UBO struct type.
    bool IsUBOBlockType(uint32_t type_id) const
    {
        return HasDecoration(type_id, spv::Decoration::Block) &&
            !HasDecoration(type_id, spv::Decoration::BufferBlock);
    }

    std::string m_BlockName;
};

} // namespace

std::vector<uint32_t> ConvertUBOToPushConstants(
    const std::vector<uint32_t>& SPIRV,
    const std::string&           BlockName)
{
    spv_target_env TargetEnv = SPIRVToolsInternal::SpvTargetEnvFromSPIRV(SPIRV);

    spvtools::Optimizer optimizer{TargetEnv};

    optimizer.SetMessageConsumer(SPIRVToolsInternal::SpvOptimizerMessageConsumer);

    // Register the pass to convert UBO to push constant using custom out-of-tree pass
    optimizer.RegisterPass(ConvertUBOToPushConstantPass::Create(BlockName));

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
