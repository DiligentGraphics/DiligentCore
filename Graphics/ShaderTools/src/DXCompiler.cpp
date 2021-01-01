/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include <memory>
#include <mutex>

// Platforms that support DXCompiler.
#if PLATFORM_WIN32
#    include "DXCompilerBaseWin32.hpp"
#elif PLATFORM_UNIVERSAL_WINDOWS
#    include "DXCompilerBaseUWP.hpp"
#elif PLATFORM_LINUX
#    include "DXCompilerBaseLiunx.hpp"
#else
#    error DXC is not supported on this platform
#endif

#include "DataBlobImpl.hpp"
#include "RefCntAutoPtr.hpp"
#include "ShaderToolsCommon.hpp"

#if D3D12_SUPPORTED
#    include <d3d12shader.h>
#endif

#include "HLSLUtils.hpp"

namespace Diligent
{

namespace
{

class DXCompilerImpl final : public DXCompilerBase
{
public:
    DXCompilerImpl(DXCompilerTarget Target, const char* pLibName) :
        m_Target{Target},
        m_LibName{pLibName ? pLibName : (Target == DXCompilerTarget::Direct3D12 ? "dxcompiler" : "spv_dxcompiler")}
    {}

    ShaderVersion GetMaxShaderModel() override final
    {
        Load();
        // mutex is not needed here
        return m_MaxShaderModel;
    }

    bool IsLoaded() override final
    {
        return GetCreateInstaceProc() != nullptr;
    }

    DxcCreateInstanceProc GetCreateInstaceProc()
    {
        return Load();
    }

    bool Compile(const CompileAttribs& Attribs) override final;

    virtual void Compile(const ShaderCreateInfo& ShaderCI,
                         ShaderVersion           ShaderModel,
                         const char*             ExtraDefinitions,
                         IDxcBlob**              ppByteCodeBlob,
                         std::vector<uint32_t>*  pByteCode,
                         IDataBlob**             ppCompilerOutput) noexcept(false) override final;

    virtual void GetD3D12ShaderReflection(IDxcBlob*                pShaderBytecode,
                                          ID3D12ShaderReflection** ppShaderReflection) override final;

    virtual bool RemapResourceBindings(const TResourceBindingMap& ResourceMap,
                                       IDxcBlob*                  pSrcBytecode,
                                       IDxcBlob**                 ppDstByteCode) override final;

private:
    DxcCreateInstanceProc Load()
    {
        std::unique_lock<std::mutex> lock{m_Guard};

        if (m_IsInitialized)
            return m_pCreateInstance;

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
                    info->GetVersion(&m_MajorVer, &m_MinorVer);

                    LOG_INFO_MESSAGE("Loaded DX Shader Compiler, version ", m_MajorVer, ".", m_MinorVer);

                    auto ver = (m_MajorVer << 16) | (m_MinorVer & 0xFFFF);

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

        return m_pCreateInstance;
    }

    bool ValidateAndSign(DxcCreateInstanceProc CreateInstance, IDxcLibrary* library, CComPtr<IDxcBlob>& compiled, IDxcBlob** ppBlobOut) const;
    bool PatchDXIL(const TResourceBindingMap& ResourceMap, String& DXIL) const;

private:
    DxcCreateInstanceProc  m_pCreateInstance = nullptr;
    bool                   m_IsInitialized   = false;
    ShaderVersion          m_MaxShaderModel;
    std::mutex             m_Guard;
    const String           m_LibName;
    const DXCompilerTarget m_Target;
    // Compiler version
    UINT32 m_MajorVer = 0;
    UINT32 m_MinorVer = 0;
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
            fileName[i] = static_cast<char>(pFilename[i]);
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

