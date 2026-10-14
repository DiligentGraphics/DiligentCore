/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

TEST(SPIRVUtils, PatchImageFormatsIgnoresInstructionOperands)
{
    // Use fixed bytecode so the constant's words cannot be changed or removed by a
    // shader compiler. Its low word is OpTypeImage (25), and its high word is the
    // image type ID (3). Scanning operands as opcodes would overwrite the image's
    // format location with the signedness operand of OpTypeInt below.
    // clang-format off
    const std::vector<uint32_t> SPIRV =
    {
        0x07230203, 0x00010000, 0, 12, 0,    // Header
        0x00020011, 1,                       // OpCapability Shader
        0x00020011, 11,                      // OpCapability Int64
        0x0003000e, 0, 1,                    // OpMemoryModel Logical GLSL450
        0x0005000f, 5, 10, 0x6e69616d, 0,    // OpEntryPoint GLCompute %10 "main"
        0x00060010, 10, 17, 1, 1, 1,         // OpExecutionMode %10 LocalSize 1 1 1
        0x00030005, 5, 0x00786554,           // OpName %5 "Tex"
        0x00040047, 5, 34, 0,                // OpDecorate %5 DescriptorSet 0
        0x00040047, 5, 33, 0,                // OpDecorate %5 Binding 0
        0x00020013, 1,                       // %1 = OpTypeVoid
        0x00030016, 2, 32,                   // %2 = OpTypeFloat 32
        0x00090019, 3, 2, 1, 0, 0, 0, 2, 0,  // %3 = OpTypeImage %2 2D 0 0 0 2 Unknown
        0x00040020, 4, 0, 3,                 // %4 = OpTypePointer UniformConstant %3
        0x0004003b, 4, 5, 0,                 // %5 = OpVariable %4 UniformConstant
        0x00040015, 6, 64, 0,                // %6 = OpTypeInt 64 0
        0x0005002b, 6, 7, 25, 3,             // %7 = OpConstant %6 0x0000000300000019
        0x00030021, 8, 1,                    // %8 = OpTypeFunction %1
        0x00040015, 9, 32, 0,                // %9 = OpTypeInt 32 0
        0x00050036, 1, 10, 0, 8,             // %10 = OpFunction %1 None %8
        0x000200f8, 11,                      // %11 = OpLabel
        0x000100fd,                          // OpReturn
        0x00010038                           // OpFunctionEnd
    };
    // clang-format on

    constexpr size_t ImageFormatWord = 47;
    ASSERT_EQ(SPIRV[ImageFormatWord], 0u); // ImageFormatUnknown

    std::vector<uint32_t> ExpectedSPIRV = SPIRV;
    ExpectedSPIRV[ImageFormatWord]      = 1; // ImageFormatRgba32f

    std::unordered_map<HashMapStringKey, TEXTURE_FORMAT> ImageFormats;
    ImageFormats.emplace("Tex", TEX_FORMAT_RGBA32_FLOAT);
    const auto PatchedSPIRV = PatchImageFormats(SPIRV, ImageFormats);
    EXPECT_EQ(PatchedSPIRV, ExpectedSPIRV);
}

} // namespace
