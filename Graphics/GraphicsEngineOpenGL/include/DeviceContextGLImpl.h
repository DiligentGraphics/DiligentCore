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

#include "DeviceContextGL.h"
#include "DeviceContextBase.h"
#include "BaseInterfacesGL.h"
#include "GLContextState.h"

namespace Diligent
{

/// Implementation of the Diligent::IDeviceContextGL interface
class DeviceContextGLImpl : public DeviceContextBase<IDeviceContextGL>
{
public:
    typedef DeviceContextBase<IDeviceContextGL> TDeviceContextBase;

    DeviceContextGLImpl( class RenderDeviceGLImpl *pDeviceGL );

    /// Queries the specific interface, see IObject::QueryInterface() for details.
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;

    virtual void SetShaders( IShader **ppShaders, Uint32 NumShadersToSet )override;

    virtual void BindShaderResources( IResourceMapping *pResourceMapping, Uint32 Flags )override;

    virtual void SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags )override;
    virtual void ClearState()override;

    virtual void SetVertexDescription( IVertexDescription *pVertexDesc )override;

    virtual void SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset )override;

    virtual void SetDepthStencilState( IDepthStencilState *pDepthStencilState, Uint32 StencilRef = 0 )override;

    virtual void SetRasterizerState( IRasterizerState *pRS )override;

    virtual void SetBlendState( IBlendState *pBS, const float* pBlendFactors, Uint32 SampleMask )override;

    virtual void SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 RTWidth, Uint32 RTHeight )override;

    virtual void SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 RTWidth, Uint32 RTHeight )override;

    virtual void SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil )override;

    virtual void Draw( DrawAttribs &DrawAttribs )override;

    virtual void DispatchCompute( const DispatchComputeAttribs &DispatchAttrs )override;

    virtual void ClearDepthStencil( ITextureView *pView, Uint32 ClearFlags, float fDepth, Uint8 Stencil)override;

    virtual void ClearRenderTarget( ITextureView *pView, const float *RGBA )override;

    virtual void Flush()override;

    void BindProgramResources( Uint32 &NewMemoryBarriers );

    GLContextState &GetContextState(){return m_ContextState;}
    
    void RebindRenderTargets();

protected:
    friend class BufferGLImpl;
    friend class TextureBaseGL;
    friend class VertexDescGLImpl;
    friend class ShaderGLImpl;

    GLContextState m_ContextState;

private:
    std::vector<class TextureBaseGL*> m_BoundWritableTextures;
    std::vector<class BufferGLImpl*> m_BoundWritableBuffers;
};

}
