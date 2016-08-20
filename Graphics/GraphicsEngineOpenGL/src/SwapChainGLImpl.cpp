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
#include "DeviceContextGLImpl.h"
#include "RenderDeviceGLImpl.h"
#include "SwapChainGLImpl.h"

namespace Diligent
{
SwapChainGLImpl::SwapChainGLImpl(IMemoryAllocator &Allocator,
                                 const SwapChainDesc& SCDesc, 
                                 RenderDeviceGLImpl* pRenderDeviceGL, 
                                 DeviceContextGLImpl* pImmediateContextGL) : 
    TSwapChainBase( Allocator, pRenderDeviceGL, pImmediateContextGL, pRenderDeviceGL->m_GLContext.GetSwapChainDesc() )
{
}

SwapChainGLImpl::~SwapChainGLImpl()
{
}

IMPLEMENT_QUERY_INTERFACE( SwapChainGLImpl, IID_SwapChainGL, TSwapChainBase )

void SwapChainGLImpl::Present()
{
    auto *pDeviceGL = ValidatedCast<RenderDeviceGLImpl>(m_pRenderDevice.RawPtr());
    pDeviceGL->m_GLContext.SwapBuffers();
}

void SwapChainGLImpl::Resize( Uint32 NewWidth, Uint32 NewHeight )
{
    if( TSwapChainBase::Resize( NewWidth, NewHeight ) )
    {
        auto pDeviceContext = m_wpDeviceContext.Lock();
        VERIFY( pDeviceContext, "Immediate context has been released" );
        if( pDeviceContext )
        {
            auto *pImmediateCtxGL = ValidatedCast<DeviceContextGLImpl>( pDeviceContext.RawPtr() );
            bool bIsDefaultFBBound = pImmediateCtxGL->IsDefaultFBBound();

            // To update the viewport is the only thing we need to do in OpenGL
            if( bIsDefaultFBBound )
            {
                // Update viewport
                pImmediateCtxGL->SetViewports( 1, nullptr, 0, 0 );
            }
        }
    }
}

}
