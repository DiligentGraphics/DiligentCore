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

#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <array>
#include <locale>
#include <cwchar>

#ifdef WIN32
#    include <Unknwn.h>
#    include <guiddef.h>
#    include <atlbase.h>
#    include <atlcom.h>
#endif

#include "../../ThirdParty/dxc/dxcapi.h"

#include "DXILUtils.hpp"
#include "DataBlobImpl.hpp"
#include "RefCntAutoPtr.hpp"

namespace Diligent
{

// Implemented in GLSLSourceBuilder.cpp
const char* GetShaderTypeDefines(SHADER_TYPE Type);

namespace
{

// clang-format off
static const char g_HLSLDefinitions[] =
{
#include "../../GraphicsEngineD3DBase/include/HLSLDefinitions_inc.fxh"
};
// clang-format on

struct VkDXILCompilerLib
{
    HMODULE               Module         = nullptr;
    DxcCreateInstanceProc CreateInstance = nullptr;
    ShaderVersion         MaxShaderModel{6, 5};

    VkDXILCompilerLib()
    {
        Module = LoadLibraryA("vk_dxcompiler.dll");
        if (Module)
        {
            CreateInstance = reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(Module, "DxcCreateInstance"));

            if (CreateInstance)
            {
                CComPtr<IDxcValidator> validator;
                if (SUCCEEDED(CreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&validator))))
                {
                    CComPtr<IDxcVersionInfo> info;
                    if (SUCCEEDED(validator->QueryInterface(IID_PPV_ARGS(&info))))
                    {
                        UINT32 ver = 0, minor = 0;
                        info->GetVersion(&ver, &minor);

                        LOG_INFO_MESSAGE("Loaded Vulkan DXIL compiler, version ", ver, ".", minor);

                        ver = (ver << 16) | (minor & 0xFFFF);

                        // map known DXC version to maximum SM
                        switch (ver)
                        {
                            case 0x10005: MaxShaderModel = {6, 5}; break;
                            case 0x10004: MaxShaderModel = {6, 4}; break;                                                 // SM 6.4 and SM 6.5 preview ???
                            case 0x10002: MaxShaderModel = {6, 2}; break;                                                 // SM 6.1 and SM 6.2 preview
                            default: MaxShaderModel = (ver > 0x10005 ? ShaderVersion{6, 6} : ShaderVersion{6, 0}); break; // unknown version
                        }
                    }
                }
            }
        }
    }

    ~VkDXILCompilerLib()
    {
        FreeLibrary(Module);
    }

    static VkDXILCompilerLib& Instance()
    {
        static VkDXILCompilerLib inst;
        return inst;
    }
};

class DxcIncludeHandlerImpl final : public IDxcIncludeHandler
{
public:
    explicit DxcIncludeHandlerImpl(IShaderSourceInputStreamFactory* pStreamFactory, CComPtr<IDxcLibrary> pLibrary) :
        m_pLibrary{pLibrary},
        m_pStreamFactory{pStreamFactory},
        m_RefCount{1}
    {
    }

    HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
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
        HRESULT                   hr = m_pLibrary->CreateBlobWithEncodingFromPinned(pFileData->GetDataPtr(), UINT32(pFileData->GetSize()), CP_UTF8, &sourceBlob);
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to allocate space for shader include file ", fileName, ".");
            return E_FAIL;
        }

        m_FileDataCache.push_back(pFileData);

        sourceBlob->QueryInterface(IID_PPV_ARGS(ppIncludeSource));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override
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

