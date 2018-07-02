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

#pragma once

/// \file
/// Declaration of Diligent::SwapChainD3D12Impl class

#include <dxgi1_4.h>
#include "SwapChainD3D12.h"
#include "SwapChainD3DBase.h"

namespace Diligent
{

class ITextureViewD3D12;
class IMemoryAllocator;
/// Implementation of the Diligent::ISwapChainD3D12 interface
class SwapChainD3D12Impl : public SwapChainD3DBase<ISwapChainD3D12, IDXGISwapChain3>
{
public:
    using TSwapChainBase = SwapChainD3DBase<ISwapChainD3D12, IDXGISwapChain3>;
    SwapChainD3D12Impl(IReferenceCounters*           pRefCounters,
                       const SwapChainDesc&          SwapChainDesc, 
                       const FullScreenModeDesc&     FSDesc,
                       class RenderDeviceD3D12Impl*  pRenderDeviceD3D12,
                       class DeviceContextD3D12Impl* pDeviceContextD3D12,
                       void* pNativeWndHandle);
    ~SwapChainD3D12Impl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface );

    virtual void Present(Uint32 SyncInterval)override final;
    virtual void Resize( Uint32 NewWidth, Uint32 NewHeight )override final;

    virtual IDXGISwapChain* GetDXGISwapChain()override final{ return m_pSwapChain; }

    virtual ITextureViewD3D12* GetCurrentBackBufferRTV()override final
    {
        auto CurrentBackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
        VERIFY_EXPR(CurrentBackBufferIndex >= 0 && CurrentBackBufferIndex < m_SwapChainDesc.BufferCount);
        return m_pBackBufferRTV[CurrentBackBufferIndex];
    }

    virtual ITextureViewD3D12* GetDepthBufferDSV()override final{return m_pDepthBufferDSV;}

private:
    virtual void UpdateSwapChain(bool CreateNew)override final;
    void InitBuffersAndViews();

    std::vector< RefCntAutoPtr<ITextureViewD3D12>, STDAllocatorRawMem<RefCntAutoPtr<ITextureViewD3D12>> > m_pBackBufferRTV;
    RefCntAutoPtr<ITextureViewD3D12> m_pDepthBufferDSV;
};

}
