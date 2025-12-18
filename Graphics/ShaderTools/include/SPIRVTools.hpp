/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#pragma once

#include <vector>
#include <string>

#include "../../Primitives/interface/Errors.hpp"
#include "FlagEnum.h"

#include "spirv-tools/libspirv.h"

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
    
    spv_target_env SpvTargetEnvFromSPIRV(const std::vector<uint32_t>& SPIRV)
    {
        if (SPIRV.size() < 2)
        {
            // Invalid SPIRV
            return SPV_ENV_VULKAN_1_0;
        }
    
        #define SPV_SPIRV_VERSION_WORD(MAJOR, MINOR) ((uint32_t(uint8_t(MAJOR)) << 16) | (uint32_t(uint8_t(MINOR)) << 8))
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
        #undef SPV_SPIRV_VERSION_WORD
    }
    
} // namespace

enum SPIRV_OPTIMIZATION_FLAGS : Uint32
{
    SPIRV_OPTIMIZATION_FLAG_NONE             = 0u,
    SPIRV_OPTIMIZATION_FLAG_LEGALIZATION     = 1u << 0u,
    SPIRV_OPTIMIZATION_FLAG_PERFORMANCE      = 1u << 1u,
    SPIRV_OPTIMIZATION_FLAG_STRIP_REFLECTION = 1u << 2u
};
DEFINE_FLAG_ENUM_OPERATORS(SPIRV_OPTIMIZATION_FLAGS);


std::vector<uint32_t> OptimizeSPIRV(const std::vector<uint32_t>& SrcSPIRV,
                                    spv_target_env               TargetEnv,
                                    SPIRV_OPTIMIZATION_FLAGS     Passes);

/// Converts a uniform buffer variable to a push constant in SPIR-V bytecode.
/// This function modifies the storage class of the specified variable from Uniform to PushConstant,
/// and removes Binding and DescriptorSet decorations.
///
/// \param [in] SPIRV      - Source SPIR-V bytecode
/// \param [in] BlockName  - Name of the uniform buffer block to convert
/// \return Modified SPIR-V bytecode, or empty vector on failure
std::vector<uint32_t> ConvertUBOToPushConstants(const std::vector<uint32_t>& SPIRV,
                                                const std::string&           BlockName);

} // namespace Diligent
