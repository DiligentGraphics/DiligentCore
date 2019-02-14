/*     Copyright 2015-2019 Egor Yusov
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

#include "TextureBaseGL.h"
#include "RenderDeviceGLImpl.h"
#include "GLTypeConversions.h"
#include "TextureViewGLImpl.h"
#include "GLContextState.h"
#include "DeviceContextGLImpl.h"
#include "EngineMemory.h"
#include "GraphicsAccessories.h"

namespace Diligent
{

TextureBaseGL::TextureBaseGL(IReferenceCounters*            pRefCounters, 
                             FixedBlockMemoryAllocator&     TexViewObjAllocator, 
                             RenderDeviceGLImpl*            pDeviceGL, 
                             const TextureDesc&             TexDesc, 
                             GLenum                         BindTarget,
                             const TextureData*             pInitData         /*= nullptr*/,
                             bool                           bIsDeviceInternal /*= false*/) : 
    TTextureBase( pRefCounters, TexViewObjAllocator, pDeviceGL, TexDesc, bIsDeviceInternal ),
    m_GlTexture(true), // Create Texture immediately
    m_BindTarget(BindTarget),
    m_GLTexFormat( TexFormatToGLInternalTexFormat(m_Desc.Format, m_Desc.BindFlags) )
    //m_uiMapTarget(0)
{
    VERIFY( m_GLTexFormat != 0, "Unsupported texture format" );
    if( TexDesc.Usage == USAGE_STATIC && pInitData == nullptr )
        LOG_ERROR_AND_THROW("Static Texture must be initialized with data at creation time");
}

static GLenum GetTextureInternalFormat(DeviceContextGLImpl *pDeviceContextGL, GLenum BindTarget, const GLObjectWrappers::GLTextureObj& GLTex, TEXTURE_FORMAT TexFmtFromDesc)
{
    auto &ContextState = pDeviceContextGL->GetContextState();
    ContextState.BindTexture(-1, BindTarget, GLTex);

    GLenum QueryBindTarget = BindTarget;
    if (BindTarget == GL_TEXTURE_CUBE_MAP || BindTarget == GL_TEXTURE_CUBE_MAP_ARRAY)
        QueryBindTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X;

    GLint GlFormat = 0;
#if GL_TEXTURE_INTERNAL_FORMAT
    glGetTexLevelParameteriv(QueryBindTarget, 0, GL_TEXTURE_INTERNAL_FORMAT, &GlFormat);
    CHECK_GL_ERROR( "Failed to get texture format through glGetTexLevelParameteriv()" );
    VERIFY(GlFormat != 0, "Unable to get texture format");
    VERIFY(TexFmtFromDesc == TEX_FORMAT_UNKNOWN || static_cast<GLenum>(GlFormat) == TexFormatToGLInternalTexFormat(TexFmtFromDesc), "Texture format does not match the format specified by the texture description");
#else
    if(TexFmtFromDesc != TEX_FORMAT_UNKNOWN)
        GlFormat = TexFormatToGLInternalTexFormat(TexFmtFromDesc);
    else
        UNSUPPORTED("Texture format cannot be queried and must be provided by the texture description");
#endif
    ContextState.BindTexture(-1, BindTarget, GLObjectWrappers::GLTextureObj(false) );

    return GlFormat;
}

