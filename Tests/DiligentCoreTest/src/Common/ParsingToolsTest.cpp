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
    Test("\r\n", true);
    Test("abc def ", true);
    Test("abc def ", true);

    Test("\n"
         "Correct");
    Test("\r"
         "Correct");
    Test("\r\n"
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
         "/* abc **\r\n"
         "/****** ***** ***\r"
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
         "/// Comment line 2\r"
         "//// Comment line 3\r\n",
         true);

    Test("/* Comment */\n",
         true);

    Test("/* Comment line 1\n"
         "Comment line 2\r"
         "Comment line 3\r\n*/",
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
         " \n //\r\n"
         " \t \r \n"
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

    const auto* StrEnd = TestStr + strlen(TestStr);
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


enum class TestTokenType
{
    Undefined,
    PreprocessorDirective,
    Operator,
    OpenBrace,
    ClosingBrace,
    OpenParen,
    ClosingParen,
    OpenSquareBracket,
    ClosingSquareBracket,
    OpenAngleBracket,
    ClosingAngleBracket,
    Identifier,
    NumericConstant,
    StringConstant,
    Semicolon,
    Comma,
    TextBlock,
    Assignment,
    ComparisonOp,
    LogicOp,
    BitwiseOp,
    IncDecOp,
    MathOp,
    Keyword1,
    Keyword2,
    Keyword3
};

struct TestToken
{
    using TokenType = TestTokenType;

    TokenType   Type = TestTokenType::Undefined;
    std::string Literal;
    std::string Delimiter;

    TestToken() {}
    TestToken(TokenType   _Type,
              std::string _Literal,
              std::string _Delimiter = "") :
        Type{_Type},
        Literal{std::move(_Literal)},
        Delimiter{std::move(_Delimiter)}
    {}

    void SetType(TokenType _Type)
    {
        Type = _Type;
    }

    bool CompareLiteral(const char* Str)
    {
        return Literal == Str;
    }

    bool CompareLiteral(const char* Start, const char* End)
    {
        return Literal == std::string{Start, End};
    }

    void ExtendLiteral(const char* Start, const char* End)
    {
        Literal.append(Start, End);
    }

    static TestToken Create(TokenType _Type, const char* DelimStart, const char* DelimEnd, const char* LiteralStart, const char* LiteralEnd)
    {
        return TestToken{_Type, std::string{LiteralStart, LiteralEnd}, std::string{DelimStart, DelimEnd}};
    }

    static TokenType FindType(const char* IdentifierStart, const char* IdentifierEnd)
    {
        if (strncmp(IdentifierStart, "Keyword1", IdentifierEnd - IdentifierStart) == 0)
            return TokenType::Keyword1;

        if (strncmp(IdentifierStart, "Keyword2", IdentifierEnd - IdentifierStart) == 0)
            return TokenType::Keyword2;

        if (strncmp(IdentifierStart, "Keyword3", IdentifierEnd - IdentifierStart) == 0)
            return TokenType::Keyword3;

        return TokenType::Identifier;
    }
};

bool FindTokenSequence(const std::vector<TestToken>& Tokens, const std::vector<TestToken>& Sequence)
{
    for (auto start_it = Tokens.begin(); start_it != Tokens.end(); ++start_it)
    {
        auto token_it = start_it;
        auto ref_it   = Sequence.begin();
        while (ref_it != Sequence.end() && token_it != Tokens.end())
        {
            if (ref_it->Type != token_it->Type || ref_it->Literal != token_it->Literal)
                break;
            ++ref_it;
            ++token_it;
        }
        if (ref_it == Sequence.end())
            return true;
    }
    return false;
}

TEST(Common_ParsingTools, Tokenizer_Preprocessor)
{
    static const char* TestStr = R"(
// Comment
#include <Include1.h>

/* Comment */
#define MACRO

void main()
{
}
// Comment
/* Comment */
)";

    const auto Tokens = Tokenize<TestToken, std::vector<TestToken>>(TestStr, TestStr + strlen(TestStr), TestToken::Create, TestToken::FindType);
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::PreprocessorDirective, "#include <Include1.h>"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::PreprocessorDirective, "#define MACRO"}}));
}

