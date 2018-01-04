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
/// Implementation of the Diligent::DeviceContextBase template class and related structures

#include "DeviceContext.h"
#include "DeviceObjectBase.h"
#include "Defines.h"
#include "ResourceMapping.h"
#include "Sampler.h"
#include "ObjectBase.h"
#include "DebugUtilities.h"
#include "SwapChain.h"
#include "ValidatedCast.h"
#include "GraphicsAccessories.h"

#ifdef _DEBUG
#   define DEBUG_CHECKS
#endif

namespace Diligent
{

/// Describes input vertex stream
struct VertexStreamInfo
{
    /// Strong reference to the buffer object
    RefCntAutoPtr<IBuffer> pBuffer;
    Uint32 Stride; ///< Stride in bytes
    Uint32 Offset; ///< Offset in bytes
    VertexStreamInfo() :
        Stride( 0 ),
        Offset( 0 )
    {}
};

/// Base implementation of the device context.

/// \tparam BaseInterface - base interface that this class will inheret.
/// \remarks Device context keeps strong references to all objects currently bound to 
///          the pipeline: buffers, states, samplers, shaders, etc.
///          The context also keeps strong references to the device and
///          the swap chain.
template<typename BaseInterface>
class DeviceContextBase : public ObjectBase<BaseInterface>
{
public:

    typedef ObjectBase<BaseInterface> TObjectBase;

    /// \param pRefCounters - reference counters object that controls the lifetime of this device context.
    /// \param pRenderDevice - render device.
    /// \param bIsDeferred - flag indicating if this instance is a deferred context
    DeviceContextBase(IReferenceCounters *pRefCounters, IRenderDevice *pRenderDevice, bool bIsDeferred) :
        TObjectBase(pRefCounters),
        m_pDevice(pRenderDevice),
        m_bIsDeferred(bIsDeferred)
    {
    }

    ~DeviceContextBase()
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_DeviceContext, TObjectBase )

    /// Base implementation of IDeviceContext::SetVertexBuffers(); validates parameters and 
    /// caches references to the buffers.
    inline virtual void SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags )override = 0;

    inline virtual void InvalidateState()override = 0;

    /// Base implementation of IDeviceContext::SetPipelineState(); caches references to the pipeline state object.
    inline virtual void SetPipelineState(IPipelineState *pPipelineState)override = 0;

    /// Base implementation of IDeviceContext::CommitShaderResources(); validates parameters.
    inline bool CommitShaderResources(IShaderResourceBinding *pShaderResourceBinding, Uint32 Flags, int);

    /// Base implementation of IDeviceContext::SetIndexBuffer(); caches the strong reference to the index buffer
    inline virtual void SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset )override = 0;

    /// Caches the viewports
    inline void SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 &RTWidth, Uint32 &RTHeight );

    /// Caches the scissor rects
    inline void SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 &RTWidth, Uint32 &RTHeight );

    /// Caches the render target and depth stencil views. Returns true if any view is different
    /// from the cached value and false otherwise.
    inline bool SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil, Uint32 Dummy = 0 );

    /// Sets the strong pointer to the swap chain
    virtual void SetSwapChain( ISwapChain *pSwapChain )override final { m_pSwapChain = pSwapChain; }

    /// Returns the swap chain
    ISwapChain *GetSwapChain() { return m_pSwapChain; }

    /// Returns true if currently bound frame buffer is the default frame buffer
    inline bool IsDefaultFBBound(){ return m_IsDefaultFramebufferBound; }

    /// Returns currently bound pipeline state and blend factors
    inline void GetPipelineState(IPipelineState **ppPSO, float* BlendFactors, Uint32 &StencilRef);

    /// Returns currently bound render targets
    inline void GetRenderTargets(Uint32 &NumRenderTargets, ITextureView **ppRTVs, ITextureView **ppDSV);

    /// Returns currently set viewports
    inline void GetViewports( Uint32 &NumViewports, Viewport *pViewports );

    /// Returns the render device
    IRenderDevice *GetDevice(){return m_pDevice;}