static TextureDesc GetTextureDescFromGLHandle(DeviceContextGLImpl *pDeviceContextGL, TextureDesc TexDesc, GLuint GLHandle, GLenum BindTarget)
{
    auto &ContextState = pDeviceContextGL->GetContextState();
    
    VERIFY(BindTarget != GL_TEXTURE_CUBE_MAP_ARRAY, "Cubemap arrays are not currently supported");

    GLObjectWrappers::GLTextureObj TmpGLTexWrapper(true, GLObjectWrappers::GLTextureCreateReleaseHelper(GLHandle));
    ContextState.BindTexture(-1, BindTarget, TmpGLTexWrapper);

    GLenum QueryBindTarget = BindTarget;
    if (BindTarget == GL_TEXTURE_CUBE_MAP)
        QueryBindTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X;


#if GL_TEXTURE_WIDTH
    GLint TexWidth = 0;
    glGetTexLevelParameteriv(QueryBindTarget, 0, GL_TEXTURE_WIDTH, &TexWidth);
    VERIFY_EXPR(TexWidth > 0);
    VERIFY(TexDesc.Width == 0 || TexDesc.Width == static_cast<Uint32>(TexWidth), "Specified texture width (", TexDesc.Width, ") does not match the actual width (", TexWidth, ")");
    TexDesc.Width = static_cast<Uint32>(TexWidth);
#else
    VERIFY(TexDesc.Width != 0, "Texture width query is not supported; it must be specified by the texture description.");
#endif
    
    if (TexDesc.Type >= RESOURCE_DIM_TEX_2D)
    {
#if GL_TEXTURE_HEIGHT
        GLint TexHeight = 0;
        glGetTexLevelParameteriv(QueryBindTarget, 0, GL_TEXTURE_HEIGHT, &TexHeight);
        VERIFY_EXPR(TexHeight > 0);

        VERIFY(TexDesc.Height == 0 || TexDesc.Height == static_cast<Uint32>(TexHeight), "Specified texture height (", TexDesc.Height,") does not match the actual height (", TexHeight, ")");
        TexDesc.Height = static_cast<Uint32>(TexHeight);
#else
        VERIFY(TexDesc.Height != 0, "Texture height query is not supported; it must be specified by the texture description.");
#endif
    }
    else
        TexDesc.Height = 1;

    if (TexDesc.Type == RESOURCE_DIM_TEX_3D)
    {
#if GL_TEXTURE_DEPTH
        GLint TexDepth = 0;
        glGetTexLevelParameteriv(QueryBindTarget, 0, GL_TEXTURE_DEPTH, &TexDepth);
        VERIFY_EXPR(TexDepth > 0);
        VERIFY(TexDesc.Depth == 0 || TexDesc.Depth == static_cast<Uint32>(TexDepth), "Specified texture depth (", TexDesc.Depth, ") does not match the actual depth (", TexDepth, ")");
        TexDesc.Depth = static_cast<Uint32>(TexDepth);
#else
        VERIFY(TexDesc.Depth != 0, "Texture depth query is not supported; it must be specified by the texture description.");
#endif
    }
    
    if (TexDesc.Type == RESOURCE_DIM_TEX_1D || TexDesc.Type == RESOURCE_DIM_TEX_2D)
        TexDesc.ArraySize = 1; // TexDesc.Depth also
    
#if GL_TEXTURE_INTERNAL_FORMAT
    GLint GlFormat = 0;
    glGetTexLevelParameteriv(QueryBindTarget, 0, GL_TEXTURE_INTERNAL_FORMAT, &GlFormat);
    CHECK_GL_ERROR( "Failed to get texture level 0 parameters through glGetTexLevelParameteriv()" );

    VERIFY(GlFormat != 0, "Unable to get texture format");
    if (TexDesc.Format != TEX_FORMAT_UNKNOWN)
        VERIFY(static_cast<GLenum>(GlFormat) == TexFormatToGLInternalTexFormat(TexDesc.Format), "Specified texture format (", GetTextureFormatAttribs(TexDesc.Format).Name,") does not match GL texture internal format (", GlFormat, ")");
    else
        TexDesc.Format = GLInternalTexFormatToTexFormat(GlFormat);
#else
    VERIFY(TexDesc.Format != TEX_FORMAT_UNKNOWN, "Texture format query is not supported; it must be specified by the texture description.");
#endif

    // GL_TEXTURE_IMMUTABLE_LEVELS is only supported in GL4.3+ and GLES3.1+
    GLint MipLevels = 0;
    glGetTexParameteriv(BindTarget, GL_TEXTURE_IMMUTABLE_LEVELS, &MipLevels);
    if(glGetError() == GL_NO_ERROR)
    {
        VERIFY(TexDesc.MipLevels == 0 || TexDesc.MipLevels == static_cast<Uint32>(MipLevels), "Specified number of mip levels (", TexDesc.MipLevels, ") does not match the actual number of mip levels (", MipLevels, ")");
        TexDesc.MipLevels = static_cast<Uint32>(MipLevels);
    }
    else
    {
        VERIFY(TexDesc.MipLevels != 0, "Unable to query the number of mip levels, so it must be specified by the texture description.");
    }
    
    ContextState.BindTexture(-1, BindTarget, GLObjectWrappers::GLTextureObj(false) );
    return TexDesc;
}