static HRESULT CompileDxilShader(const std::string&               Source,
                                 const char*                      EntryPoint,
                                 SHADER_TYPE                      ShaderType,
                                 IShaderSourceInputStreamFactory* pShaderSourceStreamFactory,
                                 ShaderVersion                    Version,
                                 std::vector<unsigned int>&       SPIRV,
                                 IDataBlob**                      ppCompilerOutput)
{
    auto& DxilLib = VkDXILCompilerLib::Instance();

    if (DxilLib.CreateInstance == nullptr)
    {
        LOG_ERROR("Failed to load vk_dxcompiler.dll");
        return E_FAIL;
    }

    // clamp to maximum supported version
    if (Version.Major > DxilLib.MaxShaderModel.Major || (Version.Major == DxilLib.MaxShaderModel.Major && Version.Minor > DxilLib.MaxShaderModel.Minor))
        Version = DxilLib.MaxShaderModel;

    if (Version.Major == 0 || Version.Major < 6)
        Version = DxilLib.MaxShaderModel;

    std::wstring Profile;
    // clang-format off
    switch (ShaderType)
    {
        case SHADER_TYPE_VERTEX:        Profile = L"vs_"; break;
        case SHADER_TYPE_PIXEL:         Profile = L"ps_"; break;
        case SHADER_TYPE_GEOMETRY:      Profile = L"gs_"; break;
        case SHADER_TYPE_HULL:          Profile = L"hs_"; break;
        case SHADER_TYPE_DOMAIN:        Profile = L"ds_"; break;
        case SHADER_TYPE_COMPUTE:       Profile = L"cs_"; break;
        case SHADER_TYPE_AMPLIFICATION: Profile = L"as_"; break;
        case SHADER_TYPE_MESH:          Profile = L"ms_"; break;
        default: UNEXPECTED("Unexpected shader type");
    }
    // clang-format on

    Profile += L'0' + Version.Major;
    Profile += L'_';
    Profile += L'0' + Version.Minor;

    HRESULT hr;

    CComPtr<IDxcLibrary> library;
    hr = DxilLib.CreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
    if (FAILED(hr))
        return hr;

    CComPtr<IDxcCompiler> compiler;
    hr = DxilLib.CreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    if (FAILED(hr))
        return hr;

    CComPtr<IDxcBlobEncoding> sourceBlob;
    hr = library->CreateBlobWithEncodingFromPinned(Source.c_str(), UINT32(Source.length()), CP_UTF8, &sourceBlob);
    if (FAILED(hr))
        return hr;

    const wchar_t* pArgs[] =
        {
            L"-spirv",
            L"-fspv-reflect",
            L"-WX", // Warnings as errors
            L"-O3", // Optimization level 3
        };

    DxcIncludeHandlerImpl IncludeHandler{pShaderSourceStreamFactory, library};

    CComPtr<IDxcOperationResult> result;
    hr = compiler->Compile(
        sourceBlob,
        L"",
        std::wstring{EntryPoint, EntryPoint + strlen(EntryPoint)}.c_str(),
        Profile.c_str(),
        pArgs, UINT32(std::size(pArgs)),
        nullptr, 0,
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
            if (SUCCEEDED(result->GetErrorBuffer(&errorsBlob)))
            {
                std::string ErrorLog;

                if (errorsBlob->GetBufferSize())
                    ErrorLog.assign(static_cast<const char*>(errorsBlob->GetBufferPointer()), errorsBlob->GetBufferSize());

                LOG_ERROR_MESSAGE("Failed to compile shader with DXIL", ErrorLog);

                if (ppCompilerOutput != nullptr)
                {
                    auto* pOutputDataBlob = MakeNewRCObj<DataBlobImpl>()(Source.length() + 1 + ErrorLog.length() + 1);
                    char* DataPtr         = reinterpret_cast<char*>(pOutputDataBlob->GetDataPtr());
                    memcpy(DataPtr, ErrorLog.data(), ErrorLog.length() + 1);
                    memcpy(DataPtr + ErrorLog.length() + 1, Source.data(), Source.length() + 1);
                    pOutputDataBlob->QueryInterface(IID_DataBlob, reinterpret_cast<IObject**>(ppCompilerOutput));
                }
            }
        }
        return hr;
    }

    CComPtr<IDxcBlob> spirvBlob;
    hr = result->GetResult(&spirvBlob);
    if (FAILED(hr))
        return hr;

    auto* ptr = static_cast<unsigned int*>(spirvBlob->GetBufferPointer());
    SPIRV.assign(ptr, ptr + (spirvBlob->GetBufferSize() / sizeof(ptr[0])));
    return S_OK;
}
} // namespace


bool HasDXILCompilerForVulkan()
{
    return VkDXILCompilerLib::Instance().CreateInstance != nullptr;
}

std::vector<unsigned int> HLSLtoSPIRVusingDXIL(const ShaderCreateInfo& Attribs,
                                               const char*             ExtraDefinitions,
                                               IDataBlob**             ppCompilerOutput)
{
    RefCntAutoPtr<IDataBlob> pFileData(MakeNewRCObj<DataBlobImpl>()(0));

    const char* SourceCode    = 0;
    int         SourceCodeLen = 0;
    if (Attribs.Source)
    {
        SourceCode    = Attribs.Source;
        SourceCodeLen = static_cast<int>(strlen(Attribs.Source));
    }
    else
    {
        VERIFY(Attribs.pShaderSourceStreamFactory, "Input stream factory is null");
        RefCntAutoPtr<IFileStream> pSourceStream;
        Attribs.pShaderSourceStreamFactory->CreateInputStream(Attribs.FilePath, &pSourceStream);
        if (pSourceStream == nullptr)
            LOG_ERROR_AND_THROW("Failed to open shader source file");

        pSourceStream->ReadBlob(pFileData);
        SourceCode    = reinterpret_cast<char*>(pFileData->GetDataPtr());
        SourceCodeLen = static_cast<int>(pFileData->GetSize());
    }

    std::string Source;
    Source.reserve(SourceCodeLen + sizeof(g_HLSLDefinitions));

    Source.append(g_HLSLDefinitions);
    if (const auto* ShaderTypeDefine = GetShaderTypeDefines(Attribs.Desc.ShaderType))
        Source += ShaderTypeDefine;

    if (ExtraDefinitions != nullptr)
        Source += ExtraDefinitions;

    if (Attribs.Macros != nullptr)
    {
        Source += '\n';
        auto* pMacro = Attribs.Macros;
        while (pMacro->Name != nullptr && pMacro->Definition != nullptr)
        {
            Source += "#define ";
            Source += pMacro->Name;
            Source += ' ';
            Source += pMacro->Definition;
            Source += "\n";
            ++pMacro;
        }
    }

    Source.append(SourceCode, SourceCodeLen);

    std::vector<unsigned int> SPIRV;
    if (FAILED(CompileDxilShader(Source, Attribs.EntryPoint, Attribs.Desc.ShaderType, Attribs.pShaderSourceStreamFactory, Attribs.HLSLVersion, SPIRV, ppCompilerOutput)))
        return {};

    return SPIRV;
}

} // namespace Diligent