        HRESULT hr = m_pLibrary->CreateBlobWithEncodingFromPinned(pFileData->GetDataPtr(), UINT32(pFileData->GetSize()), CP_UTF8, &sourceBlob);
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


std::unique_ptr<IDXCompiler> CreateDXCompiler(DXCompilerTarget Target, const char* pLibraryName)
{
    return std::unique_ptr<IDXCompiler>{new DXCompilerImpl{Target, pLibraryName}};
}

bool DXCompilerImpl::Compile(const CompileAttribs& Attribs)
{
    auto CreateInstance = GetCreateInstaceProc();

    if (CreateInstance == nullptr)
    {
        LOG_ERROR("Failed to load DXCompiler");
        return false;
    }

    DEV_CHECK_ERR(Attribs.Source != nullptr && Attribs.SourceLength > 0, "'Source' must not be null and 'SourceLength' must be greater than 0");
    DEV_CHECK_ERR(Attribs.EntryPoint != nullptr, "'EntryPoint' must not be null");
    DEV_CHECK_ERR(Attribs.Profile != nullptr, "'Profile' must not be null");
    DEV_CHECK_ERR((Attribs.pDefines != nullptr) == (Attribs.DefinesCount > 0), "'DefinesCount' must be 0 if 'pDefines' is null");
    DEV_CHECK_ERR((Attribs.pArgs != nullptr) == (Attribs.ArgsCount > 0), "'ArgsCount' must be 0 if 'pArgs' is null");
    DEV_CHECK_ERR(Attribs.ppBlobOut != nullptr, "'ppBlobOut' must not be null");
    DEV_CHECK_ERR(Attribs.ppCompilerOutput != nullptr, "'ppCompilerOutput' must not be null");

    HRESULT hr;

    // NOTE: The call to DxcCreateInstance is thread-safe, but objects created by DxcCreateInstance aren't thread-safe.
    // Compiler objects should be created and then used on the same thread.
    // https://github.com/microsoft/DirectXShaderCompiler/wiki/Using-dxc.exe-and-dxcompiler.dll#dxcompiler-dll-interface

    CComPtr<IDxcLibrary> library;
    hr = CreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DXC Library");
        return false;
    }

    CComPtr<IDxcCompiler> compiler;
    hr = CreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DXC Compiler");
        return false;
    }

    CComPtr<IDxcBlobEncoding> sourceBlob;
    hr = library->CreateBlobWithEncodingFromPinned(Attribs.Source, UINT32{Attribs.SourceLength}, CP_UTF8, &sourceBlob);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DXC Blob encoding");
        return false;
    }

    DxcIncludeHandlerImpl IncludeHandler{Attribs.pShaderSourceStreamFactory, library};

    CComPtr<IDxcOperationResult> result;
    hr = compiler->Compile(
        sourceBlob,
        L"",
        Attribs.EntryPoint,
        Attribs.Profile,
        Attribs.pArgs, UINT32{Attribs.ArgsCount},
        Attribs.pDefines, UINT32{Attribs.DefinesCount},
        Attribs.pShaderSourceStreamFactory ? &IncludeHandler : nullptr,
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
            errorsBlobUtf8->QueryInterface(IID_PPV_ARGS(Attribs.ppCompilerOutput));
        }
    }

    if (FAILED(hr))
    {
        return false;
    }

    CComPtr<IDxcBlob> compiled;
    hr = result->GetResult(&compiled);
    if (FAILED(hr))
        return false;

    // validate and sign
    if (m_Target == DXCompilerTarget::Direct3D12)
    {
        return ValidateAndSign(CreateInstance, library, compiled, Attribs.ppBlobOut);
    }

    *Attribs.ppBlobOut = compiled.Detach();
    return true;
}