protected:
    inline bool SetBlendFactors(const float *BlendFactors, int Dummy);

    inline bool SetStencilRef(Uint32 StencilRef, int Dummy);

    /// Returns the size of the currently bound render target
    inline void GetRenderTargetSize( Uint32 &RTWidth, Uint32 &RTHeight );

    /// Clears all cached resources
    inline void ClearStateCache();

    /// Strong reference to the device.
    RefCntAutoPtr<IRenderDevice> m_pDevice;
    
    /// Strong reference to the swap chain. Swap chain holds
    /// weak reference to the immediate context.
    RefCntAutoPtr<ISwapChain> m_pSwapChain;

    /// Vertex streams. Every stream holds strong reference to the buffer
    VertexStreamInfo m_VertexStreams[MaxBufferSlots];
    
    /// Number of bound vertex streams
    Uint32 m_NumVertexStreams = 0;

    /// Strong reference to the bound pipeline state object
    RefCntAutoPtr<IPipelineState> m_pPipelineState;

    /// Strong reference to the bound index buffer
    RefCntAutoPtr<IBuffer> m_pIndexBuffer;

    /// Offset from the beginning of the index buffer to the start of the index data, in bytes.
    Uint32 m_IndexDataStartOffset = 0;

	/// Current stencil reference value
    Uint32 m_StencilRef = 0;

	/// Curent blend factors
    Float32 m_BlendFactors[4] = { -1, -1, -1, -1 };

	/// Current viewports
    Viewport m_Viewports[MaxRenderTargets];
    /// Number of current viewports
    Uint32 m_NumViewports = 0;

	/// Current scissor rects
    Rect m_ScissorRects[MaxRenderTargets];
    /// Number of current scissor rects
    Uint32 m_NumScissorRects = 0;

    /// Vector of strong references to the bound render targets
    RefCntAutoPtr<ITextureView> m_pBoundRenderTargets[MaxRenderTargets];
    /// Number of bound render targets
    Uint32 m_NumBoundRenderTargets = 0;
    /// Flag indicating if default render target & depth-stencil 
    /// buffer are currently bound
    bool m_IsDefaultFramebufferBound = false;

    /// Strong references to the bound depth stencil view
    RefCntAutoPtr<ITextureView> m_pBoundDepthStencil;

    const bool m_bIsDeferred = false;
};


template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags  )
{
    if( StartSlot >= MaxBufferSlots )
    {
        LOG_ERROR_MESSAGE( "Start vertex buffer slot ", StartSlot, " is out of allowed range [0, ", MaxBufferSlots-1, "]." );
        return;
    }

    if( StartSlot + NumBuffersSet > MaxBufferSlots )
    {
        LOG_ERROR_MESSAGE( "The range of vertex buffer slots being set [", StartSlot, ", ", StartSlot + NumBuffersSet - 1, "] is out of allowed range  [0, ", MaxBufferSlots - 1, "]." );
        NumBuffersSet = MaxBufferSlots - StartSlot;
    }

    if( Flags & SET_VERTEX_BUFFERS_FLAG_RESET )
    {
        for(Uint32 s=0; s < m_NumVertexStreams; ++s)
            m_VertexStreams[s] = VertexStreamInfo();
        m_NumVertexStreams = 0;
    }
    m_NumVertexStreams = std::max(m_NumVertexStreams, StartSlot + NumBuffersSet );
    
    for( Uint32 Buff = 0; Buff < NumBuffersSet; ++Buff )
    {
        auto &CurrStream = m_VertexStreams[StartSlot + Buff];
        CurrStream.pBuffer = RefCntAutoPtr<IBuffer>( ppBuffers ? ppBuffers[Buff] : nullptr );
        CurrStream.Stride = pStrides ? pStrides[Buff] : 0;
        CurrStream.Offset = pOffsets ? pOffsets[Buff] : 0;
#ifdef DEBUG_CHECKS
        if( CurrStream.pBuffer )
        {
            const auto &BuffDesc = CurrStream.pBuffer->GetDesc();
            if( !(BuffDesc.BindFlags & BIND_VERTEX_BUFFER) )
            {
                LOG_ERROR_MESSAGE( "Buffer \"", BuffDesc.Name ? BuffDesc.Name : "", "\" being bound as vertex buffer to slot ", Buff," was not created with BIND_VERTEX_BUFFER flag" );
            }
        }
#endif
    }
    // Remove null buffers from the end of the array
    while(m_NumVertexStreams > 0 && !m_VertexStreams[m_NumVertexStreams-1].pBuffer)
        m_VertexStreams[m_NumVertexStreams--] = VertexStreamInfo();
}

