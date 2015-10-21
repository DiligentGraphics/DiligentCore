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
#include "GraphicsUtilities.h"

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
    Uint32 Stride; ///< Stride
    Uint32 Offset; ///< Offset
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
template<typename BaseInterface = IDeviceContext>
class DeviceContextBase : public ObjectBase<BaseInterface>
{
public:
    DeviceContextBase(IRenderDevice *pRenderDevice) :
        m_pDevice(pRenderDevice),
        m_IndexDataStartOffset( 0 ),
        m_StencilRef( 0 ),
        m_SamplesBlendMask( 0 )
    {
        for( int i = 0; i < 4; ++i )
            m_BlendFactors[i] = -1;
        m_VertexStreams.reserve( MaxBufferSlots );
        m_Viewports.reserve( 16 );
        m_ScissorRects.reserve( 16 );
        // Set dummy render target array size to make sure that
        // render targets are actually bound the first time 
        // SetRenderTargets() is called 
        m_pBoundRenderTargets.resize( 8 );
    }

    ~DeviceContextBase()
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_DeviceContext, ObjectBase<BaseInterface> )

    virtual void SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags ) = 0;

    virtual void ClearState() = 0;

    virtual void SetShaders( IShader **ppShaders, Uint32 NumShadersToSet )
    {
        m_pBoundShaders.resize( NumShadersToSet );
        for( Uint32 i = 0; i < NumShadersToSet; ++i )
            m_pBoundShaders[i] = ppShaders[i];
#ifdef DEBUG_CHECKS
        {
            Uint32 BoundShaders = 0;
            for( auto sh = m_pBoundShaders.begin(); sh != m_pBoundShaders.end(); ++sh )
            {
                auto ShaderType = (*sh)->GetDesc().ShaderType;
                if( BoundShaders & ShaderType )
                {
                    LOG_ERROR_MESSAGE( "More than one shader of type ", GetShaderTypeLiteralName( ShaderType ), " is being set to the pipeline" );
                    BoundShaders |= ShaderType;
                }
            }
        }
#endif
    }

    virtual void BindShaderResources( IResourceMapping *pResourceMapping, Uint32 Flags )
    {
    }

    // Body of the pure virtual function cannot be inlined
    /// Caches the strong reference to the vertex description
    virtual void SetVertexDescription( IVertexDescription *pVertexDesc ) = 0;
    
    /// Caches the strong reference to the index buffer
    virtual void SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset ) = 0;

    /// If the new depth stencil parameters differ from the cached values, 
    /// updates the values and returns true. Otherwise returns false.
    bool SetDepthStencilState( IDepthStencilState *pDSState, Uint32 StencilRef, Uint32 Dummy = 0);


    /// If the new rasterizer state differs from the cached state, 
    /// updates the cached state and returns true. Otherwise returns false.
    bool SetRasterizerState( IRasterizerState *pRS, Uint32 Dummy = 0 );


    /// If the new blend parameters differ from the cached values, 
    /// updates the cached values and returns true. Otherwise returns false.
    bool SetBlendState( IBlendState *pBS, const float* pBlendFactors, Uint32 SampleMask, Uint32 Dummy = 0 );
    
    /// Caches the viewports
    void SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 &RTWidth, Uint32 &RTHeight );

    /// Caches the scissor rects
    void SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 &RTWidth, Uint32 &RTHeight );

    /// Caches the render target and depth stencil views. Returns true if any view is different
    /// from the cached value and false otherwise.
    bool SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil, Uint32 Dummy = 0 );

    /// Sets the strong pointer to the swap chain
    void SetSwapChain( ISwapChain *pSwapChain ) { m_pSwapChain = pSwapChain; }

    bool IsDefaultFBBound(){ return m_pBoundRenderTargets.size() == 0 && m_pBoundDepthStencil == nullptr; }

    void GetDepthStencilState(IDepthStencilState **ppDSS, Uint32 &StencilRef);

    void GetRasterizerState( IRasterizerState **ppRS );

    void GetBlendState( IBlendState **ppBS, float* BlendFactors, Uint32& SamplesBlendMask );

    void GetRenderTargets(Uint32 &NumRenderTargets, ITextureView **ppRTVs, ITextureView **ppDSV);

    /// Creates and binds default depth stencil, rasterizer & blend states 
    void CreateDefaultStates();

    void GetShaders( IShader **ppShaders, Uint32 &NumShaders );

    void GetViewports( Uint32 &NumViewports, Viewport *pViewports );