TextureBaseGL::TextureBaseGL(IReferenceCounters *pRefCounters, 
                             FixedBlockMemoryAllocator& TexViewObjAllocator, 
                             RenderDeviceGLImpl *pDeviceGL, 
                             DeviceContextGLImpl *pDeviceContext, 
                             const TextureDesc& TexDesc, 
                             GLuint GLTextureHandle,
                             GLenum BindTarget,
                             bool bIsDeviceInternal/* = false*/)  : 
    TTextureBase( pRefCounters, TexViewObjAllocator, pDeviceGL, GetTextureDescFromGLHandle(pDeviceContext, TexDesc, GLTextureHandle, BindTarget), bIsDeviceInternal ),
    // Create texture object wrapper, but use external texture handle
    m_GlTexture(true, GLObjectWrappers::GLTextureCreateReleaseHelper(GLTextureHandle)),
    m_BindTarget(BindTarget),
    m_GLTexFormat( GetTextureInternalFormat(pDeviceContext, BindTarget, m_GlTexture, TexDesc.Format) )
{
}

TextureBaseGL::~TextureBaseGL()
{
    // Release all FBOs that contain current texture
    // NOTE: we cannot check if BIND_RENDER_TARGET
    // flag is set, because CopyData() can bind
    // texture as render target even when no flag
    // is set
    static_cast<RenderDeviceGLImpl*>( GetDevice() )->OnReleaseTexture(this);
}

IMPLEMENT_QUERY_INTERFACE( TextureBaseGL, IID_TextureGL, TTextureBase )