template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: SetPipelineState(IPipelineState *pPipelineState)
{
    m_pPipelineState = pPipelineState;
}

template<typename BaseInterface>
inline bool DeviceContextBase<BaseInterface> :: CommitShaderResources(IShaderResourceBinding *pShaderResourceBinding, Uint32 Flags, int)
{
#ifdef _DEBUG
    if (!m_pPipelineState)
    {
        LOG_ERROR_MESSAGE("No pipeline state is bound to the pipeline");
        return false;
    }

    if (pShaderResourceBinding)
    {
        auto *pPSO = pShaderResourceBinding->GetPipelineState();
        if (pPSO != m_pPipelineState)
        {
            LOG_ERROR_MESSAGE("Shader resource binding object does not match currently bound pipeline state");
            return false;
        }
    }
#endif
    return true;
}

template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: InvalidateState()
{
    DeviceContextBase<BaseInterface> :: ClearStateCache();
    m_IsDefaultFramebufferBound = false;
}

template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset )
{
    m_pIndexBuffer = pIndexBuffer;
    m_IndexDataStartOffset = ByteOffset;
#ifdef DEBUG_CHECKS
    const auto &BuffDesc = m_pIndexBuffer->GetDesc();
    if( !(BuffDesc.BindFlags & BIND_INDEX_BUFFER) )
    {
        LOG_ERROR_MESSAGE( "Buffer \"", BuffDesc.Name ? BuffDesc.Name : "", "\" being bound as index buffer was not created with BIND_INDEX_BUFFER flag" );
    }
#endif
}


template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: GetPipelineState(IPipelineState **ppPSO, float* BlendFactors, Uint32 &StencilRef)
{ 
    VERIFY( ppPSO != nullptr, "Null pointer provided null" );
    VERIFY( *ppPSO == nullptr, "Memory address contains a pointer to a non-null blend state" );
    if(m_pPipelineState)
    {
        m_pPipelineState->QueryInterface( IID_PipelineState, reinterpret_cast<IObject**>( ppPSO ) );
    }
    else
    {
        *ppPSO = nullptr;
    }

    for( Uint32 f = 0; f < 4; ++f )
        BlendFactors[f] = m_BlendFactors[f];
    StencilRef = m_StencilRef;
};

template<typename BaseInterface>
inline bool DeviceContextBase<BaseInterface> ::SetBlendFactors(const float *BlendFactors, int)
{
    bool FactorsDiffer = false;
    for( Uint32 f = 0; f < 4; ++f )
    {
        if( m_BlendFactors[f] != BlendFactors[f] )
            FactorsDiffer = true;
        m_BlendFactors[f] = BlendFactors[f];
    }
    return FactorsDiffer;
}

template<typename BaseInterface>
inline bool DeviceContextBase<BaseInterface> :: SetStencilRef(Uint32 StencilRef, int)
{
    if (m_StencilRef != StencilRef)
    {
        m_StencilRef = StencilRef;
        return true;
    }
    return false;
}

template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: GetRenderTargetSize( Uint32 &RTWidth, Uint32 &RTHeight )
{
    RTWidth  = 0;
    RTHeight = 0;
    // First, try to find non-null render target 
    for( Uint32 rt = 0; rt < m_NumBoundRenderTargets; ++rt )
    {
        if( auto *pRTView = m_pBoundRenderTargets[rt].RawPtr() )
        {
            // Use render target size as viewport size
            auto *pTex = pRTView->GetTexture();
            auto MipLevel = pRTView->GetDesc().MostDetailedMip;
            const auto &TexDesc = pTex->GetDesc();
            RTWidth  = TexDesc.Width >> MipLevel;
            RTHeight = TexDesc.Height >> MipLevel;
            VERIFY( RTWidth > 0 && RTHeight > 0, "RT dimension is zero" );
            break;
        }
    }

    // If render target was not found, check depth stencil
    if( RTWidth == 0 || RTHeight == 0 )
    {
        auto *pDSView = m_pBoundDepthStencil.RawPtr();
        if( pDSView != nullptr )
        {
            // Use depth stencil size
            auto *pTex = pDSView->GetTexture();
            const auto &TexDesc = pTex->GetDesc();
            RTWidth  = TexDesc.Width;
            RTHeight = TexDesc.Height;
        }
    }

    // Finally, if no render targets and depth stencil are bound,
    // use default RT size
    if( RTWidth == 0 || RTHeight == 0 )
    {
        if (m_pSwapChain)
        {
            const auto &SwapChainDesc = m_pSwapChain->GetDesc();
            RTWidth  = SwapChainDesc.Width;
            RTHeight = SwapChainDesc.Height;
        }
        else
        {
            LOG_ERROR("Failed to determine default render target size: swap chain is not initialized in the device context");
        }
    }
}

