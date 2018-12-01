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

#include "DeviceContextGL.h"
#include "DeviceContextBase.h"
#include "BaseInterfacesGL.h"
#include "GLContextState.h"
#include "GLObjectWrapper.h"
#include "BufferGLImpl.h"
#include "TextureViewGLImpl.h"
#include "PipelineStateGLImpl.h"

namespace Diligent
{

/// Implementation of the Diligent::IDeviceContextGL interface
class DeviceContextGLImpl final : public DeviceContextBase<IDeviceContextGL, BufferGLImpl, TextureViewGLImpl, PipelineStateGLImpl>
{
public:
    using TDeviceContextBase = DeviceContextBase<IDeviceContextGL, BufferGLImpl, TextureViewGLImpl, PipelineStateGLImpl>;

    DeviceContextGLImpl( IReferenceCounters *pRefCounters, class RenderDeviceGLImpl *pDeviceGL, bool bIsDeferred );

    /// Queries the specific interface, see IObject::QueryInterface() for details.
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override final;

    virtual void SetPipelineState(IPipelineState *pPipelineState)override final;

    virtual void TransitionShaderResources(IPipelineState *pPipelineState, IShaderResourceBinding *pShaderResourceBinding)override final;

    virtual void CommitShaderResources(IShaderResourceBinding *pShaderResourceBinding, COMMIT_SHADER_RESOURCES_FLAGS Flags)override final;

    virtual void SetStencilRef(Uint32 StencilRef)override final;

    virtual void SetBlendFactors(const float* pBlendFactors = nullptr)override final;

    virtual void SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pOffsets, SET_VERTEX_BUFFERS_FLAGS Flags )override final;
    
    virtual void InvalidateState()override final;

    virtual void SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset )override final;

    virtual void SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 RTWidth, Uint32 RTHeight )override final;

    virtual void SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 RTWidth, Uint32 RTHeight )override final;

    virtual void SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil, SET_RENDER_TARGETS_FLAGS Flags )override final;

    virtual void Draw( DrawAttribs &DrawAttribs )override final;

    virtual void DispatchCompute( const DispatchComputeAttribs &DispatchAttrs )override final;

    virtual void ClearDepthStencil( ITextureView *pView, CLEAR_DEPTH_STENCIL_FLAGS ClearFlags, float fDepth, Uint8 Stencil)override final;

    virtual void ClearRenderTarget( ITextureView *pView, const float *RGBA )override final;

    virtual void Flush()override final;

    virtual void UpdateBuffer(IBuffer *pBuffer, Uint32 Offset, Uint32 Size, const PVoid pData)override final;

    virtual void CopyBuffer(IBuffer *pSrcBuffer, Uint32 SrcOffset, IBuffer *pDstBuffer, Uint32 DstOffset, Uint32 Size)override final;

    virtual void MapBuffer(IBuffer* pBuffer, MAP_TYPE MapType, MAP_FLAGS MapFlags, PVoid& pMappedData)override final;

    virtual void UnmapBuffer(IBuffer* pBuffer)override final;

    virtual void UpdateTexture(ITexture* pTexture, Uint32 MipLevel, Uint32 Slice, const Box& DstBox, const TextureSubResData& SubresData)override final;

    virtual void CopyTexture(ITexture*  pSrcTexture, 
                             Uint32     SrcMipLevel,
                             Uint32     SrcSlice,
                             const Box* pSrcBox,
                             ITexture*  pDstTexture, 
                             Uint32     DstMipLevel,
                             Uint32     DstSlice,
                             Uint32     DstX,
                             Uint32     DstY,
                             Uint32     DstZ)override final;

    virtual void MapTextureSubresource( ITexture*                 pTexture,
                                        Uint32                    MipLevel,
                                        Uint32                    ArraySlice,
                                        MAP_TYPE                  MapType,
                                        MAP_FLAGS                 MapFlags,
                                        const Box*                pMapRegion,
                                        MappedTextureSubresource& MappedData )override final;


    virtual void UnmapTextureSubresource(ITexture* pTexture, Uint32 MipLevel, Uint32 ArraySlice)override final;

    
    virtual void GenerateMips( ITextureView *pTexView )override;


    virtual void FinishFrame()override final;

    virtual void TransitionResourceStates(Uint32 BarrierCount, StateTransitionDesc* pResourceBarriers)override final;

    virtual void FinishCommandList(class ICommandList **ppCommandList)override final;

    virtual void ExecuteCommandList(class ICommandList *pCommandList)override final;

    virtual void SignalFence(IFence* pFence, Uint64 Value)override final;

    virtual bool UpdateCurrentGLContext()override final;

    void BindProgramResources( Uint32 &NewMemoryBarriers, IShaderResourceBinding *pResBinding );

    GLContextState &GetContextState(){return m_ContextState;}
    
    void CommitRenderTargets();

protected:
    friend class BufferGLImpl;
    friend class TextureBaseGL;
    friend class ShaderGLImpl;

    GLContextState m_ContextState;

private:
    Uint32 m_CommitedResourcesTentativeBarriers;

    std::vector<class TextureBaseGL*> m_BoundWritableTextures;
    std::vector<class BufferGLImpl*> m_BoundWritableBuffers;

    bool m_bVAOIsUpToDate = false;
    GLObjectWrappers::GLFrameBufferObj m_DefaultFBO;
};

}
