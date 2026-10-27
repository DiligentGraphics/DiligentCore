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

#include "XXH128Hasher.hpp"
#include "ShaderToolsCommon.hpp"
#include "ShaderSourceFactoryUtils.hpp"
#include "gtest/gtest.h"

#include <array>
#include <string>

using namespace Diligent;

namespace
{

XXH128Hash ShaderHash(const ShaderCreateInfo& CI)
{
    XXH128State Hasher;
    UpdateHashWithShaderCreateInfo(Hasher, CI);
    return Hasher.Digest();
}

TEST(ShaderCreateInfoHashTest, MacroContentsAndCount)
{
    ShaderCreateInfo CI;
    CI.Source            = "void main() {}";
    ShaderMacro Macros[] = {{"A", "1"}, {"B", "2"}};
    CI.Macros            = {Macros, 2};
    const auto Original  = ShaderHash(CI);
    Macros[0].Name       = "C";
    EXPECT_FALSE(ShaderHash(CI) == Original);
    Macros[0].Name       = "A";
    Macros[0].Definition = "3";
    EXPECT_FALSE(ShaderHash(CI) == Original);
    Macros[0].Definition = "1";
    CI.Macros.Count      = 1;
    EXPECT_FALSE(ShaderHash(CI) == Original);
    CI.Macros.Count = 2;
    std::swap(Macros[0], Macros[1]);
    EXPECT_FALSE(ShaderHash(CI) == Original);

    const std::string NameA = "A", NameB = "B", DefA = "1", DefB = "2";
    const ShaderMacro Copies[] = {{NameA.c_str(), DefA.c_str()}, {NameB.c_str(), DefB.c_str()}};
    CI.Macros                  = {Copies, 2};
    EXPECT_EQ(ShaderHash(CI), Original);
}

TEST(ShaderCreateInfoHashTest, MacroBoundaries)
{
    ShaderCreateInfo CI;
    CI.Source                  = "void main() {}";
    const ShaderMacro First[]  = {{"A", "12"}};
    const ShaderMacro Second[] = {{"A1", "2"}};
    CI.Macros                  = {First, 1};
    const auto FirstHash       = ShaderHash(CI);
    CI.Macros                  = {Second, 1};
    EXPECT_FALSE(ShaderHash(CI) == FirstHash);

    const ShaderMacro TwoMacros[] = {{"A", "1"}, {"B", "2"}};
    const ShaderMacro OneMacro[]  = {{"A", "1B2"}};
    CI.Macros                     = {TwoMacros, 2};
    const auto TwoHash            = ShaderHash(CI);
    CI.Macros                     = {OneMacro, 1};
    EXPECT_FALSE(ShaderHash(CI) == TwoHash);
}

TEST(ShaderCreateInfoHashTest, BytecodeContentsAndLength)
{
    const std::array<Uint8, 4> Bytes = {{0, 1, 2, 3}};
    auto                       Copy  = Bytes;
    ShaderCreateInfo           CI;
    CI.ByteCode         = Bytes.data();
    CI.ByteCodeSize     = Bytes.size();
    const auto Original = ShaderHash(CI);
    CI.ByteCode         = Copy.data();
    EXPECT_EQ(ShaderHash(CI), Original);
    for (auto& Byte : Copy)
    {
        Byte ^= 0x80u;
        EXPECT_FALSE(ShaderHash(CI) == Original);
        Byte ^= 0x80u;
    }
    CI.ByteCodeSize   = 2;
    const auto Prefix = ShaderHash(CI);
    EXPECT_FALSE(Prefix == Original);
    Copy.back() ^= 0x80u;
    EXPECT_EQ(ShaderHash(CI), Prefix);
}

TEST(ShaderCreateInfoHashTest, SourceContentsAndLength)
{
    const char       Source[] = {'A', 'B', 'C', 'D'};
    char             Copy[]   = {'A', 'B', 'C', 'D'};
    ShaderCreateInfo CI;
    CI.Source           = Source;
    CI.SourceLength     = 3;
    const auto Original = ShaderHash(CI);
    CI.Source           = Copy;
    EXPECT_EQ(ShaderHash(CI), Original);
    Copy[3] = 'E';
    EXPECT_EQ(ShaderHash(CI), Original);
    Copy[2] = 'F';
    EXPECT_FALSE(ShaderHash(CI) == Original);
}

TEST(ShaderCreateInfoHashTest, AppendsToExistingState)
{
    ShaderCreateInfo CI;
    CI.Source = "void main() {}";
    XXH128State First;
    First.Update("prefix");
    UpdateHashWithShaderCreateInfo(First, CI);
    const auto WithPrefix = First.Digest();
    EXPECT_FALSE(WithPrefix == ShaderHash(CI));
    First.Update("suffix");
    EXPECT_FALSE(First.Digest() == WithPrefix);
    const auto WithSuffix = First.Digest();
    EXPECT_EQ(First.Digest(), WithSuffix);

    XXH128State Second;
    Second.Update("prefix");
    UpdateHashWithShaderCreateInfo(Second, CI);
    Second.Update("suffix");
    EXPECT_EQ(First.Digest(), Second.Digest());
    CI.Source = "void other() {}";
    UpdateHashWithShaderCreateInfo(First, CI);
    EXPECT_FALSE(First.Digest() == Second.Digest());
}

TEST(ShaderCreateInfoHashTest, IncludedSourceContents)
{
    constexpr char Main[]      = "#include \"Nested/Types.hlsl\"\nvoid main() {}\n";
    auto           MakeFactory = [&](const char* Config, const char* Unused) {
        return CreateMemoryShaderSourceFactory(
            {{"Main.hlsl", Main},
             {"Nested/Types.hlsl", "#include \"Config.hlsl\"\n"},
             {"Nested/Config.hlsl", Config},
             {"Unused.hlsl", Unused}},
            false);
    };
    auto             Factory = MakeFactory("#define VALUE 1\n", "unused 1");
    ShaderCreateInfo CI;
    CI.FilePath                   = "Main.hlsl";
    CI.pShaderSourceStreamFactory = Factory;
    const auto Original           = ShaderHash(CI);
    Factory                       = MakeFactory("#define VALUE 1\n", "unused 2");
    CI.pShaderSourceStreamFactory = Factory;
    EXPECT_EQ(ShaderHash(CI), Original);
    Factory                       = MakeFactory("#define VALUE 2\n", "unused 2");
    CI.pShaderSourceStreamFactory = Factory;
    EXPECT_FALSE(ShaderHash(CI) == Original);
}

TEST(ShaderCreateInfoHashTest, ShaderNameIsIgnored)
{
    ShaderCreateInfo CI;
    CI.Source           = "void main() {}";
    CI.Desc.Name        = "first";
    const auto Original = ShaderHash(CI);
    CI.Desc.Name        = "second";
    EXPECT_EQ(ShaderHash(CI), Original);
}

} // namespace