protected:
    /// Returns the size of the currently bound render target
    void GetRenderTargetSize( Uint32 &RTWidth, Uint32 &RTHeight );

    /// Strong reference to the device.
    RefCntAutoPtr<IRenderDevice> m_pDevice;
    
    /// Strong reference to the swap chain. Swap chain holds
    /// weak reference to the immediate context.
    RefCntAutoPtr<ISwapChain> m_pSwapChain;

    /// Vector of strong references to bound shaders
    std::vector< RefCntAutoPtr<IShader> > m_pBoundShaders;
    
    /// Vertex streams. Every stream holds strong reference to the buffer
    std::vector<VertexStreamInfo> m_VertexStreams;

    /// Strong reference to the bound vertex description object
    RefCntAutoPtr<IVertexDescription> m_pVertexDesc;

    /// Strong reference to the bound index buffer
    RefCntAutoPtr<IBuffer> m_pIndexBuffer;

    Uint32 m_IndexDataStartOffset;

    /// Strong references to the bound depth-stencil state
    RefCntAutoPtr< IDepthStencilState > m_pDSState;

    /// Strong references to the bound rasterizer state
    RefCntAutoPtr< IRasterizerState> m_pRasterizerState;

    /// Strong references to the bound blend state
    RefCntAutoPtr< IBlendState> m_pBlendState;

	/// Current stencil reference value
    Uint32 m_StencilRef;

	/// Curent blend factors
    Float32 m_BlendFactors[4];

	/// Current samples blend mask 
    Uint32 m_SamplesBlendMask;

	/// Current viewports
    std::vector<Viewport> m_Viewports;

	/// Current scissor rects
    std::vector<Rect> m_ScissorRects;

    /// Vector of strong references to bound render targets
    std::vector< RefCntAutoPtr<ITextureView> > m_pBoundRenderTargets;

    /// Strong references to the bound depth stencil view
    RefCntAutoPtr<ITextureView> m_pBoundDepthStencil;

private:
    RefCntAutoPtr<IDepthStencilState> m_pDefaultDSS;
    RefCntAutoPtr<IRasterizerState> m_pDefaultRS;
    RefCntAutoPtr<IBlendState> m_pDefaultBS;
};


template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags  )
{
    if( StartSlot >= MaxBufferSlots )
    {
        LOG_ERROR_MESSAGE( "Start vertex buffer slot ", StartSlot, " is out of allowed range [0, ", MaxBufferSlots-1, "]." )
        return;
    }

    if( StartSlot + NumBuffersSet > MaxBufferSlots )
    {
        LOG_ERROR_MESSAGE( "The range of vertex buffer slots being set [", StartSlot, ", ", StartSlot + NumBuffersSet - 1, "] is out of allowed range  [0, ", MaxBufferSlots - 1, "]." )
        NumBuffersSet = MaxBufferSlots - StartSlot;
    }

    if( Flags & SET_VERTEX_BUFFERS_FLAG_RESET )
    {
        m_VertexStreams.clear();
    }
    m_VertexStreams.resize( std::max(m_VertexStreams.size(), static_cast<size_t>(StartSlot + NumBuffersSet) ) );
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
    while(m_VertexStreams.size() > 0 && !m_VertexStreams.back().pBuffer)
        m_VertexStreams.pop_back();
}