TEST(Common_ParsingTools, Tokenizer_Operators)
{
    static const char* TestStr = R"(

/* Comment */
void main()
{
    // Binary operators
    a0 + a1; // Comment 2
    b0 - b1; /* Comment 3*/
/**/c0 * c1;
    d0 / d1;
    e0 % e1;
    f0 << f1;
    g0 >> g1;
    h0 & h1;
    i0 | i1;
    j0 ^ j1;

    k0 < k1;
    l0 > l1;
    m0 = m1;

    // Unary operators
    !n0;
    ~o0;

    // Assignment operators
    A0 += A1;
    B0 -= B1;
    C0 *= C1;
    D0 /= D1;
    E0 %= E1;
    F0 <<= F1;
    G0 >>= G1;
    H0 &= H1;
    I0 |= I1;
    J0 ^= J1;

    K0 <= K1;
    L0 >= L1;
    M0 == M1;
    N0 != N1;

    P0++; ++P1;
    Q0--; --Q1;
}
)";

    const auto Tokens = Tokenize<TestToken, std::vector<TestToken>>(TestStr, TestStr + strlen(TestStr), TestToken::Create, TestToken::FindType);
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "a0"}, {TestTokenType::MathOp, "+"}, {TestTokenType::Identifier, "a1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "b0"}, {TestTokenType::MathOp, "-"}, {TestTokenType::Identifier, "b1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "c0"}, {TestTokenType::MathOp, "*"}, {TestTokenType::Identifier, "c1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "d0"}, {TestTokenType::MathOp, "/"}, {TestTokenType::Identifier, "d1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "e0"}, {TestTokenType::MathOp, "%"}, {TestTokenType::Identifier, "e1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "f0"}, {TestTokenType::BitwiseOp, "<<"}, {TestTokenType::Identifier, "f1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "g0"}, {TestTokenType::BitwiseOp, ">>"}, {TestTokenType::Identifier, "g1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "h0"}, {TestTokenType::BitwiseOp, "&"}, {TestTokenType::Identifier, "h1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "i0"}, {TestTokenType::BitwiseOp, "|"}, {TestTokenType::Identifier, "i1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "j0"}, {TestTokenType::BitwiseOp, "^"}, {TestTokenType::Identifier, "j1"}}));

    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "k0"}, {TestTokenType::ComparisonOp, "<"}, {TestTokenType::Identifier, "k1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "l0"}, {TestTokenType::ComparisonOp, ">"}, {TestTokenType::Identifier, "l1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "m0"}, {TestTokenType::Assignment, "="}, {TestTokenType::Identifier, "m1"}}));

    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::LogicOp, "!"}, {TestTokenType::Identifier, "n0"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::BitwiseOp, "~"}, {TestTokenType::Identifier, "o0"}}));

    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "A0"}, {TestTokenType::Assignment, "+="}, {TestTokenType::Identifier, "A1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "B0"}, {TestTokenType::Assignment, "-="}, {TestTokenType::Identifier, "B1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "C0"}, {TestTokenType::Assignment, "*="}, {TestTokenType::Identifier, "C1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "D0"}, {TestTokenType::Assignment, "/="}, {TestTokenType::Identifier, "D1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "E0"}, {TestTokenType::Assignment, "%="}, {TestTokenType::Identifier, "E1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "F0"}, {TestTokenType::Assignment, "<<="}, {TestTokenType::Identifier, "F1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "G0"}, {TestTokenType::Assignment, ">>="}, {TestTokenType::Identifier, "G1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "H0"}, {TestTokenType::Assignment, "&="}, {TestTokenType::Identifier, "H1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "I0"}, {TestTokenType::Assignment, "|="}, {TestTokenType::Identifier, "I1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "J0"}, {TestTokenType::Assignment, "^="}, {TestTokenType::Identifier, "J1"}}));

    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "K0"}, {TestTokenType::ComparisonOp, "<="}, {TestTokenType::Identifier, "K1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "L0"}, {TestTokenType::ComparisonOp, ">="}, {TestTokenType::Identifier, "L1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "M0"}, {TestTokenType::ComparisonOp, "=="}, {TestTokenType::Identifier, "M1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "N0"}, {TestTokenType::ComparisonOp, "!="}, {TestTokenType::Identifier, "N1"}}));

    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "P0"}, {TestTokenType::IncDecOp, "++"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::IncDecOp, "++"}, {TestTokenType::Identifier, "P1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "Q0"}, {TestTokenType::IncDecOp, "--"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::IncDecOp, "--"}, {TestTokenType::Identifier, "Q1"}}));
}