void TextureBaseGL::CreateViewInternal( const struct TextureViewDesc &OrigViewDesc, class ITextureView **ppView, bool bIsDefaultView )
{
    VERIFY( ppView != nullptr, "Null pointer provided" );
    if( !ppView )return;
    VERIFY( *ppView == nullptr, "Overwriting reference to existing object may cause memory leaks" );

    *ppView = nullptr;

    try
    {
        auto ViewDesc = OrigViewDesc;
        CorrectTextureViewDesc(ViewDesc);

        auto *pDeviceGLImpl = ValidatedCast<RenderDeviceGLImpl>(GetDevice());
        auto &TexViewAllocator = pDeviceGLImpl->GetTexViewObjAllocator();
        VERIFY( &TexViewAllocator == &m_dbgTexViewObjAllocator, "Texture view allocator does not match allocator provided during texture initialization" );

        // http://www.opengl.org/wiki/Texture_Storage#Texture_views

        GLenum GLViewFormat = TexFormatToGLInternalTexFormat( ViewDesc.Format, m_Desc.BindFlags );
        VERIFY( GLViewFormat != 0, "Unsupported texture format" );
        
        TextureViewGLImpl *pViewOGL = nullptr;
        if( ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE )
        {
            bool bIsFullTextureView =
                ViewDesc.TextureDim     == m_Desc.Type       &&
                ViewDesc.Format          == GetDefaultTextureViewFormat( m_Desc.Format, ViewDesc.ViewType, m_Desc.BindFlags ) &&
                ViewDesc.MostDetailedMip == 0                &&
                ViewDesc.NumMipLevels    == m_Desc.MipLevels &&
                ViewDesc.FirstArraySlice == 0                &&
                ViewDesc.NumArraySlices  == m_Desc.ArraySize;

            pViewOGL = NEW_RC_OBJ(TexViewAllocator, "TextureViewGLImpl instance", TextureViewGLImpl, bIsDefaultView ? this : nullptr)(
                                               pDeviceGLImpl, ViewDesc, this, 
                                               !bIsFullTextureView, // Create OpenGL texture view object if view
                                                                    // does not address the whole texture
                                               bIsDefaultView
                                               );
            if( !bIsFullTextureView )
            {
                GLenum GLViewTarget = 0;
                GLuint NumLayers = ViewDesc.NumArraySlices;
                switch(ViewDesc.TextureDim)
                {
                    case RESOURCE_DIM_TEX_1D:
                        GLViewTarget = GL_TEXTURE_1D;
                        ViewDesc.NumArraySlices = NumLayers = 1;
                        break;
        
                    case RESOURCE_DIM_TEX_1D_ARRAY:
                        GLViewTarget = GL_TEXTURE_1D_ARRAY;
                        break;

                    case RESOURCE_DIM_TEX_2D:
                        GLViewTarget = m_Desc.SampleCount > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
                        ViewDesc.NumArraySlices = NumLayers = 1;
                        break;
        
                    case RESOURCE_DIM_TEX_2D_ARRAY:
                        GLViewTarget = m_Desc.SampleCount > 1 ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY;
                        break;

                    case RESOURCE_DIM_TEX_3D:
                    {
                        GLViewTarget = GL_TEXTURE_3D;
                        // If target is GL_TEXTURE_3D, NumLayers must equal 1.
                        Uint32 MipDepth = std::max(m_Desc.Depth >> ViewDesc.MostDetailedMip, 1U);
                        if(ViewDesc.FirstDepthSlice != 0 || ViewDesc.NumDepthSlices != MipDepth)
                        {
                            LOG_ERROR("3D texture view '", (ViewDesc.Name ? ViewDesc.Name : ""), "' (most detailed mip: ", ViewDesc.MostDetailedMip,
                                      "; mip levels: ", ViewDesc.NumMipLevels, "; first slice: ", ViewDesc.FirstDepthSlice,
                                      "; num depth slices: ", ViewDesc.NumDepthSlices, ") of texture '", m_Desc.Name, "' does not references"
                                      " all depth slices. 3D texture views in OpenGL must address all depth slices.");
                            ViewDesc.NumDepthSlices = MipDepth;
                            ViewDesc.FirstDepthSlice = 0;
                        }
                        NumLayers = 1;
                        break;
                    }

                    case RESOURCE_DIM_TEX_CUBE:
                        GLViewTarget = GL_TEXTURE_CUBE_MAP;
                        break;

                    case RESOURCE_DIM_TEX_CUBE_ARRAY:
                        GLViewTarget = GL_TEXTURE_CUBE_MAP_ARRAY;
                        break;

                    default: UNEXPECTED("Unsupported texture view type");
                }

                glTextureView( pViewOGL->GetHandle(), GLViewTarget, m_GlTexture, GLViewFormat, ViewDesc.MostDetailedMip, ViewDesc.NumMipLevels, ViewDesc.FirstArraySlice, NumLayers );
                CHECK_GL_ERROR_AND_THROW( "Failed to create texture view" );
                pViewOGL->SetBindTarget(GLViewTarget);
            }
        }
        else if( ViewDesc.ViewType == TEXTURE_VIEW_UNORDERED_ACCESS )
        {
            VERIFY( ViewDesc.NumArraySlices == 1 || 
                    m_Desc.Type == RESOURCE_DIM_TEX_3D && ViewDesc.NumDepthSlices == std::max(m_Desc.Depth >> ViewDesc.MostDetailedMip, 1U) ||
                    ViewDesc.NumArraySlices == m_Desc.ArraySize,
                    "Only single array/depth slice or the whole texture can be bound as UAV in OpenGL.");
            VERIFY( ViewDesc.AccessFlags != 0, "At least one access flag must be specified" );
            pViewOGL = NEW_RC_OBJ(TexViewAllocator, "TextureViewGLImpl instance", TextureViewGLImpl, bIsDefaultView ? this : nullptr)(
                                               pDeviceGLImpl, ViewDesc, this, 
                                               false, // Do NOT create texture view OpenGL object
                                               bIsDefaultView
                                               );
        }
        else if( ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET )
        {
            VERIFY( ViewDesc.NumMipLevels == 1, "Only single mip level can be bound as RTV" );
            pViewOGL = NEW_RC_OBJ(TexViewAllocator, "TextureViewGLImpl instance", TextureViewGLImpl, bIsDefaultView ? this : nullptr)(
                                               pDeviceGLImpl, ViewDesc, this, 
                                               false, // Do NOT create texture view OpenGL object
                                               bIsDefaultView
                                              );
        }
        else if( ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL )
        {
            VERIFY( ViewDesc.NumMipLevels == 1, "Only single mip level can be bound as DSV" );
            pViewOGL = NEW_RC_OBJ(TexViewAllocator, "TextureViewGLImpl instance", TextureViewGLImpl, bIsDefaultView ? this : nullptr)(
                                               pDeviceGLImpl, ViewDesc, this, 
                                               false, // Do NOT create texture view OpenGL object
                                               bIsDefaultView
                                              );
        }

        if( bIsDefaultView )
            *ppView = pViewOGL;
        else
        {
            if( pViewOGL )
            {
                pViewOGL->QueryInterface( IID_TextureView, reinterpret_cast<IObject**>(ppView) );
            }
        }
    }
    catch( const std::runtime_error& )
    {
        const auto *ViewTypeName = GetTexViewTypeLiteralName(OrigViewDesc.ViewType);
        LOG_ERROR("Failed to create view \"", OrigViewDesc.Name ? OrigViewDesc.Name : "", "\" (", ViewTypeName, ") for texture \"", m_Desc.Name ? m_Desc.Name : "", "\"" );
    }
}


