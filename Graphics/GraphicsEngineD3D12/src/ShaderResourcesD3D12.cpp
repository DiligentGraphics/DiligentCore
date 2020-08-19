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

#include "pch.h"

#include <d3dcompiler.h>
#include "ShaderResourcesD3D12.hpp"
#include "ShaderD3DBase.hpp"
#include "ShaderBase.hpp"

#ifdef HAS_DXIL_COMPILER
#    include "dxcapi.h"
#endif

namespace Diligent
{


ShaderResourcesD3D12::ShaderResourcesD3D12(ID3DBlob* pShaderBytecode, bool isDXIL, const ShaderDesc& ShdrDesc, const char* CombinedSamplerSuffix) :
    ShaderResources{ShdrDesc.ShaderType}
{
    class NewResourceHandler
    {
    public:
        // clang-format off
        void OnNewCB     (const D3DShaderResourceAttribs& CBAttribs)     {}
        void OnNewTexUAV (const D3DShaderResourceAttribs& TexUAV)        {}
        void OnNewBuffUAV(const D3DShaderResourceAttribs& BuffUAV)       {}
        void OnNewBuffSRV(const D3DShaderResourceAttribs& BuffSRV)       {}
        void OnNewSampler(const D3DShaderResourceAttribs& SamplerAttribs){}
        void OnNewTexSRV (const D3DShaderResourceAttribs& TexAttribs)    {}
        // clang-format on
    };

    CComPtr<ID3D12ShaderReflection> pShaderReflection;

    HRESULT hr;

    if (isDXIL)
    {
#ifdef HAS_DXIL_COMPILER
        const uint32_t                   DFCC_DXIL = uint32_t('D') | (uint32_t('X') << 8) | (uint32_t('I') << 16) | (uint32_t('L') << 24);
        CComPtr<IDxcContainerReflection> pReflection;
        UINT32                           shaderIdx;
        DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&pReflection));
        hr = pReflection->Load(reinterpret_cast<IDxcBlob*>(pShaderBytecode));
        CHECK_D3D_RESULT_THROW(hr, "Failed to create shader reflection instance");
        hr = pReflection->FindFirstPartKind(DFCC_DXIL, &shaderIdx);
        CHECK_D3D_RESULT_THROW(hr, "Failed to find DXIL part");
        hr = pReflection->GetPartReflection(shaderIdx, __uuidof(pShaderReflection), reinterpret_cast<void**>(&pShaderReflection));
        CHECK_D3D_RESULT_THROW(hr, "Failed to get the shader reflection");
#else
        LOG_ERROR_AND_THROW("DXIL compiler is not supported");
#endif
    }
    else
    {
        hr = D3DReflect(pShaderBytecode->GetBufferPointer(), pShaderBytecode->GetBufferSize(), __uuidof(pShaderReflection), reinterpret_cast<void**>(&pShaderReflection));
        CHECK_D3D_RESULT_THROW(hr, "Failed to get the shader reflection");
    }

    Initialize<D3D12_SHADER_DESC, D3D12_SHADER_INPUT_BIND_DESC, ID3D12ShaderReflection>(
        static_cast<ID3D12ShaderReflection*>(pShaderReflection),
        NewResourceHandler{},
        ShdrDesc.Name,
        CombinedSamplerSuffix);
}

} // namespace Diligent
