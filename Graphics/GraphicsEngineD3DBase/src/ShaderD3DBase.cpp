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
#include "DXILUtils.hpp"
#include "dxc/dxcapi.h"
#include <locale>
#include <cwchar>

namespace Diligent
{
namespace
{

static const Char* g_HLSLDefinitions =
    {
#include "HLSLDefinitions_inc.fxh"
};

static HRESULT CompileDxilShader(const char*             Source,
                                 size_t                  SourceLength,
                                 const ShaderCreateInfo& ShaderCI,
                                 LPCSTR                  profile,
                                 ID3DBlob**              ppBlobOut,
                                 ID3DBlob**              ppCompilerOutput)
{
    struct WStringChunk
    {
        WCHAR* Buffer;
        size_t Size     = 0;
        size_t Capacity = 0;

        WStringChunk()
        {
            Capacity = 1u << 12;
            Buffer   = new WCHAR[Capacity];
        }

        WStringChunk(WStringChunk&& other) :
            Buffer{other.Buffer}, Size{other.Size}, Capacity{other.Capacity}
        {
            other.Buffer = nullptr;
        }

        ~WStringChunk()
        {
            delete[] Buffer;
        }
    };
    std::vector<WStringChunk> wstringPool;

    const auto ToUnicode = [&wstringPool](const char* str) {
        const auto ConvertString = [](const char* src, WCHAR* dst, size_t len) {
            for (size_t i = 0; i < len; ++i)
                dst[i] = static_cast<WCHAR>(src[i]);
            return dst;
        };

        auto len = strlen(str) + 1;
        for (auto& chunk : wstringPool)
        {
            if (chunk.Size + len <= chunk.Capacity)
            {
                auto pos = chunk.Size;
                chunk.Size += len;
                return ConvertString(str, chunk.Buffer + pos, len);
            }
        }
        wstringPool.emplace_back();
        auto& chunk = wstringPool.back();
        chunk.Size += len;
        return ConvertString(str, chunk.Buffer, len);
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

    const wchar_t* pArgs[] =
        {
            L"-Zpc", // Matrices in column-major order
                     //L"-WX",  // Warnings as errors
#ifdef DILIGENT_DEBUG
            L"-Zi", // Debug info
            //L"-Qembed_debug", // Embed debug info into the shader (some compilers do not recognize this flag)
            L"-Od", // Disable optimization
#else
            L"-Od", // TODO: something goes wrong if used any optimizations
                    //L"-O3", // Optimization level 3
#endif
        };

    VERIFY_EXPR(__uuidof(ID3DBlob) == __uuidof(IDxcBlob));

    if (!DXILCompile(DXILCompilerTarget::Direct3D12,
                     Source, SourceLength,
                     ToUnicode(ShaderCI.EntryPoint),
                     ToUnicode(profile),
                     D3DMacros.data(), D3DMacros.size(),
                     pArgs, _countof(pArgs),
                     ShaderCI.pShaderSourceStreamFactory,
                     reinterpret_cast<IDxcBlob**>(ppBlobOut),
                     reinterpret_cast<IDxcBlob**>(ppCompilerOutput)))
    {
        return E_FAIL;
    }
    return S_OK;
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
                             size_t                  SourceLength,
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
    auto           hr = D3DCompile(Source, SourceLength, NULL, D3DMacros.data(), &IncludeImpl, ShaderCI.EntryPoint, profile, dwShaderFlags, 0, ppBlobOut, ppCompilerOutput);

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
} // namespace


ShaderD3DBase::ShaderD3DBase(const ShaderCreateInfo& ShaderCI, const ShaderVersion RequiredVersion, bool IsD3D12) :
    m_isDXIL{false}
{
    if (ShaderCI.Source || ShaderCI.FilePath)
    {
        DEV_CHECK_ERR(ShaderCI.ByteCode == nullptr, "'ByteCode' must be null when shader is created from the source code or a file");
        DEV_CHECK_ERR(ShaderCI.ByteCodeSize == 0, "'ByteCodeSize' must be 0 when shader is created from the source code or a file");

        // validate compiler type
        switch (ShaderCI.ShaderCompiler)
        {
            // clang-format off
            case SHADER_COMPILER_DEFAULT: m_isDXIL = false; break;
            case SHADER_COMPILER_DXC:     m_isDXIL = true;  break;
            case SHADER_COMPILER_FXC:     m_isDXIL = false; break;
                // clang-format on
            default: UNEXPECTED("Unsupported shader compiler"); m_isDXIL = false;
        }

        // validate shader model
        ShaderVersion ShaderModel = RequiredVersion;
        if (m_isDXIL)
        {
            ShaderModel = (ShaderModel.Major >= 6 ? ShaderModel : ShaderVersion{6, 0});

            // clamp to maximum supported version
            ShaderVersion MaxSM;
            if (DXILGetMaxShaderModel(DXILCompilerTarget::Direct3D12, MaxSM))
            {
                if (ShaderModel.Major > MaxSM.Major)
                    ShaderModel = MaxSM;

                if (ShaderModel.Major == MaxSM.Major && ShaderModel.Minor > MaxSM.Minor)
                    ShaderModel = MaxSM;
            }
        }
        else
        {
            ShaderModel = (ShaderModel.Major < 6 ? ShaderModel : (IsD3D12 ? ShaderVersion{5, 1} : ShaderVersion{5, 0}));
        }

        if (RequiredVersion.Major != 0 && (ShaderModel.Major != RequiredVersion.Major || ShaderModel.Minor != RequiredVersion.Minor))
        {
            LOG_INFO_MESSAGE("Shader '", (ShaderCI.Desc.Name != nullptr ? ShaderCI.Desc.Name : ""), "': version changed from ",
                             Uint32(RequiredVersion.Major), ".", Uint32(RequiredVersion.Minor), " to ", Uint32(ShaderModel.Major), ".", Uint32(ShaderModel.Minor));
        }

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
        strShaderProfile += '0' + (ShaderModel.Major % 10);
        strShaderProfile += "_";
        strShaderProfile += '0' + (ShaderModel.Minor % 10);

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
            auto* FileDataPtr = static_cast<Char*>(pFileData->GetDataPtr());
            auto  Size        = pFileData->GetSize();
            ShaderSource.append(FileDataPtr, FileDataPtr + Size / sizeof(*FileDataPtr));
        }

        DEV_CHECK_ERR(ShaderCI.EntryPoint != nullptr, "Entry point must not be null");

        CComPtr<ID3DBlob> errors;
        HRESULT           hr;

        if (m_isDXIL)
            hr = CompileDxilShader(ShaderSource.c_str(), ShaderSource.length(), ShaderCI, strShaderProfile.c_str(), &m_pShaderByteCode, &errors);
        else
            hr = CompileShader(ShaderSource.c_str(), ShaderSource.length(), ShaderCI, strShaderProfile.c_str(), &m_pShaderByteCode, &errors);

        const size_t CompilerMsgLen = errors ? errors->GetBufferSize() : 0;
        const char*  CompilerMsg    = CompilerMsgLen > 0 ? static_cast<const char*>(errors->GetBufferPointer()) : nullptr;

        if (CompilerMsg != nullptr && ShaderCI.ppCompilerOutput != nullptr)
        {
            auto* pOutputDataBlob = MakeNewRCObj<DataBlobImpl>()(CompilerMsgLen + 1 + ShaderSource.length() + 1);
            char* DataPtr         = static_cast<char*>(pOutputDataBlob->GetDataPtr());
            memcpy(DataPtr, CompilerMsg, CompilerMsgLen + 1);
            memcpy(DataPtr + CompilerMsgLen + 1, ShaderSource.data(), ShaderSource.length() + 1);
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