TEST(Common_ParsingTools, Tokenizer_Brackets)
{
    static const char* TestStr = R"(
// Comment
struct MyStruct
{
    int a;
};

void main()
{
    function(argument1, argument2);
    array[size];
}
)";

    const auto Tokens = Tokenize<TestToken, std::vector<TestToken>>(TestStr, TestStr + strlen(TestStr), TestToken::Create, TestToken::FindType);
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::OpenBrace, "{"}, {TestTokenType::Identifier, "int"}, {TestTokenType::Identifier, "a"}, {TestTokenType::Semicolon, ";"}, {TestTokenType::ClosingBrace, "}"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "function"}, {TestTokenType::OpenParen, "("}, {TestTokenType::Identifier, "argument1"}, {TestTokenType::Comma, ","}, {TestTokenType::Identifier, "argument2"}, {TestTokenType::ClosingParen, ")"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "array"}, {TestTokenType::OpenSquareBracket, "["}, {TestTokenType::Identifier, "size"}, {TestTokenType::ClosingSquareBracket, "]"}}));
}

TEST(Common_ParsingTools, Tokenizer_StringConstant)
{
    static const char* TestStr = R"(
void main()
{
    const char* String = "string constant";
}
)";

    const auto Tokens = Tokenize<TestToken, std::vector<TestToken>>(TestStr, TestStr + strlen(TestStr), TestToken::Create, TestToken::FindType);
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "String"}, {TestTokenType::Assignment, "="}, {TestTokenType::StringConstant, "string constant"}, {TestTokenType::Semicolon, ";"}}));
}

TEST(Common_ParsingTools, Tokenizer_FloatNumber)
{
    static const char* TestStr = R"(
void main()
{
    float Number1 = 10;
    float Number2 = 20.0;
    float Number3 = 30.0e+1;
    float Number4 = 40.0e+2f;
    float Number5 = 50.f;
    float Number6 = .123f;
}
)";

    const auto Tokens = Tokenize<TestToken, std::vector<TestToken>>(TestStr, TestStr + strlen(TestStr), TestToken::Create, TestToken::FindType);
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "Number1"}, {TestTokenType::Assignment, "="}, {TestTokenType::NumericConstant, "10"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "Number2"}, {TestTokenType::Assignment, "="}, {TestTokenType::NumericConstant, "20.0"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "Number3"}, {TestTokenType::Assignment, "="}, {TestTokenType::NumericConstant, "30.0e+1"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "Number4"}, {TestTokenType::Assignment, "="}, {TestTokenType::NumericConstant, "40.0e+2f"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "Number5"}, {TestTokenType::Assignment, "="}, {TestTokenType::NumericConstant, "50.f"}}));
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Identifier, "Number6"}, {TestTokenType::Assignment, "="}, {TestTokenType::NumericConstant, ".123f"}}));
}

TEST(Common_ParsingTools, Tokenizer_UnknownIdentifier)
{
    static const char* TestStr = R"(
void main()
{
    @ Unknown;
}
)";

    const auto Tokens = Tokenize<TestToken, std::vector<TestToken>>(TestStr, TestStr + strlen(TestStr), TestToken::Create, TestToken::FindType);
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Undefined, "@"}, {TestTokenType::Identifier, "Unknown"}}));
}

TEST(Common_ParsingTools, Tokenizer_Keywords)
{
    static const char* TestStr = R"(
void main()
{
    Keyword1 Id Keyword2(Keyword3);
}
)";

    const auto Tokens = Tokenize<TestToken, std::vector<TestToken>>(TestStr, TestStr + strlen(TestStr), TestToken::Create, TestToken::FindType);
    EXPECT_TRUE(FindTokenSequence(Tokens, {{TestTokenType::Keyword1, "Keyword1"}, {TestTokenType::Identifier, "Id"}, {TestTokenType::Keyword2, "Keyword2"}, {TestTokenType::OpenParen, "("}, {TestTokenType::Keyword3, "Keyword3"}, {TestTokenType::ClosingParen, ")"}}));
}

} // namespace
