/*     Copyright 2015-2019 Egor Yusov
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
#include "SwapChainD3D11Impl.h"
#include "RenderDeviceD3D11Impl.h"
#include "DeviceContextD3D11Impl.h"
#include <dxgi1_2.h>

namespace Diligent
{

SwapChainD3D11Impl::SwapChainD3D11Impl(IReferenceCounters*       pRefCounters,
                                       const SwapChainDesc&      SCDesc, 
                                       const FullScreenModeDesc& FSDesc,
                                       RenderDeviceD3D11Impl*    pRenderDeviceD3D11, 
                                       DeviceContextD3D11Impl*   pDeviceContextD3D11, 
                                       void*                     pNativeWndHandle) : 
    TSwapChainBase(pRefCounters, pRenderDeviceD3D11, pDeviceContextD3D11, SCDesc, FSDesc, pNativeWndHandle)
{
    auto *pd3d11Device = pRenderDeviceD3D11->GetD3D11Device();
    CreateDXGISwapChain(pd3d11Device);
    CreateRTVandDSV();
}

SwapChainD3D11Impl::~SwapChainD3D11Impl()
{
}

void SwapChainD3D11Impl::CreateRTVandDSV()
{
    auto* pRenderDeviceD3D11Impl = m_pRenderDevice.RawPtr<RenderDeviceD3D11Impl>();
    
    m_pRenderTargetView.Release();
    m_pDepthStencilView.Release();

    // Create a render target view
    CComPtr<ID3D11Texture2D> pd3dBackBuffer;
    CHECK_D3D_RESULT_THROW( m_pSwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>( static_cast<ID3D11Texture2D**>(&pd3dBackBuffer) ) ),
                            "Failed to get back buffer from swap chain" );
    static const char BackBufferName[] = "Main back buffer";
    auto hr = pd3dBackBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, _countof(BackBufferName)-1, BackBufferName);
    DEV_CHECK_ERR(SUCCEEDED(hr), "Failed to set back buffer name");

    RefCntAutoPtr<ITexture> pBackBuffer;
    pRenderDeviceD3D11Impl->CreateTextureFromD3DResource(pd3dBackBuffer, RESOURCE_STATE_UNKNOWN, &pBackBuffer);

    TextureViewDesc RTVDesc;
    RTVDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
    RTVDesc.Format = m_SwapChainDesc.ColorBufferFormat;
    RefCntAutoPtr<ITextureView> pRTV;
    pBackBuffer->CreateView(RTVDesc, &pRTV);
    m_pRenderTargetView = RefCntAutoPtr<ITextureViewD3D11>(pRTV, IID_TextureViewD3D11);
    
    // Create depth buffer
    TextureDesc DepthBufferDesc;
    DepthBufferDesc.Name = "Main depth buffer";
    DepthBufferDesc.Type = RESOURCE_DIM_TEX_2D;
    DepthBufferDesc.Width = m_SwapChainDesc.Width;
    DepthBufferDesc.Height = m_SwapChainDesc.Height;
    DepthBufferDesc.MipLevels = 1;
    DepthBufferDesc.ArraySize = 1;
    DepthBufferDesc.Format = m_SwapChainDesc.DepthBufferFormat;
    DepthBufferDesc.SampleCount = m_SwapChainDesc.SamplesCount;
    DepthBufferDesc.Usage = USAGE_DEFAULT;
    DepthBufferDesc.BindFlags = BIND_DEPTH_STENCIL;
    DepthBufferDesc.CPUAccessFlags = CPU_ACCESS_NONE;
    DepthBufferDesc.MiscFlags = MISC_TEXTURE_FLAG_NONE;
    RefCntAutoPtr<ITexture> ptex2DDepthBuffer;
    m_pRenderDevice->CreateTexture(DepthBufferDesc, TextureData{}, &ptex2DDepthBuffer);
    auto pDSV = ptex2DDepthBuffer->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    m_pDepthStencilView = RefCntAutoPtr<ITextureViewD3D11>(pDSV, IID_TextureViewD3D11);
}

IMPLEMENT_QUERY_INTERFACE( SwapChainD3D11Impl, IID_SwapChainD3D11, TSwapChainBase )

void SwapChainD3D11Impl::Present(Uint32 SyncInterval)
{
#if PLATFORM_UNIVERSAL_WINDOWS
    SyncInterval = 1; // Interval 0 is not supported on Windows Phone 
#endif

    auto pDeviceContext = m_wpDeviceContext.Lock();
    if( !pDeviceContext )
    {
        LOG_ERROR_MESSAGE( "Immediate context has been released" );
        return;
    }

    auto *pImmediateCtx = pDeviceContext.RawPtr();
    auto *pImmediateCtxD3D11 = ValidatedCast<DeviceContextD3D11Impl>( pImmediateCtx );
    // Clear the state caches to release all outstanding objects
    // that are only kept alive by references in the cache
    // It is better to do this before calling Present() as D3D11
    // also releases resources during present.
    pImmediateCtxD3D11->ReleaseCommittedShaderResources();
    // ReleaseCommittedShaderResources() does not unbind vertex and index buffers
    // as this can explicitly be done by the user


    m_pSwapChain->Present( SyncInterval, 0 );

    // A successful Present call for DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL SwapChains unbinds 
    // backbuffer 0 from all GPU writeable bind points.
    // We need to rebind all render targets to make sure that
    // the back buffer is not unbound
    pImmediateCtxD3D11->CommitRenderTargets();
}

void SwapChainD3D11Impl::UpdateSwapChain(bool CreateNew)
{
    // When switching to full screen mode, WM_SIZE is send to the window
    // and Resize() is called before the new swap chain is created
    if(!m_pSwapChain)
        return;

    auto pDeviceContext = m_wpDeviceContext.Lock();
    VERIFY(pDeviceContext, "Immediate context has been released");
    if (pDeviceContext)
    {
        auto *pImmediateCtxD3D11 = pDeviceContext.RawPtr<DeviceContextD3D11Impl>();
        bool bIsDefaultFBBound = pImmediateCtxD3D11->IsDefaultFBBound();
        if (bIsDefaultFBBound)
            pImmediateCtxD3D11->ResetRenderTargets();

        // Swap chain cannot be resized until all references are released
        m_pRenderTargetView.Release();
        m_pDepthStencilView.Release();

        try
        {
            if(CreateNew)
            {
                m_pSwapChain.Release();

                // Only one flip presentation model swap chain can be associated with an HWND.
                // We must make sure that the swap chain is actually released by D3D11 before creating a new one.
                // To force the destruction, we need to ensure no views are bound to pipeline state, and then call Flush 
                // on the immediate context. Dstruction must be forced before calling IDXGIFactory2::CreateSwapChainForHwnd(), or
                // IDXGIFactory2::CreateSwapChainForCoreWindow() again to create a new swap chain.
                // https://msdn.microsoft.com/en-us/library/windows/desktop/ff476425(v=vs.85).aspx#Defer_Issues_with_Flip
                pImmediateCtxD3D11->Flush();

                auto *pd3d11Device = m_pRenderDevice.RawPtr<RenderDeviceD3D11Impl>()->GetD3D11Device();
                CreateDXGISwapChain(pd3d11Device);
            }
            else
            {
                DXGI_SWAP_CHAIN_DESC SCDes;
                memset(&SCDes, 0, sizeof(SCDes));
                m_pSwapChain->GetDesc(&SCDes);
                CHECK_D3D_RESULT_THROW(m_pSwapChain->ResizeBuffers(SCDes.BufferCount, m_SwapChainDesc.Width,
                    m_SwapChainDesc.Height, SCDes.BufferDesc.Format,
                    SCDes.Flags),
                    "Failed to resize the DXGI swap chain");
            }

            CreateRTVandDSV();

            if (bIsDefaultFBBound)
            {
                // Set default render target and viewport
                pImmediateCtxD3D11->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                pImmediateCtxD3D11->SetViewports(1, nullptr, 0, 0);
            }
        }
        catch (const std::runtime_error &)
        {
            LOG_ERROR("Failed to resize the swap chain");
        }
    }
}

void SwapChainD3D11Impl::Resize( Uint32 NewWidth, Uint32 NewHeight )
{
    if( TSwapChainBase::Resize(NewWidth, NewHeight) )
    {
        UpdateSwapChain(false);
    }
}

}