bool DXCompilerImpl::ValidateAndSign(DxcCreateInstanceProc CreateInstance, IDxcLibrary* library, CComPtr<IDxcBlob>& compiled, IDxcBlob** ppBlobOut) const
{
    HRESULT                hr;
    CComPtr<IDxcValidator> validator;
    hr = CreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&validator));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create  DXC Validator");
        return false;
    }

    CComPtr<IDxcOperationResult> validationResult;
    hr = validator->Validate(compiled, DxcValidatorFlags_InPlaceEdit, &validationResult);

    if (validationResult == nullptr || FAILED(hr))
    {
        LOG_ERROR("Failed to validate shader bytecode");
        return false;
    }

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

#if D3D12_SUPPORTED
class ShaderReflectionViaLibraryReflection final : public ID3D12ShaderReflection
{
public:
    ShaderReflectionViaLibraryReflection(CComPtr<ID3D12LibraryReflection> pLib, ID3D12FunctionReflection* pFunc) :
        m_pLib{std::move(pLib)},
        m_pFunc{pFunc},
        m_RefCount{0}
    {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv) override
    {
        return E_FAIL;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return Atomics::AtomicIncrement(m_RefCount);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        auto RefCount = Atomics::AtomicDecrement(m_RefCount);
        VERIFY(RefCount >= 0, "Inconsistent call to ReleaseStrongRef()");
        if (RefCount == 0)
        {
            delete this;
        }
        return RefCount;
    }

    HRESULT STDMETHODCALLTYPE GetDesc(D3D12_SHADER_DESC* pDesc) override
    {
        D3D12_FUNCTION_DESC FnDesc = {};
        HRESULT             hr     = m_pFunc->GetDesc(&FnDesc);
        if (FAILED(hr))
            return hr;

        pDesc->Version                     = FnDesc.Version;
        pDesc->Creator                     = FnDesc.Creator;
        pDesc->Flags                       = FnDesc.Flags;
        pDesc->ConstantBuffers             = FnDesc.ConstantBuffers;
        pDesc->BoundResources              = FnDesc.BoundResources;
        pDesc->InputParameters             = 0;
        pDesc->OutputParameters            = 0;
        pDesc->InstructionCount            = FnDesc.InstructionCount;
        pDesc->TempRegisterCount           = FnDesc.TempRegisterCount;
        pDesc->TempArrayCount              = FnDesc.TempArrayCount;
        pDesc->DefCount                    = FnDesc.DefCount;
        pDesc->DclCount                    = FnDesc.DclCount;
        pDesc->TextureNormalInstructions   = FnDesc.TextureNormalInstructions;
        pDesc->TextureLoadInstructions     = FnDesc.TextureLoadInstructions;
        pDesc->TextureCompInstructions     = FnDesc.TextureCompInstructions;
        pDesc->TextureBiasInstructions     = FnDesc.TextureBiasInstructions;
        pDesc->TextureGradientInstructions = FnDesc.TextureGradientInstructions;
        pDesc->FloatInstructionCount       = FnDesc.FloatInstructionCount;
        pDesc->IntInstructionCount         = FnDesc.IntInstructionCount;
        pDesc->UintInstructionCount        = FnDesc.UintInstructionCount;
        pDesc->StaticFlowControlCount      = FnDesc.StaticFlowControlCount;
        pDesc->DynamicFlowControlCount     = FnDesc.DynamicFlowControlCount;
        pDesc->MacroInstructionCount       = FnDesc.MacroInstructionCount;
        pDesc->ArrayInstructionCount       = FnDesc.ArrayInstructionCount;
        pDesc->CutInstructionCount         = 0;
        pDesc->EmitInstructionCount        = 0;
        pDesc->GSOutputTopology            = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        pDesc->GSMaxOutputVertexCount      = 0;
        pDesc->InputPrimitive              = D3D_PRIMITIVE_UNDEFINED;
        pDesc->PatchConstantParameters     = 0;
        pDesc->cGSInstanceCount            = 0;
        pDesc->cControlPoints              = 0;
        pDesc->HSOutputPrimitive           = D3D_TESSELLATOR_OUTPUT_UNDEFINED;
        pDesc->HSPartitioning              = D3D_TESSELLATOR_PARTITIONING_UNDEFINED;
        pDesc->TessellatorDomain           = D3D_TESSELLATOR_DOMAIN_UNDEFINED;
        pDesc->cBarrierInstructions        = 0;
        pDesc->cInterlockedInstructions    = 0;
        pDesc->cTextureStoreInstructions   = 0;

        return S_OK;
    }

