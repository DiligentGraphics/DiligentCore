/*     Copyright 2015-2018 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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
#include "ShaderResourcesD3D12.h"
#include "ShaderD3DBase.h"
#include "ShaderBase.h"

namespace Diligent
{


ShaderResourcesD3D12::ShaderResourcesD3D12(ID3DBlob* pShaderBytecode, const ShaderDesc& ShdrDesc, const char* CombinedSamplerSuffix) :
    ShaderResources(ShdrDesc.ShaderType)
{
    class NewResourceHandler
    {
    public:
        void OnNewCB     (const D3DShaderResourceAttribs& CBAttribs)     {}
        void OnNewTexUAV (const D3DShaderResourceAttribs& TexUAV)        {}
        void OnNewBuffUAV(const D3DShaderResourceAttribs& BuffUAV)       {}
        void OnNewBuffSRV(const D3DShaderResourceAttribs& BuffSRV)       {}
        void OnNewSampler(const D3DShaderResourceAttribs& SamplerAttribs){}
        void OnNewTexSRV (const D3DShaderResourceAttribs& TexAttribs)    {}
    };
    Initialize<D3D12_SHADER_DESC, D3D12_SHADER_INPUT_BIND_DESC, ID3D12ShaderReflection>(
        pShaderBytecode,
        NewResourceHandler{},
        ShdrDesc,
        CombinedSamplerSuffix
    );
}

}
