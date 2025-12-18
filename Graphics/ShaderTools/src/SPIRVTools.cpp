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

#include "spirv-tools/optimizer.hpp"

namespace Diligent
{

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

} // namespace Diligent
