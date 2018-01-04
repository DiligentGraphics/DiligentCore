/*     Copyright 2015-2018 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include <string>
#include <sstream>
#include <locale>
#include <algorithm>
#include <cctype>
#include "DebugUtilities.h"

namespace Diligent
{

inline std::string NarrowString(const std::wstring &WideStr)
{
    std::string NarrowStr;
    const std::ctype<wchar_t>& ctfacet = std::use_facet< std::ctype<wchar_t> >( std::wstringstream().getloc() ) ;
    for( std::wstring::const_iterator CurrWChar = WideStr.begin();
         CurrWChar != WideStr.end();
         CurrWChar++ )
         NarrowStr.push_back( ctfacet.narrow( *CurrWChar, 0 ) );
         
	return NarrowStr;
}

inline std::wstring WidenString(const char *Str)
{
	std::wstring WideStr;
    const std::ctype<wchar_t>& ctfacet = std::use_facet< std::ctype<wchar_t> >( std::wstringstream().getloc() ) ;
    for( auto CurrChar = Str; *CurrChar != 0; ++CurrChar )
         WideStr.push_back( ctfacet.widen( *CurrChar ) );
         
	return WideStr;
}

inline std::wstring WidenString(const std::string &Str)
{
	std::wstring WideStr;
    const std::ctype<wchar_t>& ctfacet = std::use_facet< std::ctype<wchar_t> >( std::wstringstream().getloc() ) ;
    for( std::string::const_iterator CurrChar = Str.begin();
         CurrChar != Str.end();
         CurrChar++ )
         WideStr.push_back( ctfacet.widen( *CurrChar ) );
         
	return WideStr;
}

inline int StrCmpNoCase(const char* Str1, const char* Str2, size_t NumChars)
{
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX)
#   define _strnicmp strncasecmp
#endif

    return _strnicmp( Str1, Str2, NumChars );
}

inline int StrCmpNoCase(const char* Str1, const char* Str2)
{
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX)
#   define _stricmp strcasecmp
#endif

    return _stricmp( Str1, Str2 );
}

// Returns true if RefStr == Str + Suff
inline bool StrCmpSuff(const char *RefStr, const char *Str, const char *Suff)
{
    VERIFY_EXPR(RefStr != nullptr && Str!= nullptr && Suff != nullptr);
    if(RefStr==nullptr)
        return false;

    const auto *r = RefStr;
    const auto *s = Str;
    for(; *r!=0 && *s!=0; ++r, ++s)
    {
        if (*r != *s)
            return false;
    }

    if( *s != 0 )
    {
        VERIFY_EXPR(*r == 0);
        return false;
    }

    return strcmp(r, Suff) == 0;
}

inline void StrToLowerInPlace(std::string &str)
{
    std::transform(str.begin(), str.end(), str.begin(),
        // http://en.cppreference.com/w/cpp/string/byte/tolower
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); }
    );
}

inline std::string StrToLower(std::string str)
{
    StrToLowerInPlace(str);
    return str;
}

}
