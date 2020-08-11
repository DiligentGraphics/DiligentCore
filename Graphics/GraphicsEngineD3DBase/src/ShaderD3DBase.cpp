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
#include <locale>
#include <cwchar>

#ifdef HAS_DXIL_COMPILER
#include "dxcapi.h"
#endif

namespace Diligent
{

static const Char* g_HLSLDefinitions =
    {
#include "HLSLDefinitions_inc.fxh"
};


#ifdef HAS_DXIL_COMPILER
class DxcIncludeHandlerImpl final : public IDxcIncludeHandler
{
public:
    explicit DxcIncludeHandlerImpl(IShaderSourceInputStreamFactory* pStreamFactory, CComPtr<IDxcLibrary> pLibrary) :
        m_pLibrary{pLibrary},
        m_pStreamFactory{pStreamFactory},
        m_RefCount{1}
    {
    }

    HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource) override
    {
        String fileName = std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>, wchar_t>{}.to_bytes(pFilename);
        if (fileName.empty())
        {
            LOG_ERROR("Failed to convert shader include file name ", fileName, ". File name must be ANSI string");
            return E_FAIL;
        }

        RefCntAutoPtr<IFileStream> pSourceStream;
        m_pStreamFactory->CreateInputStream(fileName.c_str(), &pSourceStream);
        if (pSourceStream == nullptr)
        {
            LOG_ERROR("Failed to open shader include file ", fileName, ". Check that the file exists");
            return E_FAIL;
        }
        
        RefCntAutoPtr<IDataBlob> pFileData(MakeNewRCObj<DataBlobImpl>()(0));
        pSourceStream->ReadBlob(pFileData);
        
        CComPtr<IDxcBlobEncoding> sourceBlob;
        HRESULT hr = m_pLibrary->CreateBlobWithEncodingFromPinned(pFileData->GetDataPtr(), UINT32(pFileData->GetSize()), CP_UTF8, &sourceBlob);
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to allocate space for shader include file ", fileName, ".");
            return E_FAIL;
        }
        
        m_FileDataCache.push_back(pFileData);

        sourceBlob->QueryInterface(__uuidof(*ppIncludeSource), reinterpret_cast<void**>(ppIncludeSource));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override
    {
        return E_FAIL;
    }

    ULONG STDMETHODCALLTYPE AddRef(void) override
    {
        return m_RefCount++;
    }

    ULONG STDMETHODCALLTYPE Release(void) override
    {
        --m_RefCount;
        VERIFY(m_RefCount > 0, "Inconsistent call to Release()");
        return m_RefCount;
    }

private:
    CComPtr<IDxcLibrary>                  m_pLibrary;
    IShaderSourceInputStreamFactory*      m_pStreamFactory;
    ULONG                                 m_RefCount;
    std::vector<RefCntAutoPtr<IDataBlob>> m_FileDataCache;
};
#endif