    ID3D12ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetConstantBufferByIndex(UINT Index) override
    {
        return m_pFunc->GetConstantBufferByIndex(Index);
    }

    ID3D12ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetConstantBufferByName(LPCSTR Name) override
    {
        return m_pFunc->GetConstantBufferByName(Name);
    }

    HRESULT STDMETHODCALLTYPE GetResourceBindingDesc(UINT ResourceIndex, D3D12_SHADER_INPUT_BIND_DESC* pDesc) override
    {
        return m_pFunc->GetResourceBindingDesc(ResourceIndex, pDesc);
    }

    HRESULT STDMETHODCALLTYPE GetInputParameterDesc(UINT ParameterIndex, D3D12_SIGNATURE_PARAMETER_DESC* pDesc) override
    {
        UNEXPECTED("not supported");
        return E_FAIL;
    }

    HRESULT STDMETHODCALLTYPE GetOutputParameterDesc(UINT ParameterIndex, D3D12_SIGNATURE_PARAMETER_DESC* pDesc) override
    {
        UNEXPECTED("not supported");
        return E_FAIL;
    }

    HRESULT STDMETHODCALLTYPE GetPatchConstantParameterDesc(UINT ParameterIndex, D3D12_SIGNATURE_PARAMETER_DESC* pDesc) override
    {
        UNEXPECTED("not supported");
        return E_FAIL;
    }

    ID3D12ShaderReflectionVariable* STDMETHODCALLTYPE GetVariableByName(LPCSTR Name) override
    {
        return m_pFunc->GetVariableByName(Name);
    }

    HRESULT STDMETHODCALLTYPE GetResourceBindingDescByName(LPCSTR Name, D3D12_SHADER_INPUT_BIND_DESC* pDesc) override
    {
        return m_pFunc->GetResourceBindingDescByName(Name, pDesc);
    }

    UINT STDMETHODCALLTYPE GetMovInstructionCount() override
    {
        UNEXPECTED("not supported");
        return 0;
    }

    UINT STDMETHODCALLTYPE GetMovcInstructionCount() override
    {
        UNEXPECTED("not supported");
        return 0;
    }

    UINT STDMETHODCALLTYPE GetConversionInstructionCount() override
    {
        UNEXPECTED("not supported");
        return 0;
    }

    UINT STDMETHODCALLTYPE GetBitwiseInstructionCount() override
    {
        UNEXPECTED("not supported");
        return 0;
    }

    D3D_PRIMITIVE STDMETHODCALLTYPE GetGSInputPrimitive() override
    {
        UNEXPECTED("not supported");
        return D3D_PRIMITIVE_UNDEFINED;
    }

    BOOL STDMETHODCALLTYPE IsSampleFrequencyShader() override
    {
        UNEXPECTED("not supported");
        return FALSE;
    }

    UINT STDMETHODCALLTYPE GetNumInterfaceSlots() override
    {
        UNEXPECTED("not supported");
        return 0;
    }

    HRESULT STDMETHODCALLTYPE GetMinFeatureLevel(D3D_FEATURE_LEVEL* pLevel) override
    {
        UNEXPECTED("not supported");
        return E_FAIL;
    }

    UINT STDMETHODCALLTYPE GetThreadGroupSize(UINT* pSizeX, UINT* pSizeY, UINT* pSizeZ) override
    {
        UNEXPECTED("not supported");
        *pSizeX = *pSizeY = *pSizeZ = 0;
        return 0;
    }

