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

#pragma once

#include "BaseInterfacesGL.h"
#include "TextureGL.h"
#include "TextureBase.h"
#include "RenderDevice.h"
#include "GLObjectWrapper.h"
#include "TextureViewGLImpl.h"
#include "AsyncWritableResource.h"


namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Base implementation of the Diligent::ITextureGL interface
class TextureBaseGL : public TextureBase<ITextureGL, TextureViewGLImpl, FixedBlockMemoryAllocator, FixedBlockMemoryAllocator>, public AsyncWritableResource
{
public:
    typedef TextureBase<ITextureGL, TextureViewGLImpl, FixedBlockMemoryAllocator, FixedBlockMemoryAllocator> TTextureBase;

    TextureBaseGL(FixedBlockMemoryAllocator& TexObjAllocator, 
                  FixedBlockMemoryAllocator& TexViewObjAllocator, 
                  class RenderDeviceGLImpl *pDeviceGL, 
                  const TextureDesc &TexDesc, 
                  GLenum BindTarget,
                  const TextureData &InitData = TextureData(), 
                  bool bIsDeviceInternal = false);
    ~TextureBaseGL();
    
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;

    virtual void UpdateData( class GLContextState &CtxState, IDeviceContext *pContext, Uint32 MipLevel, Uint32 Slice, const Box &DstBox, const TextureSubResData &SubresData );

    //virtual void CopyData(CTexture *pSrcTexture, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size);
    virtual void Map( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData )override;
    virtual void Unmap( IDeviceContext *pContext, MAP_TYPE MapType )override;

    const GLObjectWrappers::GLTextureObj& GetGLHandle()const{ return m_GlTexture; }
    GLenum GetBindTarget()const{return m_BindTarget;}
    GLenum GetGLTexFormat()const{ return m_GLTexFormat; }

    void TextureMemoryBarrier( Uint32 RequiredBarriers, class GLContextState &GLContextState);

    virtual void AttachToFramebuffer(const struct TextureViewDesc& ViewDesc, GLenum AttachmentPoint) = 0;

    virtual void CopyData(IDeviceContext *pContext, 
                          ITexture *pSrcTexture, 
                          Uint32 SrcMipLevel,
                          Uint32 SrcSlice,
                          const Box *pSrcBox,
                          Uint32 DstMipLevel,
                          Uint32 DstSlice,
                          Uint32 DstX,
                          Uint32 DstY,
                          Uint32 DstZ)override;

protected:
    virtual void CreateViewInternal( const struct TextureViewDesc &ViewDesc, class ITextureView **ppView, bool bIsDefaultView )override;
    void SetDefaultGLParameters();

    GLObjectWrappers::GLTextureObj m_GlTexture;
    const GLenum m_BindTarget;
    const GLenum m_GLTexFormat;
    //Uint32 m_uiMapTarget;
};

}
