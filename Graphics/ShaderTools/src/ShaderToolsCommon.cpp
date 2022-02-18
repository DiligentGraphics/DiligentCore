/*
 *  Copyright 2019-2022 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#include <unordered_set>

#include "ShaderToolsCommon.hpp"
#include "BasicFileSystem.hpp"
#include "DebugUtilities.hpp"
#include "DataBlobImpl.hpp"
#include "StringDataBlobImpl.hpp"

namespace Diligent
{

namespace
{

const ShaderMacro VSMacros[]  = {{"VERTEX_SHADER", "1"}, {}};
const ShaderMacro PSMacros[]  = {{"FRAGMENT_SHADER", "1"}, {"PIXEL_SHADER", "1"}, {}};
const ShaderMacro GSMacros[]  = {{"GEOMETRY_SHADER", "1"}, {}};
const ShaderMacro HSMacros[]  = {{"TESS_CONTROL_SHADER", "1"}, {"HULL_SHADER", "1"}, {}};
const ShaderMacro DSMacros[]  = {{"TESS_EVALUATION_SHADER", "1"}, {"DOMAIN_SHADER", "1"}, {}};
const ShaderMacro CSMacros[]  = {{"COMPUTE_SHADER", "1"}, {}};
const ShaderMacro ASMacros[]  = {{"TASK_SHADER", "1"}, {"AMPLIFICATION_SHADER", "1"}, {}};
const ShaderMacro MSMacros[]  = {{"MESH_SHADER", "1"}, {}};
const ShaderMacro RGMacros[]  = {{"RAY_GEN_SHADER", "1"}, {}};
const ShaderMacro RMMacros[]  = {{"RAY_MISS_SHADER", "1"}, {}};
const ShaderMacro RCHMacros[] = {{"RAY_CLOSEST_HIT_SHADER", "1"}, {}};
const ShaderMacro RAHMacros[] = {{"RAY_ANY_HIT_SHADER", "1"}, {}};
const ShaderMacro RIMacros[]  = {{"RAY_INTERSECTION_SHADER", "1"}, {}};
const ShaderMacro RCMacros[]  = {{"RAY_CALLABLE_SHADER", "1"}, {}};

} // namespace

const ShaderMacro* GetShaderTypeMacros(SHADER_TYPE Type)
{
    static_assert(SHADER_TYPE_LAST == 0x4000, "Please update the switch below to handle the new shader type");
    switch (Type)
    {
        // clang-format off
        case SHADER_TYPE_VERTEX:           return VSMacros;
        case SHADER_TYPE_PIXEL:            return PSMacros;
        case SHADER_TYPE_GEOMETRY:         return GSMacros;
        case SHADER_TYPE_HULL:             return HSMacros;
        case SHADER_TYPE_DOMAIN:           return DSMacros;
        case SHADER_TYPE_COMPUTE:          return CSMacros;
        case SHADER_TYPE_AMPLIFICATION:    return ASMacros;
        case SHADER_TYPE_MESH:             return MSMacros;
        case SHADER_TYPE_RAY_GEN:          return RGMacros;
        case SHADER_TYPE_RAY_MISS:         return RMMacros;
        case SHADER_TYPE_RAY_CLOSEST_HIT:  return RCHMacros;
        case SHADER_TYPE_RAY_ANY_HIT:      return RAHMacros;
        case SHADER_TYPE_RAY_INTERSECTION: return RIMacros;
        case SHADER_TYPE_CALLABLE:         return RCMacros;
        // clang-format on
        case SHADER_TYPE_TILE:
            UNEXPECTED("Unsupported shader type");
            return nullptr;
        default:
            UNEXPECTED("Unexpected shader type");
            return nullptr;
    }
}

void AppendShaderMacros(std::string& Source, const ShaderMacro* Macros)
{
    if (Macros == nullptr)
        return;

    for (auto* pMacro = Macros; pMacro->Name != nullptr && pMacro->Definition != nullptr; ++pMacro)
    {
        Source += "#define ";
        Source += pMacro->Name;
        Source += ' ';
        Source += pMacro->Definition;
        Source += "\n";
    }
}

void AppendShaderTypeDefinitions(std::string& Source, SHADER_TYPE Type)
{
    AppendShaderMacros(Source, GetShaderTypeMacros(Type));
}


const char* ReadShaderSourceFile(const char*                      SourceCode,
                                 IShaderSourceInputStreamFactory* pShaderSourceStreamFactory,
                                 const char*                      FilePath,
                                 RefCntAutoPtr<IDataBlob>&        pFileData,
                                 size_t&                          SourceCodeLen) noexcept(false)
{
    if (SourceCode != nullptr)
    {
        VERIFY(FilePath == nullptr, "FilePath must be null when SourceCode is not null");
        if (SourceCodeLen == 0)
            SourceCodeLen = strlen(SourceCode);
    }
    else
    {
        if (pShaderSourceStreamFactory != nullptr)
        {
            if (FilePath != nullptr)
            {
                RefCntAutoPtr<IFileStream> pSourceStream;
                pShaderSourceStreamFactory->CreateInputStream(FilePath, &pSourceStream);
                if (pSourceStream == nullptr)
                    LOG_ERROR_AND_THROW("Failed to load shader source file '", FilePath, '\'');

                pFileData = MakeNewRCObj<DataBlobImpl>{}(0);
                pSourceStream->ReadBlob(pFileData);
                SourceCode    = reinterpret_cast<char*>(pFileData->GetDataPtr());
                SourceCodeLen = pFileData->GetSize();
            }
            else
            {
                UNEXPECTED("FilePath is null");
            }
        }
        else
        {
            UNEXPECTED("Input stream factory is null");
        }
    }

    return SourceCode;
}

void AppendShaderSourceCode(std::string& Source, const ShaderCreateInfo& ShaderCI) noexcept(false)
{
    VERIFY_EXPR(ShaderCI.ByteCode == nullptr);

    RefCntAutoPtr<IDataBlob> pFileData;

    size_t SourceCodeLen = ShaderCI.SourceLength;

    const auto* SourceCode =
        ReadShaderSourceFile(ShaderCI.Source, ShaderCI.pShaderSourceStreamFactory,
                             ShaderCI.FilePath, pFileData, SourceCodeLen);
    Source.append(SourceCode, SourceCodeLen);
}

struct IncludeStringInfo
{
    String FileName = {};
    size_t Start    = 0;
    size_t End      = 0;
};

// https://github.com/tomtom-international/cpp-dependencies/blob/a91f330e97c6b9e4e9ecd81f43c4a40e044d4bbc/src/Input.cpp
static std::vector<IncludeStringInfo> ExtractDependencies(const char* pBuffer, size_t BufferSize)
{
    std::vector<IncludeStringInfo> IncludeList{};

    if (BufferSize == 0)
        return IncludeList;

    enum State
    {
        None,
        AfterHash,
        AfterInclude,
        InsideIncludeAngleBrackets,
        InsideIncludeQuotes
    } PreprocessorState = None;
    size_t Offset       = 0;

    // Find positions of the first hash and slash
    const Char* NextHash  = static_cast<const char*>(memchr(pBuffer + Offset, '#', BufferSize - Offset));
    const Char* NextSlash = static_cast<const char*>(memchr(pBuffer + Offset, '/', BufferSize - Offset));
    size_t      Start     = 0;

    // We iterate over the characters of the buffer
    while (Offset < BufferSize)
    {
        switch (PreprocessorState)
        {
            case None:
            {
                // Find a hash character if the current position is greater NextHash
                if (NextHash && NextHash < pBuffer + Offset)
                    NextHash = static_cast<const Char*>(memchr(pBuffer + Offset, '#', BufferSize - Offset));

                // Exit from the function if a hash is not found in the buffer
                if (NextHash == nullptr)
                    return IncludeList;

                // Find a slash character if the current position is greater NextSlash
                if (NextSlash && NextSlash < pBuffer + Offset)
                    NextSlash = static_cast<const Char*>(memchr(pBuffer + Offset, '/', BufferSize - Offset));

                if (NextSlash && NextSlash < NextHash)
                {
                    // Skip all characters if the slash character is before the hash character in the buffer
                    Offset = NextSlash - pBuffer;
                    if (pBuffer[Offset + 1] == '/')
                    {
                        Offset = static_cast<const Char*>(memchr(pBuffer + Offset, '\n', BufferSize - Offset)) - pBuffer;
                    }
                    else if (pBuffer[Offset + 1] == '*')
                    {
                        do
                        {
                            const char* EndSlash = static_cast<const Char*>(memchr(pBuffer + Offset + 1, '/', BufferSize - Offset));
                            if (!EndSlash)
                                return IncludeList;
                            Offset = EndSlash - pBuffer;
                        } while (pBuffer[Offset - 1] != '*');
                    }
                }
                else
                {
                    // Move the current position to the position after the hash
                    Offset            = NextHash - pBuffer;
                    PreprocessorState = AfterHash;
                }
            }
            break;
            case AfterHash:
                // Try to find the 'include' substring in the buffer if the current position is after the hash
                if (!isspace(pBuffer[Offset]))
                {
                    if (strncmp(pBuffer + Offset, "include", 7) == 0)
                    {
                        PreprocessorState = AfterInclude;
                        Offset += 6;
                    }
                    else
                    {
                        PreprocessorState = None;
                    }
                }
                break;
            case AfterInclude:
                // Try to find the opening quotes character after the 'include' substring
                if (!isspace(pBuffer[Offset]))
                {
                    if (pBuffer[Offset] == '"')
                    {
                        Start             = Offset + 1;
                        PreprocessorState = InsideIncludeQuotes;
                    }
                    else if (pBuffer[Offset] == '<')
                    {
                        Start             = Offset + 1;
                        PreprocessorState = InsideIncludeAngleBrackets;
                    }
                    else
                    {
                        PreprocessorState = None;
                    }
                }
                break;
            case InsideIncludeQuotes:
                // Try to find the closing quotes after the opening quotes and extract the substring for IncludeHandler(...)
                switch (pBuffer[Offset])
                {
                    case '\n':
                        PreprocessorState = None; // Buggy code, skip over this include.
                        break;
                    case '"':
                        IncludeList.push_back({String{&pBuffer[Start], &pBuffer[Offset]}, static_cast<size_t>(NextHash - pBuffer), Offset + 1});

                        PreprocessorState = None;
                        break;
                }
                break;
            case InsideIncludeAngleBrackets:
                // Try to find the closing quotes after the opening quotes and extract the substring for IncludeHandler(...)
                switch (pBuffer[Offset])
                {
                    case '\n':
                        PreprocessorState = None; // Buggy code, skip over this include.
                        break;
                    case '>':
                        IncludeList.push_back({String{&pBuffer[Start], &pBuffer[Offset]}, static_cast<size_t>(NextHash - pBuffer), Offset + 1});

                        PreprocessorState = None;
                        break;
                }
                break;
        }
        Offset++;
    }

    return IncludeList;
}

bool ProcessShaderIncludes(const ShaderCreateInfo& ShaderCI, std::function<void(const ShaderIncludePreprocessInfo&)> IncludeHandler)
{
    VERIFY_EXPR(ShaderCI.Desc.Name != nullptr);

    std::unordered_set<String> Includes;

    std::function<void(IDataBlob*, const char*, IShaderSourceInputStreamFactory*)> ParseShader;
    ParseShader = [&](IDataBlob* pDataBlob, const char* FilePath, IShaderSourceInputStreamFactory* pStreamFactory) //
    {
        for (auto const& Include : ExtractDependencies(static_cast<const char*>(pDataBlob->GetConstDataPtr()), pDataBlob->GetSize()))
        {
            if (Includes.emplace(Include.FileName).second)
            {
                RefCntAutoPtr<IDataBlob> pSourceData;
                size_t                   SourceLen = 0;
                ReadShaderSourceFile(nullptr, pStreamFactory, Include.FileName.c_str(), pSourceData, SourceLen);
                ParseShader(pSourceData, Include.FileName.c_str(), pStreamFactory);
            }
        }
        IncludeHandler({pDataBlob, FilePath});
    };

    try
    {
        if (ShaderCI.Source != nullptr)
        {
            auto pSourceData = DataBlobImpl::Create(ShaderCI.SourceLength > 0 ? ShaderCI.SourceLength : strlen(ShaderCI.Source), ShaderCI.Source);
            ParseShader(pSourceData, nullptr, ShaderCI.pShaderSourceStreamFactory);
        }
        else if (ShaderCI.FilePath != nullptr && ShaderCI.pShaderSourceStreamFactory != nullptr)
        {
            RefCntAutoPtr<IDataBlob> pSourceData;
            size_t                   SourceLen = ShaderCI.SourceLength;
            ReadShaderSourceFile(ShaderCI.Source, ShaderCI.pShaderSourceStreamFactory, ShaderCI.FilePath, pSourceData, SourceLen);
            ParseShader(pSourceData, ShaderCI.FilePath, ShaderCI.pShaderSourceStreamFactory);
        }
        else
        {
            LOG_ERROR_AND_THROW("Shader create info must contain Source or FilePath with pShaderSourceStreamFactory");
        }
        return true;
    }
    catch (...)
    {
        LOG_ERROR("Failed to preprocess shader: '", ShaderCI.Desc.Name, "'.");
        return false;
    }
}

std::string UnrollShaderIncludes(const ShaderCreateInfo& ShaderCI)
{
    VERIFY_EXPR(ShaderCI.Desc.Name != nullptr);

    auto GetSourceCode = [](const char* Source, size_t SourceLength, const char* FilePath, IShaderSourceInputStreamFactory* pStreamFactory) {
        RefCntAutoPtr<IDataBlob> pDummyDataBlob;
        const auto               pData = ReadShaderSourceFile(Source, pStreamFactory, FilePath, pDummyDataBlob, SourceLength);
        return String{pData, SourceLength};
    };

    auto ValidateIncludes = [](const char* FileName, std::vector<IncludeStringInfo> const& IncludeList) {
        for (size_t i = 0; i < IncludeList.size(); ++i)
            for (size_t j = i + 1; j < IncludeList.size(); ++j)
                if (IncludeList[i].FileName == IncludeList[j].FileName)
                    LOG_WARNING_MESSAGE("Double definition of the include directive '", IncludeList[j].FileName, "' in the '", FileName, "' file");
    };

    std::unordered_set<String> Includes;

    std::function<String(const char*, size_t, const char*, IShaderSourceInputStreamFactory*)> ParseShader;
    ParseShader = [&](const char* Source, size_t SourceLength, const char* FilePath, IShaderSourceInputStreamFactory* pStreamFactory) -> String //
    {
        const auto Data = GetSourceCode(Source, SourceLength, FilePath, pStreamFactory);

        const auto IncludeList = ExtractDependencies(Data.c_str(), Data.size());
        ValidateIncludes(FilePath, IncludeList);

        String Result = Data;
        size_t Offset = 0;

        for (auto const& Include : IncludeList)
        {
            const size_t RangeSubstring = Include.End - Include.Start;
            if (Includes.emplace(Include.FileName).second)
            {
                const auto IncludedFile = ParseShader(nullptr, 0, Include.FileName.c_str(), pStreamFactory);
                Result.replace(Offset + Include.Start, RangeSubstring, IncludedFile);
                Offset += IncludedFile.size();
            }
            else
            {
                Result.replace(Offset + Include.Start, RangeSubstring, "");
            }
            Offset -= RangeSubstring;
        }
        return Result;
    };

    try
    {
        return ParseShader(ShaderCI.Source, ShaderCI.SourceLength, ShaderCI.FilePath, ShaderCI.pShaderSourceStreamFactory);
    }
    catch (...)
    {
        LOG_ERROR("Failed to merge includes for shader: '", ShaderCI.Desc.Name, "'.");
        return "";
    }
}

} // namespace Diligent
