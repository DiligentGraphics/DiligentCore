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

#pragma once

/// \file
/// Declaration of Diligent::SamplerD3D11Impl class

#include "SamplerD3D11.h"
#include "RenderDeviceD3D11.h"
#include "SamplerBase.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;
/// Implementation of the Diligent::ISamplerD3D11 interface
class SamplerD3D11Impl : public SamplerBase<ISamplerD3D11, IRenderDeviceD3D11, FixedBlockMemoryAllocator>
{
public:
    typedef SamplerBase<ISamplerD3D11, IRenderDeviceD3D11, FixedBlockMemoryAllocator> TSamplerBase;

    SamplerD3D11Impl(FixedBlockMemoryAllocator &SamplerObjAllocator, class RenderDeviceD3D11Impl *pRenderDeviceD3D11, const SamplerDesc& SamplerDesc);
    ~SamplerD3D11Impl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface ) final;

    virtual ID3D11SamplerState* GetD3D11SamplerState()override final{ return m_pd3dSampler; }

private:
    /// D3D11 sampler
    CComPtr<ID3D11SamplerState> m_pd3dSampler;
};

}
