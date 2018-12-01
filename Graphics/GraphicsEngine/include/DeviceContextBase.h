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
#include "TextureBase.h"

namespace Diligent
{

/// Describes input vertex stream
template<typename BufferImplType>
struct VertexStreamInfo
{
    VertexStreamInfo(){}

    /// Strong reference to the buffer object
    RefCntAutoPtr<BufferImplType> pBuffer;
    Uint32 Offset = 0; ///< Offset in bytes
};

/// Base implementation of the device context.

/// \tparam BaseInterface         - base interface that this class will inheret.
/// \tparam BufferImplType        - buffer implemenation type (BufferD3D11Impl, BufferD3D12Impl, etc.)
/// \tparam TextureViewImplType   - texture view implemenation type (TextureViewD3D11Impl, TextureViewD3D12Impl, etc.)
/// \tparam PipelineStateImplType - pipeline state implementation type (PipelineStateD3D11Impl, PipelineStateD3D12Impl, etc.)
/// \remarks Device context keeps strong references to all objects currently bound to 
///          the pipeline: buffers, states, samplers, shaders, etc.
///          The context also keeps strong references to the device and
///          the swap chain.
template<typename BaseInterface, 
         typename BufferImplType,
         typename TextureViewImplType,
         typename PipelineStateImplType>
class DeviceContextBase : public ObjectBase<BaseInterface>
{
public:
    using TObjectBase = ObjectBase<BaseInterface>;

    /// \param pRefCounters - reference counters object that controls the lifetime of this device context.
    /// \param pRenderDevice - render device.
    /// \param bIsDeferred - flag indicating if this instance is a deferred context
    DeviceContextBase(IReferenceCounters* pRefCounters, IRenderDevice* pRenderDevice, bool bIsDeferred) :
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
    inline virtual void SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer** ppBuffers, Uint32* pOffsets, SET_VERTEX_BUFFERS_FLAGS Flags )override = 0;

    inline virtual void InvalidateState()override = 0;

    /// Base implementation of IDeviceContext::CommitShaderResources(); validates parameters.
    inline bool CommitShaderResources(IShaderResourceBinding* pShaderResourceBinding, COMMIT_SHADER_RESOURCES_FLAGS Flags, int);

    /// Base implementation of IDeviceContext::SetIndexBuffer(); caches the strong reference to the index buffer
    inline virtual void SetIndexBuffer( IBuffer* pIndexBuffer, Uint32 ByteOffset )override = 0;

    /// Caches the viewports
    inline void SetViewports( Uint32 NumViewports, const Viewport* pViewports, Uint32& RTWidth, Uint32& RTHeight );

    /// Caches the scissor rects
    inline void SetScissorRects( Uint32 NumRects, const Rect* pRects, Uint32& RTWidth, Uint32& RTHeight );

    /// Caches the render target and depth stencil views. Returns true if any view is different
    /// from the cached value and false otherwise.
    inline bool SetRenderTargets( Uint32 NumRenderTargets, ITextureView* ppRenderTargets[], ITextureView* pDepthStencil);

    /// Base implementation of IDeviceContext::UpdateBuffer(); validates input parameters.
    virtual void UpdateBuffer(IBuffer* pBuffer, Uint32 Offset, Uint32 Size, const PVoid pData)override = 0;

    /// Base implementation of IDeviceContext::CopyBuffer(); validates input parameters.
    virtual void CopyBuffer(IBuffer *pSrcBuffer, Uint32 SrcOffset, IBuffer *pDstBuffer, Uint32 DstOffset, Uint32 Size)override = 0;

    /// Base implementation of IDeviceContext::MapBuffer(); validates input parameters.
    virtual void MapBuffer(IBuffer* pBuffer, MAP_TYPE MapType, MAP_FLAGS MapFlags, PVoid& pMappedData)override = 0;

    /// Base implementation of IDeviceContext::UnmapBuffer()
    virtual void UnmapBuffer(IBuffer* pBuffer)override = 0;

    /// Base implementaiton of IDeviceContext::UpdateData(); validates input parameters
    virtual void UpdateTexture( ITexture* pTexture, Uint32 MipLevel, Uint32 Slice, const Box& DstBox, const TextureSubResData& SubresData )override = 0;