template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 &RTWidth, Uint32 &RTHeight )
{
    if( RTWidth == 0 || RTHeight == 0 )
    {
        GetRenderTargetSize( RTWidth, RTHeight );
    }

    VERIFY(NumViewports < MaxRenderTargets, "Num viewports (", NumViewports, ") exceeds the limit (", MaxRenderTargets, ")");
    m_NumViewports = std::min(MaxRenderTargets, NumViewports);
    
    Viewport DefaultVP( 0, 0, static_cast<float>(RTWidth), static_cast<float>(RTHeight) );
    // If no viewports are specified, use default viewport
    if( m_NumViewports == 1 && pViewports == nullptr )
    {
        pViewports = &DefaultVP;
    }

    for( Uint32 vp = 0; vp < m_NumViewports; ++vp )
    {
        m_Viewports[vp] = pViewports[vp];
        VERIFY( m_Viewports[vp].Width  >= 0,  "Incorrect viewport width (",  m_Viewports[vp].Width,  ")" );
        VERIFY( m_Viewports[vp].Height >= 0 , "Incorrect viewport height (", m_Viewports[vp].Height, ")" );
        VERIFY( m_Viewports[vp].MaxDepth >= m_Viewports[vp].MinDepth, "Incorrect viewport depth range [", m_Viewports[vp].MinDepth, ", ", m_Viewports[vp].MaxDepth, "]" );
    }
}

template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: GetViewports( Uint32 &NumViewports, Viewport *pViewports )
{
    NumViewports = m_NumViewports;
    if( pViewports )
    {
        for( Uint32 vp = 0; vp < m_NumViewports; ++vp )
            pViewports[vp] = m_Viewports[vp];
    }
}

template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 &RTWidth, Uint32 &RTHeight )
{
    if( RTWidth == 0 || RTHeight == 0 )
    {
        GetRenderTargetSize( RTWidth, RTHeight );
    }

    VERIFY(NumRects < MaxRenderTargets, "Num scissor rects (", NumRects, ") exceeds the limit (", MaxRenderTargets, ")");
    m_NumScissorRects = std::min(MaxRenderTargets, NumRects);

    for( Uint32 sr = 0; sr < m_NumScissorRects; ++sr )
    {
        m_ScissorRects[sr] = pRects[sr];
        VERIFY( m_ScissorRects[sr].left <= m_ScissorRects[sr].right,  "Incorrect horizontal bounds for a scissor rect [", m_ScissorRects[sr].left, ", ", m_ScissorRects[sr].right,  ")" );
        VERIFY( m_ScissorRects[sr].top  <= m_ScissorRects[sr].bottom, "Incorrect vertical bounds for a scissor rect [",   m_ScissorRects[sr].top,  ", ", m_ScissorRects[sr].bottom, ")" );
    }
}