    UINT64 STDMETHODCALLTYPE GetRequiresFlags() override
    {
        UNEXPECTED("not supported");
        return 0;
    }

private:
    CComPtr<ID3D12LibraryReflection> m_pLib;
    ID3D12FunctionReflection*        m_pFunc = nullptr;
    Atomics::AtomicLong              m_RefCount;
};
#endif // D3D12_SUPPORTED


void DXCompilerImpl::GetD3D12ShaderReflection(IDxcBlob*                pShaderBytecode,
                                              ID3D12ShaderReflection** ppShaderReflection)
{
#if D3D12_SUPPORTED
    try
    {
        auto CreateInstance = GetCreateInstaceProc();
        if (CreateInstance == nullptr)
            return;

        CComPtr<IDxcContainerReflection> pReflection;

        auto hr = CreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&pReflection));
        if (FAILED(hr))
            LOG_ERROR_AND_THROW("Failed to create shader reflection instance");

        hr = pReflection->Load(pShaderBytecode);
        if (FAILED(hr))
            LOG_ERROR_AND_THROW("Failed to load shader reflection from bytecode");

        UINT32 shaderIdx = 0;

        hr = pReflection->FindFirstPartKind(DXC_PART_DXIL, &shaderIdx);
        if (SUCCEEDED(hr))
        {
            hr = pReflection->GetPartReflection(shaderIdx, IID_PPV_ARGS(ppShaderReflection));
            if (SUCCEEDED(hr))
                return;

            // Try to get the reflection via library reflection
            CComPtr<ID3D12LibraryReflection> pLib;

            hr = pReflection->GetPartReflection(shaderIdx, IID_PPV_ARGS(&pLib));
            if (SUCCEEDED(hr))
            {
                D3D12_LIBRARY_DESC Desc = {};
                pLib->GetDesc(&Desc);
                VERIFY_EXPR(Desc.FunctionCount == 1);

                ID3D12FunctionReflection* pFunc = pLib->GetFunctionByIndex(0);
                if (pFunc != nullptr)
                {
                    *ppShaderReflection = new ShaderReflectionViaLibraryReflection{std::move(pLib), pFunc};
                    (*ppShaderReflection)->AddRef();
                    return;
                }
            }
        }

        LOG_ERROR_AND_THROW("Failed to get the shader reflection");
    }
    catch (...)
    {
    }
#endif // D3D12_SUPPORTED
}


