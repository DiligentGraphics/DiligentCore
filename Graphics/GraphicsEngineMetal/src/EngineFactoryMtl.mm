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

/// \file
/// Routines that initialize Mtl-based engine implementation

#include "EngineFactoryMtl.h"
#include "RenderDeviceMtlImpl.h"
#include "DeviceContextMtlImpl.h"
#include "SwapChainMtlImpl.h"
#include "MtlTypeConversions.h"
#include "EngineMemory.h"
#include "EngineFactoryBase.hpp"

namespace Diligent
{

/// Engine factory for Mtl implementation
class EngineFactoryMtlImpl : public EngineFactoryBase<IEngineFactoryMtl>
{
public:
    static EngineFactoryMtlImpl* GetInstance()
    {
        static EngineFactoryMtlImpl TheFactory;
        return &TheFactory;
    }

    using TBase = EngineFactoryBase<IEngineFactoryMtl>;
    EngineFactoryMtlImpl() :
        TBase(IID_EngineFactoryMtl)
    {}

    void CreateDeviceAndContextsMtl(const EngineMtlCreateInfo&  EngineCI, 
                                    IRenderDevice**             ppDevice, 
                                    IDeviceContext**            ppContexts)override final;

   void CreateSwapChainMtl( IRenderDevice*         pDevice,
                            IDeviceContext*        pImmediateContext,
                            const SwapChainDesc&   SCDesc,
                            const NativeWindow&    Window,
                            ISwapChain**           ppSwapChain )override final;

   void AttachToMtlDevice(void*                       pMtlNativeDevice, 
                          const EngineMtlCreateInfo&  EngineCI, 
                          IRenderDevice**             ppDevice, 
                          IDeviceContext**            ppContexts)override final;
};


/// Creates render device and device contexts for Metal-based engine implementation

/// \param [in] EngineCI - Engine creation attributes.
/// \param [out] ppDevice - Address of the memory location where pointer to 
///                         the created device will be written
/// \param [out] ppContexts - Address of the memory location where pointers to 
///                           the contexts will be written. Immediate context goes at 
///                           position 0. If EngineCI.NumDeferredContexts > 0,
///                           pointers to the deferred contexts go afterwards.
void EngineFactoryMtlImpl::CreateDeviceAndContextsMtl(const EngineMtlCreateInfo&  EngineCI, 
                                                      IRenderDevice**             ppDevice, 
                                                      IDeviceContext**            ppContexts)
{
    if (EngineCI.DebugMessageCallback != nullptr)
        SetDebugMessageCallback(EngineCI.DebugMessageCallback);

    VERIFY( ppDevice && ppContexts, "Null pointer provided" );
    if( !ppDevice || !ppContexts )
        return;

    *ppDevice = nullptr;
    memset(ppContexts, 0, sizeof(*ppContexts) * (1 + EngineCI.NumDeferredContexts));

    void* pMtlDevice = nullptr;
    AttachToMtlDevice(pMtlDevice, EngineCI, ppDevice, ppContexts);
}


/// Attaches to existing Mtl render device and immediate context

/// \param [in] pMtlNativeDevice - pointer to native Mtl device
/// \param [in] pMtlImmediateContext - pointer to native Mtl immediate context
/// \param [in] EngineCI - Engine creation attributes.
/// \param [out] ppDevice - Address of the memory location where pointer to 
///                         the created device will be written
/// \param [out] ppContexts - Address of the memory location where pointers to 
///                           the contexts will be written. Immediate context goes at 
///                           position 0. If EngineCI.NumDeferredContexts > 0,
///                           pointers to the deferred contexts go afterwards.
void EngineFactoryMtlImpl::AttachToMtlDevice(void*                       pMtlNativeDevice, 
                                             const EngineMtlCreateInfo&  EngineCI, 
                                             IRenderDevice**			 ppDevice, 
                                             IDeviceContext**			 ppContexts)
{
    if (EngineCI.DebugMessageCallback != nullptr)
        SetDebugMessageCallback(EngineCI.DebugMessageCallback);

    VERIFY( ppDevice && ppContexts, "Null pointer provided" );
    if( !ppDevice || !ppContexts )
        return;

    try
    {
        SetRawAllocator(EngineCI.pRawMemAllocator);
        auto& RawAlloctor = GetRawAllocator();
        RenderDeviceMtlImpl* pRenderDeviceMtl(NEW_RC_OBJ(RawAlloctor, "RenderDeviceMtlImpl instance", RenderDeviceMtlImpl)
                                                        (RawAlloctor, this, EngineCI, pMtlNativeDevice));
        pRenderDeviceMtl->QueryInterface(IID_RenderDevice, reinterpret_cast<IObject**>(ppDevice));

        RefCntAutoPtr<DeviceContextMtlImpl> pDeviceContextMtl(NEW_RC_OBJ(RawAlloctor, "DeviceContextMtlImpl instance", DeviceContextMtlImpl)
                                                                        (RawAlloctor, pRenderDeviceMtl, EngineCI, false));
        
        // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceMtl will
        // keep a weak reference to the context
        pDeviceContextMtl->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppContexts));
        pRenderDeviceMtl->SetImmediateContext(pDeviceContextMtl);

