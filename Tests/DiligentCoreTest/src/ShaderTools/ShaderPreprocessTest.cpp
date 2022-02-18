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

#include <deque>

#include "ShaderToolsCommon.hpp"
#include "DefaultShaderSourceStreamFactory.h"
#include "RenderDevice.h"
#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

TEST(ShaderPreprocessTest, Include)
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/ShaderPreprocessor", &pShaderSourceFactory);
    ASSERT_NE(pShaderSourceFactory, nullptr);

    {
        std::deque<const char*> Includes{
            "IncludeCommon0.hlsl",
            "IncludeCommon1.hlsl",
            "IncludeBasicTest.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "IncludeBasicTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            EXPECT_EQ(SafeStrEqual(ProcessInfo.FilePath, Includes.front()), true);
            Includes.pop_front();
        });
        EXPECT_EQ(Result, true);
    }

    {
        std::deque<const char*> Includes{
            "IncludeCommon0.hlsl",
            "IncludeWhiteSpaceTest.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "IncludeWhiteSpaceTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            EXPECT_EQ(SafeStrEqual(ProcessInfo.FilePath, Includes.front()), true);
            Includes.pop_front();
        });

        EXPECT_EQ(Result, true);
    }

    {
        std::deque<const char*> Includes{
            "IncludeCommon0.hlsl",
            "IncludeCommentsSingleLineTest.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "IncludeCommentsSingleLineTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            EXPECT_EQ(SafeStrEqual(ProcessInfo.FilePath, Includes.front()), true);
            Includes.pop_front();
        });

        EXPECT_EQ(Result, true);
    }

    {
        std::deque<const char*> Includes{
            "IncludeCommon0.hlsl",
            "IncludeCommentsMultiLineTest.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "IncludeCommentsMultiLineTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            EXPECT_EQ(SafeStrEqual(ProcessInfo.FilePath, Includes.front()), true);
            Includes.pop_front();
        });

        EXPECT_EQ(Result, true);
    }

    {
        std::deque<const char*> Includes{
            "IncludeCommentsTrickyCasesTest.hlsl"};

        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "IncludeCommentsTrickyCasesTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        const auto Result = ProcessShaderIncludes(ShaderCI, [&](const ShaderIncludePreprocessInfo& ProcessInfo) {
            EXPECT_EQ(SafeStrEqual(ProcessInfo.FilePath, Includes.front()), true);
            Includes.pop_front();
        });

        EXPECT_EQ(Result, true);
    }
}

TEST(ShaderPreprocessTest, UnrollIncludes)
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    CreateDefaultShaderSourceStreamFactory("shaders/ShaderPreprocessor", &pShaderSourceFactory);
    ASSERT_NE(pShaderSourceFactory, nullptr);

    {
        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.Name                  = "TestShader";
        ShaderCI.FilePath                   = "InlineIncludeShaderTest.hlsl";
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        constexpr char RefString[] =
            "// Start InlineIncludeShaderTest.hlsl\n"
            "// Start InlineIncludeShaderCommon1.hlsl\n"
            "// #include \"InlineIncludeShaderCommon0.hlsl\"\n\n"
            "// End InlineIncludeShaderCommon1.hlsl\n\n"
            "// #include \"InlineIncludeShaderCommon2.hlsl\"\n\n"
            "// End InlineIncludeShaderTest.hlsl\n";

        auto UnrolledStr = UnrollShaderIncludes(ShaderCI);
        ASSERT_STREQ(RefString, UnrolledStr.c_str());
    }
}

} // namespace
