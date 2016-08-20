/*     Copyright 2015-2016 Egor Yusov
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
#include "SamplerD3D11Impl.h"
#include "RenderDeviceD3D11Impl.h"
#include "D3D11TypeConversions.h"

namespace Diligent
{

SamplerD3D11Impl::SamplerD3D11Impl(FixedBlockMemoryAllocator &SamplerObjAllocator, class RenderDeviceD3D11Impl *pRenderDeviceD3D11, const SamplerDesc& SamplerDesc) : 
    TSamplerBase(SamplerObjAllocator, pRenderDeviceD3D11, SamplerDesc)
{
    auto *pd3d11Device = pRenderDeviceD3D11->GetD3D11Device();
    D3D11_SAMPLER_DESC D3D11SamplerDesc = 
    {
        FilterTypeToD3D11Filter(SamplerDesc.MinFilter, SamplerDesc.MagFilter, SamplerDesc.MipFilter),
        TexAddressModeToD3D11AddressMode(SamplerDesc.AddressU),
        TexAddressModeToD3D11AddressMode(SamplerDesc.AddressV),
        TexAddressModeToD3D11AddressMode(SamplerDesc.AddressW),
        SamplerDesc.MipLODBias,
        SamplerDesc.MaxAnisotropy,
        ComparisonFuncToD3D11ComparisonFunc(SamplerDesc.ComparisonFunc),
        {SamplerDesc.BorderColor[0], SamplerDesc.BorderColor[1], SamplerDesc.BorderColor[2], SamplerDesc.BorderColor[3]},
        SamplerDesc.MinLOD,
        SamplerDesc.MaxLOD
    };
    CHECK_D3D_RESULT_THROW( pd3d11Device->CreateSamplerState(&D3D11SamplerDesc, &m_pd3dSampler),
                            "Failed to create the Direct3D11 sampler");
}

SamplerD3D11Impl::~SamplerD3D11Impl()
{

}

IMPLEMENT_QUERY_INTERFACE( SamplerD3D11Impl, IID_SamplerD3D11, TSamplerBase )

}
