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

#include "ParsingTools.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Parsing;

namespace
{

TEST(Common_ParsingTools, SkipLine)
{
    auto Test = [](const char* Str, bool EndReached = false, const char* Expected = nullptr) {
        auto* Pos = Str;
        EXPECT_EQ(SkipLine(Pos, Str + strlen(Str)), EndReached);
        if (Expected == nullptr)
            Expected = EndReached ? "" : "Correct";
        EXPECT_STREQ(Pos, Expected);
    };

    Test("", true);
    Test("abc def ", true);

    Test("abc def \n"
         "Correct",
         false,
         "\n"
         "Correct");

    Test("abc def \r"
         "Correct",
         false,
         "\r"
         "Correct");
}

TEST(Common_ParsingTools, SkipLine_GoToNext)
{
    auto Test = [](const char* Str, bool EndReached = false, const char* Expected = nullptr) {
        auto* Pos = Str;
        EXPECT_EQ(SkipLine(Pos, Str + strlen(Str), true), EndReached);
        if (Expected == nullptr)
            Expected = EndReached ? "" : "Correct";
        EXPECT_STREQ(Pos, Expected);
    };

    Test("", true);
    Test("\n", true);
    Test("abc def ", true);
    Test("abc def ", true);

    Test("\n"
         "Correct");
    Test("\r"
         "Correct");
}

TEST(Common_ParsingTools, SkipComment)
{
    auto Test = [](const char* Str, bool CommentFound = true, bool EndReached = false) {
        auto* Pos = Str;
        EXPECT_EQ(SkipComment(Pos, Str + strlen(Str)), EndReached);
        const auto Expected = EndReached ? "" : (CommentFound ? "Correct" : Str);
        EXPECT_STREQ(Pos, Expected);
    };

    Test("", false, true);
    Test("Correct", false);
    Test("/", false);
    Test("/Correct", false);

    Test("// Single-line comment", true, true);
    Test("// Single-line comment\n", true, true);

    Test("// Single-line comment\n"
         "Correct");

    Test("// Single-line comment // \n"
         "Correct");

    Test("// Single-line comment /* */ \n"
         "Correct");


    Test("/*", false);
    Test("/* abc ", false);
    Test("/* abc *", false);

    Test("/* abc *\n"
         "***\n",
         false);

    Test("/* abc */Correct");
    Test("/** abc */Correct");
    Test("/* abc **/Correct");
    Test("/*/* abc ** /* **/Correct");

    Test("/*\n"
         "/* abc **\n"
         "/****** ***** ***\n"
         " /* **/Correct");
}

TEST(Common_ParsingTools, SkipDelimeters)
{
    auto Test = [](const char* Str, bool EndReached = false, const char* Expected = nullptr) {
        auto* Pos = Str;
        EXPECT_EQ(SkipDelimeters(Pos, Str + strlen(Str)), EndReached);
        if (Expected == nullptr)
            Expected = EndReached ? "" : "Correct";
        EXPECT_STREQ(Pos, Expected);
    };

    Test("", true);
    Test(" ", true);
    Test("\t", true);
    Test("\r", true);
    Test("\n", true);
    Test("\t \r \n ", true);

    Test("Correct");
    Test(" Correct");
    Test("\tCorrect");
    Test("\rCorrect");
    Test("\nCorrect");
    Test("\t \r \n Correct");
}

TEST(Common_ParsingTools, SkipDelimetersAndComments)
{
    auto Test = [](const char* Str, bool EndReached = false) {
        auto* Pos = Str;
        EXPECT_EQ(SkipDelimetersAndComments(Pos, Str + strlen(Str)), EndReached);
        const auto* Expected = EndReached ? "" : "Correct";
        EXPECT_STREQ(Pos, Expected);
    };

    Test("", true);
    Test(" ", true);
    Test("\t", true);
    Test("\r", true);
    Test("\n", true);
    Test("\t \r \n ", true);
    Test("// Comment", true);

    Test("// Comment line 1\n"
         "/// Comment line 2\n"
         "//// Comment line 3\n",
         true);

    Test("/* Comment */\n",
         true);

    Test("/* Comment line 1\n"
         "Comment line 2\n"
         "Comment line 3\n*/",
         true);

    Test(" \t \r \n // Comment\n"
         " \t \r \n "
         "Correct");

    Test(" \t \r \n \n"
         "/* Comment */\n"
         " \t \r \n "
         "Correct");

    Test(" \t // Comment 1\n"
         " /* Comment 2 \n"
         "Comment 3 /* /* **** \r"
         "Comment 4*/ // Comment 5"
         " \n //\n"
         " \t \r \n "
         "Correct");
}

TEST(Common_ParsingTools, SkipIdentifier)
{
    auto Test = [](const char* Str, const char* Expected, bool EndReached = false) {
        auto* Pos = Str;
        EXPECT_EQ(SkipIdentifier(Pos, Str + strlen(Str)), EndReached);
        if (Expected == nullptr)
            Expected = Str;
        EXPECT_STREQ(Pos, Expected);
    };

    Test("", nullptr, true);
    Test(" ", nullptr, false);
    Test("3abc", nullptr);
    Test("*", nullptr);
    Test("_", "", true);
    Test("_3", "", true);
    Test("_a", "", true);
    Test("_a1b2c3", "", true);
    Test("_?", "?");
    Test("_3+1", "+1");
    Test("_a = 10", " = 10");
    Test("_a1b2c3[5]", "[5]");
}

TEST(Common_ParsingTools, SplitString)
{
    static const char* TestStr = R"(
Lorem ipsum //dolor sit amet, consectetur
adipiscing elit, /* sed do eiusmod tempor incididunt 
ut labore et dolore magna*/ aliqua.   Ut 
// enim ad minim veniam, quis nostrud exercitation 
/// ullamco laboris nisi /* ut aliquip ex ea commodo consequat*/.
   Duis aute  irure //dolor in //reprehenderit in voluptate   velit esse 
/* cillum dolore eu fugiat 
/* nulla /* pariatur. 
*/ /*Excepteur 
*/ 
sint occaecat //cupidatat non proident.
)";

    std::vector<std::string> Chunks = {"Lorem", "ipsum", "adipiscing", "elit", ",", "aliqua.", "Ut", "Duis", "aute", "irure", "sint", "occaecat", ""};
    auto                     ref_it = Chunks.begin();

    const auto StrEnd = TestStr + strlen(TestStr);
    SplitString(TestStr, StrEnd,
                [&](const char* DelimStart, const char*& Pos) //
                {
                    if (ref_it == Chunks.end())
                    {
                        ADD_FAILURE() << "Unexpected string " << Pos;
                        return false;
                    }

                    if (strncmp(Pos, ref_it->c_str(), ref_it->length()) != 0)
                    {
                        ADD_FAILURE() << Pos << " != " << *ref_it;
                        return false;
                    }

                    Pos += ref_it->length();
                    ++ref_it;
                    return true;
                });

    EXPECT_EQ(ref_it, Chunks.end());
}

} // namespace
