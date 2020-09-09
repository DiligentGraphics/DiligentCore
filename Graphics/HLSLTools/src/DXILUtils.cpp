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
#include <mutex>

// Platforms that has DXCompiler.
#if defined(PLATFORM_WIN32) || defined(PLATFORM_UNIVERSAL_WINDOWS) || defined(PLATFORM_LINUX)

#    include "DXCompilerBaseLiunx.hpp"
#    include "DXCompilerBaseUWP.hpp"
#    include "DXCompilerBaseWin32.hpp"

#    include "DataBlobImpl.hpp"
#    include "RefCntAutoPtr.hpp"

namespace Diligent
{
namespace
{

class DXCompilerImpl final : public DXCompilerBase
{
public:
    DXCompilerImpl(DXCompilerTarget Target, const char* pLibName) :
        m_Target{Target},
        m_LibName{pLibName ? pLibName : ""}
    {}

    ShaderVersion GetMaxShaderModel() override
    {
        Load();
        // mutex is not needed here
        return m_MaxShaderModel;
    }

    bool IsLoaded() override
    {
        return GetCreateInstaceProc() != nullptr;
    }

    DxcCreateInstanceProc GetCreateInstaceProc()
    {
        Load();
        // mutex is not needed here
        return m_pCreateInstance;
    }

    bool Compile(const char*                      Source,
                 size_t                           SourceLength,
                 const wchar_t*                   EntryPoint,
                 const wchar_t*                   Profile,
                 const DxcDefine*                 pDefines,
                 size_t                           DefinesCount,
                 const wchar_t**                  pArgs,
                 size_t                           ArgsCount,
                 IShaderSourceInputStreamFactory* pShaderSourceStreamFactory,
                 IDxcBlob**                       ppBlobOut,
                 IDxcBlob**                       ppCompilerOutput) override;

private:
    void Load()
    {
        std::unique_lock<std::mutex> lock{m_Guard};

        if (m_IsInitialized)
            return;

        m_IsInitialized   = true;
        m_pCreateInstance = DXCompilerBase::Load(m_Target, m_LibName);

        if (m_pCreateInstance)
        {
            CComPtr<IDxcValidator> validator;
            if (SUCCEEDED(m_pCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&validator))))
            {
                CComPtr<IDxcVersionInfo> info;
                if (SUCCEEDED(validator->QueryInterface(IID_PPV_ARGS(&info))))
                {
                    UINT32 ver = 0, minor = 0;
                    info->GetVersion(&ver, &minor);

                    LOG_INFO_MESSAGE("Loaded DX Shader Compiler, version ", ver, ".", minor);

                    ver = (ver << 16) | (minor & 0xFFFF);

                    // map known DXC version to maximum SM
                    switch (ver)
                    {
                        case 0x10005: m_MaxShaderModel = {6, 5}; break; // SM 6.5 and SM 6.6 preview
                        case 0x10004: m_MaxShaderModel = {6, 4}; break; // SM 6.4 and SM 6.5 preview
                        case 0x10003:
                        case 0x10002: m_MaxShaderModel = {6, 1}; break; // SM 6.1 and SM 6.2 preview
                        default: m_MaxShaderModel = (ver > 0x10005 ? ShaderVersion{6, 6} : ShaderVersion{6, 0}); break;
                    }
                }
            }
        }
    }

private:
    DxcCreateInstanceProc  m_pCreateInstance = nullptr;
    bool                   m_IsInitialized   = false;
    ShaderVersion          m_MaxShaderModel;
    std::mutex             m_Guard;
    const String           m_LibName;
    const DXCompilerTarget m_Target;
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
        if (pFilename == nullptr)
            return E_FAIL;

        String fileName;
        fileName.resize(wcslen(pFilename));
        for (size_t i = 0; i < fileName.size(); ++i)
        {
            fileName[i] = char(pFilename[i]);
        }

        if (fileName.empty())
        {
            LOG_ERROR("Failed to convert shader include file name ", fileName, ". File name must be ANSI string");
            return E_FAIL;
        }

