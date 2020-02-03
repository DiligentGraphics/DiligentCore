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

#include "SwapChainMtlImpl.h"
#include "RenderDeviceMtlImpl.h"
#include "DeviceContextMtlImpl.h"

namespace Diligent
{

SwapChainMtlImpl::SwapChainMtlImpl(IReferenceCounters*       pRefCounters,
                                   const SwapChainDesc&      SCDesc, 
                                   RenderDeviceMtlImpl*      pRenderDeviceMtl, 
                                   DeviceContextMtlImpl*     pDeviceContextMtl, 
                                   const NativeWindow&       Window) :
    TSwapChainBase(pRefCounters, pRenderDeviceMtl, pDeviceContextMtl, SCDesc)
{
    LOG_ERROR_AND_THROW("Swap chain is not implemented in Metal backend");
}

SwapChainMtlImpl::~SwapChainMtlImpl()
{
}

IMPLEMENT_QUERY_INTERFACE( SwapChainMtlImpl, IID_SwapChainMtl, TSwapChainBase )

void SwapChainMtlImpl::Present(Uint32 SyncInterval)
{
    LOG_ERROR_MESSAGE("SwapChainMtlImpl::Present() is not implemented");
}


void SwapChainMtlImpl::Resize( Uint32 NewWidth, Uint32 NewHeight )
{
    if( TSwapChainBase::Resize(NewWidth, NewHeight) )
    {
        LOG_ERROR_MESSAGE("SwapChainMtlImpl::Resize() is not implemented");
    }
}

void SwapChainMtlImpl::SetFullscreenMode(const DisplayModeAttribs &DisplayMode)
{
    LOG_ERROR_MESSAGE("SwapChainMtlImpl::SetFullscreenMode() is not implemented");
}

void SwapChainMtlImpl::SetWindowedMode()
{
    LOG_ERROR_MESSAGE("SwapChainMtlImpl::SetWindowedMode() is not implemented");
}

ITextureView* SwapChainMtlImpl::GetCurrentBackBufferRTV()
{
    LOG_ERROR_MESSAGE("SwapChainMtlImpl::GetCurrentBackBufferRTV() is not implemented");
    return nullptr;
}

ITextureView* SwapChainMtlImpl::GetDepthBufferDSV()
{
    LOG_ERROR_MESSAGE("SwapChainMtlImpl::GetDepthBufferDSV() is not implemented");
    return nullptr;
}

}
