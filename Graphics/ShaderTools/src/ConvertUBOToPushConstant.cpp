/*
 *  Copyright 2019-2026 Diligent Graphics LLC
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

#include "spirv-tools/optimizer.hpp"

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

std::vector<uint32_t> ConvertUBOToPushConstants(
    const std::vector<uint32_t>& SPIRV,
    const std::string&           BlockName)
{
    spv_target_env TargetEnv = SPIRVToolsInternal::SpvTargetEnvFromSPIRV(SPIRV);

    spvtools::Optimizer optimizer{TargetEnv};

    optimizer.SetMessageConsumer(SPIRVToolsInternal::SpvOptimizerMessageConsumer);

    // Inline all function calls to eliminate OpFunctionCall instructions
    optimizer.RegisterPass(spvtools::CreateInlineExhaustivePass());

    // Inlining tends to leave junk behind. You can clean it up
    optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass());
    optimizer.RegisterPass(spvtools::CreateEliminateDeadFunctionsPass());

    // Convert the selected UBO through the public SPIRV-Tools API
    optimizer.RegisterPass(spvtools::CreateConvertUBOToPushConstantPass(BlockName));

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
