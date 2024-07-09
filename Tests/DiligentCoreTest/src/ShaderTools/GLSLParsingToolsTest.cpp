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

#include "GLSLParsingTools.hpp"

#include "TestingEnvironment.hpp"
#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

TEST(GLSLParsingToolsTest, ExtractGLSLImageFormatFromComment)
{
    auto Test = [](const std::string& Str, const std::string& RefFormat) {
        std::string Format = Parsing::ExtractGLSLImageFormatFromComment(Str.begin(), Str.end());
        EXPECT_STREQ(Format.c_str(), RefFormat.c_str()) << Str;
    };
    Test("", "");
    Test(" ", "");
    Test(" format", "");
    Test(" /format", "");
    Test(" // ", "");
    Test(" /* ", "");
    Test(" // forma", "");
    Test(" /* form", "");
    Test(" // forma ", "");
    Test(" /* form ", "");
    Test(" // format", "");
    Test(" /* format", "");
    Test(" // format-", "");
    Test(" /* format:", "");
    Test(" // format=12", "");
    Test(" /* format=34", "");
    Test(" // format=rgba", "rgba");
    Test(" /* format=rg32f", "rg32f");
    Test(" // format=rg8u ", "rg8u");
    Test(" /* format=rg16f ", "rg16f");
    Test(" // format=rg16u\n", "rg16u");
    Test(" /* format=r16f\n", "r16f");
    Test(" /* format=r16f*/", "r16f");
    Test(" /* format=r16f */", "r16f");
    Test(" /* format =rg16f ", "rg16f");
    Test(" // format =rg16u\n", "rg16u");
    Test(" /* format= rg16f ", "rg16f");
    Test(" // format= rg16u\n", "rg16u");
    Test(" /* format = rg16f ", "rg16f");
    Test(" // format = rg16u\n", "rg16u");
}


TEST(GLSLParsingToolsTest, ParseGLSLImageFormat)
{
    EXPECT_EQ(Parsing::ParseGLSLImageFormat(""), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(Parsing::ParseGLSLImageFormat(""), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(Parsing::ParseGLSLImageFormat(""), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(Parsing::ParseGLSLImageFormat(""), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(Parsing::ParseGLSLImageFormat(""), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(Parsing::ParseGLSLImageFormat(""), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(Parsing::ParseGLSLImageFormat(""), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(Parsing::ParseGLSLImageFormat(""), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(Parsing::ParseGLSLImageFormat(""), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(Parsing::ParseGLSLImageFormat(""), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(Parsing::ParseGLSLImageFormat(""), TEX_FORMAT_UNKNOWN);

    const std::vector<std::pair<const char*, TEXTURE_FORMAT>> TestFormats = {
        {"", TEX_FORMAT_UNKNOWN},
        {"abc", TEX_FORMAT_UNKNOWN},
        {"123", TEX_FORMAT_UNKNOWN},
        {"r", TEX_FORMAT_UNKNOWN},
        {"rg", TEX_FORMAT_UNKNOWN},
        {"rgb", TEX_FORMAT_UNKNOWN},
        {"rgba", TEX_FORMAT_UNKNOWN},
        {"rgbaw", TEX_FORMAT_UNKNOWN},
        {"r0", TEX_FORMAT_UNKNOWN},
        {"r9", TEX_FORMAT_UNKNOWN},
        {"r1000", TEX_FORMAT_UNKNOWN},

        {"r8", TEX_FORMAT_R8_UNORM},
        {"rg8", TEX_FORMAT_RG8_UNORM},
        {"rgba8", TEX_FORMAT_RGBA8_UNORM},

        {"r16", TEX_FORMAT_R16_UNORM},
        {"rg16", TEX_FORMAT_RG16_UNORM},
        {"rgba16", TEX_FORMAT_RGBA16_UNORM},

        {"r16f", TEX_FORMAT_R16_FLOAT},
        {"rg16f", TEX_FORMAT_RG16_FLOAT},
        {"rgba16f", TEX_FORMAT_RGBA16_FLOAT},

        {"r32f", TEX_FORMAT_R32_FLOAT},
        {"rg32f", TEX_FORMAT_RG32_FLOAT},
        {"rgba32f", TEX_FORMAT_RGBA32_FLOAT},

        {"r8i", TEX_FORMAT_R8_SINT},
        {"rg8i", TEX_FORMAT_RG8_SINT},
        {"rgba8i", TEX_FORMAT_RGBA8_SINT},

        {"r16i", TEX_FORMAT_R16_SINT},
        {"rg16i", TEX_FORMAT_RG16_SINT},
        {"rgba16i", TEX_FORMAT_RGBA16_SINT},

        {"r32i", TEX_FORMAT_R32_SINT},
        {"rg32i", TEX_FORMAT_RG32_SINT},
        {"rgba32i", TEX_FORMAT_RGBA32_SINT},

        {"r8ui", TEX_FORMAT_R8_UINT},
        {"rg8ui", TEX_FORMAT_RG8_UINT},
        {"rgba8ui", TEX_FORMAT_RGBA8_UINT},

        {"r16ui", TEX_FORMAT_R16_UINT},
        {"rg16ui", TEX_FORMAT_RG16_UINT},
        {"rgba16ui", TEX_FORMAT_RGBA16_UINT},

        {"r32ui", TEX_FORMAT_R32_UINT},
        {"rg32ui", TEX_FORMAT_RG32_UINT},
        {"rgba32ui", TEX_FORMAT_RGBA32_UINT},

        {"r8_snorm", TEX_FORMAT_R8_SNORM},
        {"rg8_snorm", TEX_FORMAT_RG8_SNORM},
        {"rgba8_snorm", TEX_FORMAT_RGBA8_SNORM},

        {"r16_snorm", TEX_FORMAT_R16_SNORM},
        {"rg16_snorm", TEX_FORMAT_RG16_SNORM},
        {"rgba16_snorm", TEX_FORMAT_RGBA16_SNORM},

        {"r11f_g11f_b10f", TEX_FORMAT_R11G11B10_FLOAT},
        {"rgb10_a2", TEX_FORMAT_RGB10A2_UNORM},
        {"rgb10_a2ui", TEX_FORMAT_RGB10A2_UINT},

        {"rgb8", TEX_FORMAT_UNKNOWN},
    };
    for (const auto& TestFormat : TestFormats)
    {
        EXPECT_EQ(Parsing::ParseGLSLImageFormat(TestFormat.first), TestFormat.second) << TestFormat.first;
    }
}

} // namespace
