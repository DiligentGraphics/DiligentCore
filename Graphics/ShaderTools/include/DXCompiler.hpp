/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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

#pragma once

#include <vector>
#include <memory>

#include "Constants.h"
#include "Shader.h"
#include "DataBlob.h"
#include "ResourceBindingMap.hpp"

// defined in dxcapi.h
struct DxcDefine;
struct IDxcBlob;

// defined in d3d12shader.h
struct ID3D12ShaderReflection;

namespace Diligent
{

enum class DXCompilerTarget
{
    Direct3D12, // compiles to DXIL
    Vulkan,     // compiles to SPIRV
};

/// DXC compiler interface.
class IDXCompiler
{
public:
    virtual ~IDXCompiler() {}

    virtual ShaderVersion GetMaxShaderModel() = 0;

    virtual bool IsLoaded() = 0;

    virtual Version GetVersion() = 0;

    struct CompileAttribs
    {
        const char*                      Source                     = nullptr;
        Uint32                           SourceLength               = 0;
        const wchar_t*                   EntryPoint                 = nullptr;
        const wchar_t*                   Profile                    = nullptr;
        const DxcDefine*                 pDefines                   = nullptr;
        Uint32                           DefinesCount               = 0;
        const wchar_t**                  pArgs                      = nullptr;
        Uint32                           ArgsCount                  = 0;
        IShaderSourceInputStreamFactory* pShaderSourceStreamFactory = nullptr;
        IDxcBlob**                       ppBlobOut                  = nullptr;
        IDxcBlob**                       ppCompilerOutput           = nullptr;
    };
    /// Compiles HLSL source code to DXIL or SPIRV.
    ///
    /// \remarks    The method is thread-safe.
    ///             For each compilation, the method creates a new IDxcCompiler instance.
    virtual bool Compile(const CompileAttribs& Attribs) = 0;

    virtual void Compile(const ShaderCreateInfo& ShaderCI,
                         ShaderVersion           ShaderModel,
                         const char*             Preamble,
                         IDxcBlob**              ppByteCodeBlob,
                         std::vector<uint32_t>*  pByteCode,
                         IDataBlob**             ppCompilerOutput) noexcept(false) = 0;


    using BindInfo            = ResourceBinding::BindInfo;
    using TResourceBindingMap = ResourceBinding::TMap;

    /// \param [in]  ResourceMap   - Resource binding map. For every resource in the
    ///                              source byte code it must define the binding
    ///                              (shader register and space).
    /// \param [in]  pSrcBytecode  - Source byte code.
    /// \param [out] ppDstByteCode - Memory location where the pointer to the byte code
    ///                              with the remapped bindings will be written.
    ///
    /// \return     true if the remapping was successful, and false otherwise.
    virtual bool RemapResourceBindings(const TResourceBindingMap& ResourceMap,
                                       IDxcBlob*                  pSrcBytecode,
                                       IDxcBlob**                 ppDstByteCode) = 0;

    /// Attempts to extract shader reflection from the bytecode using DXC.
    virtual void GetD3D12ShaderReflection(IDxcBlob*                pShaderBytecode,
                                          ID3D12ShaderReflection** ppShaderReflection) = 0;
};

// Use this function to load the DX Compiler library.
// pLibraryName is an optional path to the library. If not provided, default
// path is used.
std::unique_ptr<IDXCompiler> CreateDXCompiler(DXCompilerTarget Target, Uint32 APIVersion, const char* pLibraryName);

bool IsDXILBytecode(const void* pBytecode, size_t Size);

/// Creates a DXC blob wrapper around the provided data blob.
void CreateDxcBlobWrapper(IDataBlob* pDataBlob, IDxcBlob** pDxcBlobWrapper);

} // namespace Diligent