        // validate file name
        if (fileName.size() > 2 && fileName[0] == '.' && (fileName[1] == '\\' || fileName[1] == '/'))
            fileName.erase(0, 2);

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

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
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

} // namespace


IDxCompilerLibrary* CreateDXCompiler(DXCompilerTarget Target, const char* pLibraryName)
{
    return new DXCompilerImpl{Target, pLibraryName};
}

bool DXCompilerImpl::Compile(const char*                      Source,
                             size_t                           SourceLength,
                             const wchar_t*                   EntryPoint,
                             const wchar_t*                   Profile,
                             const DxcDefine*                 pDefines,
                             size_t                           DefinesCount,
                             const wchar_t**                  pArgs,
                             size_t                           ArgsCount,
                             IShaderSourceInputStreamFactory* pShaderSourceStreamFactory,
                             IDxcBlob**                       ppBlobOut,
                             IDxcBlob**                       ppCompilerOutput)
{
    auto CreateInstance = GetCreateInstaceProc();

    if (CreateInstance == nullptr)
    {
        LOG_ERROR("Failed to load DXCompiler");
        return false;
    }

    DEV_CHECK_ERR(Source != nullptr && SourceLength > 0, "'Source' must not be null and 'SourceLength' must be greater than 0");
    DEV_CHECK_ERR(EntryPoint != nullptr, "'EntryPoint' must not be null");
    DEV_CHECK_ERR(Profile != nullptr, "'Profile' must not be null");
    DEV_CHECK_ERR((pDefines != nullptr) == (DefinesCount > 0), "'DefinesCount' must be 0 if 'pDefines' is null");
    DEV_CHECK_ERR((pArgs != nullptr) == (ArgsCount > 0), "'ArgsCount' must be 0 if 'pArgs' is null");
    DEV_CHECK_ERR(ppBlobOut != nullptr, "'ppBlobOut' must not be null");
    DEV_CHECK_ERR(ppCompilerOutput != nullptr, "'ppCompilerOutput' must not be null");

    HRESULT hr;

    CComPtr<IDxcLibrary> library;
    hr = CreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
    if (FAILED(hr))
        return false;

    CComPtr<IDxcCompiler> compiler;
    hr = CreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    if (FAILED(hr))
        return false;

    CComPtr<IDxcBlobEncoding> sourceBlob;
    hr = library->CreateBlobWithEncodingFromPinned(Source, UINT32(SourceLength), CP_UTF8, &sourceBlob);
    if (FAILED(hr))
        return false;

    DxcIncludeHandlerImpl IncludeHandler{pShaderSourceStreamFactory, library};

    CComPtr<IDxcOperationResult> result;
    hr = compiler->Compile(
        sourceBlob,
        L"",
        EntryPoint,
        Profile,
        pArgs, UINT32(ArgsCount),
        pDefines, UINT32(DefinesCount),
        pShaderSourceStreamFactory ? &IncludeHandler : nullptr,
        &result);

    if (SUCCEEDED(hr))
    {
        HRESULT status;
        if (SUCCEEDED(result->GetStatus(&status)))
            hr = status;
    }

    if (result)
    {
        CComPtr<IDxcBlobEncoding> errorsBlob;
        CComPtr<IDxcBlobEncoding> errorsBlobUtf8;
        if (SUCCEEDED(result->GetErrorBuffer(&errorsBlob)) && SUCCEEDED(library->GetBlobAsUtf8(errorsBlob, &errorsBlobUtf8)))
        {
            errorsBlobUtf8->QueryInterface(IID_PPV_ARGS(ppCompilerOutput));
        }
    }

    if (FAILED(hr))
        return false; // compilation failed

    CComPtr<IDxcBlob> compiled;
    hr = result->GetResult(&compiled);
    if (FAILED(hr))
        return false;

    // validate and sign in
    if (m_Target == DXCompilerTarget::Direct3D12)
    {
        CComPtr<IDxcValidator> validator;
        hr = CreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&validator));
        if (FAILED(hr))
            return false;

        CComPtr<IDxcOperationResult> validationResult;
        hr = validator->Validate(compiled, DxcValidatorFlags_InPlaceEdit, &validationResult);

        if (validationResult == nullptr || FAILED(hr))
            return false; // validation failed

        HRESULT status = E_FAIL;
        validationResult->GetStatus(&status);

        if (SUCCEEDED(status))
        {
            CComPtr<IDxcBlob> validated;
            hr = validationResult->GetResult(&validated);
            if (FAILED(hr))
                return false;

            *ppBlobOut = validated ? validated.Detach() : compiled.Detach();
            return true;
        }
        else
        {
            CComPtr<IDxcBlobEncoding> validationOutput;
            CComPtr<IDxcBlobEncoding> validationOutputUtf8;
            validationResult->GetErrorBuffer(&validationOutput);
            library->GetBlobAsUtf8(validationOutput, &validationOutputUtf8);

            size_t      ValidationMsgLen = validationOutputUtf8 ? validationOutputUtf8->GetBufferSize() : 0;
            const char* ValidationMsg    = ValidationMsgLen > 0 ? static_cast<const char*>(validationOutputUtf8->GetBufferPointer()) : "";

            LOG_ERROR("Shader validation failed: ", ValidationMsg);
            return false;
        }
    }

    *ppBlobOut = compiled.Detach();
    return true;
}

