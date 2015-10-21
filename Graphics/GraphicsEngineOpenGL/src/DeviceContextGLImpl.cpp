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

#include "pch.h"
#include "DeviceContextGLImpl.h"
#include "RenderDeviceGLImpl.h"
#include "GLTypeConversions.h"

#include <iostream>
#include <fstream>
#include <string>

#include "BufferGLImpl.h"
#include "VertexDescGLImpl.h"
#include "ShaderGLImpl.h"
#include "VAOCache.h"
#include "ProgramPipelineCache.h"
#include "Texture1D_OGL.h"
#include "Texture1DArray_OGL.h"
#include "Texture2D_OGL.h"
#include "Texture2DArray_OGL.h"
#include "Texture3D_OGL.h"
#include "SamplerGLImpl.h"
#include "GraphicsUtilities.h"
#include "BufferViewGLImpl.h"
#include "DSStateGLImpl.h"
#include "BlendStateGLImpl.h"
#include "RasterizerStateGLImpl.h"

using namespace std;

namespace Diligent
{
    DeviceContextGLImpl::DeviceContextGLImpl( class RenderDeviceGLImpl *pDeviceGL ) : 
        TDeviceContextBase(pDeviceGL),
        m_ContextState(pDeviceGL)
    {
        m_BoundWritableTextures.reserve( 16 );
        m_BoundWritableBuffers.reserve( 16 );

        // When GL_FRAMEBUFFER_SRGB is enabled, and if the destination image is in the sRGB colorspace
        // then OpenGL will assume the shader's output is in the linear RGB colorspace. It will therefore 
        // convert the output from linear RGB to sRGB.
        // Any writes to images that are not in the sRGB format should not be affected.
        // Thus this setting should be just set once and left that way
        glEnable(GL_FRAMEBUFFER_SRGB);
        if( glGetError() != GL_NO_ERROR )
            LOG_ERROR_MESSAGE("Failed to enable SRGB framebuffers");
    }

    IMPLEMENT_QUERY_INTERFACE( DeviceContextGLImpl, IID_DeviceContextGL, TDeviceContextBase )


    void DeviceContextGLImpl::SetShaders( IShader **ppShaders, Uint32 NumShadersToSet )
    {
        TDeviceContextBase::SetShaders( ppShaders, NumShadersToSet );

    }

    void DeviceContextGLImpl::BindShaderResources( IResourceMapping *pResourceMapping, Uint32 Flags )
    {
        TDeviceContextBase::BindShaderResources( pResourceMapping, Flags );
        if( m_pDevice->GetDeviceCaps().bSeparableProgramSupported )
        {
            for( auto it = m_pBoundShaders.begin(); it != m_pBoundShaders.end(); ++it )
            {
                (*it)->BindResources( pResourceMapping, Flags );
            }
        }
        else
        {
            auto *pRenderDeviceGL = ValidatedCast<RenderDeviceGLImpl>(m_pDevice.RawPtr());
            auto &PipelineCache = pRenderDeviceGL->m_PipelineCache;
            auto &PipelineOrProg = PipelineCache.GetProgramPipeline( m_pBoundShaders.data(), (Uint32)m_pBoundShaders.size() );
            if( PipelineOrProg.Program )
                PipelineOrProg.Program.BindResources( pResourceMapping, Flags );
        }
    }

