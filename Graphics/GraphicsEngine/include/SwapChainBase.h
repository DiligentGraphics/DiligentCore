/*     Copyright 2019 Diligent Graphics LLC
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
/// Implementation of the Diligent::SwapChainBase template class

#include <array>

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "DeviceObjectBase.h"
#include "Errors.h"

namespace Diligent
{

/// Base implementation of the swap chain.

/// \tparam BaseInterface - base interface that this class will inheret 
///                         (Diligent::ISwapChainGL, Diligent::ISwapChainD3D11,
///                          Diligent::ISwapChainD3D12 or Diligent::ISwapChainVk).
/// \remarks Swap chain holds the strong reference to the device and a weak reference to the
///          immediate context.
template<class BaseInterface>
class SwapChainBase : public ObjectBase<BaseInterface>
{
public:
    typedef ObjectBase<BaseInterface> TObjectBase;

    /// \param pRefCounters - reference counters object that controls the lifetime of this swap chain.
	/// \param pDevice - pointer to the device.
	/// \param pDeviceContext - pointer to the device context.
	/// \param SCDesc - swap chain description
    SwapChainBase(IReferenceCounters*  pRefCounters,
                  IRenderDevice*       pDevice,
                  IDeviceContext*      pDeviceContext,
                  const SwapChainDesc& SCDesc) :
        TObjectBase       {pRefCounters},
        m_pRenderDevice   {pDevice       },
        m_wpDeviceContext {pDeviceContext},
        m_SwapChainDesc   {SCDesc        }
    {
    }

    SwapChainBase             (const SwapChainBase&)  = delete;
    SwapChainBase             (      SwapChainBase&&) = delete;
    SwapChainBase& operator = (const SwapChainBase&)  = delete;
    SwapChainBase& operator = (     SwapChainBase&&)  = delete;

    ~SwapChainBase()
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_SwapChain, TObjectBase )

    /// Implementation of ISwapChain::GetDesc()
    virtual const SwapChainDesc& GetDesc()const override final
    {
        return m_SwapChainDesc;
    }
    
protected:
    bool Resize( Uint32 NewWidth, Uint32 NewHeight, Int32 Dummy = 0/*To be different from virtual function*/ )
    {
        if( NewWidth != 0 && NewHeight != 0 &&
            (m_SwapChainDesc.Width != NewWidth || m_SwapChainDesc.Height != NewHeight) )
        {
            m_SwapChainDesc.Width = NewWidth;
            m_SwapChainDesc.Height = NewHeight;
            LOG_INFO_MESSAGE("Resizing the swap chain to ", m_SwapChainDesc.Width, "x", m_SwapChainDesc.Height);
            return true;
        }

        return false;
    }

    template<typename DeviceContextImplType>
    bool UnbindRenderTargets(DeviceContextImplType* pImmediateCtx,
                             ITextureView*          ppBackBufferRTVs[],
                             Uint32                 NumBackBufferRTVs,
                             ITextureView*          pDSV)
    {
        bool RebindRenderTargets = false; 
        bool UnbindRenderTargets = false;
        if (m_SwapChainDesc.IsPrimary)
        {
            RebindRenderTargets = UnbindRenderTargets = pImmediateCtx->IsDefaultFBBound();
        }
        else
        {
            std::array<ITextureView*, MaxRenderTargets> pBoundRTVs = {};
            RefCntAutoPtr<ITextureView> pBoundDSV;
            Uint32 NumRenderTargets = 0;
            pImmediateCtx->GetRenderTargets(NumRenderTargets, pBoundRTVs.data(), &pBoundDSV);
            for (Uint32 i=0; i < NumRenderTargets; ++i)
            {
                for (Uint32 j=0; j < NumBackBufferRTVs; ++j)
                {
                    if (pBoundRTVs[i] == ppBackBufferRTVs[j])
                        UnbindRenderTargets = true;
                }
            }
            if (pBoundDSV == pDSV)
                UnbindRenderTargets = true;

            for (auto pRTV : pBoundRTVs)
            {
                if (pRTV != nullptr)
                    pRTV->Release();
            }
        }

        if (UnbindRenderTargets)
            pImmediateCtx->ResetRenderTargets();

        return RebindRenderTargets;
    }

    /// Strong reference to the render device
    RefCntAutoPtr<IRenderDevice> m_pRenderDevice;
    
    /// Weak references to the immediate device context. The context holds
    /// the strong reference to the swap chain.
    RefCntWeakPtr<IDeviceContext> m_wpDeviceContext;

    /// Swap chain description
    SwapChainDesc m_SwapChainDesc;
};

}
