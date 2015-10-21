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
/// Implementation of the Diligent::TextureBase template class

#include "Texture.h"
#include "DeviceObjectBase.h"
#include "GraphicsUtilities.h"
#include <memory>

namespace Diligent
{

void ValidateTextureDesc(const TextureDesc& TexDesc);
void ValidateUpdateDataParams( const TextureDesc &TexDesc, Uint32 MipLevel, Uint32 Slice, const Box &DstBox, const TextureSubResData &SubresData );
void VliadateCopyTextureDataParams( const TextureDesc &SrcTexDesc, Uint32 SrcMipLevel, Uint32 SrcSlice, const Box *pSrcBox,
                                    const TextureDesc &DstTexDesc, Uint32 DstMipLevel, Uint32 DstSlice,
                                    Uint32 DstX, Uint32 DstY, Uint32 DstZ );

/// Base implementation of the ITexture interface

/// \tparam BaseInterface - base interface that this class will inheret 
///                         (Diligent::ITextureD3D11 or Diligent::ITextureGL).
/// \tparam TTextureViewImpl - type of the texture view implementation 
///                            (Diligent::TextureViewD3D11Impl or Diligent::TextureViewGLImpl).
template<class BaseInterface, class TTextureViewImpl>
class TextureBase : public DeviceObjectBase<BaseInterface, TextureDesc>
{
public:
    typedef DeviceObjectBase<BaseInterface, TextureDesc> TDeviceObjectBase;

	/// \param pDevice - pointer to the device
	/// \param Desc - texture description
	/// \param bIsDeviceInternal - flag indicating if the texture is an internal device object and 
	///							   must not keep a strong reference to the device
    TextureBase( IRenderDevice *pDevice, const TextureDesc& Desc, bool bIsDeviceInternal = false ) :
        TDeviceObjectBase( pDevice, Desc, nullptr, bIsDeviceInternal )
    {
        if( this->m_Desc.MipLevels == 0 )
        {
            // Compute the number of levels in the full mipmap chain based 
            // on the maximum texture dimension
            Uint32 MaxDim = 0; 
            if( this->m_Desc.Type == TEXTURE_TYPE_1D ||
                this->m_Desc.Type == TEXTURE_TYPE_1D_ARRAY )
            {
                MaxDim = this->m_Desc.Width;
            }
            else if( this->m_Desc.Type == TEXTURE_TYPE_2D ||
                     this->m_Desc.Type == TEXTURE_TYPE_2D_ARRAY )
            { 
                MaxDim = std::max( this->m_Desc.Width, this->m_Desc.Height );
            }
            else if( this->m_Desc.Type == TEXTURE_TYPE_3D )
            {
                MaxDim = std::max( this->m_Desc.Width, this->m_Desc.Height );
                MaxDim = std::max( MaxDim, this->m_Desc.Depth );
            }
            else
            {
                UNEXPECTED( "Unkwnown texture type" );
            }
            while( (MaxDim >> this->m_Desc.MipLevels) > 0 )
                ++this->m_Desc.MipLevels;
            VERIFY( MaxDim >= (1U << (this->m_Desc.MipLevels-1)) && MaxDim < (1U << this->m_Desc.MipLevels), "Incorrect number of Mip levels" )
        }

        // Validate correctness of texture description
        ValidateTextureDesc( this->m_Desc );
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_Texture, TDeviceObjectBase )
   
    virtual void CreateView( const struct TextureViewDesc &ViewDesc, ITextureView **ppView )override
    {
        CreateViewInternal( ViewDesc, ppView, false );
    }

    virtual void UpdateData( IDeviceContext *pContext, Uint32 MipLevel, Uint32 Slice, const Box &DstBox, const TextureSubResData &SubresData ) = 0;


    virtual void CopyData( IDeviceContext *pContext,
                           ITexture *pSrcTexture,
                           Uint32 SrcMipLevel,
                           Uint32 SrcSlice,
                           const Box *pSrcBox,
                           Uint32 DstMipLevel,
                           Uint32 DstSlice,
                           Uint32 DstX,
                           Uint32 DstY,
                           Uint32 DstZ ) = 0;

    virtual void Map( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData ) = 0;

    virtual void Unmap( IDeviceContext *pContext ) = 0;

    void CreateDefaultViews();

protected:
    
    virtual void CreateViewInternal( const struct TextureViewDesc &ViewDesc, ITextureView **ppView, bool bIsDefaultView ) = 0;

    // WARNING! We cannot use ITextureView here, because ITextureView has no virtual dtor!
    std::unique_ptr<TTextureViewImpl> m_pDefaultSRV;
    std::unique_ptr<TTextureViewImpl> m_pDefaultRTV;
    std::unique_ptr<TTextureViewImpl> m_pDefaultDSV;
    std::unique_ptr<TTextureViewImpl> m_pDefaultUAV;

