/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

#include <unordered_map>
#include <D3Dcompiler.h>

#include "D3DErrors.hpp"
#include "DataBlobImpl.hpp"
#include "RefCntAutoPtr.hpp"
#include <atlcomcli.h>
#include "ShaderD3DBase.hpp"

namespace Diligent
{

static const Char* g_HLSLDefinitions =
    {
#include "HLSLDefinitions_inc.fxh"
};

class D3DIncludeImpl : public ID3DInclude
{
public:
    D3DIncludeImpl(IShaderSourceInputStreamFactory* pStreamFactory) :
        m_pStreamFactory{pStreamFactory}
    {
    }

    STDMETHOD(Open)
    (THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes)
    {
        RefCntAutoPtr<IFileStream> pSourceStream;
        m_pStreamFactory->CreateInputStream(pFileName, &pSourceStream);
        if (pSourceStream == nullptr)
        {
            LOG_ERROR("Failed to open shader include file ", pFileName, ". Check that the file exists");
            return E_FAIL;
        }

        RefCntAutoPtr<IDataBlob> pFileData(MakeNewRCObj<DataBlobImpl>()(0));
        pSourceStream->ReadBlob(pFileData);
        *ppData = pFileData->GetDataPtr();
        *pBytes = static_cast<UINT>(pFileData->GetSize());

        m_DataBlobs.insert(std::make_pair(*ppData, pFileData));

        return S_OK;
    }