#    if D3D12_SUPPORTED
#        define FOURCC(a, b, c, d) (uint32_t{((d) << 24) | ((c) << 16) | ((b) << 8) | (a)})

bool DxcGetShaderReflection(IDxCompilerLibrary*      pLibrary,
                            IDxcBlob*                pShaderBytecode,
                            ID3D12ShaderReflection** ppShaderReflection) noexcept(false)
{
    HRESULT hr;
    bool    IsDXIL         = false;
    auto    CreateInstance = pLibrary ? static_cast<DXCompilerImpl*>(pLibrary)->GetCreateInstaceProc() : nullptr;

    if (CreateInstance != nullptr)
    {
        const uint32_t                   DFCC_DXIL = FOURCC('D', 'X', 'I', 'L');
        CComPtr<IDxcContainerReflection> pReflection;
        UINT32                           shaderIdx;

        hr = CreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&pReflection));
        if (FAILED(hr))
            LOG_ERROR_AND_THROW("Failed to create shader reflection instance");

        hr = pReflection->Load(pShaderBytecode);
        if (FAILED(hr))
            LOG_ERROR_AND_THROW("Failed to load shader reflection from bytecode");

        hr     = pReflection->FindFirstPartKind(DFCC_DXIL, &shaderIdx);
        IsDXIL = SUCCEEDED(hr);
        if (IsDXIL)
        {
            hr = pReflection->GetPartReflection(shaderIdx, __uuidof(*ppShaderReflection), reinterpret_cast<void**>(ppShaderReflection));
            if (FAILED(hr))
                LOG_ERROR_AND_THROW("Failed to get the shader reflection");
        }
    }
    return IsDXIL;
}
#    endif


#    if VULKAN_SUPPORTED
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

} // namespace