template<typename BaseInterface>
inline bool DeviceContextBase<BaseInterface> :: SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil, Uint32 Dummy )
{
    bool bBindRenderTargets = false;
    if( NumRenderTargets != m_NumBoundRenderTargets )
    {
        bBindRenderTargets = true;
        for(Uint32 rt = NumRenderTargets; rt < m_NumBoundRenderTargets; ++rt )
            m_pBoundRenderTargets[rt].Release();

        m_NumBoundRenderTargets = NumRenderTargets;
    }

    for( Uint32 rt = 0; rt < NumRenderTargets; ++rt )
    {
        auto *pRTView = ppRenderTargets[rt];
#ifdef _DEBUG
        if( pRTView )
        {
            const auto &ViewDesc = pRTView->GetDesc();
            VERIFY( ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET, "Texture view object named \"", ViewDesc.Name ? ViewDesc.Name : "", "\" has incorrect view type (", GetTexViewTypeLiteralName(ViewDesc.ViewType), "). Render target view is expected" );
        }
#endif
        // Here both views a certainly live object, since we store
        // strong references to all bound render targets. So we
        // can safely compare pointers.
        if( m_pBoundRenderTargets[rt] != pRTView )
        {
            m_pBoundRenderTargets[rt] = pRTView;
            bBindRenderTargets = true;
        }
    }

#ifdef _DEBUG
    if( pDepthStencil )
    {
        const auto &ViewDesc = pDepthStencil->GetDesc();
        VERIFY( ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL, "Texture view object named \"", ViewDesc.Name ? ViewDesc.Name : "", "\" has incorrect view type (", GetTexViewTypeLiteralName(ViewDesc.ViewType), "). Depth stencil view is expected" );
    }
#endif

    if( m_pBoundDepthStencil != pDepthStencil)
    {
        m_pBoundDepthStencil = pDepthStencil;
        bBindRenderTargets = true;
    }

    if (NumRenderTargets == 0 && pDepthStencil == nullptr)
    {
        if (!m_IsDefaultFramebufferBound)
        {
            m_IsDefaultFramebufferBound = true;
            bBindRenderTargets = true;
        }
    }
    else
        m_IsDefaultFramebufferBound = false;

    return bBindRenderTargets;
}

template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: GetRenderTargets( Uint32 &NumRenderTargets, ITextureView **ppRTVs, ITextureView **ppDSV )
{
    NumRenderTargets = m_NumBoundRenderTargets;

    if( ppRTVs )
    {
        for( Uint32 rt = 0; rt < NumRenderTargets; ++rt )
        {
            VERIFY( ppRTVs[rt] == nullptr, "Non-null pointer found in RTV array element #", rt );
            auto pBoundRTV = m_pBoundRenderTargets[rt];
            if( pBoundRTV )
                pBoundRTV->QueryInterface( IID_TextureView, reinterpret_cast<IObject**>(ppRTVs + rt) );
            else
                ppRTVs[rt] = nullptr;
        }
        for( Uint32 rt = NumRenderTargets; rt < MaxRenderTargets; ++rt )
        {
            VERIFY( ppRTVs[rt] == nullptr, "Non-null pointer found in RTV array element #", rt );
            ppRTVs[rt] = nullptr;
        }
    }

    if( ppDSV )
    {
        VERIFY( *ppDSV == nullptr, "Non-null DSV pointer found" );
        if( m_pBoundDepthStencil )
            m_pBoundDepthStencil->QueryInterface( IID_TextureView, reinterpret_cast<IObject**>(ppDSV) );
        else
            *ppDSV = nullptr;
    }
}

template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: ClearStateCache()
{
    for(Uint32 stream=0; stream < m_NumVertexStreams; ++stream)
        m_VertexStreams[stream] = VertexStreamInfo();
#ifdef _DEBUG
    for(Uint32 stream=m_NumVertexStreams; stream < _countof(m_VertexStreams); ++stream)
    {
        VERIFY(m_VertexStreams[stream].pBuffer == nullptr, "Unexpected non-null buffer");
        VERIFY(m_VertexStreams[stream].Offset == 0, "Unexpected non-zero offset");
        VERIFY(m_VertexStreams[stream].Stride == 0, "Unexpected non-zero stride");
    }
#endif
    m_NumVertexStreams = 0;

    m_pPipelineState.Release();

    m_pIndexBuffer.Release();
    m_IndexDataStartOffset = 0;

    m_StencilRef = 0;

    for( int i = 0; i < 4; ++i )
        m_BlendFactors[i] = -1;

	for(Uint32 vp=0; vp < m_NumViewports; ++vp)
        m_Viewports[vp] = Viewport();
    m_NumViewports = 0;

	for(Uint32 sr=0; sr < m_NumScissorRects; ++sr)
        m_ScissorRects[sr] = Rect();
    m_NumScissorRects = 0;

    // Vector of strong references to bound render targets
    for(Uint32 rt=0; rt < m_NumBoundRenderTargets; ++rt )
        m_pBoundRenderTargets[rt].Release();
#ifdef _DEBUG
    for (Uint32 rt = m_NumBoundRenderTargets; rt < _countof(m_pBoundRenderTargets); ++rt)
    {
        VERIFY(m_pBoundRenderTargets[rt] == nullptr, "Non-null render target found");
    }
#endif
    m_NumBoundRenderTargets = 0;

    m_pBoundDepthStencil.Release();
}

}
