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
#include "SwapChainD3D11Impl.h"
#include "RenderDeviceD3D11Impl.h"
#include "DeviceContextD3D11Impl.h"
#include <dxgi1_2.h>

using namespace Diligent;

namespace Diligent
{

SwapChainD3D11Impl::SwapChainD3D11Impl(IReferenceCounters *pRefCounters,
                                       const SwapChainDesc& SCDesc, 
                                       const FullScreenModeDesc& FSDesc,
                                       RenderDeviceD3D11Impl* pRenderDeviceD3D11, 
                                       DeviceContextD3D11Impl* pDeviceContextD3D11, 
                                       void* pNativeWndHandle) : 
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
    auto *pDevice = ValidatedCast<RenderDeviceD3D11Impl>(m_pRenderDevice.RawPtr())->GetD3D11Device();

    m_pRenderTargetView.Release();
    m_pDepthStencilView.Release();

    // Create a render target view
    CComPtr<ID3D11Texture2D> pBackBuffer;
    CHECK_D3D_RESULT_THROW( m_pSwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>( static_cast<ID3D11Texture2D**>(&pBackBuffer) ) ),
                            "Failed to get back buffer from swap chain" );

    D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
    RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    // We need to explicitly specify RTV format, as we may need to create RGBA8_UNORM_SRGB RTV for
    // a RGBA8_UNORM swap chain
    RTVDesc.Format = TexFormatToDXGI_Format(m_SwapChainDesc.ColorBufferFormat);
    RTVDesc.Texture2D.MipSlice = 0;
    CHECK_D3D_RESULT_THROW( pDevice->CreateRenderTargetView( pBackBuffer, &RTVDesc, &m_pRenderTargetView ),
                            "Failed to get RTV for the back buffer" );

    // Create depth buffer
    D3D11_TEXTURE2D_DESC DepthBufferDesc;
    DepthBufferDesc.Width = m_SwapChainDesc.Width;
    DepthBufferDesc.Height = m_SwapChainDesc.Height;
    DepthBufferDesc.MipLevels = 1;
    DepthBufferDesc.ArraySize = 1;
    auto DepthFormat = TexFormatToDXGI_Format( m_SwapChainDesc.DepthBufferFormat );
    DepthBufferDesc.Format = DepthFormat;
    DepthBufferDesc.SampleDesc.Count = m_SwapChainDesc.SamplesCount;
    DepthBufferDesc.SampleDesc.Quality = 0;
    DepthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    DepthBufferDesc.CPUAccessFlags = 0;
    DepthBufferDesc.MiscFlags = 0;
    CComPtr<ID3D11Texture2D> ptex2DDepthBuffer;
    CHECK_D3D_RESULT_THROW( pDevice->CreateTexture2D( &DepthBufferDesc, NULL, &ptex2DDepthBuffer ),
                            "Failed to create the depth buffer" );

    // Create DSV
    CHECK_D3D_RESULT_THROW( pDevice->CreateDepthStencilView( ptex2DDepthBuffer, NULL, &m_pDepthStencilView ),
                            "Failed to create the DSV for the depth buffer" );
}

IMPLEMENT_QUERY_INTERFACE( SwapChainD3D11Impl, IID_SwapChainD3D11, TSwapChainBase )

void SwapChainD3D11Impl::Present()
{
    UINT SyncInterval = 0;
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
        auto *pImmediateCtxD3D11 = ValidatedCast<DeviceContextD3D11Impl>(pDeviceContext.RawPtr());
        bool bIsDefaultFBBound = pImmediateCtxD3D11->IsDefaultFBBound();
        if (bIsDefaultFBBound)
        {
            ITextureView *pNullTexView[] = { nullptr };
            pImmediateCtxD3D11->SetRenderTargets(_countof(pNullTexView), pNullTexView, nullptr);
        }

        // Swap chain cannot be resized until all references are released
        m_pRenderTargetView.Release();
        m_pDepthStencilView.Release();

        try
        {
            if(CreateNew)
            {
                m_pSwapChain.Release();
                auto *pd3d11Device = ValidatedCast<RenderDeviceD3D11Impl>(m_pRenderDevice.RawPtr())->GetD3D11Device();
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
                pImmediateCtxD3D11->SetRenderTargets(0, nullptr, nullptr);
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