void TextureBaseGL::UpdateData(  GLContextState& CtxState,  Uint32 MipLevel, Uint32 Slice, const Box &DstBox, const TextureSubResData &SubresData )
{
    // GL_TEXTURE_UPDATE_BARRIER_BIT:
    //      Writes to a texture via glTex( Sub )Image*, glCopyTex( Sub )Image*, glClearTex*Image, 
    //      glCompressedTex( Sub )Image*, and reads via glTexImage() after the barrier will reflect 
    //      data written by shaders prior to the barrier. Additionally, texture writes from these 
    //      commands issued after the barrier will not execute until all shader writes initiated prior 
    //      to the barrier complete
    TextureMemoryBarrier( GL_TEXTURE_UPDATE_BARRIER_BIT, CtxState );
}

//void TextureBaseGL :: UpdateData(Uint32 Offset, Uint32 Size, const PVoid pData)
//{
//    CTexture::UpdateData(Offset, Size, pData);
//    
//    glBindTexture(GL_ARRAY_Texture, m_GlTexture);
//    glTextureSubData(GL_ARRAY_Texture, Offset, Size, pData);
//    glBindTexture(GL_ARRAY_Texture, 0);
//}
//

void TextureBaseGL :: CopyData(DeviceContextGLImpl *pDeviceCtxGL, 
                               TextureBaseGL *pSrcTextureGL, 
                               Uint32 SrcMipLevel,
                               Uint32 SrcSlice,
                               const Box *pSrcBox,
                               Uint32 DstMipLevel,
                               Uint32 DstSlice,
                               Uint32 DstX,
                               Uint32 DstY,
                               Uint32 DstZ)
{
    const auto& SrcTexDesc = pSrcTextureGL->GetDesc();

    Box SrcBox;
    if( pSrcBox == nullptr )
    {
        SrcBox.MaxX = std::max( SrcTexDesc.Width >> SrcMipLevel, 1u );
        if( SrcTexDesc.Type == RESOURCE_DIM_TEX_1D || 
            SrcTexDesc.Type == RESOURCE_DIM_TEX_1D_ARRAY )
            SrcBox.MaxY = 1;
        else
            SrcBox.MaxY = std::max( SrcTexDesc.Height >> SrcMipLevel, 1u );

        if( SrcTexDesc.Type == RESOURCE_DIM_TEX_3D )
            SrcBox.MaxZ = std::max( SrcTexDesc.Depth >> SrcMipLevel, 1u );
        else
            SrcBox.MaxZ = 1;
        pSrcBox = &SrcBox;
    }

#if GL_ARB_copy_image
    if( glCopyImageSubData )
    {
        GLint SrcSliceY = (SrcTexDesc.Type == RESOURCE_DIM_TEX_1D_ARRAY) ? SrcSlice : 0;
        GLint SrcSliceZ = (SrcTexDesc.Type == RESOURCE_DIM_TEX_2D_ARRAY) ? SrcSlice : 0;
        GLint DstSliceY = (m_Desc.Type == RESOURCE_DIM_TEX_1D_ARRAY) ? DstSlice : 0;
        GLint DstSliceZ = (m_Desc.Type == RESOURCE_DIM_TEX_2D_ARRAY) ? DstSlice : 0;
        glCopyImageSubData(
            pSrcTextureGL->GetGLHandle(),
            pSrcTextureGL->GetBindTarget(),
            SrcMipLevel,
            pSrcBox->MinX,
            pSrcBox->MinY + SrcSliceY,
            pSrcBox->MinZ + SrcSliceZ, // Slice must be zero for 3D texture
            GetGLHandle(),
            GetBindTarget(),
            DstMipLevel,
            DstX,
            DstY + DstSliceY,
            DstZ + DstSliceZ, // Slice must be zero for 3D texture
            pSrcBox->MaxX - pSrcBox->MinX,
            pSrcBox->MaxY - pSrcBox->MinY,
            pSrcBox->MaxZ - pSrcBox->MinZ );
        CHECK_GL_ERROR( "glCopyImageSubData() failed" );
    }
    else
#endif
    {
        const auto &FmtAttribs = GetDevice()->GetTextureFormatInfoExt( m_Desc.Format );
        if( !FmtAttribs.ColorRenderable )
        {
            LOG_ERROR_MESSAGE( "Unable to perform copy operation because ", FmtAttribs.Name, " is not color renderable format" );
            return;
        }

        auto* pRenderDeviceGL = ValidatedCast<RenderDeviceGLImpl>(GetDevice());
#ifdef _DEBUG
        {
            auto& TexViewObjAllocator = pRenderDeviceGL->GetTexViewObjAllocator();
            VERIFY( &TexViewObjAllocator == &m_dbgTexViewObjAllocator, "Texture view allocator does not match allocator provided during texture initialization" );
        }
#endif
        auto &TexRegionRender = pRenderDeviceGL->m_TexRegionRender;
        TexRegionRender.SetStates(pDeviceCtxGL);

        // Create temporary SRV for the entire source texture
        TextureViewDesc SRVDesc;
        SRVDesc.TextureDim = SrcTexDesc.Type;
        SRVDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
        CorrectTextureViewDesc( SRVDesc );
        // Note: texture view allocates memory for the copy of the name
        // If the name is empty, memory should not be allocated
        // We have to provide allocator even though it will never be used
        TextureViewGLImpl SRV( GetReferenceCounters(), GetDevice(), SRVDesc, pSrcTextureGL,
            false, // Do NOT create texture view OpenGL object
            true   // The view, like default view, should not
                   // keep strong reference to the texture
            );

        for( Uint32 DepthSlice = 0; DepthSlice < pSrcBox->MaxZ - pSrcBox->MinZ; ++DepthSlice )
        {
            // Create temporary RTV for the target subresource
            TextureViewDesc RTVDesc;
            RTVDesc.TextureDim = m_Desc.Type;
            RTVDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
            RTVDesc.FirstArraySlice = DepthSlice + DstSlice;
            RTVDesc.MostDetailedMip = DstMipLevel;
            RTVDesc.NumArraySlices = 1;
            CorrectTextureViewDesc( RTVDesc );
            // Note: texture view allocates memory for the copy of the name
            // If the name is empty, memory should not be allocated
            // We have to provide allocator even though it will never be used
            TextureViewGLImpl RTV( GetReferenceCounters(), GetDevice(), RTVDesc, this,
                false, // Do NOT create texture view OpenGL object
                true   // The view, like default view, should not
                       // keep strong reference to the texture
                );

            ITextureView *pRTVs[] = { &RTV };
            pDeviceCtxGL->SetRenderTargets( _countof( pRTVs ), pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );

            // No need to set up the viewport as SetRenderTargets() does that

            TexRegionRender.Render( pDeviceCtxGL,
                                    &SRV,
                                    SrcTexDesc.Type,
                                    SrcTexDesc.Format,
                                    static_cast<Int32>(pSrcBox->MinX) - static_cast<Int32>(DstX),
                                    static_cast<Int32>(pSrcBox->MinY) - static_cast<Int32>(DstY),
                                    SrcSlice + pSrcBox->MinZ + DepthSlice,
                                    SrcMipLevel );
        }

        TexRegionRender.RestoreStates(pDeviceCtxGL);
    }
}