    STDMETHOD(Close)
    (THIS_ LPCVOID pData)
    {
        m_DataBlobs.erase(pData);
        return S_OK;
    }

private:
    IShaderSourceInputStreamFactory*                      m_pStreamFactory;
    std::unordered_map<LPCVOID, RefCntAutoPtr<IDataBlob>> m_DataBlobs;
};

static HRESULT CompileShader(const char*                      Source,
                             LPCSTR                           strFunctionName,
                             const D3D_SHADER_MACRO*          pDefines,
                             IShaderSourceInputStreamFactory* pIncludeStreamFactory,
                             LPCSTR                           profile,
                             ID3DBlob**                       ppBlobOut,
                             ID3DBlob**                       ppCompilerOutput)
{
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows
    // the shaders to be optimized and to run exactly the way they will run in
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#else
    // Warning: do not use this flag as it causes shader compiler to fail the compilation and
    // report strange errors:
    // dwShaderFlags |= D3D10_SHADER_OPTIMIZATION_LEVEL3;
#endif
    HRESULT hr;
    //	do
    //	{
    auto SourceLen = strlen(Source);

    D3DIncludeImpl IncludeImpl(pIncludeStreamFactory);
    hr = D3DCompile(Source, SourceLen, NULL, pDefines, &IncludeImpl, strFunctionName, profile, dwShaderFlags, 0, ppBlobOut, ppCompilerOutput);

    //		if( FAILED(hr) || errors )
    //		{
    //			if( FAILED(hr)
    //#if PLATFORM_WIN32
    //                && IDRETRY != MessageBoxW( NULL, L"Failed to compile shader", L"FX Error", MB_ICONERROR | (Source == nullptr ? MB_ABORTRETRYIGNORE : 0) )
    //#endif
    //                )
    //			{
    //				break;
    //			}
    //		}
    //	} while( FAILED(hr) );
    return hr;
}

ShaderD3DBase::ShaderD3DBase(const ShaderCreateInfo& ShaderCI, const char* ShaderModel)
{
    if (ShaderCI.Source || ShaderCI.FilePath)
    {
        DEV_CHECK_ERR(ShaderCI.ByteCode == nullptr, "'ByteCode' must be null when shader is created from the source code or a file");
        DEV_CHECK_ERR(ShaderCI.ByteCodeSize == 0, "'ByteCodeSize' must be 0 when shader is created from the source code or a file");

        std::string strShaderProfile;
        switch (ShaderCI.Desc.ShaderType)
        {
            // clang-format off
            case SHADER_TYPE_VERTEX:  strShaderProfile="vs"; break;
            case SHADER_TYPE_PIXEL:   strShaderProfile="ps"; break;
            case SHADER_TYPE_GEOMETRY:strShaderProfile="gs"; break;
            case SHADER_TYPE_HULL:    strShaderProfile="hs"; break;
            case SHADER_TYPE_DOMAIN:  strShaderProfile="ds"; break;
            case SHADER_TYPE_COMPUTE: strShaderProfile="cs"; break;
                // clang-format on

            default: UNEXPECTED("Unknown shader type");
        }
        strShaderProfile += "_";
        strShaderProfile += ShaderModel;

        String ShaderSource(g_HLSLDefinitions);
        if (ShaderCI.Source)
        {
            DEV_CHECK_ERR(ShaderCI.FilePath == nullptr, "'FilePath' is expected to be null when shader source code is provided");
            ShaderSource.append(ShaderCI.Source);
        }
        else
        {
            DEV_CHECK_ERR(ShaderCI.pShaderSourceStreamFactory, "Input stream factory is null");
            RefCntAutoPtr<IFileStream> pSourceStream;
            ShaderCI.pShaderSourceStreamFactory->CreateInputStream(ShaderCI.FilePath, &pSourceStream);
            RefCntAutoPtr<IDataBlob> pFileData(MakeNewRCObj<DataBlobImpl>()(0));
            if (pSourceStream == nullptr)
                LOG_ERROR_AND_THROW("Failed to open shader source file");
            pSourceStream->ReadBlob(pFileData);
            // Null terminator is not read from the stream!
            auto* FileDataPtr = reinterpret_cast<Char*>(pFileData->GetDataPtr());
            auto  Size        = pFileData->GetSize();
            ShaderSource.append(FileDataPtr, FileDataPtr + Size / sizeof(*FileDataPtr));
        }

        const D3D_SHADER_MACRO*       pDefines = nullptr;
        std::vector<D3D_SHADER_MACRO> D3DMacros;
        if (ShaderCI.Macros)
        {
            for (auto* pCurrMacro = ShaderCI.Macros; pCurrMacro->Name && pCurrMacro->Definition; ++pCurrMacro)
            {
                D3DMacros.push_back({pCurrMacro->Name, pCurrMacro->Definition});
            }
            D3DMacros.push_back({nullptr, nullptr});
            pDefines = D3DMacros.data();
        }

        DEV_CHECK_ERR(ShaderCI.EntryPoint != nullptr, "Entry point must not be null");
        CComPtr<ID3DBlob> errors;
        auto              hr = CompileShader(ShaderSource.c_str(), ShaderCI.EntryPoint, pDefines, ShaderCI.pShaderSourceStreamFactory, strShaderProfile.c_str(), &m_pShaderByteCode, &errors);

        const char* CompilerMsg = errors ? reinterpret_cast<const char*>(errors->GetBufferPointer()) : nullptr;
        if (CompilerMsg != nullptr && ShaderCI.ppCompilerOutput != nullptr)
        {
            auto  ErrorMsgLen     = strlen(CompilerMsg);
            auto* pOutputDataBlob = MakeNewRCObj<DataBlobImpl>()(ErrorMsgLen + 1 + ShaderSource.length() + 1);
            char* DataPtr         = reinterpret_cast<char*>(pOutputDataBlob->GetDataPtr());
            memcpy(DataPtr, CompilerMsg, ErrorMsgLen + 1);
            memcpy(DataPtr + ErrorMsgLen + 1, ShaderSource.data(), ShaderSource.length() + 1);
            pOutputDataBlob->QueryInterface(IID_DataBlob, reinterpret_cast<IObject**>(ShaderCI.ppCompilerOutput));
        }

        if (FAILED(hr))
        {
            ComErrorDesc ErrDesc(hr);
            if (ShaderCI.ppCompilerOutput != nullptr)
            {
                LOG_ERROR_AND_THROW("Failed to compile D3D shader \"", (ShaderCI.Desc.Name != nullptr ? ShaderCI.Desc.Name : ""), "\" (", ErrDesc.Get(), ").");
            }
            else
            {
                LOG_ERROR_AND_THROW("Failed to compile D3D shader \"", (ShaderCI.Desc.Name != nullptr ? ShaderCI.Desc.Name : ""), "\" (", ErrDesc.Get(), "):\n", (CompilerMsg != nullptr ? CompilerMsg : "<no compiler log available>"));
            }
        }
    }
    else if (ShaderCI.ByteCode)
    {
        DEV_CHECK_ERR(ShaderCI.ByteCodeSize != 0, "ByteCode size must be greater than 0");
        CHECK_D3D_RESULT_THROW(D3DCreateBlob(ShaderCI.ByteCodeSize, &m_pShaderByteCode), "Failed to create D3D blob");
        memcpy(m_pShaderByteCode->GetBufferPointer(), ShaderCI.ByteCode, ShaderCI.ByteCodeSize);
    }
    else
    {
        LOG_ERROR_AND_THROW("Shader source must be provided through one of the 'Source', 'FilePath' or 'ByteCode' members");
    }
}

} // namespace Diligent
