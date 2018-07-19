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

namespace Diligent
{

/// Describes input vertex stream
struct VertexStreamInfo
{
    /// Strong reference to the buffer object
    RefCntAutoPtr<IBuffer> pBuffer;
    Uint32 Offset = 0; ///< Offset in bytes
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
    inline virtual void SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pOffsets, Uint32 Flags )override = 0;

    inline virtual void InvalidateState()override = 0;

    /// Base implementation of IDeviceContext::SetPipelineState(); caches references to the pipeline state object.
    inline virtual void SetPipelineState(IPipelineState *pPipelineState)override = 0;

    /// Base implementation of IDeviceContext::CommitShaderResources(); validates parameters.
    template<typename PSOImplType>
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

    inline void ResetRenderTargets();

    bool IsDeferred()const{return m_bIsDeferred;}

protected:
    inline bool SetBlendFactors(const float *BlendFactors, int Dummy);

    inline bool SetStencilRef(Uint32 StencilRef, int Dummy);

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
    Viewport m_Viewports[MaxViewports];
    /// Number of current viewports
    Uint32 m_NumViewports = 0;

	/// Current scissor rects
    Rect m_ScissorRects[MaxViewports];
    /// Number of current scissor rects
    Uint32 m_NumScissorRects = 0;

    /// Vector of strong references to the bound render targets
    RefCntAutoPtr<ITextureView> m_pBoundRenderTargets[MaxRenderTargets];
    /// Number of bound render targets
    Uint32 m_NumBoundRenderTargets = 0;
    /// Width of the currently bound framebuffer
    Uint32 m_FramebufferWidth = 0;
    /// Height of the currently bound framebuffer
    Uint32 m_FramebufferHeight = 0;
    /// Number of array slices in the currently bound framebuffer
    Uint32 m_FramebufferSlices = 0;
    /// Flag indicating if default render target & depth-stencil 
    /// buffer are currently bound
    bool m_IsDefaultFramebufferBound = false;

    /// Strong references to the bound depth stencil view
    RefCntAutoPtr<ITextureView> m_pBoundDepthStencil;

    const bool m_bIsDeferred = false;
};