void TextureBaseGL::TextureMemoryBarrier( Uint32 RequiredBarriers, GLContextState &GLContextState )
{
#if GL_ARB_shader_image_load_store
    #ifdef _DEBUG
    {
        constexpr Uint32 TextureBarriers =
            GL_TEXTURE_FETCH_BARRIER_BIT       |
            GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
            GL_PIXEL_BUFFER_BARRIER_BIT        |
            GL_TEXTURE_UPDATE_BARRIER_BIT      |
            GL_FRAMEBUFFER_BARRIER_BIT;
        VERIFY( (RequiredBarriers & TextureBarriers) != 0, "At least one texture memory barrier flag should be set" );
        VERIFY( (RequiredBarriers & ~TextureBarriers) == 0, "Inappropriate texture memory barrier flag" );
    }
    #endif
    
    GLContextState.EnsureMemoryBarrier( RequiredBarriers, this );
#endif
}

void TextureBaseGL::SetDefaultGLParameters()
{
#ifdef _DEBUG
    GLint BoundTex;
    GLint TextureBinding = 0;
    switch( m_BindTarget )
    {
        case GL_TEXTURE_1D:         TextureBinding = GL_TEXTURE_BINDING_1D;       break;
        case GL_TEXTURE_1D_ARRAY:   TextureBinding = GL_TEXTURE_BINDING_1D_ARRAY; break;
        case GL_TEXTURE_2D:         TextureBinding = GL_TEXTURE_BINDING_2D;       break;
        case GL_TEXTURE_2D_ARRAY:   TextureBinding = GL_TEXTURE_BINDING_2D_ARRAY; break;
        case GL_TEXTURE_2D_MULTISAMPLE:         TextureBinding = GL_TEXTURE_BINDING_2D_MULTISAMPLE;       break;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:   TextureBinding = GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY; break;
        case GL_TEXTURE_3D:         TextureBinding = GL_TEXTURE_BINDING_3D;       break;
        case GL_TEXTURE_CUBE_MAP:   TextureBinding = GL_TEXTURE_BINDING_CUBE_MAP; break;
        case GL_TEXTURE_CUBE_MAP_ARRAY: TextureBinding = GL_TEXTURE_BINDING_CUBE_MAP_ARRAY; break;
        default: UNEXPECTED("Unknown bind target");
    }
    glGetIntegerv( TextureBinding, &BoundTex );
    CHECK_GL_ERROR( "Failed to set GL_TEXTURE_MIN_FILTER texture parameter" );
    VERIFY( static_cast<GLuint>(BoundTex) == m_GlTexture, "Current texture is not bound to GL context" );
#endif

    if( m_BindTarget != GL_TEXTURE_2D_MULTISAMPLE &&
        m_BindTarget != GL_TEXTURE_2D_MULTISAMPLE_ARRAY )
    {
        // Note that texture bound to image unit must be complete.
        // That means that if an integer texture is being bound, its 
        // GL_TEXTURE_MIN_FILTER and GL_TEXTURE_MAG_FILTER must be NEAREST,
        // otherwise it will be incomplete

        // The default value of GL_TEXTURE_MIN_FILTER is GL_NEAREST_MIPMAP_LINEAR
        // Reset it to GL_NEAREST to avoid incompletness issues with integer textures
        glTexParameteri( m_BindTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
        CHECK_GL_ERROR( "Failed to set GL_TEXTURE_MIN_FILTER texture parameter" );

        // The default value of GL_TEXTURE_MAG_FILTER is GL_LINEAR
        glTexParameteri( m_BindTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
        CHECK_GL_ERROR( "Failed to set GL_TEXTURE_MAG_FILTER texture parameter" );
    }
}

}