    void DeviceContextGLImpl::SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags )
    {
        TDeviceContextBase::SetVertexBuffers( StartSlot, NumBuffersSet, ppBuffers, pStrides, pOffsets, Flags );

    }

    void DeviceContextGLImpl::ClearState()
    {
        TDeviceContextBase::ClearState();

    }

    void DeviceContextGLImpl::SetVertexDescription( IVertexDescription *pVertexDesc )
    {
        TDeviceContextBase::SetVertexDescription( pVertexDesc );
    }

    void DeviceContextGLImpl::SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset )
    {
        TDeviceContextBase::SetIndexBuffer( pIndexBuffer, ByteOffset );
    }

    void DeviceContextGLImpl::SetDepthStencilState( IDepthStencilState *pDepthStencilState, Uint32 StencilRef )
    {
        if( TDeviceContextBase::SetDepthStencilState( pDepthStencilState, StencilRef ) )
        {
            CHECK_DYNAMIC_TYPE( DSStateGLImpl, pDepthStencilState );
            auto *pDSSGL = static_cast<DSStateGLImpl*>(pDepthStencilState);
            const auto &DSSDesc = pDepthStencilState->GetDesc();

            m_ContextState.EnableDepthTest( DSSDesc.DepthEnable );
            m_ContextState.EnableDepthWrites( DSSDesc.DepthWriteEnable );
            m_ContextState.SetDepthFunc( DSSDesc.DepthFunc );

            m_ContextState.EnableStencilTest( DSSDesc.StencilEnable );

            m_ContextState.SetStencilWriteMask( DSSDesc.StencilWriteMask );

            {
                const auto &FrontFace = DSSDesc.FrontFace;
                m_ContextState.SetStencilFunc( GL_FRONT, FrontFace.StencilFunc, StencilRef, DSSDesc.StencilReadMask );
                m_ContextState.SetStencilOp( GL_FRONT, FrontFace.StencilFailOp, FrontFace.StencilDepthFailOp, FrontFace.StencilPassOp );
            }

            {
                const auto &BackFace = DSSDesc.BackFace;
                m_ContextState.SetStencilFunc( GL_BACK, BackFace.StencilFunc, StencilRef, DSSDesc.StencilReadMask );
                m_ContextState.SetStencilOp( GL_BACK, BackFace.StencilFailOp, BackFace.StencilDepthFailOp, BackFace.StencilPassOp );
            }
        }
    }

    void DeviceContextGLImpl::SetRasterizerState( IRasterizerState *pRasterizerState )
    {
        if( TDeviceContextBase::SetRasterizerState( pRasterizerState ) )
        {
            CHECK_DYNAMIC_TYPE( RasterizerStateGLImpl, pRasterizerState );
            auto *pRSGL = static_cast<RasterizerStateGLImpl*>(pRasterizerState);
            const auto &RSDesc = pRasterizerState->GetDesc();

            m_ContextState.SetFillMode(RSDesc.FillMode);
            m_ContextState.SetCullMode(RSDesc.CullMode);
            m_ContextState.SetFrontFace(RSDesc.FrontCounterClockwise);
            m_ContextState.SetDepthBias( static_cast<Float32>( RSDesc.DepthBias ), RSDesc.SlopeScaledDepthBias );
            if( RSDesc.DepthBiasClamp != 0 )
                LOG_WARNING_MESSAGE( "Depth bias clamp is not supported on OpenGL" );
            m_ContextState.SetDepthClamp( RSDesc.DepthClipEnable );
            m_ContextState.EnableScissorTest( RSDesc.ScissorEnable );
            if( RSDesc.AntialiasedLineEnable )
                LOG_WARNING_MESSAGE( "Line antialiasing is not supported on OpenGL" );
        }
    }


    void DeviceContextGLImpl::SetBlendState( IBlendState *pBS, const float* pBlendFactors, Uint32 SampleMask )
    {
        if( TDeviceContextBase::SetBlendState( pBS, pBlendFactors, SampleMask ) )
        {
            CHECK_DYNAMIC_TYPE( BlendStateGLImpl, pBS );
            auto *pBSGL = static_cast<BlendStateGLImpl*>(pBS);
            const auto BSDsc = pBS->GetDesc();
            m_ContextState.SetBlendState(BSDsc, m_BlendFactors, SampleMask);
        }
    }

    void DeviceContextGLImpl::SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 RTWidth, Uint32 RTHeight  )
    {
        TDeviceContextBase::SetViewports( NumViewports, pViewports, RTWidth, RTHeight  );

        VERIFY( NumViewports == m_Viewports.size(), "Unexpected number of viewports" );
        if( NumViewports == 1 )
        {
            const auto &vp = m_Viewports[0];
            // Note that OpenGL and DirectX use different origin of 
            // the viewport in window coordinates:
            //
            // DirectX (0,0)
            //     \ ____________
            //      |            |
            //      |            |
            //      |            |
            //      |            |
            //      |____________|
            //     /
            //  OpenGL (0,0)
            //
            float BottomLeftY = static_cast<float>(RTHeight) - (vp.TopLeftY + vp.Height);
            float BottomLeftX = vp.TopLeftX;

            Int32 x = static_cast<int>(BottomLeftX);
            Int32 y = static_cast<int>(BottomLeftY);
            Int32 w = static_cast<int>(vp.Width);
            Int32 h = static_cast<int>(vp.Height);
            if( static_cast<float>(x) == BottomLeftX &&
                static_cast<float>(y) == BottomLeftY &&
                static_cast<float>(w) == vp.Width &&
                static_cast<float>(h) == vp.Height )
            {
                glViewport( x, y, w, h );
            }
            else
            {
                glViewportIndexedf( 0, BottomLeftX, BottomLeftY, vp.Width, vp.Height );
            }
            CHECK_GL_ERROR( "Failed to set viewport" );

            glDepthRangef( vp.MinDepth, vp.MaxDepth );
            CHECK_GL_ERROR( "Failed to set depth range" );
        }
        else
        {
            for( Uint32 i = 0; i < NumViewports; ++i )
            {
                const auto &vp = m_Viewports[i];
                float BottomLeftY = static_cast<float>(RTHeight) - (vp.TopLeftY + vp.Height);
                float BottomLeftX = vp.TopLeftX;
                glViewportIndexedf( i, BottomLeftX, BottomLeftY, vp.Width, vp.Height );
                CHECK_GL_ERROR( "Failed to set viewport #", i );
                glDepthRangef( vp.MinDepth, vp.MaxDepth );
                CHECK_GL_ERROR( "Failed to set depth range for viewport #", i );
            }
        }
    }

    void DeviceContextGLImpl::SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 RTWidth, Uint32 RTHeight  )
    {
        TDeviceContextBase::SetScissorRects(NumRects, pRects, RTWidth, RTHeight);

        VERIFY( NumRects == m_ScissorRects.size(), "Unexpected number of scissor rects" );
        if( NumRects == 1 )
        {
            const auto &Rect = m_ScissorRects[0];
            // Note that OpenGL and DirectX use different origin 
            // of the viewport in window coordinates:
            //
            // DirectX (0,0)
            //     \ ____________
            //      |            |
            //      |            |
            //      |            |
            //      |            |
            //      |____________|
            //     /
            //  OpenGL (0,0)
            //
            auto glBottom = RTHeight - Rect.bottom;

            auto width  = Rect.right - Rect.left;
            auto height = Rect.bottom - Rect.top;
            glScissor( Rect.left, glBottom, width, height );
            CHECK_GL_ERROR( "Failed to set scissor rect" );
        }
        else
        {
            for( Uint32 sr = 0; sr < NumRects; ++sr )
            {
                const auto &Rect = m_ScissorRects[sr];
                auto glBottom = RTHeight - Rect.bottom;
                auto width  = Rect.right - Rect.left;
                auto height = Rect.bottom - Rect.top;
                glScissorIndexed(sr, Rect.left, glBottom, width, height );
                CHECK_GL_ERROR( "Failed to set scissor rect #", sr );
            }
        }
    }

    void DeviceContextGLImpl::RebindRenderTargets()
    {
        Uint32 NumRenderTargets = static_cast<Uint32>( m_pBoundRenderTargets.size() );
        VERIFY( NumRenderTargets < MaxRenderTargets, "Too many render targets (", NumRenderTargets, ") are being set" );
        
        NumRenderTargets = std::min( NumRenderTargets, MaxRenderTargets );
        ITextureView *pBoundRTVs[MaxRenderTargets] = {};
        for( Uint32 rt = 0; rt < NumRenderTargets; ++rt )
            pBoundRTVs[rt] = m_pBoundRenderTargets[rt];

        auto *pRenderDeviceGL = ValidatedCast<RenderDeviceGLImpl>(m_pDevice.RawPtr());
        auto &FBOCache = pRenderDeviceGL->m_FBOCache;
        const auto& FBO = FBOCache.GetFBO( NumRenderTargets, pBoundRTVs, m_pBoundDepthStencil, m_ContextState );
        // Even though the write mask only applies to writes to a framebuffer, the mask state is NOT 
        // Framebuffer state. So it is NOT part of a Framebuffer Object or the Default Framebuffer. 
        // Binding a new framebuffer will NOT affect the mask.
        m_ContextState.BindFBO( FBO );

        // Set the viewport to match the render target size
        SetViewports(1, nullptr, 0, 0);
    }

    void DeviceContextGLImpl::SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil )
    {
        if( TDeviceContextBase::SetRenderTargets( NumRenderTargets, ppRenderTargets, pDepthStencil ) )
            RebindRenderTargets();
    }

    void DeviceContextGLImpl::BindProgramResources( Uint32 &NewMemoryBarriers )
    {
        auto *pRenderDeviceGL = ValidatedCast<RenderDeviceGLImpl>(m_pDevice.RawPtr());
        auto &PipelineCache = pRenderDeviceGL->m_PipelineCache;
        auto &PipelineOrProg = PipelineCache.GetProgramPipeline( m_pBoundShaders.data(), (Uint32)m_pBoundShaders.size() );

        const auto &DeviceCaps = pRenderDeviceGL->GetDeviceCaps();
        GLuint Prog = PipelineOrProg.Program;
        GLuint Pipeline = PipelineOrProg.Pipeline;
        VERIFY( Prog ^ Pipeline, "Only one of program or pipeline can be specified" );
        if( !(Prog || Pipeline) )
        {
            LOG_ERROR_MESSAGE( "No program/program pipeline is set for the draw call" )
            return;
        }
        auto ProgramPipelineSupported = DeviceCaps.bSeparableProgramSupported;

        // WARNING: glUseProgram() overrides glBindProgramPipeline(). That is, if you have a program in use and
        // a program pipeline bound, all rendering will use the program that is in use, not the pipeline programs!!!
        // So make sure that glUseProgram(0) has been called if pipeline is in use
        m_ContextState.SetProgram( PipelineOrProg.Program );
        if( ProgramPipelineSupported )
            m_ContextState.SetPipeline( PipelineOrProg.Pipeline );

        size_t NumPrograms = ProgramPipelineSupported ? m_pBoundShaders.size() : 1;
        GLuint UniformBuffBindPoint = 0;
        GLuint TextureIndex = 0;
        m_BoundWritableTextures.clear();
        m_BoundWritableBuffers.clear();
        for( size_t ProgNum = 0; ProgNum < NumPrograms; ++ProgNum )
        {
            auto *pShaderGL = static_cast<ShaderGLImpl*>(m_pBoundShaders[ProgNum].RawPtr());
            auto &GLProgramObj = ProgramPipelineSupported ? pShaderGL->m_GlProgObj : PipelineOrProg.Program;

#ifdef VERIFY_RESOURCE_BINDINGS
            GLProgramObj.dbgVerifyResourceBindings();
#endif

            GLuint GLProgID = GLProgramObj;
            auto &UniformBlocks = GLProgramObj.GetUniformBlocks();
            for( auto it = UniformBlocks.begin(); it != UniformBlocks.end(); ++it )
            {
                auto& pResource = it->pResource;
                if( pResource )
                {
                    CHECK_DYNAMIC_TYPE( BufferGLImpl, pResource.RawPtr() );
                    auto *pBufferOGL = static_cast<BufferGLImpl*>(pResource.RawPtr());
                    pBufferOGL->BufferMemoryBarrier(
                        GL_UNIFORM_BARRIER_BIT,// Shader uniforms sourced from buffer objects after the barrier 
                                               // will reflect data written by shaders prior to the barrier
                        m_ContextState);

                    glBindBufferBase( GL_UNIFORM_BUFFER, UniformBuffBindPoint, pBufferOGL->m_GlBuffer );
                    CHECK_GL_ERROR( "Failed to bind uniform buffer" );
                    //glBindBufferRange(GL_UNIFORM_BUFFER, it->Index, pBufferOGL->m_GlBuffer, 0, pBufferOGL->GetDesc().uiSizeInBytes);

                    glUniformBlockBinding( GLProgID, it->Index, UniformBuffBindPoint );
                    CHECK_GL_ERROR( "glUniformBlockBinding() failed" );

                    ++UniformBuffBindPoint;
                }
                else
                {
#define LOG_MISSING_BINDING(VarType, VarName) LOG_ERROR_MESSAGE( "No ", VarType, " is bound to \"", VarName, "\" variable in shader \"", pShaderGL->GetDesc().Name, "\"" );
                    LOG_MISSING_BINDING("uniform buffer", it->Name)
                }
            }

            auto &Samplers = GLProgramObj.GetSamplers();
            for( auto it = Samplers.begin(); it != Samplers.end(); ++it )
            {
                auto &pResource = it->pResource;
                if( pResource )
                {
                    if( it->Type == GL_SAMPLER_BUFFER ||
                        it->Type == GL_INT_SAMPLER_BUFFER ||
                        it->Type == GL_UNSIGNED_INT_SAMPLER_BUFFER )
                    {
                        CHECK_DYNAMIC_TYPE( BufferViewGLImpl, pResource.RawPtr() );
                        auto *pBufViewOGL = static_cast<BufferViewGLImpl*>(pResource.RawPtr());
                        auto *pBuffer = pBufViewOGL->GetBuffer();

                        m_ContextState.BindTexture( TextureIndex, GL_TEXTURE_BUFFER, pBufViewOGL->GetTexBufferHandle() );
                        m_ContextState.BindSampler( TextureIndex, GLObjectWrappers::GLSamplerObj(false) ); // Use default texture sampling parameters

                        CHECK_DYNAMIC_TYPE( BufferGLImpl, pBuffer );
                        static_cast<BufferGLImpl*>(pBuffer)->BufferMemoryBarrier(
                            GL_TEXTURE_FETCH_BARRIER_BIT, // Texture fetches from shaders, including fetches from buffer object 
                                                          // memory via buffer textures, after the barrier will reflect data 
                                                          // written by shaders prior to the barrier
                            m_ContextState);
                    }
                    else
                    {
                        CHECK_DYNAMIC_TYPE( TextureViewGLImpl, pResource.RawPtr() );
                        auto *pTexViewOGL = static_cast<TextureViewGLImpl*>(pResource.RawPtr());
                        m_ContextState.BindTexture( TextureIndex, pTexViewOGL->GetBindTarget(), pTexViewOGL->GetHandle() );

                        auto *pTexture = pTexViewOGL->GetTexture();
                        CHECK_DYNAMIC_TYPE( TextureBaseGL, pTexture );
                        static_cast<TextureBaseGL*>(pTexture)->TextureMemoryBarrier(
                            GL_TEXTURE_FETCH_BARRIER_BIT, // Texture fetches from shaders, including fetches from buffer object 
                                                          // memory via buffer textures, after the barrier will reflect data 
                                                          // written by shaders prior to the barrier
                            m_ContextState);

                        auto pSampler = pTexViewOGL->GetSampler();
                        if( pSampler )
                        {
                            auto *pSamplerGL = ValidatedCast<SamplerGLImpl>( pSampler );
                            m_ContextState.BindSampler( TextureIndex, pSamplerGL->GetHandle() );
                        }
                    }

                    // Texture is now bound to texture slot TextureIndex.
                    // We now need to set the program uniform to use that slot
                    if( ProgramPipelineSupported )
                    {
                        // glProgramUniform1i does not require program to be bound to the pipeline
                        glProgramUniform1i( GLProgramObj, it->Location, TextureIndex );
                    }
                    else
                    {
                        // glUniform1i requires program to be bound to the pipeline
                        glUniform1i( it->Location, TextureIndex );
                    }
                    CHECK_GL_ERROR( "Failed to bind sampler uniform to texture slot" );

                    ++TextureIndex;
                }
                else
                {
                    LOG_MISSING_BINDING("texture sampler", it->Name)
                }
            }

            auto &Images = GLProgramObj.GetImages();
            for( auto it = Images.begin(); it != Images.end(); ++it )
            {
                auto &pResource = it->pResource;
                if( pResource )
                {
                    CHECK_DYNAMIC_TYPE( TextureViewGLImpl, pResource.RawPtr() );
                    auto *pTexViewOGL = static_cast<TextureViewGLImpl*>(pResource.RawPtr());
                    const auto &ViewDesc = pTexViewOGL->GetDesc();

                    if( ViewDesc.AccessFlags & UAV_ACCESS_FLAG_WRITE )
                    {
                        auto *pTex = pTexViewOGL->GetTexture();
                        CHECK_DYNAMIC_TYPE( TextureBaseGL, pTex );
                        auto *pTexGL = static_cast<TextureBaseGL*>(pTex);

                        pTexGL->TextureMemoryBarrier(
                            GL_SHADER_IMAGE_ACCESS_BARRIER_BIT,// Memory accesses using shader image load, store, and atomic built-in 
                                                               // functions issued after the barrier will reflect data written by shaders 
                                                               // prior to the barrier. Additionally, image stores and atomics issued after 
                                                               // the barrier will not execute until all memory accesses (e.g., loads, 
                                                               // stores, texture fetches, vertex fetches) initiated prior to the barrier 
                                                               // complete.
                            m_ContextState);
                        // We cannot set pending memory barriers here, because
                        // if some texture is bound twice, the logic will fail
                        m_BoundWritableTextures.push_back( pTexGL );
                    }

#ifdef _DEBUG
                    // Check that the texure being bound has immutable storage
                    {
                        m_ContextState.BindTexture( -1, pTexViewOGL->GetBindTarget(), pTexViewOGL->GetHandle() );
                        GLint IsImmutable = 0;
                        glGetTexParameteriv( pTexViewOGL->GetBindTarget(), GL_TEXTURE_IMMUTABLE_FORMAT, &IsImmutable );
                        CHECK_GL_ERROR( "glGetTexParameteriv() failed" );
                        VERIFY( IsImmutable, "Only immutable textures can be bound to pipeline using glBindImageTexture()" );
                        m_ContextState.BindTexture( -1, pTexViewOGL->GetBindTarget(), GLObjectWrappers::GLTextureObj(false) );
                    }
#endif
                    auto GlTexFormat = TexFormatToGLInternalTexFormat( ViewDesc.Format );
                    // Note that if a format qulifier is specified in the shader, the format
                    // must match it

                    GLboolean Layered = ViewDesc.NumArraySlices > 1 && ViewDesc.FirstArraySlice == 0;
                    // If "layered" is TRUE, the entire Mip level is bound. Layer parameter is ignored in this
                    // case. If "layered" is FALSE, only the single layer identified by "layer" will
                    // be bound. When "layered" is FALSE, the single bound layer is treated as a 2D texture.
                    GLint Layer = ViewDesc.FirstArraySlice;

                    auto GLAccess = AccessFlags2GLAccess( ViewDesc.AccessFlags );
                    // WARNING: Texture being bound to the image unit must be complete
                    // That means that if an integer texture is being bound, its 
                    // GL_TEXTURE_MIN_FILTER and GL_TEXTURE_MAG_FILTER must be NEAREST,
                    // otherwise it will be incomplete
                    m_ContextState.BindImage( it->BindingPoint, pTexViewOGL, ViewDesc.MostDetailedMip, Layered, Layer, GLAccess, GlTexFormat );
                }
                else
                {
                    LOG_MISSING_BINDING("image", it->Name)
                }
            }

            auto &StorageBlocks = GLProgramObj.GetStorageBlocks();
            for( auto it = StorageBlocks.begin(); it != StorageBlocks.end(); ++it )
            {
                auto &pResource = it->pResource;
                if( pResource )
                {
                    CHECK_DYNAMIC_TYPE( BufferViewGLImpl, pResource.RawPtr() );
                    auto *pBufferViewOGL = static_cast<BufferViewGLImpl*>(pResource.RawPtr());
                    const auto &ViewDesc = pBufferViewOGL->GetDesc();
                    VERIFY( ViewDesc.ViewType == BUFFER_VIEW_UNORDERED_ACCESS, "Incorrect buffer view type" );

                    auto *pBuffer = pBufferViewOGL->GetBuffer();
                    CHECK_DYNAMIC_TYPE( BufferGLImpl, pBuffer );
                    auto *pBufferOGL = static_cast<BufferGLImpl*>(pBuffer);

                    pBufferOGL->BufferMemoryBarrier(
                        GL_SHADER_STORAGE_BARRIER_BIT,// Accesses to shader storage blocks after the barrier 
                                                      // will reflect writes prior to the barrier
                        m_ContextState);

                    glBindBufferRange( GL_SHADER_STORAGE_BUFFER, it->Binding, pBufferOGL->m_GlBuffer, ViewDesc.ByteOffset, ViewDesc.ByteWidth );
                    CHECK_GL_ERROR( "Failed to bind shader storage buffer" );

                    m_BoundWritableBuffers.push_back( pBufferOGL );
                }
                else
                {
                    LOG_MISSING_BINDING("shader storage block", it->Name )
                }
            }
        }

        // Go through the list of textures bound as AUVs and set the required memory barriers
        for( auto pWritableTex = m_BoundWritableTextures.begin(); pWritableTex != m_BoundWritableTextures.end(); ++pWritableTex )
        {
            Uint32 TextureMemBarriers =
                GL_TEXTURE_UPDATE_BARRIER_BIT // Writes to a texture via glTex(Sub)Image*, glCopyTex(Sub)Image*, 
                                              // glClearTex*Image, glCompressedTex(Sub)Image*, and reads via
                                              // glGetTexImage() after the barrier will reflect data written by 
                                              // shaders prior to the barrier

                | GL_TEXTURE_FETCH_BARRIER_BIT  // Texture fetches from shaders, including fetches from buffer object 
                                                // memory via buffer textures, after the barrier will reflect data 
                                                // written by shaders prior to the barrier

                | GL_PIXEL_BUFFER_BARRIER_BIT // Reads and writes of buffer objects via the GL_PIXEL_PACK_BUFFER and
                                              // GL_PIXEL_UNPACK_BUFFER bidnings after the barrier will reflect data 
                                              // written by shaders prior to the barrier

                | GL_FRAMEBUFFER_BARRIER_BIT // Reads and writes via framebuffer object attachments after the 
                                             // barrier will reflect data written by shaders prior to the barrier. 
                                             // Additionally, framebuffer writes issued after the barrier will wait 
                                             // on the completion of all shader writes issued prior to the barrier.

                | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;

            NewMemoryBarriers |= TextureMemBarriers;

            // Set new required barriers for the time when texture is used next time
            (*pWritableTex)->SetPendingMemoryBarriers( TextureMemBarriers );
        }

        for( auto pWritableBuff = m_BoundWritableBuffers.begin(); pWritableBuff != m_BoundWritableBuffers.end(); ++pWritableBuff )
        {
            Uint32 BufferMemoryBarriers =
                GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT |
                GL_ELEMENT_ARRAY_BARRIER_BIT |
                GL_UNIFORM_BARRIER_BIT |
                GL_COMMAND_BARRIER_BIT | 
                GL_BUFFER_UPDATE_BARRIER_BIT |
                GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT |
                GL_SHADER_STORAGE_BARRIER_BIT |
                GL_TEXTURE_FETCH_BARRIER_BIT;

            NewMemoryBarriers |= BufferMemoryBarriers;
            // Set new required barriers for the time when buffer is used next time
            (*pWritableBuff)->SetPendingMemoryBarriers( BufferMemoryBarriers );
        }
    }

    void DeviceContextGLImpl::Draw( DrawAttribs &DrawAttribs )
    {
        if( DrawAttribs.Topology == PRIMITIVE_TOPOLOGY_UNDEFINED )
        {
            LOG_ERROR_MESSAGE("Primitive topology is undefined");
            return;
        }

        RefCntWeakPtr<IBuffer> pIndexBuffer;
        if( DrawAttribs.IsIndexed )
            pIndexBuffer = m_pIndexBuffer;

        // Obtain strong reference to the vertex description object and index buffer
        auto &spVertexDesc = m_pVertexDesc;
        auto *pRenderDeviceGL = ValidatedCast<RenderDeviceGLImpl>(m_pDevice.RawPtr());
        if( spVertexDesc )
        {
            auto &VAOCache = pRenderDeviceGL->m_VAOCache;
            auto spIndexBuff = pIndexBuffer.Lock();
            const auto& VAO = VAOCache.GetVAO( *spVertexDesc, spIndexBuff, m_VertexStreams, m_ContextState );
            m_ContextState.BindVAO( VAO );
        }
        else
        {
            // Draw command will fail if no VAO is bound. If no vertex description is set
            // (which is the case if, for instance, the command only inputs VertexID),
            // use empty VAO
            m_ContextState.BindVAO( pRenderDeviceGL->m_EmptyVAO );
        }

        auto GlTopology = PrimitiveTopologyToGLTopology( DrawAttribs.Topology );
        GLenum IndexType = 0;
        Uint32 FirstIndexByteOffset = 0;
        if( DrawAttribs.IsIndexed )
        {
            IndexType = TypeToGLType( DrawAttribs.IndexType );
            VERIFY( IndexType == GL_UNSIGNED_BYTE || IndexType == GL_UNSIGNED_SHORT || IndexType == GL_UNSIGNED_INT,
                    "Unsupported index type" );
            VERIFY( m_pIndexBuffer, "Index Buffer is not bound to the pipeline" );
            FirstIndexByteOffset = static_cast<Uint32>(GetValueSize( DrawAttribs.IndexType )) * DrawAttribs.FirstIndexLocation + m_IndexDataStartOffset;
        }

        Uint32 NewMemoryBarriers = 0;
        BindProgramResources( NewMemoryBarriers );

        // NOTE: Base Vertex and Base Instance versions are not supported even in OpenGL ES 3.1
        // This functionality can be emulated by adjusting stream offsets. This, however may cause
        // errors in case instance data is read from the same stream as vertex data. Thus handling
        // such cases is left to the application

        // http://www.opengl.org/wiki/Vertex_Rendering
        if( DrawAttribs.IsIndirect )
        {
            // The indirect rendering functions take their data from the buffer currently bound to the 
            // GL_DRAW_INDIRECT_BUFFER binding. Thus, any of indirect draw functions will fail if no buffer is 
            // bound to that binding.
            VERIFY( DrawAttribs.pIndirectDrawAttribs, "Indirect draw command attributes buffer is not set" );
            if( DrawAttribs.pIndirectDrawAttribs )
            {
                auto *pBufferOGL = static_cast<BufferGLImpl*>(DrawAttribs.pIndirectDrawAttribs);

                pBufferOGL->BufferMemoryBarrier(
                    GL_COMMAND_BARRIER_BIT,// Command data sourced from buffer objects by
                                           // Draw*Indirect and DispatchComputeIndirect commands after the barrier
                                           // will reflect data written by shaders prior to the barrier.The buffer 
                                           // objects affected by this bit are derived from the DRAW_INDIRECT_BUFFER 
                                           // and DISPATCH_INDIRECT_BUFFER bindings.
                    m_ContextState);

                glBindBuffer( GL_DRAW_INDIRECT_BUFFER, pBufferOGL->m_GlBuffer );
            }

            if( DrawAttribs.IsIndexed )
            {
                //typedef  struct {
                //    GLuint  count;
                //    GLuint  instanceCount;
                //    GLuint  firstIndex;
                //    GLuint  baseVertex;
                //    GLuint  baseInstance;
                //} DrawElementsIndirectCommand;
                glDrawElementsIndirect( GlTopology, IndexType, reinterpret_cast<const void*>( static_cast<size_t>(DrawAttribs.IndirectDrawArgsOffset) ) );
                // Note that on GLES 3.1, baseInstance is present but reserved and must be zero
                CHECK_GL_ERROR( "glDrawElementsIndirect() failed" );
            }
            else
            {
                //typedef  struct {
                //   GLuint  count;
                //   GLuint  instanceCount;
                //   GLuint  first;
                //   GLuint  baseInstance;
                //} DrawArraysIndirectCommand;
                glDrawArraysIndirect( GlTopology, reinterpret_cast<const void*>( static_cast<size_t>(DrawAttribs.IndirectDrawArgsOffset) ) );
                // Note that on GLES 3.1, baseInstance is present but reserved and must be zero
                CHECK_GL_ERROR( "glDrawArraysIndirect() failed" );
            }

            glBindBuffer( GL_DRAW_INDIRECT_BUFFER, 0 );
        }
        else
        {
            if( DrawAttribs.NumInstances > 1 )
            {
                if( DrawAttribs.IsIndexed )
                {
                    if( DrawAttribs.BaseVertex )
                    {
                        if( DrawAttribs.FirstInstanceLocation )
                            glDrawElementsInstancedBaseVertexBaseInstance( GlTopology, DrawAttribs.NumIndices, IndexType, reinterpret_cast<GLvoid*>( static_cast<size_t>(FirstIndexByteOffset) ), DrawAttribs.NumInstances, DrawAttribs.BaseVertex, DrawAttribs.FirstInstanceLocation );
                        else
                            glDrawElementsInstancedBaseVertex( GlTopology, DrawAttribs.NumIndices, IndexType, reinterpret_cast<GLvoid*>( static_cast<size_t>(FirstIndexByteOffset) ), DrawAttribs.NumInstances, DrawAttribs.BaseVertex );
                    }
                    else
                    {
                        if( DrawAttribs.FirstInstanceLocation )
                            glDrawElementsInstancedBaseInstance( GlTopology, DrawAttribs.NumIndices, IndexType, reinterpret_cast<GLvoid*>( static_cast<size_t>(FirstIndexByteOffset) ), DrawAttribs.NumInstances, DrawAttribs.FirstInstanceLocation );
                        else
                            glDrawElementsInstanced( GlTopology, DrawAttribs.NumIndices, IndexType, reinterpret_cast<GLvoid*>( static_cast<size_t>(FirstIndexByteOffset) ), DrawAttribs.NumInstances );
                    }
                }
                else
                {
                    if( DrawAttribs.FirstInstanceLocation )
                        glDrawArraysInstancedBaseInstance( GlTopology, DrawAttribs.StartVertexLocation, DrawAttribs.NumVertices, DrawAttribs.NumInstances, DrawAttribs.FirstInstanceLocation );
                    else
                        glDrawArraysInstanced( GlTopology, DrawAttribs.StartVertexLocation, DrawAttribs.NumVertices, DrawAttribs.NumInstances );
                }
            }
            else
            {
                if( DrawAttribs.IsIndexed )
                {
                    if( DrawAttribs.BaseVertex )
                        glDrawElementsBaseVertex( GlTopology, DrawAttribs.NumIndices, IndexType, reinterpret_cast<GLvoid*>( static_cast<size_t>(FirstIndexByteOffset) ), DrawAttribs.BaseVertex );
                    else
                        glDrawElements( GlTopology, DrawAttribs.NumIndices, IndexType, reinterpret_cast<GLvoid*>( static_cast<size_t>(FirstIndexByteOffset) ) );
                }
                else
                    glDrawArrays( GlTopology, DrawAttribs.StartVertexLocation, DrawAttribs.NumVertices );
            }
            CHECK_GL_ERROR( "OpenGL draw command failed" );
        }

        // IMPORTANT: new pending memory barriers in the context must be set
        // after all previous barriers were executed.
        m_ContextState.SetPendingMemoryBarriers( NewMemoryBarriers );
    }

    void DeviceContextGLImpl::DispatchCompute( const DispatchComputeAttribs &DispatchAttrs )
    {
        Uint32 NewMemoryBarriers = 0;
        BindProgramResources( NewMemoryBarriers );

        if( DispatchAttrs.pIndirectDispatchAttribs )
        {
            CHECK_DYNAMIC_TYPE( BufferGLImpl, DispatchAttrs.pIndirectDispatchAttribs );
            auto *pBufferOGL = static_cast<BufferGLImpl*>(DispatchAttrs.pIndirectDispatchAttribs);
            pBufferOGL->BufferMemoryBarrier(
                GL_COMMAND_BARRIER_BIT,// Command data sourced from buffer objects by
                                       // Draw*Indirect and DispatchComputeIndirect commands after the barrier
                                       // will reflect data written by shaders prior to the barrier.The buffer 
                                       // objects affected by this bit are derived from the DRAW_INDIRECT_BUFFER 
                                       // and DISPATCH_INDIRECT_BUFFER bindings.
                m_ContextState);

            glBindBuffer( GL_DISPATCH_INDIRECT_BUFFER, pBufferOGL->m_GlBuffer );
            CHECK_GL_ERROR( "Failed to bind a buffer for dispatch indirect command" );

            glDispatchComputeIndirect( DispatchAttrs.DispatchArgsByteOffset );
            CHECK_GL_ERROR( "glDispatchComputeIndirect() failed" );

            glBindBuffer( GL_DISPATCH_INDIRECT_BUFFER, 0 );
        }
        else
        {
            glDispatchCompute( DispatchAttrs.ThreadGroupCountX, DispatchAttrs.ThreadGroupCountY, DispatchAttrs.ThreadGroupCountZ );
            CHECK_GL_ERROR( "glDispatchCompute() failed" );
        }

        // IMPORTANT: new pending memory barriers in the context must be set
        // after all previous barriers were executed.
        m_ContextState.SetPendingMemoryBarriers( NewMemoryBarriers );
    }

    void DeviceContextGLImpl::ClearDepthStencil( ITextureView *pView, Uint32 ClearFlags, float fDepth, Uint8 Stencil )
    {
        // Unlike OpenGL, in D3D10+, the full extent of the resource view is always cleared. 
        // Viewport and scissor settings are not applied.
        if( pView != nullptr )
        {
            const auto& ViewDesc = pView->GetDesc();
            VERIFY( ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL, "Incorrect view type: depth stencil is expected" );
            CHECK_DYNAMIC_TYPE( TextureViewGLImpl, pView );
            auto *pViewGL = static_cast<TextureViewGLImpl*>(pView);
            if( pView != m_pBoundDepthStencil )
            {
                UNEXPECTED( "Depth stencil buffer being cleared is not bound to the pipeline" );
                LOG_ERROR_MESSAGE( "Depth stencil buffer must be bound to the pipeline to be cleared" );
            }
        }
        else
        {
            if( !(nullptr == m_pBoundDepthStencil && m_pBoundRenderTargets.size() == 0) )
            {
                UNEXPECTED( "Default depth stencil buffer being cleared is not bound to the pipeline" );
                LOG_ERROR_MESSAGE( "Default depth stencil buffer must be bound to the pipeline to be cleared" );
            }
        }
        Uint32 glClearFlags = 0;
        if( ClearFlags & CLEAR_DEPTH_FLAG )   glClearFlags |= GL_DEPTH_BUFFER_BIT;
        if( ClearFlags & CLEAR_STENCIL_FLAG ) glClearFlags |= GL_STENCIL_BUFFER_BIT;
        glClearDepthf( fDepth );
        glClearStencil( Stencil );
        // If depth writes are disabled, glClear() will not clear depth buffer!
        bool DepthWritesEnabled = m_ContextState.GetDepthWritesEnabled();
        m_ContextState.EnableDepthWrites( True );
        bool ScissorTestEnabled = m_ContextState.GetScissorTestEnabled();
        m_ContextState.EnableScissorTest( False );
        // The pixel ownership test, the scissor test, dithering, and the buffer writemasks affect 
        // the operation of glClear. The scissor box bounds the cleared region. Alpha function, 
        // blend function, logical operation, stenciling, texture mapping, and depth-buffering 
        // are ignored by glClear.
        glClear(glClearFlags);
        CHECK_GL_ERROR( "glClear() failed" );
        m_ContextState.EnableDepthWrites( DepthWritesEnabled );
        m_ContextState.EnableScissorTest( ScissorTestEnabled );
    }

    void DeviceContextGLImpl::ClearRenderTarget( ITextureView *pView, const float *RGBA )
    {
        // Unlike OpenGL, in D3D10+, the full extent of the resource view is always cleared. 
        // Viewport and scissor settings are not applied.

        Int32 RTIndex = -1;
        if( pView != nullptr )
        {
            const auto& ViewDesc = pView->GetDesc();
            VERIFY( ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET, "Incorrect view type: render target is expected" );
            CHECK_DYNAMIC_TYPE( TextureViewGLImpl, pView );
            auto *pViewGL = static_cast<TextureViewGLImpl*>(pView);
            for( Uint32 rt = 0; rt < m_pBoundRenderTargets.size(); ++rt )
                if( m_pBoundRenderTargets[rt] == pView )
                {
                    RTIndex = rt;
                    break;
                }

            if( RTIndex == -1 )
            {
                UNEXPECTED( "Render target being cleared is not bound to the pipeline" );
                LOG_ERROR_MESSAGE( "Render target must be bound to the pipeline to be cleared" );
            }
        }
        else
        {
            if( m_pBoundRenderTargets.size() == 0 && m_pBoundDepthStencil == nullptr )
                RTIndex = 0;
            else
            {
                UNEXPECTED( "Default render target must be bound to the pipeline to be cleared" );
                LOG_ERROR_MESSAGE( "Default render target must be bound to the pipeline to be cleared" );
            }
        }

        static const float Zero[4] = { 0, 0, 0, 0 };
        if( RGBA == nullptr )
            RGBA = Zero;
        
        // The pixel ownership test, the scissor test, dithering, and the buffer writemasks affect 
        // the operation of glClear. The scissor box bounds the cleared region. Alpha function, 
        // blend function, logical operation, stenciling, texture mapping, and depth-buffering 
        // are ignored by glClear.

        // Disable scissor test
        bool ScissorTestEnabled = m_ContextState.GetScissorTestEnabled();
        m_ContextState.EnableScissorTest( False );

        // Set write mask
        Uint32 WriteMask = 0;
        Bool bIndependentBlend = False;
        m_ContextState.GetColorWriteMask( RTIndex, WriteMask, bIndependentBlend );
        m_ContextState.SetColorWriteMask( RTIndex, COLOR_MASK_ALL, bIndependentBlend );
        
        glClearBufferfv( GL_COLOR, RTIndex, RGBA );
        CHECK_GL_ERROR( "glClearBufferfv() failed" );

        m_ContextState.SetColorWriteMask( RTIndex, WriteMask, bIndependentBlend );
        m_ContextState.EnableScissorTest( ScissorTestEnabled );
    }

    void DeviceContextGLImpl::Flush()
    {
        glFlush();
    }

}
