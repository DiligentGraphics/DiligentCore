/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include <string.h>

#include "../../Primitives/interface/BasicTypes.h"
#include "../../Primitives/interface/CommonDefinitions.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

inline bool IsNum(char c)
{
    return c >= '0' && c <= '9';
}

/// Returns the number of characters at the beginning of the string that form a
/// floating point number.
inline size_t CountFloatNumberChars(const char* str)
{
    if (str == NULL)
        return 0;

    const char* num_end = str;
    const char* c       = str;
    if (*c == 0)
        return 0;

    if (*c == '+' || *c == '-')
        ++c;

    if (*c == 0)
        return 0;

    if (*c == '0' && IsNum(*(c + 1)))
    {
        // 01 is invalid
        return c - str + 1;
    }

    while (IsNum(*c))
        num_end = ++c;

    if (*c == '.')
    {
        if (c != str && IsNum(c[-1]))
        {
            // . as well as +. or -. are not valid numbers, however 0., +0., and -0. are.
            num_end = c + 1;
        }

        ++c;
        while (IsNum(*c))
            num_end = ++c;

        if (*c == 'e' || *c == 'E')
        {
            if (c - str < 2 || !IsNum(c[-2]))
            {
                // .e as well as +.e are invalid
                return num_end - str;
            }
        }
    }
    else if (*c == 'e' || *c == 'E')
    {
        if (c - str < 1 || !IsNum(c[-1]))
        {
            // e as well as e+1 are invalid
            return num_end - str;
        }
    }

    if (*c == 'e' || *c == 'E')
    {
        ++c;
        if (*c != '+' && *c != '-')
            return num_end - str;

        ++c;
        while (IsNum(*c))
            num_end = ++c;
    }

    return num_end - str;
}

inline bool SafeStrEqual(const char* Str0, const char* Str1)
{
    if ((Str0 == NULL) != (Str1 == NULL))
        return false;

    if (Str0 != NULL && Str1 != NULL)
        return strcmp(Str0, Str1) == 0;

    return true;
}

DILIGENT_END_NAMESPACE // namespace Diligent