void DXCompilerImpl::Compile(const ShaderCreateInfo& ShaderCI,
                             ShaderVersion           ShaderModel,
                             const char*             ExtraDefinitions,
                             IDxcBlob**              ppByteCodeBlob,
                             std::vector<uint32_t>*  pByteCode,
                             IDataBlob**             ppCompilerOutput) noexcept(false)
{
    if (!IsLoaded())
    {
        UNEXPECTED("DX compiler is not loaded");
        return;
    }

    ShaderVersion MaxSM = GetMaxShaderModel();

    // validate shader version
    if (ShaderModel == ShaderVersion{})
    {
        ShaderModel = MaxSM;
    }
    else if (ShaderModel.Major < 6)
    {
        LOG_INFO_MESSAGE("DXC only supports shader model 6.0+. Upgrading the specified shader model ",
                         Uint32{ShaderModel.Major}, '_', Uint32{ShaderModel.Minor}, " to 6_0");
        ShaderModel = ShaderVersion{6, 0};
    }
    else if ((ShaderModel.Major > MaxSM.Major) ||
             (ShaderModel.Major == MaxSM.Major && ShaderModel.Minor > MaxSM.Minor))
    {
        LOG_WARNING_MESSAGE("The maximum supported shader model by DXC is ", Uint32{MaxSM.Major}, '_', Uint32{MaxSM.Minor},
                            ". The specified shader model ", Uint32{ShaderModel.Major}, '_', Uint32{ShaderModel.Minor}, " will be downgraded.");
        ShaderModel = MaxSM;
    }

    const auto         Profile = GetHLSLProfileString(ShaderCI.Desc.ShaderType, ShaderModel);
    const std::wstring wstrProfile{Profile.begin(), Profile.end()};
    const std::wstring wstrEntryPoint{ShaderCI.EntryPoint, ShaderCI.EntryPoint + strlen(ShaderCI.EntryPoint)};

    std::vector<const wchar_t*> DxilArgs;
    if (m_Target == DXCompilerTarget::Direct3D12)
    {
        DxilArgs.push_back(L"-Zpc"); // Matrices in column-major order

        //DxilArgs.push_back(L"-WX");  // Warnings as errors
#ifdef DILIGENT_DEBUG
        DxilArgs.push_back(L"-Zi"); // Debug info
        DxilArgs.push_back(L"-Od"); // Disable optimization
        if (m_MajorVer > 1 || m_MajorVer == 1 && m_MinorVer >= 5)
        {
            // Silence the following warning:
            // no output provided for debug - embedding PDB in shader container.  Use -Qembed_debug to silence this warning.
            DxilArgs.push_back(L"-Qembed_debug");
        }
#else
        if (m_MajorVer > 1 || m_MajorVer == 1 && m_MinorVer >= 5)
            DxilArgs.push_back(L"-O3"); // Optimization level 3
        else
            DxilArgs.push_back(L"-Od"); // TODO: something goes wrong if optimization is enabled
#endif
    }
    else if (m_Target == DXCompilerTarget::Vulkan)
    {
        const Uint32 RayTracingStages =
            SHADER_TYPE_RAY_GEN | SHADER_TYPE_RAY_MISS | SHADER_TYPE_RAY_CLOSEST_HIT |
            SHADER_TYPE_RAY_ANY_HIT | SHADER_TYPE_RAY_INTERSECTION | SHADER_TYPE_CALLABLE;

        DxilArgs.assign(
            {
                L"-spirv",
                L"-fspv-reflect",
                //L"-WX", // Warnings as errors
                L"-O3", // Optimization level 3
            });

        if (ShaderCI.Desc.ShaderType & RayTracingStages)
        {
            // add default extensions because we override them
            DxilArgs.push_back(L"-fspv-extension=SPV_GOOGLE_hlsl_functionality1");
            DxilArgs.push_back(L"-fspv-extension=SPV_GOOGLE_user_type");

            DxilArgs.push_back(L"-fspv-extension=SPV_NV_ray_tracing"); // TODO: should be SPV_KHR_ray_tracing, current version may not work on AMD
            //DxilArgs.push_back(L"-fspv-target-env=vulkan1.2"); // required for SPV_KHR_ray_tracing
        }
    }
    else
    {
        UNEXPECTED("Unknown compiler target");
    }


    CComPtr<IDxcBlob> pDXIL;
    CComPtr<IDxcBlob> pDxcLog;

    IDXCompiler::CompileAttribs CA;

    auto Source = BuildHLSLSourceString(ShaderCI, ExtraDefinitions);

    CA.Source                     = Source.c_str();
    CA.SourceLength               = static_cast<Uint32>(Source.length());
    CA.EntryPoint                 = wstrEntryPoint.c_str();
    CA.Profile                    = wstrProfile.c_str();
    CA.pDefines                   = nullptr;
    CA.DefinesCount               = 0;
    CA.pArgs                      = DxilArgs.data();
    CA.ArgsCount                  = static_cast<Uint32>(DxilArgs.size());
    CA.pShaderSourceStreamFactory = ShaderCI.pShaderSourceStreamFactory;
    CA.ppBlobOut                  = &pDXIL;
    CA.ppCompilerOutput           = &pDxcLog;

    auto result = Compile(CA);
    HandleHLSLCompilerResult(result, pDxcLog.p, Source, ShaderCI.Desc.Name, ppCompilerOutput);

    if (result && pDXIL && pDXIL->GetBufferSize() > 0)
    {
        if (pByteCode != nullptr)
            pByteCode->assign(static_cast<uint32_t*>(pDXIL->GetBufferPointer()),
                              static_cast<uint32_t*>(pDXIL->GetBufferPointer()) + pDXIL->GetBufferSize() / sizeof(uint32_t));

        if (ppByteCodeBlob != nullptr)
            *ppByteCodeBlob = pDXIL.Detach();
    }
}

