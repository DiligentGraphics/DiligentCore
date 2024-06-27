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

} // namespace Diligent