static HRESULT CompileDxilShader(const char*             Source,
                                 const ShaderCreateInfo& ShaderCI,
                                 LPCSTR                  profile,
                                 ID3DBlob**              ppBlobOut,
                                 ID3DBlob**              ppCompilerOutput)
{
#ifdef HAS_DXIL_COMPILER
    std::vector<WCHAR> unicodeBuffer; unicodeBuffer.resize(1u << 15);
    size_t             unicodeBufferOffset = 0;

    const auto ToUnicode = [&unicodeBuffer, &unicodeBufferOffset](const char* str) {
        auto len = strlen(str) + 1;
        auto pos = unicodeBufferOffset;
        unicodeBufferOffset += len;
        VERIFY(unicodeBufferOffset < unicodeBuffer.size(), "buffer overflow");
        for (size_t i = 0; i < len; ++i) {
            unicodeBuffer[pos + i] = str[i];
        }
        return &unicodeBuffer[pos];
    };
    
    std::vector<DxcDefine> D3DMacros;
    switch (ShaderCI.Desc.ShaderType)
    {
        case SHADER_TYPE_VERTEX:
            D3DMacros.push_back({L"VERTEX_SHADER", L"1"});
            break;

        case SHADER_TYPE_PIXEL:
            D3DMacros.push_back({L"FRAGMENT_SHADER", L"1"});
            D3DMacros.push_back({L"PIXEL_SHADER", L"1"});
            break;

        case SHADER_TYPE_GEOMETRY:
            D3DMacros.push_back({L"GEOMETRY_SHADER", L"1"});
            break;

        case SHADER_TYPE_HULL:
            D3DMacros.push_back({L"TESS_CONTROL_SHADER", L"1"});
            D3DMacros.push_back({L"HULL_SHADER", L"1"});
            break;

        case SHADER_TYPE_DOMAIN:
            D3DMacros.push_back({L"TESS_EVALUATION_SHADER", L"1"});
            D3DMacros.push_back({L"DOMAIN_SHADER", L"1"});
            break;

        case SHADER_TYPE_COMPUTE:
            D3DMacros.push_back({L"COMPUTE_SHADER", L"1"});
            break;
                
        case SHADER_TYPE_AMPLIFICATION:
            D3DMacros.push_back({L"TASK_SHADER", L"1"});
            D3DMacros.push_back({L"AMPLIFICATION_SHADER", L"1"});
            break;

        case SHADER_TYPE_MESH:
            D3DMacros.push_back({L"MESH_SHADER", L"1"});
            break;

        default: UNEXPECTED("Unexpected shader type");
    }

    if (ShaderCI.Macros)
    {
        for (auto* pCurrMacro = ShaderCI.Macros; pCurrMacro->Name && pCurrMacro->Definition; ++pCurrMacro)
        {
            D3DMacros.push_back({ToUnicode(pCurrMacro->Name), ToUnicode(pCurrMacro->Definition)});
        }
    }

    HRESULT hr;

    CComPtr<IDxcLibrary> library;
    hr = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
    if (FAILED(hr))
        return hr;

    CComPtr<IDxcCompiler> compiler;
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    if (FAILED(hr))
        return hr;

    CComPtr<IDxcBlobEncoding> sourceBlob;
    hr = library->CreateBlobWithEncodingFromPinned(Source, UINT32(strlen(Source)), CP_UTF8, &sourceBlob);
    if (FAILED(hr))
        return hr;
    
    const wchar_t* pArgs[] =
    {
        L"-Zpc",            // Matrices in column-major order
        L"-WX",             // Warnings as errors
#   ifdef DILIGENT_DEBUG
        L"-Zi",             // Debug info
        //L"-Qembed_debug",   // Embed debug info into the shader (some compilers do not recognize this flag)
        L"-Od",             // Disable optimization
#   else
        L"-O3",             // Optimization level 3
#   endif
    };

    DxcIncludeHandlerImpl IncludeHandler{ShaderCI.pShaderSourceStreamFactory, library};

    CComPtr<IDxcOperationResult> result;
    hr = compiler->Compile(
        sourceBlob,
        L"",
        ToUnicode(ShaderCI.EntryPoint),
        ToUnicode(profile),
        pArgs, UINT32(std::size(pArgs)),
        D3DMacros.data(), UINT32(D3DMacros.size()),
        &IncludeHandler,
        &result);

    if (SUCCEEDED(hr))
    {
        HRESULT status;
        if (SUCCEEDED(result->GetStatus(&status)))
            hr = status;
    }

    if (FAILED(hr))
    {
        if (result)
        {
            CComPtr<IDxcBlobEncoding> errorsBlob;
            if (SUCCEEDED(result->GetErrorBuffer(&errorsBlob)) && errorsBlob)
            {
                errorsBlob->QueryInterface(__uuidof(*ppCompilerOutput), reinterpret_cast<void**>(ppCompilerOutput));
            }
        }
        return hr;
    }

    hr = result->GetResult(reinterpret_cast<IDxcBlob**>(ppBlobOut));
    return hr;
#else
    UNSUPPORTED("Shader model 6 and above requires DXIL compiler");
    return E_FAIL;
#endif
}


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