    ITextureView* GetDefaultView( TEXTURE_VIEW_TYPE ViewType )override
    {
        switch( ViewType )
        {
            case TEXTURE_VIEW_SHADER_RESOURCE:  return m_pDefaultSRV.get();
            case TEXTURE_VIEW_RENDER_TARGET:    return m_pDefaultRTV.get();
            case TEXTURE_VIEW_DEPTH_STENCIL:    return m_pDefaultDSV.get();
            case TEXTURE_VIEW_UNORDERED_ACCESS: return m_pDefaultUAV.get();
            default: UNEXPECTED( "Unknown view type" ); return nullptr;
        }
    }

    void CorrectTextureViewDesc( struct TextureViewDesc &ViewDesc );

    // Corrects texture view format. For instance, if texture format is R32_TYPELESS,
    // then for a depth-stencil view, the function will return D32_FLOAT, but for a shader resource
    // view, it will return R32_FLOAT.
    TEXTURE_FORMAT CorrectTextureViewFormat( TEXTURE_FORMAT Fmt, TEXTURE_VIEW_TYPE TexViewType );
};


template<class BaseInterface, class TTextureViewImpl>
TEXTURE_FORMAT TextureBase<BaseInterface, TTextureViewImpl> :: CorrectTextureViewFormat( TEXTURE_FORMAT Fmt, TEXTURE_VIEW_TYPE TexViewType )
{
    // Correct texture view format for depth stencil view
    if( TexViewType == TEXTURE_VIEW_DEPTH_STENCIL )
    {
        if( Fmt == TEX_FORMAT_R32_FLOAT || Fmt == TEX_FORMAT_R32_TYPELESS )
        {
            Fmt = TEX_FORMAT_D32_FLOAT;
        }
        else if( Fmt == TEX_FORMAT_R16_UNORM || Fmt == TEX_FORMAT_R16_TYPELESS )
        {
            Fmt = TEX_FORMAT_D16_UNORM;
        }
        else if( Fmt == TEX_FORMAT_R24G8_TYPELESS || 
                 Fmt == TEX_FORMAT_R24_UNORM_X8_TYPELESS ||
                 Fmt == TEX_FORMAT_X24_TYPELESS_G8_UINT )
        {
            Fmt = TEX_FORMAT_D24_UNORM_S8_UINT;
        }
        else if( Fmt == TEX_FORMAT_R32G8X24_TYPELESS ||
                 Fmt == TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
                 Fmt == TEX_FORMAT_X32_TYPELESS_G8X24_UINT )
        {
            Fmt = TEX_FORMAT_D32_FLOAT_S8X24_UINT;
        }
    }
    else
    {
        // Correct texture view format for SR, RT or UA views
        if( Fmt == TEX_FORMAT_D32_FLOAT || Fmt == TEX_FORMAT_R32_TYPELESS )
        {
            Fmt = TEX_FORMAT_R32_FLOAT;
        }
        else if( Fmt == TEX_FORMAT_D16_UNORM || Fmt == TEX_FORMAT_R16_TYPELESS )
        {
            Fmt = TEX_FORMAT_R16_UNORM;
        }
        else if( Fmt == TEX_FORMAT_D24_UNORM_S8_UINT ||
                 Fmt == TEX_FORMAT_R24G8_TYPELESS )
        {
            Fmt = TEX_FORMAT_R24_UNORM_X8_TYPELESS;
        }
        else if( Fmt == TEX_FORMAT_D32_FLOAT_S8X24_UINT ||
                 Fmt == TEX_FORMAT_R32G8X24_TYPELESS )
        {
            Fmt = TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        }
    }

    return Fmt;
}

template<class BaseInterface, class TTextureViewImpl>
void TextureBase<BaseInterface, TTextureViewImpl> :: CorrectTextureViewDesc(struct TextureViewDesc &ViewDesc)
{
#define TEX_VIEW_VALIDATION_ERROR(...) LOG_ERROR_AND_THROW( "Texture view \"", ViewDesc.Name ? ViewDesc.Name : "", "\": ", ##__VA_ARGS__ );

    if( !(ViewDesc.ViewType > TEXTURE_VIEW_UNDEFINED && ViewDesc.ViewType < TEXTURE_VIEW_NUM_VIEWS) )
        TEX_VIEW_VALIDATION_ERROR( "Texture view type is not specified" );

    if( ViewDesc.MostDetailedMip + ViewDesc.NumMipLevels > this->m_Desc.MipLevels )
        TEX_VIEW_VALIDATION_ERROR( "Most detailed mip (", ViewDesc.MostDetailedMip, ") and number of mip levels in the view (", ViewDesc.NumMipLevels, ") specify more levels than target texture has (", this->m_Desc.MipLevels, ")" );

    if( ViewDesc.Format == TEX_FORMAT_UNKNOWN )
        ViewDesc.Format = CorrectTextureViewFormat( this->m_Desc.Format, ViewDesc.ViewType );

    if( ViewDesc.TextureType == TEXTURE_TYPE_UNDEFINED )
        ViewDesc.TextureType = this->m_Desc.Type;

    switch( this->m_Desc.Type )
    {
        case TEXTURE_TYPE_1D:
            if( ViewDesc.TextureType != TEXTURE_TYPE_1D )
            {
                TEX_VIEW_VALIDATION_ERROR( "Incorrect texture type for Texture 1D view: only Texture 1D is allowed" );
            }
        break;

        case TEXTURE_TYPE_1D_ARRAY:
            if( ViewDesc.TextureType != TEXTURE_TYPE_1D_ARRAY && 
                ViewDesc.TextureType != TEXTURE_TYPE_1D )
            {
                TEX_VIEW_VALIDATION_ERROR( "Incorrect view type for Texture 1D Array: only Texture 1D or Texture 1D Array are allowed" );
            }
        break;

        case TEXTURE_TYPE_2D:
            if(ViewDesc.TextureType != TEXTURE_TYPE_2D )
            {
                TEX_VIEW_VALIDATION_ERROR( "Incorrect texture type for Texture 2D view: only Texture 2D is allowed" );
            }
        break;

        case TEXTURE_TYPE_2D_ARRAY:
            if( ViewDesc.TextureType != TEXTURE_TYPE_2D_ARRAY && 
                ViewDesc.TextureType != TEXTURE_TYPE_2D )
            {
                TEX_VIEW_VALIDATION_ERROR( "Incorrect texture type for Texture 2D Array view: only Texture 2D or Texture 2D Array are allowed" );
            }
        break;

        case TEXTURE_TYPE_3D:
            if( ViewDesc.TextureType != TEXTURE_TYPE_3D )
            {
                TEX_VIEW_VALIDATION_ERROR( "Incorrect texture type for Texture 3D view: only Texture 3D is allowed" );
            }
        break;

        default:
            UNEXPECTED( "Unexpected texture type" );
        break;
    }

    if( ViewDesc.TextureType == TEXTURE_TYPE_1D_ARRAY || 
        ViewDesc.TextureType == TEXTURE_TYPE_2D_ARRAY )
    {
        if( ViewDesc.FirstArraySlice + ViewDesc.NumArraySlices > this->m_Desc.ArraySize )
            TEX_VIEW_VALIDATION_ERROR( "First slice (", ViewDesc.FirstArraySlice, ") and number of slices in the view (", ViewDesc.NumArraySlices, ") specify more slices than target texture has (", this->m_Desc.ArraySize, ")" );
    }
    else if( ViewDesc.TextureType == TEXTURE_TYPE_3D )
    {
        auto MipDepth = this->m_Desc.Depth >> ViewDesc.MostDetailedMip;
        if( ViewDesc.FirstDepthSlice + ViewDesc.NumDepthSlices > MipDepth )
            TEX_VIEW_VALIDATION_ERROR( "First slice (", ViewDesc.FirstDepthSlice, ") and number of slices in the view (", ViewDesc.NumDepthSlices, ") specify more slices than target 3D texture mip level has (", MipDepth, ")" );
    }
#undef TEX_VIEW_VALIDATION_ERROR

    if( ViewDesc.NumMipLevels == 0 )
    {
        if( ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE )
            ViewDesc.NumMipLevels = this->m_Desc.MipLevels - ViewDesc.MostDetailedMip;
        else
            ViewDesc.NumMipLevels = 1;
    }
        
    if( ViewDesc.NumArraySlices == 0 )
    {
        if( ViewDesc.TextureType == TEXTURE_TYPE_1D_ARRAY || 
            ViewDesc.TextureType == TEXTURE_TYPE_2D_ARRAY )
            ViewDesc.NumArraySlices = this->m_Desc.ArraySize - ViewDesc.FirstArraySlice;
        else if( ViewDesc.TextureType == TEXTURE_TYPE_3D )
        {
            auto MipDepth = this->m_Desc.Depth >> ViewDesc.MostDetailedMip;
            ViewDesc.NumDepthSlices = MipDepth - ViewDesc.FirstDepthSlice;
        }
        else
            ViewDesc.NumArraySlices = 1;
    }

    if( (ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET)  && 
        ( ViewDesc.Format == TEX_FORMAT_R8_SNORM  || ViewDesc.Format == TEX_FORMAT_RG8_SNORM  || ViewDesc.Format == TEX_FORMAT_RGBA8_SNORM ||
          ViewDesc.Format == TEX_FORMAT_R16_SNORM || ViewDesc.Format == TEX_FORMAT_RG16_SNORM || ViewDesc.Format == TEX_FORMAT_RGBA16_SNORM ) )
    {
        const auto *FmtName = GetTextureFormatAttribs( ViewDesc.Format ).Name;
        LOG_WARNING_MESSAGE( FmtName, " render target view is created.\n" 
                             "There might be an issue in OpenGL driver on NVidia hardware: when rendering to SNORM textures, all negative values are clamped to zero.\n"
                             "Use UNORM format instead." );
    }
}

template<class BaseInterface, class TTextureViewImpl>
void TextureBase<BaseInterface, TTextureViewImpl> :: CreateDefaultViews()
{
    if(this->m_Desc.BindFlags & BIND_SHADER_RESOURCE )
    {
        TextureViewDesc ViewDesc;
        ViewDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
        ITextureView *pSRV = nullptr;
        CreateViewInternal( ViewDesc, &pSRV, true );
        m_pDefaultSRV.reset( static_cast<TTextureViewImpl*>(pSRV) );
        VERIFY( m_pDefaultSRV->GetDesc().ViewType == TEXTURE_VIEW_SHADER_RESOURCE, "Unexpected view type" );
    }

    if(this->m_Desc.BindFlags & BIND_RENDER_TARGET )
    {
        TextureViewDesc ViewDesc;
        ViewDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
        ITextureView *pRTV = nullptr;
        CreateViewInternal( ViewDesc, &pRTV, true );
        m_pDefaultRTV.reset( static_cast<TTextureViewImpl*>(pRTV) );
        VERIFY( m_pDefaultRTV->GetDesc().ViewType == TEXTURE_VIEW_RENDER_TARGET, "Unexpected view type" );
    }

    if(this->m_Desc.BindFlags & BIND_DEPTH_STENCIL )
    {
        TextureViewDesc ViewDesc;
        ViewDesc.ViewType = TEXTURE_VIEW_DEPTH_STENCIL;
        ITextureView *pDSV = nullptr;
        CreateViewInternal( ViewDesc, &pDSV, true );
        m_pDefaultDSV.reset( static_cast<TTextureViewImpl*>(pDSV) );
        VERIFY( m_pDefaultDSV->GetDesc().ViewType == TEXTURE_VIEW_DEPTH_STENCIL, "Unexpected view type" );
    }

    if(this->m_Desc.BindFlags & BIND_UNORDERED_ACCESS )
    {
        TextureViewDesc ViewDesc;
        ViewDesc.ViewType = TEXTURE_VIEW_UNORDERED_ACCESS;
        ViewDesc.AccessFlags = UAV_ACCESS_FLAG_READ_WRITE;
        ITextureView *pUAV = nullptr;
        CreateViewInternal( ViewDesc, &pUAV, true );
        m_pDefaultUAV.reset( static_cast<TTextureViewImpl*>(pUAV) );
        VERIFY( m_pDefaultUAV->GetDesc().ViewType == TEXTURE_VIEW_UNORDERED_ACCESS, "Unexpected view type" );
   }
}


template<class BaseInterface, class TTextureViewImpl>
void TextureBase<BaseInterface, TTextureViewImpl> :: UpdateData( IDeviceContext *pContext, Uint32 MipLevel, Uint32 Slice, const Box &DstBox, const TextureSubResData &SubresData )
{
    ValidateUpdateDataParams( this->m_Desc, MipLevel, Slice, DstBox, SubresData );
}

template<class BaseInterface, class TTextureViewImpl>
void TextureBase<BaseInterface, TTextureViewImpl> :: CopyData( IDeviceContext *pContext,
                                                                ITexture *pSrcTexture,
                                                                Uint32 SrcMipLevel,
                                                                Uint32 SrcSlice,
                                                                const Box *pSrcBox,
                                                                Uint32 DstMipLevel,
                                                                Uint32 DstSlice,
                                                                Uint32 DstX,
                                                                Uint32 DstY,
                                                                Uint32 DstZ )
{
    VERIFY( pContext, "pContext is null" );
    VERIFY( pSrcTexture, "pSrcTexture is null" );
    VliadateCopyTextureDataParams( pSrcTexture->GetDesc(), SrcMipLevel, SrcSlice, pSrcBox,
                                   this->GetDesc(), DstMipLevel, DstSlice, DstX, DstY, DstZ );
}

template<class BaseInterface, class TTextureViewImpl>
void TextureBase<BaseInterface, TTextureViewImpl> :: Map( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData )
{
}

template<class BaseInterface, class TTextureViewImpl>
void TextureBase<BaseInterface, TTextureViewImpl> :: Unmap( IDeviceContext *pContext )
{
}

}