std::vector<uint32_t> DXILtoSPIRV(IDxCompilerLibrary*     pLibrary,
                                  const ShaderCreateInfo& Attribs,
                                  const char*             ExtraDefinitions,
                                  IDataBlob**             ppCompilerOutput) noexcept(false)
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

    // validate shader version
    ShaderVersion ShaderModel = Attribs.HLSLVersion;
    ShaderVersion MaxSM       = pLibrary->GetMaxShaderModel();

    if (ShaderModel.Major < 6 || ShaderModel.Major > MaxSM.Major)
        ShaderModel = MaxSM;

    if (ShaderModel.Major == MaxSM.Major && ShaderModel.Minor > MaxSM.Minor)
        ShaderModel = MaxSM;

    std::wstring Profile;
    switch (Attribs.Desc.ShaderType)
    {
        // clang-format off
        case SHADER_TYPE_VERTEX:        Profile = L"vs_"; break;
        case SHADER_TYPE_PIXEL:         Profile = L"ps_"; break;
        case SHADER_TYPE_GEOMETRY:      Profile = L"gs_"; break;
        case SHADER_TYPE_HULL:          Profile = L"hs_"; break;
        case SHADER_TYPE_DOMAIN:        Profile = L"ds_"; break;
        case SHADER_TYPE_COMPUTE:       Profile = L"cs_"; break;
        case SHADER_TYPE_AMPLIFICATION: Profile = L"as_"; break;
        case SHADER_TYPE_MESH:          Profile = L"ms_"; break;
        default: UNEXPECTED("Unexpected shader type");
            // clang-format on
    }

    Profile += L'0' + (ShaderModel.Major % 10);
    Profile += L'_';
    Profile += L'0' + (ShaderModel.Minor % 10);

    const wchar_t* pArgs[] =
        {
            L"-spirv",
            L"-fspv-reflect",
            L"-fspv-target-env=vulkan1.0",
            //L"-WX", // Warnings as errors
            L"-O3", // Optimization level 3
        };

    CComPtr<IDxcBlob> compiled;
    CComPtr<IDxcBlob> errors;

    bool result = pLibrary->Compile(Source.c_str(), Source.length(),
                                    std::wstring{Attribs.EntryPoint, Attribs.EntryPoint + strlen(Attribs.EntryPoint)}.c_str(),
                                    Profile.c_str(),
                                    nullptr, 0,
                                    pArgs, _countof(pArgs),
                                    Attribs.pShaderSourceStreamFactory,
                                    &compiled,
                                    &errors);

    const size_t CompilerMsgLen = errors ? errors->GetBufferSize() : 0;
    const char*  CompilerMsg    = CompilerMsgLen > 0 ? static_cast<const char*>(errors->GetBufferPointer()) : nullptr;

    if (CompilerMsg != nullptr && ppCompilerOutput != nullptr)
    {
        auto* pOutputDataBlob = MakeNewRCObj<DataBlobImpl>()(Source.length() + 1 + CompilerMsgLen + 1);
        char* DataPtr         = static_cast<char*>(pOutputDataBlob->GetDataPtr());
        memcpy(DataPtr, CompilerMsg, CompilerMsgLen + 1);
        memcpy(DataPtr + CompilerMsgLen + 1, Source.data(), Source.length() + 1);
        pOutputDataBlob->QueryInterface(IID_DataBlob, reinterpret_cast<IObject**>(ppCompilerOutput));
    }

    std::vector<uint32_t> SPIRV;

    if (!result)
    {
        if (ppCompilerOutput != nullptr)
        {
            LOG_ERROR_AND_THROW("Failed to compile Vulkan shader \"", (Attribs.Desc.Name != nullptr ? Attribs.Desc.Name : ""), "\".");
        }
        else
        {
            LOG_ERROR_AND_THROW("Failed to compile Vukan shader \"", (Attribs.Desc.Name != nullptr ? Attribs.Desc.Name : ""), "\":\n", (CompilerMsg != nullptr ? CompilerMsg : "<no compiler log available>"));
        }
    }

    if (result && compiled && compiled->GetBufferSize() > 0)
    {
        SPIRV.assign(static_cast<uint32_t*>(compiled->GetBufferPointer()),
                     static_cast<uint32_t*>(compiled->GetBufferPointer()) + compiled->GetBufferSize() / sizeof(uint32_t));
    }
    return SPIRV;
}

#    endif // VULKAN_SUPPORTED

} // namespace Diligent

#else

#    include "DXILUtils.hpp"

namespace Diligent
{

IDxCompilerLibrary* CreateDXCompiler(DXCompilerTarget Target, const char* pLibraryName)
{
    return nullptr;
}

std::vector<uint32_t> DXILtoSPIRV(IDxCompilerLibrary*     pLibrary,
                                  const ShaderCreateInfo& Attribs,
                                  const char*             ExtraDefinitions,
                                  IDataBlob**             ppCompilerOutput) noexcept(false)
{
    return {};
}

} // namespace Diligent
#endif
