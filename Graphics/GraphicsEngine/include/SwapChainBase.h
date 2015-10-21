/*     Copyright 2015 Egor Yusov
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

#include "SwapChain.h"
#include "DeviceObjectBase.h"

namespace Diligent
{

/// Base implementation of the swap chain.

/// \remarks Swap chain holds the strong reference to the device and a weak reference to the
///          immediate context.
template<class BaseInterface = ISwapChain>
class SwapChainBase : public ObjectBase<BaseInterface>
{
public:
    typedef ObjectBase<BaseInterface> TObjectBase;

    SwapChainBase( IRenderDevice *pDevice,
                    IDeviceContext *pDeviceContext,
                    const SwapChainDesc& SCDesc ) : 
        m_pRenderDevice(pDevice),
        m_wpDeviceContext(pDeviceContext),
        m_SwapChainDesc(SCDesc)
    {
    }

    ~SwapChainBase()
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_SwapChain, TObjectBase )

    virtual const SwapChainDesc& GetDesc()const
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
            return true;
        }

        return false;
    }
    
    /// Strong reference to the render device
    Diligent::RefCntAutoPtr<IRenderDevice> m_pRenderDevice;
    
    /// Weak references to the immediate device context. The context holds
    /// the strong reference to the swap chain.
    Diligent::RefCntWeakPtr<IDeviceContext> m_wpDeviceContext;

    /// Swap chain description
    SwapChainDesc m_SwapChainDesc;

private:
    SwapChainBase( const SwapChainBase& );
    SwapChainBase( SwapChainBase&& );
    const SwapChainBase& operator = ( const SwapChainBase& );
    const SwapChainBase& operator = ( SwapChainBase&& );
};

}