static HRESULT CompileShader(const char*             Source,
                             const ShaderCreateInfo& ShaderCI,
                             LPCSTR                  profile,
                             ID3DBlob**              ppBlobOut,
                             ID3DBlob**              ppCompilerOutput)
{
    std::vector<D3D_SHADER_MACRO> D3DMacros;
    switch (ShaderCI.Desc.ShaderType)
    {
        case SHADER_TYPE_VERTEX:
            D3DMacros.push_back({"VERTEX_SHADER", "1"});
            break;

        case SHADER_TYPE_PIXEL:
            D3DMacros.push_back({"FRAGMENT_SHADER", "1"});
            D3DMacros.push_back({"PIXEL_SHADER", "1"});
            break;

        case SHADER_TYPE_GEOMETRY:
            D3DMacros.push_back({"GEOMETRY_SHADER", "1"});
            break;

        case SHADER_TYPE_HULL:
            D3DMacros.push_back({"TESS_CONTROL_SHADER", "1"});
            D3DMacros.push_back({"HULL_SHADER", "1"});
            break;

        case SHADER_TYPE_DOMAIN:
            D3DMacros.push_back({"TESS_EVALUATION_SHADER", "1"});
            D3DMacros.push_back({"DOMAIN_SHADER", "1"});
            break;

        case SHADER_TYPE_COMPUTE:
            D3DMacros.push_back({"COMPUTE_SHADER", "1"});
            break;

        default: UNEXPECTED("Unexpected shader type");
    }

    if (ShaderCI.Macros)
    {
        for (auto* pCurrMacro = ShaderCI.Macros; pCurrMacro->Name && pCurrMacro->Definition; ++pCurrMacro)
        {
            D3DMacros.push_back({pCurrMacro->Name, pCurrMacro->Definition});
        }
    }

    D3DMacros.push_back({nullptr, nullptr});

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DILIGENT_DEBUG)
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

    //	do
    //	{

    D3DIncludeImpl IncludeImpl(ShaderCI.pShaderSourceStreamFactory);
    auto SourceLen = strlen(Source);
    auto hr = D3DCompile(Source, SourceLen, NULL, D3DMacros.data(), &IncludeImpl, ShaderCI.EntryPoint, profile, dwShaderFlags, 0, ppBlobOut, ppCompilerOutput);

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

ShaderD3DBase::ShaderD3DBase(const ShaderCreateInfo& ShaderCI, Uint8 ShaderModel) :
    m_isDXIL{false}
{
    if (ShaderCI.Source || ShaderCI.FilePath)
    {
        DEV_CHECK_ERR(ShaderCI.ByteCode == nullptr, "'ByteCode' must be null when shader is created from the source code or a file");
        DEV_CHECK_ERR(ShaderCI.ByteCodeSize == 0, "'ByteCodeSize' must be 0 when shader is created from the source code or a file");

        std::string strShaderProfile;
        switch (ShaderCI.Desc.ShaderType)
        {
            // clang-format off
            case SHADER_TYPE_VERTEX:        strShaderProfile="vs"; break;
            case SHADER_TYPE_PIXEL:         strShaderProfile="ps"; break;
            case SHADER_TYPE_GEOMETRY:      strShaderProfile="gs"; break;
            case SHADER_TYPE_HULL:          strShaderProfile="hs"; break;
            case SHADER_TYPE_DOMAIN:        strShaderProfile="ds"; break;
            case SHADER_TYPE_COMPUTE:       strShaderProfile="cs"; break;
            case SHADER_TYPE_AMPLIFICATION: strShaderProfile="as"; break;
            case SHADER_TYPE_MESH:          strShaderProfile="ms"; break;
                // clang-format on

            default: UNEXPECTED("Unknown shader type");
        }
        strShaderProfile += "_";
        strShaderProfile += '0' + ((ShaderModel >> 4) & 0xF);
        strShaderProfile += "_";
        strShaderProfile += ((ShaderModel & 0xF) == 0xF) ? 'x' : ('0' + (ShaderModel & 0xF));

        m_isDXIL = (ShaderModel >= 0x60);

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

        DEV_CHECK_ERR(ShaderCI.EntryPoint != nullptr, "Entry point must not be null");

        CComPtr<ID3DBlob> errors;
        HRESULT           hr;
        
        if (m_isDXIL)
            hr = CompileDxilShader(ShaderSource.c_str(), ShaderCI, strShaderProfile.c_str(), &m_pShaderByteCode, &errors);
        else
            hr = CompileShader(ShaderSource.c_str(), ShaderCI, strShaderProfile.c_str(), &m_pShaderByteCode, &errors);

        const char* CompilerMsg = errors ? static_cast<const char*>(errors->GetBufferPointer()) : nullptr;
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