    /// Base implementaiton of IDeviceContext::CopyTexture(); validates input parameters
    virtual void CopyTexture( ITexture* pSrcTexture,
                              Uint32 SrcMipLevel,
                              Uint32 SrcSlice,
                              const Box *pSrcBox,
                              ITexture* pDstTexture,
                              Uint32 DstMipLevel,
                              Uint32 DstSlice,
                              Uint32 DstX,
                              Uint32 DstY,
                              Uint32 DstZ )override = 0;

    /// Base implementaiton of IDeviceContext::MapTextureSubresource()
    virtual void MapTextureSubresource(ITexture*                 pTexture,
                                       Uint32                    MipLevel,
                                       Uint32                    ArraySlice,
                                       MAP_TYPE                  MapType,
                                       MAP_FLAGS                 MapFlags,
                                       const Box*                pMapRegion,
                                       MappedTextureSubresource& MappedData)override = 0;

    /// Base implementaiton of IDeviceContext::UnmapTextureSubresource()
    virtual void UnmapTextureSubresource(ITexture* pTexture, Uint32 MipLevel, Uint32 ArraySlice)override = 0;

    virtual void GenerateMips(ITextureView* pTexView)override = 0;

    /// Sets the strong pointer to the swap chain
    virtual void SetSwapChain( ISwapChain* pSwapChain )override final { m_pSwapChain = pSwapChain; }

    /// Returns the swap chain
    ISwapChain *GetSwapChain() { return m_pSwapChain; }

    /// Returns true if currently bound frame buffer is the default frame buffer
    inline bool IsDefaultFBBound(){ return m_IsDefaultFramebufferBound; }

    /// Returns currently bound pipeline state and blend factors
    inline void GetPipelineState(IPipelineState** ppPSO, float* BlendFactors, Uint32& StencilRef);

    /// Returns currently bound render targets
    inline void GetRenderTargets(Uint32& NumRenderTargets, ITextureView** ppRTVs, ITextureView** ppDSV);

    /// Returns currently set viewports
    inline void GetViewports( Uint32& NumViewports, Viewport* pViewports );

    /// Returns the render device
    IRenderDevice *GetDevice(){return m_pDevice;}

    inline void ResetRenderTargets();

    bool IsDeferred()const{return m_bIsDeferred;}

protected:
    inline bool SetBlendFactors(const float* BlendFactors, int Dummy);

    inline bool SetStencilRef(Uint32 StencilRef, int Dummy);

    inline void SetPipelineState(PipelineStateImplType* pPipelineState, int /*Dummy*/);

    /// Clears all cached resources
    inline void ClearStateCache();

#ifdef DEVELOPMENT
    bool DvpVerifyDrawArguments(const DrawAttribs& drawAttribs);
    bool DvpVerifyDispatchArguments(const DispatchComputeAttribs &DispatchAttrs);
    void DvpVerifyStateTransitionDesc(const StateTransitionDesc& Barrier);
#endif

    /// Strong reference to the device.
    RefCntAutoPtr<IRenderDevice> m_pDevice;
    
    /// Strong reference to the swap chain. Swap chain holds
    /// weak reference to the immediate context.
    RefCntAutoPtr<ISwapChain> m_pSwapChain;

    /// Vertex streams. Every stream holds strong reference to the buffer
    VertexStreamInfo<BufferImplType> m_VertexStreams[MaxBufferSlots];
    
    /// Number of bound vertex streams
    Uint32 m_NumVertexStreams = 0;

    /// Strong reference to the bound pipeline state object.
    /// Use final PSO implementation type to avoid virtual calls to AddRef()/Release().
    /// We need to keep strong reference as we examine previous pipeline state in
    /// SetPipelineState()
    RefCntAutoPtr<PipelineStateImplType> m_pPipelineState;

    /// Strong reference to the bound index buffer.
    /// Use final buffer implementation type to avoid virtual calls to AddRef()/Release()
    RefCntAutoPtr<BufferImplType> m_pIndexBuffer;

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

    /// Vector of strong references to the bound render targets.
    /// Use final texture view implementation type to avoid virtual calls to AddRef()/Release()
    RefCntAutoPtr<TextureViewImplType> m_pBoundRenderTargets[MaxRenderTargets];
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

    /// Strong references to the bound depth stencil view.
    /// Use final texture view implementation type to avoid virtual calls to AddRef()/Release()
    RefCntAutoPtr<TextureViewImplType> m_pBoundDepthStencil;

    const bool m_bIsDeferred = false;
};