        for (Uint32 DeferredCtx = 0; DeferredCtx < EngineCI.NumDeferredContexts; ++DeferredCtx)
        {
            RefCntAutoPtr<DeviceContextMtlImpl> pDeferredCtxMtl(
                NEW_RC_OBJ(RawAlloctor, "DeviceContextMtlImpl instance", DeviceContextMtlImpl)
                          (RawAlloctor, pRenderDeviceMtl, EngineCI, true));
            // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceD3D12 will
            // keep a weak reference to the context
            pDeferredCtxMtl->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppContexts + 1 + DeferredCtx));
            pRenderDeviceMtl->SetDeferredContext(DeferredCtx, pDeferredCtxMtl);
        }
    }
    catch( const std::runtime_error & )
    {
        if( *ppDevice )
        {
            (*ppDevice)->Release();
            *ppDevice = nullptr;
        }
        for(Uint32 ctx=0; ctx < 1 + EngineCI.NumDeferredContexts; ++ctx)
        {
            if( ppContexts[ctx] != nullptr )
            {
                ppContexts[ctx]->Release();
                ppContexts[ctx] = nullptr;
            }
        }

        LOG_ERROR( "Failed to initialize Mtl device and contexts" );
    }
}

/// Creates a swap chain for Direct3D11-based engine implementation

/// \param [in] pDevice - Pointer to the render device
/// \param [in] pImmediateContext - Pointer to the immediate device context
/// \param [in] SCDesc - Swap chain description
/// \param [in] FSDesc - Fullscreen mode description
/// \param [in] pNativeWndHandle - Platform-specific native handle of the window 
///                                the swap chain will be associated with:
///                                * On Win32 platform, this should be window handle (HWND)
///                                * On Universal Windows Platform, this should be reference to the 
///                                  core window (Windows::UI::Core::CoreWindow)
///                                
/// \param [out] ppSwapChain    - Address of the memory location where pointer to the new 
///                               swap chain will be written
void EngineFactoryMtlImpl::CreateSwapChainMtl(IRenderDevice*       pDevice,
                                              IDeviceContext*      pImmediateContext,
                                              const SwapChainDesc& SCDesc,
                                              const NativeWindow&  Window,
                                              ISwapChain**         ppSwapChain )
{
    VERIFY( ppSwapChain, "Null pointer provided" );
    if( !ppSwapChain )
        return;

    *ppSwapChain = nullptr;

    try
    {
        auto *pDeviceMtl = ValidatedCast<RenderDeviceMtlImpl>( pDevice );
        auto *pDeviceContextMtl = ValidatedCast<DeviceContextMtlImpl>(pImmediateContext);
        auto &RawMemAllocator = GetRawAllocator();

        auto *pSwapChainMtl = NEW_RC_OBJ(RawMemAllocator,  "SwapChainMtlImpl instance", SwapChainMtlImpl)
                                          (SCDesc, pDeviceMtl, pDeviceContextMtl, Window);
        pSwapChainMtl->QueryInterface( IID_SwapChain, reinterpret_cast<IObject**>(ppSwapChain) );
    }
    catch( const std::runtime_error & )
    {
        if( *ppSwapChain )
        {
            (*ppSwapChain)->Release();
            *ppSwapChain = nullptr;
        }

        LOG_ERROR( "Failed to create the swap chain" );
    }
}

IEngineFactoryMtl* GetEngineFactoryMtl()
{
    return EngineFactoryMtlImpl::GetInstance();
}

}