template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: ClearState()
{
    UNSUPPORTED("This function is not implemented")
    //m_VertexStreams.clear();
    //m_pIndexBuffer.Release();
    //m_IndexDataStartOffset = 0;
}

template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: SetVertexDescription( IVertexDescription *pVertexDesc )
{
    m_pVertexDesc = pVertexDesc;
}

template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset )
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


/// \param [in] pDSState - pointer to the new depth-stencil state interface.
/// \param [in] StencilRef - new stencil reference value.
/// \param [in] Dummy - dummy argument required to make function signature distinct
///                     from IDeviceContext::SetDepthStencilState(). This is required
///                     because otherwise this overloaded function will only differ in 
///                     the return type which is not allowed.
/// \return 
/// - True if either new depth stencil state or stencil reference value differs from
///   currently cached values.
/// - False otherwise.
/// 
/// \remarks The method caches *strong reference* to the provided interface in m_pDSState.
template<typename BaseInterface>
bool DeviceContextBase<BaseInterface> :: SetDepthStencilState( IDepthStencilState *pDSState, Uint32 StencilRef, Uint32 Dummy )
{
    // If null depth-stencil state is provided, bind default state
    if( !pDSState )
        pDSState = m_pDefaultDSS;

    // Here m_pDSState is certainly a live object because we keep the
    // strong reference. pDSState is also clearly live object, so we can 
    // safely compare pointers.
    if( m_pDSState != pDSState || m_StencilRef != StencilRef )
    {
        m_pDSState = pDSState;
        m_StencilRef = StencilRef;
        return true;
    }

    return false;
}

/// \param [in] pRS - pointer to the new rasterizer state interface.
/// \param [in] Dummy - dummy argument required to make function signature distinct
///                     from IDeviceContext::SetRasterizerState(). This is required
///                     because otherwise this overloaded function will only differ in 
///                     the return type which is not allowed.
/// \return 
/// - True if the new rasterizer state differ from the cached state.
/// - False otherwise.
///
/// \remarks The method caches *strong reference* to the provided interface in m_pRasterizerState.
template<typename BaseInterface>
bool DeviceContextBase<BaseInterface> :: SetRasterizerState( IRasterizerState *pRS, Uint32 Dummy )
{
    // If null rasterizer state provided, bind default state
    if( !pRS )
        pRS = m_pDefaultRS;

    // Here m_pRasterizerState is certainly a live object because we keep the
    // strong reference. pRS is also clearly live object, so we can 
    // safely compare pointers.
    if( m_pRasterizerState != pRS )
    {
        m_pRasterizerState = pRS;
        return true;
    }

    return false;
}


/// \param [in] pBS - pointer to the new blend state interface.
/// \param [in] pBlendFactors - new blend factors.
/// \param [in] SampleMask - new sample mask.
/// \param [in] Dummy - dummy argument required to make function signature distinct
///                     from IDeviceContext::SetBlendState(). This is required
///                     because otherwise this overloaded function will only differ in 
///                     the return type which is not allowed.
/// \return 
/// - True if either the new blend state, blend factors or sample mask value differs from
///   the currently cached values.
/// - False otherwise.
///
/// \remarks The method caches *strong reference* to the provided interface in m_pBlendState.
template<typename BaseInterface>
bool DeviceContextBase<BaseInterface> :: SetBlendState( IBlendState *pBS, const float* pBlendFactors, Uint32 SampleMask, Uint32 Dummy )
{
    // If null blend state provided, bind default state
    if( !pBS )
        pBS = m_pDefaultBS;

    static const float DefaultBlendFactors[4] = { 1, 1, 1, 1 };
    if( pBlendFactors == nullptr )
        pBlendFactors = DefaultBlendFactors;

    // Here m_pBlendState is certainly a live object because we keep the
    // strong reference. pBS is also clearly live object, so we can 
    // safely compare pointers.
    if( m_pBlendState != pBS ||
        m_BlendFactors[0] != pBlendFactors[0] ||
        m_BlendFactors[1] != pBlendFactors[1] ||
        m_BlendFactors[2] != pBlendFactors[2] ||
        m_BlendFactors[3] != pBlendFactors[3] ||
        m_SamplesBlendMask != SampleMask )
    {
        m_pBlendState = pBS;
        m_SamplesBlendMask = SampleMask;
        for( int i = 0; i < 4; ++i )
            m_BlendFactors[i] = pBlendFactors[i];

        return true;
    }

    return false;
}

