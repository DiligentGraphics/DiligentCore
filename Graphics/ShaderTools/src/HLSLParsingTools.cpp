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

#include "HLSLParsingTools.hpp"
#include "HLSLTokenizer.hpp"
#include "GLSLParsingTools.hpp"

namespace Diligent
{

namespace Parsing
{


static void ExtractAnnotationsFromDelimiter(const std::string& Delim, ImageFormatAndAccess& Info)
{
    size_t Pos = 0;
    while (Pos < Delim.size())
    {
        size_t Begin = Delim.find("/*", Pos);
        if (Begin == std::string::npos)
            break;

        size_t End = Delim.find("*/", Begin + 2);
        if (End == std::string::npos)
            break;

        auto CmtBeg = Delim.begin() + Begin;
        auto CmtEnd = Delim.begin() + End + 2;


        // Try to extract image format from a comment like:
        //     /*format=rg8*/
        if (Info.Format == TEX_FORMAT_UNKNOWN)
        {
            std::string Format = ExtractGLSLImageFormatFromComment(CmtBeg, CmtEnd);
            if (!Format.empty())
                Info.Format = ParseGLSLImageFormat(Format);
        }


        // Try to extract access mode from a comment like:
        //     /*access=read*/
        //     /*access=write*/
        //     /*access=read_write*/
        std::string Access = ExtractGLSLAccessModeFromComment(CmtBeg, CmtEnd);
        if (!Access.empty())
            Info.AccessMode = ParseGLSLImageAccessMode(Access);

        Pos = End + 2;
    }
}

static std::pair<std::string, ImageFormatAndAccess> ParseRWTextureDefinition(
    HLSLTokenizer::TokenListType::const_iterator& Token,
    HLSLTokenizer::TokenListType::const_iterator  End)
{
    // RWTexture2D<unorm /*format=rg8*/ /*access=write*/ float4> g_RWTex;
    // ^ - RWTexture* keyword

    ++Token;
    // RWTexture2D<unorm /*format=rg8*/ /*access=write*/ float4> g_RWTex;
    //            ^ - '<' after RWTexture*
    if (Token == End || Token->Literal != "<")
        return {};

    ImageFormatAndAccess Info; /// Format = UNKNOWN, AccessMode = UNKNOWN by default

    // Walk through all tokens inside the '<' ... '>' list and look for
    // comments that annotate format and access mode.
    while (Token != End && Token->Literal != ">")
    {
        ++Token;
        if (Token != End)
        {
            /// Any comments preceding the current token are stored in Delimiter.
            if (!Token->Delimiter.empty())
            {
                ExtractAnnotationsFromDelimiter(Token->Delimiter, Info);
            }
        }
    }

    // RWTexture2D<unorm /*format=rg8*/ /*access=write*/ float4> g_RWTex;
    //                                                          ^ - '>' reached

    if (Token == End)
        return {};

    ++Token;
    if (Token == End)
        return {};

    // RWTexture2D<unorm /*format=rg8*/ /*access=write*/ float4> g_RWTex;
    //                                                           ^ - texture variable identifier
    if (Token->Type != HLSLTokenType::Identifier)
        return {};

    return {Token->Literal, Info};
}


std::unordered_map<HashMapStringKey, ImageFormatAndAccess> ExtractGLSLImageFormatsAndAccessModeFromHLSL(const std::string& HLSLSource)
{
    HLSLTokenizer                      Tokenizer;
    const HLSLTokenizer::TokenListType Tokens = Tokenizer.Tokenize(HLSLSource);

    std::unordered_map<HashMapStringKey, ImageFormatAndAccess> ImageFormats;

    auto Token      = Tokens.begin();
    int  ScopeLevel = 0;
    while (Token != Tokens.end())
    {
        if (Token->Type == HLSLTokenType::OpenBrace ||
            Token->Type == HLSLTokenType::OpenParen ||
            Token->Type == HLSLTokenType::OpenAngleBracket ||
            Token->Type == HLSLTokenType::OpenSquareBracket)
            ++ScopeLevel;

        if (Token->Type == HLSLTokenType::ClosingBrace ||
            Token->Type == HLSLTokenType::ClosingParen ||
            Token->Type == HLSLTokenType::ClosingAngleBracket ||
            Token->Type == HLSLTokenType::ClosingSquareBracket)
            --ScopeLevel;

        if (ScopeLevel < 0)
        {
            // No matching parenthesis found - stop parsing
            break;
        }

        if (ScopeLevel == 0 &&
            (Token->Type == HLSLTokenType::kw_RWTexture1D ||
             Token->Type == HLSLTokenType::kw_RWTexture1DArray ||
             Token->Type == HLSLTokenType::kw_RWTexture2D ||
             Token->Type == HLSLTokenType::kw_RWTexture2DArray ||
             Token->Type == HLSLTokenType::kw_RWTexture3D))
        {
            auto        NameAndInfo = ParseRWTextureDefinition(Token, Tokens.end());
            const auto& Name        = NameAndInfo.first;
            const auto& Info        = NameAndInfo.second;

            const bool HasFormat           = (Info.Format != TEX_FORMAT_UNKNOWN);
            const bool HasNonDefaultAccess = (Info.AccessMode != IMAGE_ACCESS_MODE_UNKNOWN);

            if (HasFormat || HasNonDefaultAccess)
            {
                auto InsertedIt = ImageFormats.emplace(Name, Info);
                if (!InsertedIt.second)
                {
                    const auto& Existing = InsertedIt.first->second;

                    if (Existing.Format != Info.Format)
                    {
                        LOG_WARNING_MESSAGE("Different formats are specified for the same RWTexture '", Name, "'. Note that the parser does not support preprocessing.");
                    }

                    if (Existing.AccessMode != Info.AccessMode)
                    {
                        LOG_WARNING_MESSAGE("Different access modes are specified for the same RWTexture '", Name, "'. Note that the parser does not support preprocessing.");
                    }
                }
            }
        }
        else
        {
            ++Token;
        }
    }

    return ImageFormats;
}

} // namespace Parsing

} // namespace Diligent