template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pOffsets, Uint32 Flags  )
{
#ifdef DEVELOPMENT
    if ( StartSlot >= MaxBufferSlots )
    {
        LOG_ERROR_MESSAGE( "Start vertex buffer slot ", StartSlot, " is out of allowed range [0, ", MaxBufferSlots-1, "]." );
        return;
    }

    if ( StartSlot + NumBuffersSet > MaxBufferSlots )
    {
        LOG_ERROR_MESSAGE( "The range of vertex buffer slots being set [", StartSlot, ", ", StartSlot + NumBuffersSet - 1, "] is out of allowed range  [0, ", MaxBufferSlots - 1, "]." );
        NumBuffersSet = MaxBufferSlots - StartSlot;
    }
#endif

    if ( Flags & SET_VERTEX_BUFFERS_FLAG_RESET )
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
        CurrStream.Offset = pOffsets ? pOffsets[Buff] : 0;
#ifdef DEVELOPMENT
        if ( CurrStream.pBuffer )
        {
            const auto &BuffDesc = CurrStream.pBuffer->GetDesc();
            if ( !(BuffDesc.BindFlags & BIND_VERTEX_BUFFER) )
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
template<typename PSOImplType>
inline bool DeviceContextBase<BaseInterface> :: CommitShaderResources(IShaderResourceBinding *pShaderResourceBinding, Uint32 Flags, int)
{
#ifdef DEVELOPMENT
    if (!m_pPipelineState)
    {
        LOG_ERROR_MESSAGE("No pipeline state is bound to the pipeline");
        return false;
    }

    if (pShaderResourceBinding)
    {
        auto *pPSOImpl = m_pPipelineState.RawPtr<PSOImplType>();
        if (pPSOImpl->IsIncompatibleWith(pShaderResourceBinding->GetPipelineState()))
        {
            LOG_ERROR_MESSAGE("Shader resource binding object is not compatible with the currently bound pipeline state");
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
#ifdef DEVELOPMENT
    const auto &BuffDesc = m_pIndexBuffer->GetDesc();
    if ( !(BuffDesc.BindFlags & BIND_INDEX_BUFFER) )
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
    if (m_pPipelineState)
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
        if ( m_BlendFactors[f] != BlendFactors[f] )
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
inline void DeviceContextBase<BaseInterface> :: SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 &RTWidth, Uint32 &RTHeight )
{
    if ( RTWidth == 0 || RTHeight == 0 )
    {
        RTWidth = m_FramebufferWidth;
        RTHeight = m_FramebufferHeight;
    }

    VERIFY(NumViewports < MaxViewports, "Number of viewports (", NumViewports, ") exceeds the limit (", MaxViewports, ")");
    m_NumViewports = std::min(MaxViewports, NumViewports);
    
    Viewport DefaultVP( 0, 0, static_cast<float>(RTWidth), static_cast<float>(RTHeight) );
    // If no viewports are specified, use default viewport
    if ( m_NumViewports == 1 && pViewports == nullptr )
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
    if ( pViewports )
    {
        for( Uint32 vp = 0; vp < m_NumViewports; ++vp )
            pViewports[vp] = m_Viewports[vp];
    }
}

template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 &RTWidth, Uint32 &RTHeight )
{
    if ( RTWidth == 0 || RTHeight == 0 )
    {
        RTWidth = m_FramebufferWidth;
        RTHeight = m_FramebufferHeight;
    }

    VERIFY(NumRects < MaxViewports, "Number of scissor rects (", NumRects, ") exceeds the limit (", MaxViewports, ")");
    m_NumScissorRects = std::min(MaxViewports, NumRects);

    for( Uint32 sr = 0; sr < m_NumScissorRects; ++sr )
    {
        m_ScissorRects[sr] = pRects[sr];
        VERIFY( m_ScissorRects[sr].left <= m_ScissorRects[sr].right,  "Incorrect horizontal bounds for a scissor rect [", m_ScissorRects[sr].left, ", ", m_ScissorRects[sr].right,  ")" );
        VERIFY( m_ScissorRects[sr].top  <= m_ScissorRects[sr].bottom, "Incorrect vertical bounds for a scissor rect [",   m_ScissorRects[sr].top,  ", ", m_ScissorRects[sr].bottom, ")" );
    }
}

template<typename BaseInterface>
inline bool DeviceContextBase<BaseInterface> :: SetRenderTargets( Uint32 NumRenderTargets, ITextureView* ppRenderTargets[], ITextureView* pDepthStencil, Uint32 Dummy )
{
    bool bBindRenderTargets = false;
    m_FramebufferWidth  = 0;
    m_FramebufferHeight = 0;
    m_FramebufferSlices = 0;

    ITextureView* pDefaultRTV = nullptr;
    bool IsDefaultFrambuffer = NumRenderTargets == 0 && pDepthStencil == nullptr;
    bBindRenderTargets = (m_IsDefaultFramebufferBound != IsDefaultFrambuffer);
    m_IsDefaultFramebufferBound = IsDefaultFrambuffer;
    if (m_IsDefaultFramebufferBound)
    {
        VERIFY(m_pSwapChain, "Swap chain is not initialized in the device context");

        NumRenderTargets = 1;
        pDefaultRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        ppRenderTargets = &pDefaultRTV;
        pDepthStencil = m_pSwapChain->GetDepthBufferDSV();

        const auto &SwapChainDesc = m_pSwapChain->GetDesc();
        m_FramebufferWidth  = SwapChainDesc.Width;
        m_FramebufferHeight = SwapChainDesc.Height;
        m_FramebufferSlices = 1;
    }

    if ( NumRenderTargets != m_NumBoundRenderTargets )
    {
        bBindRenderTargets = true;
        for(Uint32 rt = NumRenderTargets; rt < m_NumBoundRenderTargets; ++rt )
            m_pBoundRenderTargets[rt].Release();

        m_NumBoundRenderTargets = NumRenderTargets;
    }

    for( Uint32 rt = 0; rt < NumRenderTargets; ++rt )
    {
        auto *pRTView = ppRenderTargets[rt];
        if ( pRTView )
        {
            const auto &RTVDesc = pRTView->GetDesc();
#ifdef DEVELOPMENT
            if (RTVDesc.ViewType != TEXTURE_VIEW_RENDER_TARGET)
                LOG_ERROR("Texture view object named \"", RTVDesc.Name ? RTVDesc.Name : "", "\" has incorrect view type (", GetTexViewTypeLiteralName(RTVDesc.ViewType), "). Render target view is expected" );
#endif
            // Use this RTV to set the render target size
            if (m_FramebufferWidth == 0)
            {
                auto *pTex = pRTView->GetTexture();
                const auto &TexDesc = pTex->GetDesc();
                m_FramebufferWidth  = std::max(TexDesc.Width  >> RTVDesc.MostDetailedMip, 1U);
                m_FramebufferHeight = std::max(TexDesc.Height >> RTVDesc.MostDetailedMip, 1U);
                m_FramebufferSlices = RTVDesc.NumArraySlices;
            }
            else
            {
#ifdef DEVELOPMENT
                const auto &TexDesc = pRTView->GetTexture()->GetDesc();
                if (m_FramebufferWidth != std::max(TexDesc.Width  >> RTVDesc.MostDetailedMip, 1U))
                    LOG_ERROR("Render target width (", std::max(TexDesc.Width  >> RTVDesc.MostDetailedMip, 1U), ") specified by RTV '", RTVDesc.Name, "' is inconsistent with the width of previously bound render targets (", m_FramebufferWidth, ")");
                if (m_FramebufferHeight != std::max(TexDesc.Height >> RTVDesc.MostDetailedMip, 1U))
                    LOG_ERROR("Render target height (", std::max(TexDesc.Height >> RTVDesc.MostDetailedMip, 1U), ") specified by RTV '", RTVDesc.Name, "' is inconsistent with the height of previously bound render targets (", m_FramebufferHeight, ")");
                if (m_FramebufferSlices != RTVDesc.NumArraySlices)
                    LOG_ERROR("Number of slices (", RTVDesc.NumArraySlices, ") specified by RTV '", RTVDesc.Name, "' is inconsistent with the number of slices in previously bound render targets (", m_FramebufferSlices, ")");
#endif
            }
        }

        // Here both views are certainly live objects, since we store
        // strong references to all bound render targets. So we
        // can safely compare pointers.
        if ( m_pBoundRenderTargets[rt] != pRTView )
        {
            m_pBoundRenderTargets[rt] = pRTView;
            bBindRenderTargets = true;
        }
    }

    if ( pDepthStencil )
    {
        const auto &DSVDesc = pDepthStencil->GetDesc();
#ifdef DEVELOPMENT
        if (DSVDesc.ViewType != TEXTURE_VIEW_DEPTH_STENCIL)
            LOG_ERROR("Texture view object named \"", DSVDesc.Name ? DSVDesc.Name : "", "\" has incorrect view type (", GetTexViewTypeLiteralName(DSVDesc.ViewType), "). Depth stencil view is expected" );
#endif

        // Use depth stencil size to set render target size
        if (m_FramebufferWidth == 0)
        {
            auto *pTex = pDepthStencil->GetTexture();
            const auto &TexDesc = pTex->GetDesc();
            m_FramebufferWidth  = std::max(TexDesc.Width  >> DSVDesc.MostDetailedMip, 1U);
            m_FramebufferHeight = std::max(TexDesc.Height >> DSVDesc.MostDetailedMip, 1U);
            m_FramebufferSlices = DSVDesc.NumArraySlices;
        }
        else
        {
#ifdef DEVELOPMENT
            const auto &TexDesc = pDepthStencil->GetTexture()->GetDesc();
            if (m_FramebufferWidth  != std::max(TexDesc.Width  >> DSVDesc.MostDetailedMip, 1U))
                LOG_ERROR("Depth-stencil target width (", std::max(TexDesc.Width >> DSVDesc.MostDetailedMip, 1U), ") specified by DSV '", DSVDesc.Name, "' is inconsistent with the width of previously bound render targets (", m_FramebufferWidth, ")");
            if (m_FramebufferHeight != std::max(TexDesc.Height >> DSVDesc.MostDetailedMip, 1U))
                LOG_ERROR("Depth-stencil target height (", std::max(TexDesc.Height >> DSVDesc.MostDetailedMip, 1U), ") specified by DSV '", DSVDesc.Name, "' is inconsistent with the height of previously bound render targets (", m_FramebufferHeight, ")");
            if (m_FramebufferSlices != DSVDesc.NumArraySlices)
                LOG_ERROR("Number of slices (", DSVDesc.NumArraySlices, ") specified by DSV '", DSVDesc.Name, "' is inconsistent with the number of slices in previously bound render targets (", m_FramebufferSlices, ")");
#endif
        }
    }

    if ( m_pBoundDepthStencil != pDepthStencil)
    {
        m_pBoundDepthStencil = pDepthStencil;
        bBindRenderTargets = true;
    }


    VERIFY_EXPR(m_FramebufferWidth > 0 && m_FramebufferHeight > 0 && m_FramebufferSlices > 0);

    return bBindRenderTargets;
}

template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: GetRenderTargets( Uint32 &NumRenderTargets, ITextureView **ppRTVs, ITextureView **ppDSV )
{
    NumRenderTargets = m_NumBoundRenderTargets;

    if ( ppRTVs )
    {
        for( Uint32 rt = 0; rt < NumRenderTargets; ++rt )
        {
            VERIFY( ppRTVs[rt] == nullptr, "Non-null pointer found in RTV array element #", rt );
            auto pBoundRTV = m_pBoundRenderTargets[rt];
            if ( pBoundRTV )
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

    if ( ppDSV )
    {
        VERIFY( *ppDSV == nullptr, "Non-null DSV pointer found" );
        if ( m_pBoundDepthStencil )
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

    ResetRenderTargets();
}

template<typename BaseInterface>
inline void DeviceContextBase<BaseInterface> :: ResetRenderTargets()
{
    for (Uint32 rt = 0; rt < m_NumBoundRenderTargets; ++rt)
        m_pBoundRenderTargets[rt].Release();
#ifdef _DEBUG
    for (Uint32 rt = m_NumBoundRenderTargets; rt < _countof(m_pBoundRenderTargets); ++rt)
    {
        VERIFY(m_pBoundRenderTargets[rt] == nullptr, "Non-null render target found");
    }
#endif
    m_NumBoundRenderTargets = 0;
    m_FramebufferWidth = 0;
    m_FramebufferHeight = 0;
    m_FramebufferSlices = 0;
    m_IsDefaultFramebufferBound = false;

    m_pBoundDepthStencil.Release();
}

}
