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

#include "SPIRVUtils.hpp"

#if DILIGENT_USE_SPIRV_CROSS_GLSL
#    include "SPIRVShaderResources.hpp" // required for diligent_spirv_cross
#    include "spirv_glsl.hpp"

#    include <mutex>
#endif

namespace Diligent
{

void WarmUpSPIRVCrossGLSLCompiler()
{
#if DILIGENT_USE_SPIRV_CROSS_GLSL
    static std::once_flag WarmUpFlag;

    std::call_once(WarmUpFlag, []() {
        // Force SPIRV-Cross to initialize its GLSL keyword tables before async shader tasks use them.
        static const uint32_t TrivialComputeShaderSPIRV[] =
            {
                0x07230203, 0x00010000, 0, 5, 0, // Header
                0x00020011, 1,                   // OpCapability Shader
                0x0003000e, 0, 1,                // OpMemoryModel Logical GLSL450
                0x0005000f, 5, 3, 0x6e69616d, 0, // OpEntryPoint GLCompute %3 "main"
                0x00060010, 3, 17, 1, 1, 1,      // OpExecutionMode %3 LocalSize 1 1 1
                0x00020013, 1,                   // %void = OpTypeVoid
                0x00030021, 2, 1,                // %fn = OpTypeFunction %void
                0x00050036, 1, 3, 0, 2,          // %3 = OpFunction %void None %fn
                0x000200f8, 4,                   // %4 = OpLabel
                0x000100fd,                      // OpReturn
                0x00010038                       // OpFunctionEnd
            };

        try
        {
            diligent_spirv_cross::CompilerGLSL Compiler{
                TrivialComputeShaderSPIRV,
                sizeof(TrivialComputeShaderSPIRV) / sizeof(TrivialComputeShaderSPIRV[0])};

            diligent_spirv_cross::CompilerGLSL::Options Options;
            Options.version = 450;
            Compiler.set_common_options(Options);
            (void)Compiler.compile();
        }
        catch (const std::exception& e)
        {
            LOG_WARNING_MESSAGE("Failed to warm up SPIRV-Cross GLSL compiler: ", e.what());
        }
        catch (...)
        {
            LOG_WARNING_MESSAGE("Failed to warm up SPIRV-Cross GLSL compiler.");
        }
    });
#endif
}

} // namespace Diligent