template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: 
            SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer** ppBuffers, Uint32* pOffsets, SET_VERTEX_BUFFERS_FLAGS Flags  )
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
        // Reset only these buffer slots that are not being set.
        // It is very important to not reset buffers that stay unchanged
        // as AddRef()/Release() are not free
        for (Uint32 s = 0; s < StartSlot; ++s)
            m_VertexStreams[s] = VertexStreamInfo<BufferImplType>{};
        for (Uint32 s = StartSlot + NumBuffersSet; s < m_NumVertexStreams; ++s)
            m_VertexStreams[s] = VertexStreamInfo<BufferImplType>{};
        m_NumVertexStreams = 0;
    }
    m_NumVertexStreams = std::max(m_NumVertexStreams, StartSlot + NumBuffersSet );
    
    for( Uint32 Buff = 0; Buff < NumBuffersSet; ++Buff )
    {
        auto &CurrStream = m_VertexStreams[StartSlot + Buff];
        CurrStream.pBuffer = ppBuffers ? ValidatedCast<BufferImplType>(ppBuffers[Buff]) : nullptr;
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
        m_VertexStreams[m_NumVertexStreams--] = VertexStreamInfo<BufferImplType>{};
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: SetPipelineState(PipelineStateImplType* pPipelineState, int /*Dummy*/)
{
    m_pPipelineState = pPipelineState;
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline bool DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: 
            CommitShaderResources(IShaderResourceBinding* pShaderResourceBinding, COMMIT_SHADER_RESOURCES_FLAGS Flags, int)
{
#ifdef DEVELOPMENT
    if (!m_pPipelineState)
    {
        LOG_ERROR_MESSAGE("No pipeline state is bound to the pipeline");
        return false;
    }

    if (pShaderResourceBinding)
    {
        if (m_pPipelineState->IsIncompatibleWith(pShaderResourceBinding->GetPipelineState()))
        {
            LOG_ERROR_MESSAGE("Shader resource binding object is not compatible with the currently bound pipeline state");
            return false;
        }
    }
#endif
    return true;
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: InvalidateState()
{
    DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: ClearStateCache();
    m_IsDefaultFramebufferBound = false;
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: SetIndexBuffer( IBuffer* pIndexBuffer, Uint32 ByteOffset )
{
    m_pIndexBuffer = ValidatedCast<BufferImplType>(pIndexBuffer);
    m_IndexDataStartOffset = ByteOffset;
#ifdef DEVELOPMENT
    const auto& BuffDesc = m_pIndexBuffer->GetDesc();
    if ( !(BuffDesc.BindFlags & BIND_INDEX_BUFFER) )
    {
        LOG_ERROR_MESSAGE( "Buffer \"", BuffDesc.Name ? BuffDesc.Name : "", "\" being bound as index buffer was not created with BIND_INDEX_BUFFER flag" );
    }
#endif
}


template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: GetPipelineState(IPipelineState** ppPSO, float* BlendFactors, Uint32& StencilRef)
{ 
    VERIFY( ppPSO != nullptr, "Null pointer provided null" );
    VERIFY(* ppPSO == nullptr, "Memory address contains a pointer to a non-null blend state" );
    if (m_pPipelineState)
    {
        m_pPipelineState->QueryInterface( IID_PipelineState, reinterpret_cast<IObject**>( ppPSO ) );
    }
    else
    {
       * ppPSO = nullptr;
    }

    for( Uint32 f = 0; f < 4; ++f )
        BlendFactors[f] = m_BlendFactors[f];
    StencilRef = m_StencilRef;
};

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline bool DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::SetBlendFactors(const float* BlendFactors, int)
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

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline bool DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: SetStencilRef(Uint32 StencilRef, int)
{
    if (m_StencilRef != StencilRef)
    {
        m_StencilRef = StencilRef;
        return true;
    }
    return false;
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: SetViewports( Uint32 NumViewports, const Viewport* pViewports, Uint32& RTWidth, Uint32& RTHeight )
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

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: GetViewports( Uint32 &NumViewports, Viewport* pViewports )
{
    NumViewports = m_NumViewports;
    if ( pViewports )
    {
        for( Uint32 vp = 0; vp < m_NumViewports; ++vp )
            pViewports[vp] = m_Viewports[vp];
    }
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: SetScissorRects( Uint32 NumRects, const Rect* pRects, Uint32& RTWidth, Uint32& RTHeight )
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

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline bool DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::
            SetRenderTargets( Uint32 NumRenderTargets, ITextureView* ppRenderTargets[], ITextureView* pDepthStencil )
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
        auto* pRTView = ppRenderTargets[rt];
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
                auto* pTex = pRTView->GetTexture();
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
            m_pBoundRenderTargets[rt] = ValidatedCast<TextureViewImplType>(pRTView);
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
            auto* pTex = pDepthStencil->GetTexture();
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
        m_pBoundDepthStencil = ValidatedCast<TextureViewImplType>(pDepthStencil);
        bBindRenderTargets = true;
    }


    VERIFY_EXPR(m_FramebufferWidth > 0 && m_FramebufferHeight > 0 && m_FramebufferSlices > 0);

    return bBindRenderTargets;
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: GetRenderTargets( Uint32 &NumRenderTargets, ITextureView** ppRTVs, ITextureView** ppDSV )
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
        VERIFY(* ppDSV == nullptr, "Non-null DSV pointer found" );
        if ( m_pBoundDepthStencil )
            m_pBoundDepthStencil->QueryInterface( IID_TextureView, reinterpret_cast<IObject**>(ppDSV) );
        else
           * ppDSV = nullptr;
    }
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: ClearStateCache()
{
    for(Uint32 stream=0; stream < m_NumVertexStreams; ++stream)
        m_VertexStreams[stream] = VertexStreamInfo<BufferImplType>{};
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

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: ResetRenderTargets()
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


template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::
            UpdateBuffer(IBuffer* pBuffer, Uint32 Offset, Uint32 Size, const PVoid pData)
{
    VERIFY(pBuffer != nullptr, "Buffer must not be null");
    const auto& BuffDesc = ValidatedCast<BufferImplType>(pBuffer)->GetDesc();
    DEV_CHECK_ERR(BuffDesc.Usage == USAGE_DEFAULT, "Unable to update buffer '", BuffDesc.Name, "': only USAGE_DEFAULT buffers can be updated with UpdateData()");
    DEV_CHECK_ERR(Offset < BuffDesc.uiSizeInBytes, "Unable to update buffer '", BuffDesc.Name, "': offset (", Offset, ") exceeds the buffer size (", BuffDesc.uiSizeInBytes, ")" );
    DEV_CHECK_ERR(Size + Offset <= BuffDesc.uiSizeInBytes, "Unable to update buffer '", BuffDesc.Name, "': Update region [", Offset, ",", Size + Offset, ") is out of buffer bounds [0,", BuffDesc.uiSizeInBytes, ")" );
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::
            CopyBuffer(IBuffer *pSrcBuffer, Uint32 SrcOffset, IBuffer *pDstBuffer, Uint32 DstOffset, Uint32 Size)
{
    VERIFY(pSrcBuffer != nullptr, "Source buffer must not be null");
    VERIFY(pDstBuffer != nullptr, "Destination buffer must not be null");
    const auto& SrcBufferDesc = ValidatedCast<BufferImplType>(pSrcBuffer)->GetDesc();
    const auto& DstBufferDesc = ValidatedCast<BufferImplType>(pDstBuffer)->GetDesc();
    DEV_CHECK_ERR( DstOffset + Size <= DstBufferDesc.uiSizeInBytes, "Failed to copy buffer '", SrcBufferDesc.Name, "' to '", DstBufferDesc.Name, "': Destination range [", DstOffset, ",", DstOffset + Size, ") is out of buffer bounds [0,", DstBufferDesc.uiSizeInBytes, ")" );
    DEV_CHECK_ERR( SrcOffset + Size <= SrcBufferDesc.uiSizeInBytes, "Failed to copy buffer '", SrcBufferDesc.Name, "' to '", DstBufferDesc.Name, "': Source range [", SrcOffset, ",", SrcOffset + Size, ") is out of buffer bounds [0,", SrcBufferDesc.uiSizeInBytes, ")" );
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::
            MapBuffer( IBuffer* pBuffer, MAP_TYPE MapType, MAP_FLAGS MapFlags, PVoid& pMappedData )
{
    VERIFY(pBuffer, "pBuffer must not be null");
    
    const auto& BuffDesc = pBuffer->GetDesc();

    pMappedData = nullptr;
    switch( MapType )
    {
    case MAP_READ:
        DEV_CHECK_ERR( BuffDesc.Usage == USAGE_CPU_ACCESSIBLE,      "Only buffers with usage USAGE_CPU_ACCESSIBLE can be read from" );
        DEV_CHECK_ERR( (BuffDesc.CPUAccessFlags & CPU_ACCESS_READ), "Buffer being mapped for reading was not created with CPU_ACCESS_READ flag" );
        DEV_CHECK_ERR( (MapFlags & MAP_FLAG_DISCARD) == 0, "MAP_FLAG_DISCARD is not valid when mapping buffer for reading" );
        break;

    case MAP_WRITE:
        DEV_CHECK_ERR( BuffDesc.Usage == USAGE_DYNAMIC || BuffDesc.Usage == USAGE_CPU_ACCESSIBLE, "Only buffers with usage USAGE_CPU_ACCESSIBLE or USAGE_DYNAMIC can be mapped for writing" );
        DEV_CHECK_ERR( (BuffDesc.CPUAccessFlags & CPU_ACCESS_WRITE), "Buffer being mapped for writing was not created with CPU_ACCESS_WRITE flag" );
        break;

    case MAP_READ_WRITE:
        DEV_CHECK_ERR( BuffDesc.Usage == USAGE_CPU_ACCESSIBLE,       "Only buffers with usage USAGE_CPU_ACCESSIBLE can be mapped for reading and writing" );
        DEV_CHECK_ERR( (BuffDesc.CPUAccessFlags & CPU_ACCESS_WRITE), "Buffer being mapped for reading & writing was not created with CPU_ACCESS_WRITE flag" );
        DEV_CHECK_ERR( (BuffDesc.CPUAccessFlags & CPU_ACCESS_READ),  "Buffer being mapped for reading & writing was not created with CPU_ACCESS_READ flag" );
        DEV_CHECK_ERR( (MapFlags & MAP_FLAG_DISCARD) == 0, "MAP_FLAG_DISCARD is not valid when mapping buffer for reading and writing" );
        break;

    default: UNEXPECTED( "Unknown map type" );
    }
    
    if (BuffDesc.Usage == USAGE_DYNAMIC)
    {
        DEV_CHECK_ERR((MapFlags & (MAP_FLAG_DISCARD | MAP_FLAG_DO_NOT_SYNCHRONIZE)) != 0 && MapType == MAP_WRITE, "Dynamic buffers can only be mapped for writing with MAP_FLAG_DISCARD or MAP_FLAG_DO_NOT_SYNCHRONIZE flag");
        DEV_CHECK_ERR((MapFlags & (MAP_FLAG_DISCARD | MAP_FLAG_DO_NOT_SYNCHRONIZE)) != (MAP_FLAG_DISCARD | MAP_FLAG_DO_NOT_SYNCHRONIZE), "When mapping dynamic buffer, only one of MAP_FLAG_DISCARD or MAP_FLAG_DO_NOT_SYNCHRONIZE flags must be specified");
    }

    if ( (MapFlags & MAP_FLAG_DISCARD) != 0 )
    {
        DEV_CHECK_ERR( BuffDesc.Usage == USAGE_DYNAMIC || BuffDesc.Usage == USAGE_CPU_ACCESSIBLE, "Only dynamic and staging buffers can be mapped with discard flag" );
        DEV_CHECK_ERR( MapType == MAP_WRITE, "MAP_FLAG_DISCARD is only valid when mapping buffer for writing" );
    }
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::
            UnmapBuffer(IBuffer* pBuffer)
{
    VERIFY(pBuffer, "pBuffer must not be null");
}


template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> :: 
            UpdateTexture(ITexture* pTexture, Uint32 MipLevel, Uint32 Slice, const Box& DstBox, const TextureSubResData& SubresData)
{
    VERIFY( pTexture != nullptr, "pTexture must not be null" );
    ValidateUpdateTextureParams( pTexture->GetDesc(), MipLevel, Slice, DstBox, SubresData );
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::
            CopyTexture( ITexture*  pSrcTexture,
                         Uint32     SrcMipLevel,
                         Uint32     SrcSlice,
                         const Box* pSrcBox,
                         ITexture*  pDstTexture,
                         Uint32     DstMipLevel,
                         Uint32     DstSlice,
                         Uint32     DstX,
                         Uint32     DstY,
                         Uint32     DstZ )
{
    VERIFY( pSrcTexture, "pSrcTexture must not be null" );
    VERIFY( pDstTexture, "pSrcTexture must not be null" );
    ValidateCopyTextureParams( pSrcTexture->GetDesc(), SrcMipLevel, SrcSlice, pSrcBox,
                               pDstTexture->GetDesc(), DstMipLevel, DstSlice, DstX, DstY, DstZ );
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::
            MapTextureSubresource(ITexture*                 pTexture,
                                  Uint32                    MipLevel,
                                  Uint32                    ArraySlice,
                                  MAP_TYPE                  MapType,
                                  MAP_FLAGS                 MapFlags,
                                  const Box*                pMapRegion,
                                  MappedTextureSubresource& MappedData)
{
    VERIFY(pTexture, "pTexture must not be null");
    ValidateMapTextureParams(pTexture->GetDesc(), MipLevel, ArraySlice, MapType, MapFlags, pMapRegion);
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::
            UnmapTextureSubresource(ITexture* pTexture, Uint32 MipLevel, Uint32 ArraySlice)
{
    VERIFY(pTexture, "pTexture must not be null");
    DEV_CHECK_ERR(MipLevel < pTexture->GetDesc().MipLevels, "Mip level is out of range");
    DEV_CHECK_ERR(ArraySlice < pTexture->GetDesc().ArraySize, "Array slice is out of range");
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::
            GenerateMips(ITextureView* pTexView)
{
    VERIFY(pTexView != nullptr, "pTexView must not be null");
#ifdef DEVELOPMENT
    const auto& ViewDesc = pTexView->GetDesc();
    DEV_CHECK_ERR( ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE, "GenerateMips() is allowed for shader resource views only, ", GetTexViewTypeLiteralName(ViewDesc.ViewType), " is not allowed." );
#endif
}

#ifdef DEVELOPMENT
template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline bool DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::
            DvpVerifyDrawArguments(const DrawAttribs& drawAttribs)
{
    if (!m_pPipelineState)
    {
        LOG_ERROR("No pipeline state is bound for a draw command");
        return false;
    }

    if (m_pPipelineState->GetDesc().IsComputePipeline)
    {
        LOG_ERROR("Pipeline state bound for a draw command is a compute pipeline");
        return false;
    }

    if(drawAttribs.pIndirectDrawAttribs == nullptr)
    {
        if (drawAttribs.NumIndices == 0)
            LOG_WARNING_MESSAGE(drawAttribs.IsIndexed ? "Number of indices to draw is zero" : "Number of vertices to draw is zero");

        if (drawAttribs.NumInstances == 0)
        {
            LOG_ERROR("Number of instances cannot be 0. Use 1 for a non-instanced draw command.");
            return false;
        }
    }

    if (drawAttribs.IsIndexed && drawAttribs.IndexType != VT_UINT16 && drawAttribs.IndexType != VT_UINT32)
    {
        LOG_ERROR("For an indexed draw command IndexType must be VT_UINT16 or VT_UINT32");
        return false;
    }
    
    return true;
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
inline bool DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::
            DvpVerifyDispatchArguments(const DispatchComputeAttribs& DispatchAttrs)
{
    if (!m_pPipelineState)
    {
        LOG_ERROR("No pipeline state is bound for a dispatch command");
        return false;
    }

    if (!m_pPipelineState->GetDesc().IsComputePipeline)
    {
        LOG_ERROR("Pipeline state bound for a draw command is a graphics pipeline");
        return false;
    }

    if(DispatchAttrs.pIndirectDispatchAttribs == nullptr)
    {
        if (DispatchAttrs.ThreadGroupCountX == 0)
            LOG_WARNING_MESSAGE("ThreadGroupCountX is zero");

        if (DispatchAttrs.ThreadGroupCountY == 0)
            LOG_WARNING_MESSAGE("ThreadGroupCountY is zero");

        if (DispatchAttrs.ThreadGroupCountZ == 0)
            LOG_WARNING_MESSAGE("ThreadGroupCountZ is zero");
    }

    return true;
}

template<typename BaseInterface, typename BufferImplType, typename TextureViewImplType, typename PipelineStateImplType>
void DeviceContextBase<BaseInterface, BufferImplType, TextureViewImplType, PipelineStateImplType> ::
     DvpVerifyStateTransitionDesc(const StateTransitionDesc& Barrier)
{
    DEV_CHECK_ERR((Barrier.pTexture != nullptr) ^ (Barrier.pBuffer != nullptr), "Exactly one of pTexture or pBuffer members of StateTransitionDesc must not be null");
    DEV_CHECK_ERR(Barrier.NewState != RESOURCE_STATE_UNKNOWN, "New resource state can't be unknown");
    if (Barrier.pTexture)
    {
        const auto& TexDesc = Barrier.pTexture->GetDesc();
        
        DEV_CHECK_ERR(VerifyResourceStates(Barrier.NewState, true), "Invlaid new state specified for texture '", TexDesc.Name, "'");
        auto OldState = Barrier.OldState != RESOURCE_STATE_UNKNOWN ? Barrier.OldState : Barrier.pTexture->GetState();
        DEV_CHECK_ERR(OldState != RESOURCE_STATE_UNKNOWN, "The state of texture '", TexDesc.Name, "' is unknown to the engine and is not explicitly specified in the barrier");
        DEV_CHECK_ERR(VerifyResourceStates(OldState, true), "Invlaid old state specified for texture '", TexDesc.Name, "'");

        DEV_CHECK_ERR(Barrier.FirstMipLevel < TexDesc.MipLevels, "First mip level (", Barrier.FirstMipLevel, ") specified by the barrier is "
                      "out of range. Texture '", TexDesc.Name, "' has only ", TexDesc.MipLevels, " mip level(s)");
        DEV_CHECK_ERR(Barrier.MipLevelsCount == StateTransitionDesc::RemainingMipLevels || Barrier.FirstMipLevel + Barrier.MipLevelsCount < TexDesc.MipLevels,
                      "Mip level range ", Barrier.FirstMipLevel, "..", Barrier.FirstMipLevel+Barrier.MipLevelsCount-1, " "
                      "specified by the barrier is out of range. Texture '", TexDesc.Name, "' has only ", TexDesc.MipLevels, " mip level(s)");

        DEV_CHECK_ERR(Barrier.FirstArraySlice < TexDesc.ArraySize, "First array slice (", Barrier.FirstArraySlice, ") specified by the barrier is "
                      "out of range. Array size of texture '", TexDesc.Name, "' is ", TexDesc.ArraySize);
        DEV_CHECK_ERR(Barrier.ArraySliceCount == StateTransitionDesc::RemainingArraySlices || Barrier.FirstArraySlice + Barrier.ArraySliceCount < TexDesc.ArraySize,
                      "Array slice range ", Barrier.FirstArraySlice, "..", Barrier.FirstArraySlice+Barrier.ArraySliceCount-1, " "
                      "specified by the barrier is out of range. Array size of texture '", TexDesc.Name, "' is ", TexDesc.ArraySize);
        
        auto DevType = m_pDevice->GetDeviceCaps().DevType;
        if (DevType != DeviceType::D3D12 && DevType != DeviceType::Vulkan)
        {
            DEV_CHECK_ERR(Barrier.FirstMipLevel == 0 && (Barrier.MipLevelsCount == StateTransitionDesc::RemainingMipLevels || Barrier.MipLevelsCount == TexDesc.MipLevels),
                          "Failed to transition texture '", TexDesc.Name, "': only whole resources can be transitioned on this device");
            DEV_CHECK_ERR(Barrier.FirstArraySlice == 0 && (Barrier.ArraySliceCount == StateTransitionDesc::RemainingArraySlices || Barrier.ArraySliceCount == TexDesc.ArraySize),
                          "Failed to transition texture '", TexDesc.Name, "': only whole resources can be transitioned on this device");
        }
    }
    else
    {
        const auto& BuffDesc = Barrier.pBuffer->GetDesc();
        DEV_CHECK_ERR(VerifyResourceStates(Barrier.NewState, false), "Invlaid new state specified for buffer '", BuffDesc.Name, "'");
        auto OldState = Barrier.OldState != RESOURCE_STATE_UNKNOWN ? Barrier.OldState : Barrier.pBuffer->GetState();
        DEV_CHECK_ERR(OldState != RESOURCE_STATE_UNKNOWN, "The state of buffer '", BuffDesc.Name, "' is unknown to the engine and is not explicitly specified in the barrier");
        DEV_CHECK_ERR(VerifyResourceStates(OldState, false), "Invlaid old state specified for buffer '", BuffDesc.Name, "'");
    }
}

#endif // DEVELOPMENT


}
