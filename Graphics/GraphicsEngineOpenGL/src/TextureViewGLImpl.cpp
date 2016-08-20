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

#include "TextureViewGLImpl.h"
#include "RenderDeviceGLImpl.h"
#include "TextureBaseGL.h"
#include "DeviceContextGLImpl.h"

namespace Diligent
{
    TextureViewGLImpl::TextureViewGLImpl( FixedBlockMemoryAllocator &TexViewObjAllocator,
                                          IRenderDevice *pDevice, 
                                          const TextureViewDesc& ViewDesc, 
                                          TextureBaseGL* pTexture,
                                          bool bCreateGLViewTex,
                                          bool bIsDefaultView ) :
        TTextureViewBase(TexViewObjAllocator, pDevice, ViewDesc, pTexture, bIsDefaultView),
        m_ViewTexGLHandle( bCreateGLViewTex ),
        m_ViewTexBindTarget(0)
    {
    }

    TextureViewGLImpl::~TextureViewGLImpl()
    {
    }

    IMPLEMENT_QUERY_INTERFACE( TextureViewGLImpl, IID_TextureViewGL, TTextureViewBase )

    const GLObjectWrappers::GLTextureObj& TextureViewGLImpl::GetHandle()
    {
        if( m_ViewTexGLHandle )
            return m_ViewTexGLHandle;
        else
        {
            auto *pTexture = GetTexture();
            CHECK_DYNAMIC_TYPE( TextureBaseGL, pTexture );
            return static_cast<TextureBaseGL*>(pTexture)->GetGLHandle();
        }
    }

    GLenum TextureViewGLImpl::GetBindTarget()
    {
        if( m_ViewTexGLHandle )
            return m_ViewTexBindTarget;
        else
        {
            auto *pTexture = GetTexture();
            CHECK_DYNAMIC_TYPE( TextureBaseGL, pTexture );
            return static_cast<TextureBaseGL*>(pTexture)->GetBindTarget();
        }
    }

    void TextureViewGLImpl::GenerateMips( IDeviceContext *pContext )
    {
        auto pCtxGL = ValidatedCast<DeviceContextGLImpl>( pContext );
        auto &GLState = pCtxGL->GetContextState();
        auto BindTarget = GetBindTarget();
        GLState.BindTexture( -1, BindTarget, GetHandle() );
        glGenerateMipmap( BindTarget );
        CHECK_GL_ERROR( "Failed to generate mip maps" );
        GLState.BindTexture( -1, BindTarget, GLObjectWrappers::GLTextureObj(false) );
    }
}
