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

#include "BasicTypes.h"

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
    static const Char* Delimeters = " \t\r\n";
    return strchr(Delimeters, Symbol) != nullptr;
}

/// Returns true if the symbol is a statement separator symbol
inline bool IsStatementSeparator(Char Symbol)
{
    static const Char* StatementSeparator = ";}";
    return strchr(StatementSeparator, Symbol) != nullptr;
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
    if (Pos == End)
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
    if (NextPos == End)
        return false;

    if (*NextPos == '/')
    {
        // Single-line comment (// Comment)
        Pos = NextPos + 1;
        //  // Comment
        //    ^
        //    Pos
        for (; Pos != End && !IsNewLine(*Pos); ++Pos)
            ;
        //  // Comment
        //            ^
        //           Pos
        if (Pos != End && IsNewLine(*Pos))
        {
            ++Pos;
            //  // Comment
            //
            //  ^
            //  Pos
        }
        return Pos == End;
    }
    else if (*NextPos == '*')
    {
        // Mulit-line comment (/* comment */)
        ++NextPos;
        //  /* Comment
        //    ^
        while (NextPos != End)
        {
            if (*NextPos == '*')
            {
                //  /* Comment */
                //             ^
                //           NextPos
                ++NextPos;
                if (NextPos == End)
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
                    return Pos == End;
                }
            }
            else
            {
                ++NextPos;
            }
        }
    }

    return Pos == End;
}


/// Skips all delimiters starting from the given position.

/// \param[inout] Pos - starting position.
/// \param[in]    End - end of the input string.
///
/// \return true if the end of the string has been reached, and false otherwise.
///
/// \remarks    Pos is updated to the position of the first non-delimiter symbol.
template <typename InteratorType>
bool SkipDelimeters(InteratorType& Pos, const InteratorType& End)
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
bool SkipDelimetersAndComments(IteratorType& Pos, const IteratorType& End)
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

} // namespace Parsing

} // namespace Diligent