template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: GetDepthStencilState(IDepthStencilState **ppDSS, Uint32 &StencilRef)
{ 
    VERIFY( ppDSS != nullptr, "Null pointer provided null" );
    VERIFY( *ppDSS == nullptr, "Memory address contains a pointer to a non-null depth-stencil state" );
    VERIFY( m_pDSState, "No depth-stencil state is bound. At least default state must always be bound. Did you forget to call CreateDefaultStates()?" );
    m_pDSState->QueryInterface( IID_DepthStencilState, reinterpret_cast<IObject**>( ppDSS ) );
    StencilRef = m_StencilRef;
};

template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: GetRasterizerState(IRasterizerState **ppRS )
{
    VERIFY( ppRS != nullptr, "Null pointer provided null" );
    VERIFY( *ppRS == nullptr, "Memory address contains a pointer to a non-null rasterizer state" );
    VERIFY( m_pRasterizerState, "No rasterizer state is bound. At least default state must always be bound. Did you forget to call CreateDefaultStates()?" );
    m_pRasterizerState->QueryInterface( IID_RasterizerState, reinterpret_cast<IObject**>( ppRS ) );
}

template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: GetBlendState( IBlendState **ppBS, float* BlendFactors, Uint32& SamplesBlendMask )
{ 
    VERIFY( ppBS != nullptr, "Null pointer provided null" );
    VERIFY( *ppBS == nullptr, "Memory address contains a pointer to a non-null blend state" );
    VERIFY( m_pBlendState, "No blend state is bound. At least default state must always be bound. Did you forget to call CreateDefaultStates()?" );
    m_pBlendState->QueryInterface( IID_BlendState, reinterpret_cast<IObject**>( ppBS ) );
    for( Uint32 f = 0; f < 4; ++f )
        BlendFactors[f] = m_BlendFactors[f];
    SamplesBlendMask = m_SamplesBlendMask;
}

template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: GetRenderTargetSize( Uint32 &RTWidth, Uint32 &RTHeight )
{
    RTWidth  = 0;
    RTHeight = 0;
    // First, try to find non-null render target 
    for( Uint32 rt = 0; rt < m_pBoundRenderTargets.size(); ++rt )
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
        const auto &SwapChainDesc = m_pSwapChain->GetDesc();
        RTWidth  = SwapChainDesc.Width;
        RTHeight = SwapChainDesc.Height;
    }
}

template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 &RTWidth, Uint32 &RTHeight )
{
    if( RTWidth == 0 || RTHeight == 0 )
    {
        GetRenderTargetSize( RTWidth, RTHeight );
    }

    m_Viewports.resize(NumViewports);
    Viewport DefaultVP( 0, 0, static_cast<float>(RTWidth), static_cast<float>(RTHeight) );
    // If no viewports are specified, use default viewport
    if( NumViewports == 1 && pViewports == nullptr )
    {
        pViewports = &DefaultVP;
    }

    for( Uint32 vp = 0; vp < NumViewports; ++vp )
    {
        m_Viewports[vp] = pViewports[vp];
        VERIFY( m_Viewports[vp].Width  >= 0,  "Incorrect viewport width (",  m_Viewports[vp].Width,  ")" );
        VERIFY( m_Viewports[vp].Height >= 0 , "Incorrect viewport height (", m_Viewports[vp].Height, ")" );
        VERIFY( m_Viewports[vp].MaxDepth >= m_Viewports[vp].MinDepth, "Incorrect viewport depth range [", m_Viewports[vp].MinDepth, ", ", m_Viewports[vp].MaxDepth, "]" );
    }
}