bool DXCompilerImpl::RemapResourceBindings(const TResourceBindingMap& ResourceMap,
                                           IDxcBlob*                  pSrcBytecode,
                                           IDxcBlob**                 ppDstByteCode)
{
#if D3D12_SUPPORTED
    auto CreateInstance = GetCreateInstaceProc();

    if (CreateInstance == nullptr)
    {
        LOG_ERROR("Failed to load DXCompiler");
        return false;
    }

    HRESULT              hr;
    CComPtr<IDxcLibrary> library;
    hr = CreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DXC Library");
        return false;
    }

    CComPtr<IDxcAssembler> assembler;
    hr = CreateInstance(CLSID_DxcAssembler, IID_PPV_ARGS(&assembler));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DXC assembler");
        return false;
    }

    CComPtr<IDxcCompiler> compiler;
    hr = CreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DXC Compiler");
        return false;
    }

    CComPtr<IDxcBlobEncoding> disasm;
    hr = compiler->Disassemble(pSrcBytecode, &disasm);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to disassemble bytecode");
        return false;
    }

    String dxilAsm;
    dxilAsm.assign(static_cast<const char*>(disasm->GetBufferPointer()), disasm->GetBufferSize());

    if (!PatchDXIL(ResourceMap, dxilAsm))
    {
        LOG_ERROR("Failed to patch resource bindings");
        return false;
    }

    CComPtr<IDxcBlobEncoding> patchedDisasm;
    hr = library->CreateBlobWithEncodingFromPinned(dxilAsm.data(), static_cast<UINT32>(dxilAsm.size()), 0, &patchedDisasm);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create disassemble blob");
        return false;
    }

    CComPtr<IDxcOperationResult> dxilResult;
    hr = assembler->AssembleToContainer(patchedDisasm, &dxilResult);
    if (FAILED(hr) || dxilResult == nullptr)
    {
        LOG_ERROR("Failed to create DXIL container");
        return false;
    }

    HRESULT status = E_FAIL;
    dxilResult->GetStatus(&status);

    if (FAILED(status))
    {
        CComPtr<IDxcBlobEncoding> errorsBlob;
        CComPtr<IDxcBlobEncoding> errorsBlobUtf8;
        if (SUCCEEDED(dxilResult->GetErrorBuffer(&errorsBlob)) && SUCCEEDED(library->GetBlobAsUtf8(errorsBlob, &errorsBlobUtf8)))
        {
            String errorLog;
            errorLog.assign(static_cast<const char*>(errorsBlobUtf8->GetBufferPointer()), errorsBlobUtf8->GetBufferSize());
            LOG_ERROR_MESSAGE("Compilation message: ", errorLog);
        }
        else
            LOG_ERROR("Failed to compile patched asm");

        return false;
    }

    CComPtr<IDxcBlob> compiled;
    hr = dxilResult->GetResult(static_cast<IDxcBlob**>(&compiled));
    if (FAILED(hr))
        return false;

    return ValidateAndSign(CreateInstance, library, compiled, ppDstByteCode);
#else

    return false;
#endif // D3D12_SUPPORTED
}

