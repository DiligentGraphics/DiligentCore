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

#pragma once

/// \file
/// Parsing tools

#include <cstring>
#include <sstream>

#include "../../Primitives/interface/BasicTypes.h"
#include "../../Platforms/Basic/interface/DebugUtilities.hpp"
#include "StringTools.h"

namespace Diligent
{

namespace Parsing
{

/// Returns true if the symbol is a white space or tab
inline bool IsWhitespace(Char Symbol)
{
    return Symbol == ' ' || Symbol == '\t';
}

/// Returns true if the symbol is a new line symbol
inline bool IsNewLine(Char Symbol)
{
    return Symbol == '\r' || Symbol == '\n';
}

/// Returns true if the symbol is a delimiter symbol (white space or new line)
inline bool IsDelimiter(Char Symbol)
{
    static const Char* Delimiters = " \t\r\n";
    return strchr(Delimiters, Symbol) != nullptr;
}

/// Returns true if the symbol is a statement separator symbol
inline bool IsStatementSeparator(Char Symbol)
{
    static const Char* StatementSeparator = ";}";
    return strchr(StatementSeparator, Symbol) != nullptr;
}


/// Skips all symbols until the end of the line.

/// \param[inout] Pos          - starting position.
/// \param[in]    End          - end of the input string.
/// \param[in]    GoToNextLine - whether to go to the next line.
///                              If true, the Pos will point to the symbol following
///                              the new line character at the end of the string.
///                              If false, the Pos will point to the new line
///                              character at the end of the string.
/// \return         true if the end of the string has been reached, and false otherwise.
template <typename InteratorType>
inline bool SkipLine(InteratorType& Pos, const InteratorType& End, bool GoToNextLine = false)
{
    while (Pos != End && *Pos != '\0' && !IsNewLine(*Pos))
        ++Pos;
    if (GoToNextLine && Pos != End && IsNewLine(*Pos))
    {
        ++Pos;
        if (Pos[-1] == '\r' && Pos != End && Pos[0] == '\n')
        {
            ++Pos;
            // treat \r\n as a single ending
        }
    }
    return Pos == End;
}


/// Skips single-line and multi-line comments starting from the given position.

/// \param[inout] Pos - starting position.
/// \param[in]    End - end of the input string.
///
/// \return     true if the end of the string has been reached, and false otherwise.
///
/// \remarks    If the comment is found, Pos is updated to the position
///             immediately after the end of the comment.
///             If the comment is not found, Pos is left unchanged.
template <typename InteratorType>
bool SkipComment(InteratorType& Pos, const InteratorType& End)
{
    if (Pos == End || *Pos == '\0')
        return true;

    //  // Comment       /* Comment
    //  ^                ^
    //  Pos              Pos
    if (*Pos != '/')
        return false;

    auto NextPos = Pos + 1;
    //  // Comment       /* Comment
    //   ^                ^
    //  NextPos           NextPos
    if (NextPos == End || *NextPos == '\0')
        return false;

    if (*NextPos == '/')
    {
        // Single-line comment (// Comment)
        Pos = NextPos + 1;
        //  // Comment
        //    ^
        //    Pos

        SkipLine(Pos, End, true);
        //  // Comment
        //
        //  ^
        //  Pos

        return Pos == End || *Pos == '\0';
    }
    else if (*NextPos == '*')
    {
        // Mulit-line comment (/* comment */)
        ++NextPos;
        //  /* Comment
        //    ^
        while (NextPos != End && *NextPos != '\0')
        {
            if (*NextPos == '*')
            {
                //  /* Comment */
                //             ^
                //           NextPos
                ++NextPos;
                if (NextPos == End || *NextPos == '\0')
                    return false;

                //  /* Comment */
                //              ^
                //            NextPos
                if (*NextPos == '/')
                {
                    Pos = NextPos + 1;
                    //  /* Comment */
                    //               ^
                    //              Pos
                    return Pos == End || *Pos == '\0';
                }
            }
            else
            {
                ++NextPos;
            }
        }
    }

    return Pos == End || *Pos == '\0';
}


/// Skips all delimiters starting from the given position.

/// \param[inout] Pos - starting position.
/// \param[in]    End - end of the input string.
///
/// \return true if the end of the string has been reached, and false otherwise.
///
/// \remarks    Pos is updated to the position of the first non-delimiter symbol.
template <typename InteratorType>
bool SkipDelimiters(InteratorType& Pos, const InteratorType& End)
{
    for (; Pos != End && IsDelimiter(*Pos); ++Pos)
        ;
    return Pos == End;
}


/// Skips all comments and all delimiters starting from the given position.

/// \param[inout] Pos - starting position.
/// \param[in]    End - end of the input string.
///
/// \return true if the end of the string has been reached, and false otherwise.
///
/// \remarks    Pos is updated to the position of the first non-comment
///             non-delimiter symbol.
template <typename IteratorType>
bool SkipDelimitersAndComments(IteratorType& Pos, const IteratorType& End)
{
    bool DelimiterSkipped = false;
    bool CommentSkipped   = false;
    do
    {
        DelimiterSkipped = false;
        for (; Pos != End && IsDelimiter(*Pos); ++Pos)
            DelimiterSkipped = true;

        const auto StartPos = Pos;
        SkipComment(Pos, End);
        CommentSkipped = (StartPos != Pos);
    } while ((Pos != End) && (DelimiterSkipped || CommentSkipped));

    return Pos == End;
}


/// Skips one identifier starting from the given position.

/// \param[inout] Pos - starting position.
/// \param[in]    End - end of the input string.
///
/// \return     true if the end of the string has been reached, and false otherwise.
///
/// \remarks    Pos is updated to the position of the first symbol
///             after the identifier.
template <typename IteratorType>
inline bool SkipIdentifier(IteratorType& Pos, const IteratorType& End)
{
    if (Pos == End)
        return true;

    if (isalpha(*Pos) || *Pos == '_')
    {
        ++Pos;
        if (Pos == End)
            return true;
    }
    else
        return false;

    for (; Pos != End && (isalnum(*Pos) || *Pos == '_'); ++Pos)
        ;

    return Pos == End;
}


/// Splits string into chunks separated by comments and delimiters.
///
/// \param [in] Start   - start of the string to split.
/// \param [in] End     - end of the string to split.
/// \param [in] Handler - user-provided handler to call for each chunk.
///
/// \remarks    The function starts from the beginning of the strings
///             and splits it into chunks seprated by comments and delimiters.
///             For each chunk, it calls the user-provided handler and passes
///             the start of the preceding comments/delimiters part. The handler
///             must then process the text at the current position and move the pointer.
///             It should return true to continue processing, and false to stop it.
template <typename IteratorType, typename HandlerType>
void SplitString(const IteratorType& Start, const IteratorType& End, HandlerType Handler)
{
    auto Pos = Start;
    while (Pos != End)
    {
        auto DelimStart = Pos;
        SkipDelimitersAndComments(Pos, End);
        auto OrigPos = Pos;
        if (!Handler(DelimStart, Pos))
            break;
        VERIFY(Pos == End || OrigPos != Pos, "Position has not been updated by the handler.");
    }
}



/// Skips a floating point number starting from the given position.

/// \param[inout] Pos - starting position.
/// \param[in]    End - end of the input string.
template <typename IteratorType>
void SkipFloatNumber(IteratorType& Pos, const IteratorType& End)
{
    const auto Start = Pos;

#define CHECK_END()                 \
    do                              \
    {                               \
        if (c == End || *c == '\0') \
            return;                 \
    } while (false)

    auto c = Pos;
    CHECK_END();

    if (*c == '+' || *c == '-')
        ++c;
    CHECK_END();

    if (*c == '0' && IsNum(c[1]))
    {
        // 01 is invalid
        Pos = c + 1;
        return;
    }

    const auto HasIntegerPart = IsNum(*c);
    if (HasIntegerPart)
    {
        while (c != End && IsNum(*c))
            Pos = ++c;
        CHECK_END();
    }

    const auto HasDecimalPart = (*c == '.');
    if (HasDecimalPart)
    {
        ++c;
        if (HasIntegerPart)
        {
            // . as well as +. or -. are not valid numbers, however 0., +0., and -0. are.
            Pos = c;
        }

        while (c != End && IsNum(*c))
            Pos = ++c;
        CHECK_END();
    }

    const auto HasExponent = (*c == 'e' || *c == 'E');
    if (HasExponent)
    {
        if (!HasIntegerPart)
        {
            // .e, e, e+1, +.e are invalid
            return;
        }

        ++c;
        if (c == End || (*c != '+' && *c != '-'))
        {
            // 10e&
            return;
        }

        ++c;
        if (c == End || !IsNum(*c))
        {
            // 10e+x
            return;
        }

        while (c != End && IsNum(*c))
            Pos = ++c;
    }

    if ((HasDecimalPart || HasExponent) && c != End && Pos > Start && (*c == 'f' || *c == 'F'))
    {
        // 10.f, 10e+3f, 10.e+3f, 10.4e+3f
        Pos = ++c;
    }
#undef CHECK_END
}


/// Prints a parsing context around the given position in the string.

/// \param[in]  Start    - start of the string.
/// \param[in]  End      - end of the string.
/// \param[in]  Pos      - position around which to print the context and
///                        which will be highlighted by ^.
/// \param[in]  NumLines - the number of lines above and below.
///
/// \remarks    The context looks like shown below:
///
///                 Lorem ipsum dolor sit amet, consectetur
///                 adipiscing elit, sed do eiusmod tempor
///                 incididunt ut labore et dolore magna aliqua.
///                                      ^
///                 Ut enim ad minim veniam, quis nostrud
///                 exercitation ullamco lab
///
template <typename IteratorType>
std::string GetContext(const IteratorType& Start, const IteratorType& End, IteratorType Pos, size_t NumLines)
{
    auto CtxStart = Pos;
    while (CtxStart > Start && !IsNewLine(CtxStart[-1]))
        --CtxStart;
    const size_t CharPos = Pos - CtxStart; // Position of the character in the line

    SkipLine(Pos, End);

    std::stringstream Ctx;
    {
        size_t LineAbove = 0;
        while (LineAbove < NumLines && CtxStart > Start)
        {
            VERIFY_EXPR(IsNewLine(CtxStart[-1]));
            if (CtxStart[-1] == '\n' && CtxStart > Start + 1 && CtxStart[-2] == '\r')
                --CtxStart;
            if (CtxStart > Start)
                --CtxStart;
            while (CtxStart > Start && !IsNewLine(CtxStart[-1]))
                --CtxStart;
            ++LineAbove;
        }
        VERIFY_EXPR(CtxStart == Start || IsNewLine(CtxStart[-1]));
        Ctx.write(CtxStart, Pos - CtxStart);
    }

    Ctx << std::endl;
    for (size_t i = 0; i < CharPos; ++i)
        Ctx << ' ';
    Ctx << '^';

    {
        auto   CtxEnd    = Pos;
        size_t LineBelow = 0;
        while (LineBelow < NumLines && CtxEnd != End && *CtxEnd != '\0')
        {
            if (*CtxEnd == '\r' && CtxEnd + 1 != End && CtxEnd[+1] == '\n')
                ++CtxEnd;
            if (CtxEnd != End)
                ++CtxEnd;
            SkipLine(CtxEnd, End);
            ++LineBelow;
        }
        Ctx.write(Pos, CtxEnd - Pos);
    }

    return Ctx.str();
}


/// Tokenizes the given string using the C-language syntax

/// \param [in] SourceStart  - start of the source string.
/// \param [in] SourceEnd    - end of the source string.
/// \param [in] CreateToken  - a handler called every time a new token should
///                            be created.
/// \param [in] GetTokenType - a function that should return the token type
///                            for the given literal.
/// \return     Tokenized representation of the source string
template <typename TokenClass,
          typename ContainerType,
          typename IteratorType,
          typename CreateTokenFuncType,
          typename GetTokenTypeFunctType>
ContainerType Tokenize(const IteratorType&   SourceStart,
                       const IteratorType&   SourceEnd,
                       CreateTokenFuncType   CreateToken,
                       GetTokenTypeFunctType GetTokenType) noexcept(false)
{
    using TokenType = typename TokenClass::TokenType;

    ContainerType Tokens;
    // Push empty node in the beginning of the list to facilitate
    // backwards searching
    Tokens.emplace_back(TokenClass{});

    Parsing::SplitString(SourceStart, SourceEnd, [&](const auto& DelimStart, auto& Pos) {
        const auto DelimEnd = Pos;

        auto LiteralStart = Pos;
        auto LiteralEnd   = DelimStart;

        auto Type = TokenType::Undefined;

        if (Pos == SourceEnd)
        {
            Tokens.push_back(CreateToken(Type, DelimStart, DelimEnd, LiteralStart, Pos));
            return false;
        }

        auto AddDoubleCharToken = [&](TokenType DoubleCharType) {
            if (!Tokens.empty() && DelimStart == DelimEnd)
            {
                auto& LastToken = Tokens.back();
                if (LastToken.CompareLiteral(Pos, Pos + 1))
                {
                    LastToken.SetType(DoubleCharType);
                    LastToken.ExtendLiteral(Pos, Pos + 1);
                    ++Pos;
                    return true;
                }
            }

            return false;
        };

#define SINGLE_CHAR_TOKEN(TYPE) \
    Type = TokenType::TYPE;     \
    ++Pos;                      \
    break

        switch (*Pos)
        {
            case '#':
                Type = TokenType::PreprocessorDirective;
                Parsing::SkipLine(Pos, SourceEnd);
                break;

            case '=':
            {
                if (!Tokens.empty() && DelimStart == DelimEnd)
                {
                    auto& LastToken = Tokens.back();
                    // +=, -=, *=, /=, %=, <<=, >>=, &=, |=, ^=
                    for (const char* op : {"+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^"})
                    {
                        if (LastToken.CompareLiteral(op))
                        {
                            LastToken.SetType(TokenType::Assignment);
                            LastToken.ExtendLiteral(Pos, Pos + 1);
                            ++Pos;
                            return Pos != SourceEnd;
                        }
                    }

                    // <=, >=, ==, !=
                    for (const char* op : {"<", ">", "=", "!"})
                    {
                        if (LastToken.CompareLiteral(op))
                        {
                            LastToken.SetType(TokenType::ComparisonOp);
                            LastToken.ExtendLiteral(Pos, Pos + 1);
                            ++Pos;
                            return Pos != SourceEnd;
                        }
                    }
                }

                SINGLE_CHAR_TOKEN(Assignment);
            }

            case '|':
            case '&':
                if (AddDoubleCharToken(TokenType::LogicOp))
                    return Pos != SourceEnd;
                SINGLE_CHAR_TOKEN(BitwiseOp);

            case '<':
            case '>':
                // Note: we do not distinguish between comparison operators
                // and template arguments like in Texture2D<float> at this
                // point.
                if (AddDoubleCharToken(TokenType::BitwiseOp))
                    return Pos != SourceEnd;
                SINGLE_CHAR_TOKEN(ComparisonOp);

            case '+':
            case '-':
                // We do not currently distinguish between math operator a + b,
                // unary operator -a and numerical constant -1:
                if (AddDoubleCharToken(TokenType::IncDecOp))
                    return Pos != SourceEnd;
                SINGLE_CHAR_TOKEN(MathOp);

            case '~':
            case '^':
                SINGLE_CHAR_TOKEN(BitwiseOp);

            case '*':
            case '/':
            case '%':
                SINGLE_CHAR_TOKEN(MathOp);

            case '!':
                SINGLE_CHAR_TOKEN(LogicOp);

            case ',':
                SINGLE_CHAR_TOKEN(Comma);

            case ';':
                SINGLE_CHAR_TOKEN(Semicolon);

            // clang-format off
            case '(': SINGLE_CHAR_TOKEN(OpenParen);
            case ')': SINGLE_CHAR_TOKEN(ClosingParen);
            case '{': SINGLE_CHAR_TOKEN(OpenBrace);
            case '}': SINGLE_CHAR_TOKEN(ClosingBrace);
            case '[': SINGLE_CHAR_TOKEN(OpenSquareBracket);
            case ']': SINGLE_CHAR_TOKEN(ClosingSquareBracket);
          //case '<': SINGLE_CHAR_TOKEN(OpenAngleBracket);
          //case '>': SINGLE_CHAR_TOKEN(ClosingAngleBracket);
                // clang-format on

            case '"':
            {
                // Skip quotes
                Type = TokenType::StringConstant;
                ++LiteralStart;
                ++Pos;
                while (Pos != SourceEnd && *Pos != '\0' && *Pos != '"')
                    ++Pos;
                LiteralEnd = Pos;
                if (Pos != SourceEnd && *Pos == '"')
                    ++Pos;

                break;
            }

            default:
            {
                Parsing::SkipIdentifier(Pos, SourceEnd);
                if (LiteralStart != Pos)
                {
                    Type = GetTokenType(LiteralStart, Pos);
                    if (Type == TokenType::Undefined)
                        Type = TokenType::Identifier;
                }
                else
                {
                    Parsing::SkipFloatNumber(Pos, SourceEnd);
                    if (LiteralStart != Pos)
                    {
                        Type = TokenType::NumericConstant;
                    }
                }

                if (Type == TokenType::Undefined)
                {
                    ++Pos; // Add single character
                }
            }
        }

        if (LiteralEnd == DelimStart)
            LiteralEnd = Pos;

        Tokens.push_back(CreateToken(Type, DelimStart, DelimEnd, LiteralStart, LiteralEnd));
        return Pos != SourceEnd;
    } //
    );
#undef SINGLE_CHAR_TOKEN

    return Tokens;
}

} // namespace Parsing

} // namespace Diligent