template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: GetViewports( Uint32 &NumViewports, Viewport *pViewports )
{
    NumViewports = static_cast<Uint32>( m_Viewports.size() );
    if( pViewports )
    {
        for( Uint32 vp = 0; vp < m_Viewports.size(); ++vp )
            pViewports[vp] = m_Viewports[vp];
    }
}

template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 &RTWidth, Uint32 &RTHeight )
{
    if( RTWidth == 0 || RTHeight == 0 )
    {
        GetRenderTargetSize( RTWidth, RTHeight );
    }

    m_ScissorRects.resize( NumRects );
    for( Uint32 sr = 0; sr < NumRects; ++sr )
    {
        m_ScissorRects[sr] = pRects[sr];
        VERIFY( m_ScissorRects[sr].left <= m_ScissorRects[sr].right,  "Incorrect horizontal bounds for a scissor rect [", m_ScissorRects[sr].left, ", ", m_ScissorRects[sr].right,  ")" );
        VERIFY( m_ScissorRects[sr].top  <= m_ScissorRects[sr].bottom, "Incorrect vertical bounds for a scissor rect [",   m_ScissorRects[sr].top,  ", ", m_ScissorRects[sr].bottom, ")" );
    }
}

template<typename BaseInterface>
bool DeviceContextBase<BaseInterface> :: SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil, Uint32 Dummy )
{
    bool bBindRenderTargets = false;
    if( NumRenderTargets != m_pBoundRenderTargets.size() )
    {
        bBindRenderTargets = true;
        m_pBoundRenderTargets.resize(NumRenderTargets);
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

    return bBindRenderTargets;
}

template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: GetRenderTargets( Uint32 &NumRenderTargets, ITextureView **ppRTVs, ITextureView **ppDSV )
{
    NumRenderTargets = static_cast<Uint32>( m_pBoundRenderTargets.size() );

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
void DeviceContextBase<BaseInterface> :: CreateDefaultStates()
{
    DepthStencilStateDesc DefaultDSSDesc;
    DefaultDSSDesc.Name = "Default depth stencil state";
    m_pDevice->CreateDepthStencilState( DefaultDSSDesc, &m_pDefaultDSS );
    
    RasterizerStateDesc DefaultRSDesc;
    DefaultRSDesc.Name = "Default rasterizer state";
    m_pDevice->CreateRasterizerState( DefaultRSDesc, &m_pDefaultRS );

    BlendStateDesc DefaultBlendState;
    DefaultBlendState.Name = "Default blend state";
    m_pDevice->CreateBlendState( DefaultBlendState, &m_pDefaultBS );

    SetDepthStencilState( m_pDefaultDSS, 0 );
    SetRasterizerState( m_pDefaultRS );
    float BlendFactors[4] = { 1, 1, 1, 1 };
    SetBlendState( m_pDefaultBS, BlendFactors, 0xFFFFFFFF );
}

template<typename BaseInterface>
void DeviceContextBase<BaseInterface> :: GetShaders( IShader **ppShaders, Uint32 &NumShaders )
{
    NumShaders = static_cast<Uint32>( m_pBoundShaders.size() );
    if( ppShaders == nullptr )
        return;
    for( Uint32 s = 0; s < NumShaders; ++s )
    { 
        VERIFY( ppShaders[s] == nullptr, "Non-null pointer found in shader array element #", s );
        if( auto pBoundShader = m_pBoundShaders[s] )
            pBoundShader->QueryInterface( IID_Shader, reinterpret_cast<IObject**>( ppShaders + s ) );
        else
            ppShaders[s] = nullptr;
    }
}

}