bool DXCompilerImpl::PatchDXIL(const TResourceBindingMap& ResourceMap, String& DXIL) const
{
    bool RemappingOK = true;
    for (auto& ResPair : ResourceMap)
    {
        // Patch metadata resource record

        // https://github.com/microsoft/DirectXShaderCompiler/blob/master/docs/DXIL.rst#metadata-resource-records
        // Idx | Type            | Description
        // ----|-----------------|------------------------------------------------------------------------------------------
        //  0  | i32             | Unique resource record ID, used to identify the resource record in createHandle operation.
        //  1  | Pointer         | Pointer to a global constant symbol with the original shape of resource and element type
        //  2  | Metadata string | Name of resource variable.
        //  3  | i32             | Bind space ID of the root signature range that corresponds to this resource.
        //  4  | i32             | Bind lower bound of the root signature range that corresponds to this resource.
        //  5  | i32             | Range size of the root signature range that corresponds to this resource.

        // Example:
        //
        // !158 = !{i32 0, %"class.RWTexture2D<vector<float, 4> >"* @"\01?g_ColorBuffer@@3V?$RWTexture2D@V?$vector@M$03@@@@A", !"g_ColorBuffer", i32 -1, i32 -1, i32 1, i32 2, i1 false, i1 false, i1 false, !159}

        const auto* Name      = ResPair.first.GetStr();
        const auto& BindPoint = ResPair.second;
        const auto  DxilName  = String{"!\""} + Name + "\"";

        size_t pos = DXIL.find(DxilName);
        if (pos == String::npos)
            continue;

        // !"g_ColorBuffer", i32 -1, i32 -1,
        // ^

        pos += DxilName.length();
        // !"g_ColorBuffer", i32 -1, i32 -1,
        //                 ^

        auto ReplaceRecord = [&](const std::string& NewValue, const char* RecordName) //
        {
#define CHECK_PATCHING_ERROR(Cond, ...)                                                       \
    if (!(Cond))                                                                              \
    {                                                                                         \
        LOG_ERROR_MESSAGE("Unable to patch DXIL for resource '", Name, "': ", ##__VA_ARGS__); \
        return false;                                                                         \
    }
            // , i32 -1
            // ^
            pos = DXIL.find_first_of(',', pos);
            CHECK_PATCHING_ERROR(pos != String::npos, RecordName, " record is not found")

            ++pos;
            // , i32 -1
            //  ^

            while (pos < DXIL.length() && DXIL[pos] == ' ')
                ++pos;
            CHECK_PATCHING_ERROR(pos < DXIL.length(), RecordName, " record type is missing")
            // , i32 -1
            //   ^

            static const String i32 = "i32";
            CHECK_PATCHING_ERROR(std::strncmp(&DXIL[pos], i32.c_str(), i32.length()) == 0, "unexpected ", RecordName, " record type")
            pos += i32.length();
            // , i32 -1
            //      ^

            pos = DXIL.find_first_of("+-0123456789", pos);
            CHECK_PATCHING_ERROR(pos != String::npos, RecordName, " record data is missing")
            // , i32 -1
            //       ^

            auto RecordEndPos = DXIL.find_first_not_of("0123456789", pos + 1);
            CHECK_PATCHING_ERROR(pos != String::npos, "unable to find the end of the ", RecordName, " record data")
            // , i32 -1
            //         ^
            //    RecordEndPos

            DXIL.replace(pos, RecordEndPos - pos, NewValue);
            // , i32 1
            //         ^
            //    RecordEndPos

            pos += NewValue.length();
            // , i32 1
            //        ^

#undef CHECK_PATCHING_ERROR

            return true;
        };

        // !"g_ColorBuffer", i32 -1, i32 -1,
        //                 ^
        if (!ReplaceRecord("0", "space"))
        {
            RemappingOK = false;
            continue;
        }
        // !"g_ColorBuffer", i32 0, i32 -1,
        //                        ^

        if (!ReplaceRecord(std::to_string(BindPoint), "binding"))
        {
            RemappingOK = false;
            continue;
        }
        // !"g_ColorBuffer", i32 0, i32 1,
        //                               ^
    }
    return RemappingOK;
}

} // namespace Diligent
