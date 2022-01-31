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

#include <functional>
#include <unordered_set>

#include "ShaderPreprocessor.hpp"
#include "RenderDevice.h"
#include "RefCntAutoPtr.hpp"
#include "DataBlobImpl.hpp"
#include "ShaderToolsCommon.hpp"

namespace
{

// https://github.com/tomtom-international/cpp-dependencies/blob/a91f330e97c6b9e4e9ecd81f43c4a40e044d4bbc/src/Input.cpp
void ExtractDependencies(std::function<void(const Diligent::String&)> IncludeHandler, const char* pBuffer, size_t BufferSize)
{
    if (BufferSize == 0)
        return;

    enum State
    {
        None,
        AfterHash,
        AfterInclude,
        InsideIncludeBrackets
    } PreprocessorState = None;
    size_t Offset       = 0;

    // Find positions of the first hash and slash
    const char* NextHash  = static_cast<const char*>(memchr(pBuffer + Offset, '#', BufferSize - Offset));
    const char* NextSlash = static_cast<const char*>(memchr(pBuffer + Offset, '/', BufferSize - Offset));
    size_t      Start     = 0;

    //We iterate over the characters of the buffer
    while (Offset < BufferSize)
    {
        switch (PreprocessorState)
        {
            case None:
            {
                // Find a hash character if the current position is greater NextHash
                if (NextHash && NextHash < pBuffer + Offset)
                    NextHash = static_cast<const char*>(memchr(pBuffer + Offset, '#', BufferSize - Offset));

                // Exit from the function if a hash is not found in the buffer
                if (NextHash == nullptr)
                    return;

                // Find a slash character if the current position is greater NextSlash
                if (NextSlash && NextSlash < pBuffer + Offset)
                    NextSlash = static_cast<const char*>(memchr(pBuffer + Offset, '/', BufferSize - Offset));

                if (NextSlash && NextSlash < NextHash)
                {
                    // Skip all characters if the slash character is before the hash character in the buffer
                    Offset = NextSlash - pBuffer;
                    if (pBuffer[Offset + 1] == '/')
                    {
                        Offset = static_cast<const char*>(memchr(pBuffer + Offset, '\n', BufferSize - Offset)) - pBuffer;
                    }
                    else if (pBuffer[Offset + 1] == '*')
                    {
                        do
                        {
                            const char* EndSlash = static_cast<const char*>(memchr(pBuffer + Offset + 1, '/', BufferSize - Offset));
                            if (!EndSlash)
                                return;
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
                        PreprocessorState = InsideIncludeBrackets;
                    }
                    else
                    {
                        PreprocessorState = None;
                    }
                }
                break;
            case InsideIncludeBrackets:
                // Try to find the closing quotes after the opening quotes and extract the substring for IncludeHandler(...)
                switch (pBuffer[Offset])
                {
                    case '\n':
                        PreprocessorState = None; // Buggy code, skip over this include.
                        break;
                    case '"':
                        IncludeHandler(std::string(&pBuffer[Start], &pBuffer[Offset]));
                        PreprocessorState = None;
                        break;
                }
                break;
        }
        Offset++;
    }
}

} // namespace

namespace Diligent
{

bool ShaderIncludePreprocessor(const ShaderCreateInfo& ShaderCI, std::function<void(const ShaderIncludePreprocessorInfo&)> DataHandler)
{
    VERIFY_EXPR(ShaderCI.Desc.Name != nullptr);

    std::unordered_set<String> Includes;

    std::function<void(const ShaderIncludePreprocessorInfo&, IShaderSourceInputStreamFactory*)> ParseShader;
    ParseShader = [&](const ShaderIncludePreprocessorInfo& PreprocessorInfo, IShaderSourceInputStreamFactory* pStreamFactory) //
    {
        auto IncludeHandler = [&](const String& Include) //
        {
            if (Includes.emplace(Include).second)
            {
                RefCntAutoPtr<IDataBlob> pSourceData;
                size_t                   SourceLen = 0;
                ReadShaderSourceFile(nullptr, ShaderCI.pShaderSourceStreamFactory, Include.c_str(), pSourceData, SourceLen);
                ParseShader({pSourceData, Include.c_str()}, pStreamFactory);
            }
        };

        ExtractDependencies(IncludeHandler, static_cast<const char*>(PreprocessorInfo.DataBlob->GetConstDataPtr()), PreprocessorInfo.DataBlob->GetSize());
        DataHandler(PreprocessorInfo);
    };

    try
    {
        if (ShaderCI.Source != nullptr)
        {
            auto pSourceData = DataBlobImpl::Create(ShaderCI.SourceLength > 0 ? ShaderCI.SourceLength : strlen(ShaderCI.Source), ShaderCI.Source);
            ParseShader({pSourceData, nullptr}, ShaderCI.pShaderSourceStreamFactory);
        }
        else if (ShaderCI.FilePath != nullptr && ShaderCI.pShaderSourceStreamFactory != nullptr)
        {
            RefCntAutoPtr<IDataBlob> pSourceData;
            size_t                   SourceLen = ShaderCI.SourceLength;
            ReadShaderSourceFile(ShaderCI.Source, ShaderCI.pShaderSourceStreamFactory, ShaderCI.FilePath, pSourceData, SourceLen);
            ParseShader({pSourceData, ShaderCI.FilePath}, ShaderCI.pShaderSourceStreamFactory);
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

} // namespace Diligent
