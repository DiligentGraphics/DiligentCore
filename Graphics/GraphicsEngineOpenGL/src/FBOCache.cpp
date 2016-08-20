/*     Copyright 2015-2016 Egor Yusov
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

#include "FBOCache.h"
#include "RenderDeviceGLImpl.h"
#include "TextureBaseGL.h"
#include "GLContextState.h"

namespace Diligent
{

bool FBOCache::FBOCacheKey::operator == (const FBOCacheKey &Key)const
{
    if( Hash !=0 && Key.Hash !=0 && Hash != Key.Hash )
        return false;

    if( NumRenderTargets != Key.NumRenderTargets )
        return false;
    for( Uint32 rt = 0; rt < NumRenderTargets;++rt )
    { 
        if( RTIds[rt] != Key.RTIds[rt] )
            return false;
        if( RTIds[rt] )
        {
            if( !(RTVDescs[rt] == Key.RTVDescs[rt]) )
                return false;
        }
    }
    if( DSId != Key.DSId )
        return false;
    if( DSId )
    {
        if( !(DSVDesc == Key.DSVDesc) )
            return false;
    }
    return true;
}

std::size_t FBOCache::FBOCacheKeyHashFunc::operator() ( const FBOCacheKey& Key )const
{
    if( Key.Hash == 0 )
    {
        std::hash<TextureViewDesc> TexViewDescHasher;
        Key.Hash = 0;
        HashCombine( Key.Hash, Key.NumRenderTargets );
        for( Uint32 rt = 0; rt < Key.NumRenderTargets; ++rt )
        {
            HashCombine( Key.Hash, Key.RTIds[rt] );
            if( Key.RTIds[rt] )
                HashCombine( Key.Hash, TexViewDescHasher( Key.RTVDescs[rt] ) );
        }
        HashCombine( Key.Hash, Key.DSId );
        if( Key.DSId )
            HashCombine( Key.Hash, TexViewDescHasher( Key.DSVDesc ) );
    }
    return Key.Hash;
}


FBOCache::FBOCache()
{
    m_Cache.max_load_factor(0.5f);
    m_TexIdToKey.max_load_factor(0.5f);
}

FBOCache::~FBOCache()
{
    VERIFY( m_Cache.empty(), "FBO cache is not empty. Are there any unreleased objects?" );
    VERIFY( m_TexIdToKey.empty(), "TexIdToKey cache is not empty.");
}

void FBOCache::OnReleaseTexture(ITexture *pTexture)
{
    ThreadingTools::LockHelper CacheLock(m_CacheLockFlag);
    auto *pTexGL = ValidatedCast<TextureBaseGL>( pTexture );
    // Find all FBOs that this texture used in
    auto EqualRange = m_TexIdToKey.equal_range(pTexGL->GetUniqueID());
    for(auto It = EqualRange.first; It != EqualRange.second; ++It)
    {
        m_Cache.erase(It->second);
    }
    m_TexIdToKey.erase(EqualRange.first, EqualRange.second);
}

const GLObjectWrappers::GLFrameBufferObj& FBOCache::GetFBO( Uint32 NumRenderTargets, 
                                                              ITextureView *ppRenderTargets[], 
                                                              ITextureView *pDepthStencil,
                                                              GLContextState &ContextState )
{
    // Pop null render targets from the end of the list
    while( NumRenderTargets > 0 && ppRenderTargets[NumRenderTargets - 1] == nullptr )
        --NumRenderTargets;

    if( NumRenderTargets == 0 && pDepthStencil == nullptr )
    {
        static const GLObjectWrappers::GLFrameBufferObj DefaultFBO( false );
        return DefaultFBO;
    }

    // Lock the cache
    ThreadingTools::LockHelper CacheLock(m_CacheLockFlag);
   
    // Construct the key
    FBOCacheKey Key;
    VERIFY( NumRenderTargets < MaxRenderTargets, "Too many render targets being set" );
    NumRenderTargets = std::min( NumRenderTargets, MaxRenderTargets );
    Key.NumRenderTargets = NumRenderTargets;
    for( Uint32 rt = 0; rt < NumRenderTargets; ++rt )
    {
        auto *pRTView = ppRenderTargets[rt];
        if( !pRTView )
            continue;

        auto *pTex = pRTView->GetTexture();
        CHECK_DYNAMIC_TYPE( TextureBaseGL, pTex );
        auto *pTexGL = static_cast<TextureBaseGL*>(pTex);
        pTexGL->TextureMemoryBarrier(
            GL_FRAMEBUFFER_BARRIER_BIT,// Reads and writes via framebuffer object attachments after the 
                                       // barrier will reflect data written by shaders prior to the barrier. 
                                       // Additionally, framebuffer writes issued after the barrier will wait 
                                       // on the completion of all shader writes issued prior to the barrier.
            ContextState);

        Key.RTIds[rt] = pTexGL->GetUniqueID();
        Key.RTVDescs[rt] = pRTView->GetDesc();
    }

    if( pDepthStencil )
    {
        auto *pTex = pDepthStencil->GetTexture();
        CHECK_DYNAMIC_TYPE( TextureBaseGL, pTex );
        auto *pTexGL = static_cast<TextureBaseGL*>(pTex);
        pTexGL->TextureMemoryBarrier( GL_FRAMEBUFFER_BARRIER_BIT, ContextState );
        Key.DSId = pTexGL->GetUniqueID();
        Key.DSVDesc = pDepthStencil->GetDesc();
    }

    // Try to find FBO in the map
    auto It = m_Cache.find(Key);
    if( It != m_Cache.end() )
    {
        return It->second;
    }
    else
    {
        // Create new FBO
        GLObjectWrappers::GLFrameBufferObj NewFBO(true);

        ContextState.BindFBO(NewFBO);

        // Initialize FBO
        for( Uint32 rt = 0; rt < NumRenderTargets; ++rt )
        {
            if( auto *pRTView = ppRenderTargets[rt] )
            {
                auto *pTexture = pRTView->GetTexture();
                const auto &ViewDesc = pRTView->GetDesc();
                CHECK_DYNAMIC_TYPE( TextureBaseGL, pTexture );
                auto *pTexGL = static_cast<TextureBaseGL*>(pTexture);
                pTexGL->AttachToFramebuffer( ViewDesc, GL_COLOR_ATTACHMENT0 + rt );
            }
        }

        if( auto *pDSView = pDepthStencil )
        {
            auto *pTexture = pDSView->GetTexture();
            const auto &ViewDesc = pDSView->GetDesc();
            CHECK_DYNAMIC_TYPE( TextureBaseGL, pTexture );
            auto *pTexGL = static_cast<TextureBaseGL*>(pTexture);
            GLenum AttachmentPoint = 0;
            if( ViewDesc.Format == TEX_FORMAT_D32_FLOAT || 
                ViewDesc.Format == TEX_FORMAT_D16_UNORM )
            {
                auto GLTexFmt = pTexGL->GetGLTexFormat();
                VERIFY( GLTexFmt == GL_DEPTH_COMPONENT32F || GLTexFmt == GL_DEPTH_COMPONENT16, 
                        "Inappropriate internal texture format (", GLTexFmt, ") for depth attachment. "
                        "GL_DEPTH_COMPONENT32F or GL_DEPTH_COMPONENT16 is expected");

                AttachmentPoint = GL_DEPTH_ATTACHMENT;
            }
            else if( ViewDesc.Format == TEX_FORMAT_D32_FLOAT_S8X24_UINT ||
                     ViewDesc.Format == TEX_FORMAT_D24_UNORM_S8_UINT )
            {
                auto GLTexFmt = pTexGL->GetGLTexFormat();
                VERIFY( GLTexFmt == GL_DEPTH24_STENCIL8 || GLTexFmt == GL_DEPTH32F_STENCIL8, 
                        "Inappropriate internal texture format (", GLTexFmt, ") for depth-stencil attachment. "
                        "GL_DEPTH24_STENCIL8 or GL_DEPTH32F_STENCIL8 is expected");

                AttachmentPoint = GL_DEPTH_STENCIL_ATTACHMENT;
            }
            else
            {
                UNEXPECTED( GetTextureFormatAttribs(ViewDesc.Format).Name, " is not valid depth-stencil view format" );
            }
            pTexGL->AttachToFramebuffer( ViewDesc, AttachmentPoint );
        }

        // We now need to set mapping between shader outputs and 
        // color attachments. This largely redundant step is performed
        // by glDrawBuffers()
        static const GLenum DrawBuffers[] = 
        { 
            GL_COLOR_ATTACHMENT0, 
            GL_COLOR_ATTACHMENT1, 
            GL_COLOR_ATTACHMENT2, 
            GL_COLOR_ATTACHMENT3,
            GL_COLOR_ATTACHMENT4,
            GL_COLOR_ATTACHMENT5,
            GL_COLOR_ATTACHMENT6,
            GL_COLOR_ATTACHMENT7,
            GL_COLOR_ATTACHMENT8,
            GL_COLOR_ATTACHMENT9,
            GL_COLOR_ATTACHMENT10,
            GL_COLOR_ATTACHMENT11,
            GL_COLOR_ATTACHMENT12,
            GL_COLOR_ATTACHMENT13,
            GL_COLOR_ATTACHMENT14,
            GL_COLOR_ATTACHMENT15
        };
        // The state set by glDrawBuffers() is part of the state of the framebuffer. 
        // So it can be set up once and left it set.
        glDrawBuffers(NumRenderTargets, DrawBuffers);
        CHECK_GL_ERROR( "Failed to set draw buffers via glDrawBuffers()" );

        GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if( Status != GL_FRAMEBUFFER_COMPLETE )
        {
            const Char *StatusString = "Unknown";
            switch( Status )
            {
                case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:         StatusString = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";         break;
                case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: StatusString = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT"; break;
                case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:        StatusString = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";        break;
                case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:        StatusString = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";        break;
                case GL_FRAMEBUFFER_UNSUPPORTED:                   StatusString = "GL_FRAMEBUFFER_UNSUPPORTED";                   break;
                case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:        StatusString = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";        break;
                case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:      StatusString = "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";      break;
            }
            LOG_ERROR( "Framebuffer is incomplete. FB status: ", StatusString );
            UNEXPECTED( "Framebuffer is incomplete" );
        }

        auto NewElems = m_Cache.emplace( make_pair(Key, std::move(NewFBO)) );
        // New element must be actually inserted
        VERIFY( NewElems.second, "New element was not inserted" ); 
        if( Key.DSId  )
            m_TexIdToKey.insert( make_pair(Key.DSId, Key) );
        for( Uint32 rt = 0; rt < NumRenderTargets; ++rt )
        {
            if( Key.RTIds[rt] )
                m_TexIdToKey.insert( make_pair(Key.RTIds[rt], Key) );
        }

        return NewElems.first->second;
    }
}

}
