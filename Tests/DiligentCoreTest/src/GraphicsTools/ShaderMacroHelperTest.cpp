/*
 *  Copyright 2023 Diligent Graphics LLC
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

#include "ShaderMacroHelper.hpp"

#include <vector>

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

static void VerifyMacros(const ShaderMacroHelper& TestHelper, const std::vector<std::pair<const char*, const char*>>& RefMacros)
{
    ShaderMacroArray Macros = TestHelper;
    ASSERT_EQ(Macros.Count, RefMacros.size());
    for (size_t i = 0; i < Macros.Count; ++i)
    {
        EXPECT_STREQ(Macros[i].Name, RefMacros[i].first);
        EXPECT_STREQ(Macros[i].Definition, RefMacros[i].second);
        EXPECT_STREQ(Macros[i].Definition, TestHelper.Find(Macros[i].Name));
    }
    EXPECT_EQ(TestHelper.Find("Nonexistent"), nullptr);
}

TEST(ShaderMacroHelper, AddInt)
{
    ShaderMacroHelper Macros;
    Macros
        .Add("MACRO1", Int8{127})
        .Add("MACRO2", Int16{32767})
        .Add("MACRO3", Int32{2147483647})
        .Add("MACRO4", Int8{-128})
        .Add("MACRO5", Int16{-32768})
        .Add("MACRO6", Int32{-2147483647 - 1});
    VerifyMacros(Macros,
                 {
                     {"MACRO1", "127"},
                     {"MACRO2", "32767"},
                     {"MACRO3", "2147483647"},
                     {"MACRO4", "-128"},
                     {"MACRO5", "-32768"},
                     {"MACRO6", "-2147483648"},
                 });
}

TEST(ShaderMacroHelper, AddUint)
{
    ShaderMacroHelper Macros;
    Macros
        .Add("MACRO1", Uint8{128})
        .Add("MACRO2", Uint16{32768})
        .Add("MACRO3", Uint32{2147483648});
    VerifyMacros(Macros,
                 {
                     {"MACRO1", "128u"},
                     {"MACRO2", "32768u"},
                     {"MACRO3", "2147483648u"},
                 });
}

TEST(ShaderMacroHelper, AddFloat)
{
    ShaderMacroHelper Macros;
    Macros
        .Add("MACRO1", 0.f)
        .Add("MACRO2", 1.f)
        .Add("MACRO3", -2.f)
        .Add("MACRO4", 3.125f)
        .Add("MACRO5", -4.625f);
    VerifyMacros(Macros,
                 {
                     {"MACRO1", "0.0"},
                     {"MACRO2", "1.0"},
                     {"MACRO3", "-2.0"},
                     {"MACRO4", "3.125"},
                     {"MACRO5", "-4.625"},
                 });
}

TEST(ShaderMacroHelper, AddBool)
{
    ShaderMacroHelper Macros;
    Macros
        .Add("MACRO1", true)
        .Add("MACRO2", false);
    VerifyMacros(Macros,
                 {
                     {"MACRO1", "1"},
                     {"MACRO2", "0"},
                 });
}

TEST(ShaderMacroHelper, AddString)
{
    ShaderMacroHelper Macros;
    Macros
        .Add("MACRO1", "abc")
        .Add("MACRO2", "XYZ");
    VerifyMacros(Macros,
                 {
                     {"MACRO1", "abc"},
                     {"MACRO2", "XYZ"},
                 });
}

TEST(ShaderMacroHelper, Update)
{
    ShaderMacroHelper Macros;
    Macros
        .Add("MACRO1", "abc")
        .Add("MACRO2", 1)
        .Add("MACRO3", 2.f)
        .Add("MACRO4", 3u);

    Macros
        .Update("MACRO1", "ABC")
        .Update("MACRO2", 2)
        .Update("MACRO3", 3.f)
        .Update("MACRO4", 4u);

    VerifyMacros(Macros,
                 {
                     {"MACRO1", "ABC"},
                     {"MACRO2", "2"},
                     {"MACRO3", "3.0"},
                     {"MACRO4", "4u"},
                 });
}


TEST(ShaderMacroHelper, Remove)
{
    ShaderMacroHelper Macros;
    Macros
        .Add("MACRO1", "ABC")
        .Add("MACRO2", 1)
        .Add("MACRO3", 3.f)
        .Add("MACRO4", 4u);

    Macros.Remove("MACRO2");
    VerifyMacros(Macros,
                 {
                     {"MACRO1", "ABC"},
                     {"MACRO3", "3.0"},
                     {"MACRO4", "4u"},
                 });

    Macros.Remove("MACRO1");
    VerifyMacros(Macros,
                 {
                     {"MACRO3", "3.0"},
                     {"MACRO4", "4u"},
                 });

    Macros.Remove("MACRO4");
    VerifyMacros(Macros,
                 {
                     {"MACRO3", "3.0"},
                 });

    Macros.Remove("MACRO3");
    VerifyMacros(Macros, {});
}

TEST(ShaderMacroHelper, OperatorPlusEqual1)
{
    ShaderMacroHelper Macros;
    Macros
        .Add("MACRO1", "ABC")
        .Add("MACRO2", 2);

    {
        ShaderMacroHelper Macros2;
        Macros2
            .Add("MACRO3", 3.f)
            .Add("MACRO4", 4u)
            .Add("MACRO5", true);
        Macros += Macros2;
    }

    VerifyMacros(Macros,
                 {
                     {"MACRO1", "ABC"},
                     {"MACRO2", "2"},
                     {"MACRO3", "3.0"},
                     {"MACRO4", "4u"},
                     {"MACRO5", "1"},
                 });
}

TEST(ShaderMacroHelper, OperatorPlusEqual2)
{
    ShaderMacroHelper Macros;
    Macros
        .Add("MACRO1", "ABC")
        .Add("MACRO2", 2);
    Macros += ShaderMacro{"MACRO3", "3.0"};
    Macros += {"MACRO4", "MNP"};

    VerifyMacros(Macros,
                 {{"MACRO1", "ABC"},
                  {"MACRO2", "2"},
                  {"MACRO3", "3.0"},
                  {"MACRO4", "MNP"}});
}

TEST(ShaderMacroHelper, OperatorPlusEqual3)
{
    ShaderMacroHelper Macros;
    Macros
        .Add("MACRO1", "ABC")
        .Add("MACRO2", 2);

    ShaderMacro AddMacros[] =
        {
            {"MACRO3", "3.0"},
            {"MACRO4", "MNP"},
        };
    Macros += {AddMacros, _countof(AddMacros)};

    VerifyMacros(Macros,
                 {{"MACRO1", "ABC"},
                  {"MACRO2", "2"},
                  {"MACRO3", "3.0"},
                  {"MACRO4", "MNP"}});
}


TEST(ShaderMacroHelper, OperatorPlus1)
{
    ShaderMacroHelper Macros;
    {
        ShaderMacroHelper Macros1;
        Macros1
            .Add("MACRO1", "ABC")
            .Add("MACRO2", 2);

        ShaderMacroHelper Macros2;
        Macros2
            .Add("MACRO3", 3.f)
            .Add("MACRO4", 4u)
            .Add("MACRO5", true);
        Macros = Macros1 + Macros2;
    }

    VerifyMacros(Macros,
                 {
                     {"MACRO1", "ABC"},
                     {"MACRO2", "2"},
                     {"MACRO3", "3.0"},
                     {"MACRO4", "4u"},
                     {"MACRO5", "1"},
                 });
}

TEST(ShaderMacroHelper, OperatorPlus2)
{
    ShaderMacroHelper Macros;
    {
        ShaderMacroHelper Macros1;
        Macros1
            .Add("MACRO1", "ABC")
            .Add("MACRO2", 2);
        Macros = Macros1 + ShaderMacro{"MACRO3", "3.0"};
    }

    VerifyMacros(Macros,
                 {
                     {"MACRO1", "ABC"},
                     {"MACRO2", "2"},
                     {"MACRO3", "3.0"},
                 });
}

TEST(ShaderMacroHelper, OperatorPlus3)
{
    ShaderMacroHelper Macros;

    {
        ShaderMacroHelper Macros1;
        Macros1
            .Add("MACRO1", "ABC")
            .Add("MACRO2", 2);

        ShaderMacro AddMacros[] =
            {
                {"MACRO3", "3.0"},
                {"MACRO4", "MNP"},
            };
        Macros = Macros1 + ShaderMacroArray{AddMacros, _countof(AddMacros)};
    }

    VerifyMacros(Macros,
                 {{"MACRO1", "ABC"},
                  {"MACRO2", "2"},
                  {"MACRO3", "3.0"},
                  {"MACRO4", "MNP"}});
}

} // namespace
