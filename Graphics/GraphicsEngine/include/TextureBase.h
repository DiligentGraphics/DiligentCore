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
/// Implementation of the Diligent::TextureBase template class

#include "Texture.h"
#include "DeviceObjectBase.h"
#include "GraphicsAccessories.h"
#include "STDAllocator.h"
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
///                         (Diligent::ITextureD3D11, Diligent::ITextureD3D12 or Diligent::ITextureGL).
/// \tparam TTextureViewImpl - type of the texture view implementation 
///                            (Diligent::TextureViewD3D11Impl, Diligent::TextureViewD3D12Impl or Diligent::TextureViewGLImpl).
/// \tparam TTexViewObjAllocator - type of the allocator that is used to allocate memory for the texture view object instances
template<class BaseInterface, class TTextureViewImpl, class TTexViewObjAllocator>
class TextureBase : public DeviceObjectBase<BaseInterface, TextureDesc>
{
public:
    typedef DeviceObjectBase<BaseInterface, TextureDesc> TDeviceObjectBase;

    /// \param pRefCounters - reference counters object that controls the lifetime of this texture.
    /// \param TexViewObjAllocator - allocator that is used to allocate memory for the instances of the texture view object.
    ///                              This parameter is only used for debug purposes.
	/// \param pDevice - pointer to the device
	/// \param Desc - texture description
	/// \param bIsDeviceInternal - flag indicating if the texture is an internal device object and 
	///							   must not keep a strong reference to the device
    TextureBase( IReferenceCounters *pRefCounters, 
                 TTexViewObjAllocator &TexViewObjAllocator,
                 IRenderDevice *pDevice, 
                 const TextureDesc& Desc, 
                 bool bIsDeviceInternal = false ) :
        TDeviceObjectBase( pRefCounters, pDevice, Desc, bIsDeviceInternal ),
#ifdef _DEBUG
        m_dbgTexViewObjAllocator(TexViewObjAllocator),
#endif
        m_pDefaultSRV(nullptr, STDDeleter<TTextureViewImpl, TTexViewObjAllocator>(TexViewObjAllocator)),
        m_pDefaultRTV(nullptr, STDDeleter<TTextureViewImpl, TTexViewObjAllocator>(TexViewObjAllocator)),
        m_pDefaultDSV(nullptr, STDDeleter<TTextureViewImpl, TTexViewObjAllocator>(TexViewObjAllocator)),
        m_pDefaultUAV(nullptr, STDDeleter<TTextureViewImpl, TTexViewObjAllocator>(TexViewObjAllocator))
    {
        if( this->m_Desc.MipLevels == 0 )
        {
            // Compute the number of levels in the full mipmap chain
            if( this->m_Desc.Type == RESOURCE_DIM_TEX_1D ||
                this->m_Desc.Type == RESOURCE_DIM_TEX_1D_ARRAY )
            {
                this->m_Desc.MipLevels = ComputeMipLevelsCount(this->m_Desc.Width);
            }
            else if( this->m_Desc.Type == RESOURCE_DIM_TEX_2D ||
                     this->m_Desc.Type == RESOURCE_DIM_TEX_2D_ARRAY || 
                     this->m_Desc.Type == RESOURCE_DIM_TEX_CUBE || 
                     this->m_Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY )
            { 
                this->m_Desc.MipLevels = ComputeMipLevelsCount( this->m_Desc.Width, this->m_Desc.Height );
            }
            else if( this->m_Desc.Type == RESOURCE_DIM_TEX_3D )
            {
                this->m_Desc.MipLevels = ComputeMipLevelsCount( this->m_Desc.Width, this->m_Desc.Height, this->m_Desc.Depth );
            }
            else
            {
                UNEXPECTED( "Unkwnown texture type" );
            }
        }

        // Validate correctness of texture description
        ValidateTextureDesc( this->m_Desc );
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_Texture, TDeviceObjectBase )
   
    /// Implementaiton of ITexture::CreateView(); calls CreateViewInternal() virtual function that
    /// creates texture view for the specific engine implementation.
    virtual void CreateView( const struct TextureViewDesc &ViewDesc, ITextureView **ppView )override
    {
        CreateViewInternal( ViewDesc, ppView, false );
    }

    /// Base implementaiton of ITexture::UpdateData(); validates input parameters
    virtual void UpdateData( IDeviceContext *pContext, Uint32 MipLevel, Uint32 Slice, const Box &DstBox, const TextureSubResData &SubresData )override = 0;

    /// Base implementaiton of ITexture::CopyData(); validates input parameters
    virtual void CopyData( IDeviceContext *pContext,
                           ITexture *pSrcTexture,
                           Uint32 SrcMipLevel,
                           Uint32 SrcSlice,
                           const Box *pSrcBox,
                           Uint32 DstMipLevel,
                           Uint32 DstSlice,
                           Uint32 DstX,
                           Uint32 DstY,
                           Uint32 DstZ )override = 0;

    /// Base implementaiton of ITexture::Map()
    virtual void Map( IDeviceContext *pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags, MappedTextureSubresource &MappedData )override = 0;

    /// Base implementaiton of ITexture::Unmap()
    virtual void Unmap( IDeviceContext *pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags )override = 0;

    /// Creates default texture views.

    ///
    /// - Creates default shader resource view addressing the entire texture if Diligent::BIND_SHADER_RESOURCE flag is set.
    /// - Creates default render target view addressing the most detailed mip level if Diligent::BIND_RENDER_TARGET flag is set.
    /// - Creates default depth-stencil view addressing the most detailed mip level if Diligent::BIND_DEPTH_STENCIL flag is set.
    /// - Creates default unordered access view addressing the entire texture if Diligent::BIND_UNORDERED_ACCESS flag is set.
    ///
    /// The function calls CreateViewInternal().
    void CreateDefaultViews();

protected:
    
    /// Pure virtual function that creates texture view for the specific engine implementation.
    virtual void CreateViewInternal( const struct TextureViewDesc &ViewDesc, ITextureView **ppView, bool bIsDefaultView ) = 0;

#ifdef _DEBUG
    TTexViewObjAllocator &m_dbgTexViewObjAllocator;
#endif
    // WARNING! We cannot use ITextureView here, because ITextureView has no virtual dtor!
    /// Default SRV addressing the entire texture
    std::unique_ptr<TTextureViewImpl, STDDeleter<TTextureViewImpl, TTexViewObjAllocator>> m_pDefaultSRV;
    /// Default RTV addressing the most detailed mip level
    std::unique_ptr<TTextureViewImpl, STDDeleter<TTextureViewImpl, TTexViewObjAllocator>> m_pDefaultRTV;
    /// Default DSV addressing the most detailed mip level
    std::unique_ptr<TTextureViewImpl, STDDeleter<TTextureViewImpl, TTexViewObjAllocator>> m_pDefaultDSV;
    /// Default UAV addressing the entire texture
    std::unique_ptr<TTextureViewImpl, STDDeleter<TTextureViewImpl, TTexViewObjAllocator>> m_pDefaultUAV;

    /// Implementation of ITexture::GetDefaultView().
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
};


template<class BaseInterface, class TTextureViewImpl, class TTexViewObjAllocator>
void TextureBase<BaseInterface, TTextureViewImpl, TTexViewObjAllocator> :: CorrectTextureViewDesc(struct TextureViewDesc &ViewDesc)
{
#define TEX_VIEW_VALIDATION_ERROR(...) LOG_ERROR_AND_THROW( "Texture view \"", ViewDesc.Name ? ViewDesc.Name : "", "\": ", ##__VA_ARGS__ )

    if( !(ViewDesc.ViewType > TEXTURE_VIEW_UNDEFINED && ViewDesc.ViewType < TEXTURE_VIEW_NUM_VIEWS) )
        TEX_VIEW_VALIDATION_ERROR( "Texture view type is not specified" );

    if( ViewDesc.MostDetailedMip + ViewDesc.NumMipLevels > this->m_Desc.MipLevels )
        TEX_VIEW_VALIDATION_ERROR( "Most detailed mip (", ViewDesc.MostDetailedMip, ") and number of mip levels in the view (", ViewDesc.NumMipLevels, ") specify more levels than target texture has (", this->m_Desc.MipLevels, ")" );

    if( ViewDesc.Format == TEX_FORMAT_UNKNOWN )
        ViewDesc.Format = GetDefaultTextureViewFormat( this->m_Desc.Format, ViewDesc.ViewType, this->m_Desc.BindFlags );

    if( ViewDesc.TextureDim == RESOURCE_DIM_UNDEFINED )
    {
        if (this->m_Desc.Type == RESOURCE_DIM_TEX_CUBE || this->m_Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
        {
            switch (ViewDesc.ViewType)
            {
                case TEXTURE_VIEW_SHADER_RESOURCE: 
                    ViewDesc.TextureDim = this->m_Desc.Type;
                break;

                case TEXTURE_VIEW_RENDER_TARGET: 
                case TEXTURE_VIEW_DEPTH_STENCIL: 
                case TEXTURE_VIEW_UNORDERED_ACCESS: 
                    ViewDesc.TextureDim = RESOURCE_DIM_TEX_2D_ARRAY;
                break;

                default: UNEXPECTED("Unexpected view type");
            }
        }
        else
        {
            ViewDesc.TextureDim = this->m_Desc.Type;
        }
    }

    switch( this->m_Desc.Type )
    {
        case RESOURCE_DIM_TEX_1D:
            if( ViewDesc.TextureDim != RESOURCE_DIM_TEX_1D )
            {
                TEX_VIEW_VALIDATION_ERROR( "Incorrect texture type for Texture 1D view: only Texture 1D is allowed" );
            }
        break;

        case RESOURCE_DIM_TEX_1D_ARRAY:
            if( ViewDesc.TextureDim != RESOURCE_DIM_TEX_1D_ARRAY && 
                ViewDesc.TextureDim != RESOURCE_DIM_TEX_1D )
            {
                TEX_VIEW_VALIDATION_ERROR( "Incorrect view type for Texture 1D Array: only Texture 1D or Texture 1D Array are allowed" );
            }
        break;

        case RESOURCE_DIM_TEX_2D:
            if(ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D )
            {
                TEX_VIEW_VALIDATION_ERROR( "Incorrect texture type for Texture 2D view: only Texture 2D is allowed" );
            }
        break;

        case RESOURCE_DIM_TEX_2D_ARRAY:
            if( ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D_ARRAY && 
                ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D )
            {
                TEX_VIEW_VALIDATION_ERROR( "Incorrect texture type for Texture 2D Array view: only Texture 2D or Texture 2D Array are allowed" );
            }
        break;

        case RESOURCE_DIM_TEX_3D:
            if( ViewDesc.TextureDim != RESOURCE_DIM_TEX_3D )
            {
                TEX_VIEW_VALIDATION_ERROR( "Incorrect texture type for Texture 3D view: only Texture 3D is allowed" );
            }
        break;

        case RESOURCE_DIM_TEX_CUBE:
            if(ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE)
            {
                if(ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D &&
                   ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D_ARRAY && 
                   ViewDesc.TextureDim != RESOURCE_DIM_TEX_CUBE)
                {
                    TEX_VIEW_VALIDATION_ERROR( "Incorrect texture type for Texture cube SRV: Texture 2D, Texture 2D array or Texture Cube is allowed" );
                }
            }
            else
            {
                if(ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D &&
                   ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D_ARRAY)
                {
                    TEX_VIEW_VALIDATION_ERROR( "Incorrect texture type for Texture cube non-shader resource view: Texture 2D or Texture 2D array is allowed" );
                }
            }
        break;

        case RESOURCE_DIM_TEX_CUBE_ARRAY:
            if(ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE)
            {
                if(ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D &&
                   ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D_ARRAY && 
                   ViewDesc.TextureDim != RESOURCE_DIM_TEX_CUBE && 
                   ViewDesc.TextureDim != RESOURCE_DIM_TEX_CUBE_ARRAY)
                {
                    TEX_VIEW_VALIDATION_ERROR( "Incorrect texture type for Texture cube array SRV: Texture 2D, Texture 2D array, Texture Cube or Texture Cube Array is allowed" );
                }
            }
            else
            {
                if(ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D &&
                   ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D_ARRAY)
                {
                    TEX_VIEW_VALIDATION_ERROR( "Incorrect texture type for Texture cube array non-shader resource view: Texture 2D or Texture 2D array is allowed" );
                }
            }
        break;

        default:
            UNEXPECTED( "Unexpected texture type" );
        break;
    }

    if ( ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE )
    {
        VERIFY(ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE, "Unexpected view type: SRV is expected");
        if(ViewDesc.NumArraySlices != 6 && ViewDesc.NumArraySlices != 0)
            TEX_VIEW_VALIDATION_ERROR( "Texture cube SRV is expected to have 6 array slices, while ", ViewDesc.NumArraySlices, " is provided" );
        if(ViewDesc.FirstArraySlice != 0)
            TEX_VIEW_VALIDATION_ERROR( "First slice (", ViewDesc.FirstArraySlice, ") must be 0 for non-array texture cube SRV" );
    }
    if ( ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE_ARRAY )
    {
        VERIFY(ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE, "Unexpected view type: SRV is expected");
        if((ViewDesc.NumArraySlices % 6) != 0)
            TEX_VIEW_VALIDATION_ERROR( "Number of slices in texture cube array SRV is expected to be multiple of 6. ", ViewDesc.NumArraySlices, " slices provided." );
    }

    if (ViewDesc.TextureDim == RESOURCE_DIM_TEX_1D ||
        ViewDesc.TextureDim == RESOURCE_DIM_TEX_2D)
    {
        if(ViewDesc.FirstArraySlice != 0)
            TEX_VIEW_VALIDATION_ERROR( "First slice (", ViewDesc.FirstArraySlice, ") must be 0 for non-array texture 1D/2D views" );

        if(ViewDesc.NumArraySlices > 1)
            TEX_VIEW_VALIDATION_ERROR( "Number of slices in the view (", ViewDesc.NumArraySlices, ") must be 1 (or 0) for non-array texture 1D/2D views" );
    }
    else if( ViewDesc.TextureDim == RESOURCE_DIM_TEX_1D_ARRAY || 
             ViewDesc.TextureDim == RESOURCE_DIM_TEX_2D_ARRAY || 
             ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE     || 
             ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE_ARRAY )
    {
        if( ViewDesc.FirstArraySlice + ViewDesc.NumArraySlices > this->m_Desc.ArraySize )
            TEX_VIEW_VALIDATION_ERROR( "First slice (", ViewDesc.FirstArraySlice, ") and number of slices in the view (", ViewDesc.NumArraySlices, ") specify more slices than target texture has (", this->m_Desc.ArraySize, ")" );
    }
    else if( ViewDesc.TextureDim == RESOURCE_DIM_TEX_3D )
    {
        auto MipDepth = this->m_Desc.Depth >> ViewDesc.MostDetailedMip;
        if( ViewDesc.FirstDepthSlice + ViewDesc.NumDepthSlices > MipDepth )
            TEX_VIEW_VALIDATION_ERROR( "First slice (", ViewDesc.FirstDepthSlice, ") and number of slices in the view (", ViewDesc.NumDepthSlices, ") specify more slices than target 3D texture mip level has (", MipDepth, ")" );
    }
    else
    {
        UNEXPECTED("Unexpected texture dimension");
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
        if( ViewDesc.TextureDim == RESOURCE_DIM_TEX_1D_ARRAY || 
            ViewDesc.TextureDim == RESOURCE_DIM_TEX_2D_ARRAY || 
            ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE     || 
            ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE_ARRAY )
            ViewDesc.NumArraySlices = this->m_Desc.ArraySize - ViewDesc.FirstArraySlice;
        else if( ViewDesc.TextureDim == RESOURCE_DIM_TEX_3D )
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

template<class BaseInterface, class TTextureViewImpl,  class TTexViewObjAllocator>
void TextureBase<BaseInterface, TTextureViewImpl, TTexViewObjAllocator> :: CreateDefaultViews()
{
    const auto& TexFmtAttribs = GetTextureFormatAttribs(this->m_Desc.Format);
    if (TexFmtAttribs.ComponentType == COMPONENT_TYPE_UNDEFINED)
    {
        // Cannot create default view for TYPELESS formats
        return;
    }

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


template<class BaseInterface, class TTextureViewImpl, class TTexViewObjAllocator>
void TextureBase<BaseInterface, TTextureViewImpl, TTexViewObjAllocator> :: UpdateData( IDeviceContext *pContext, Uint32 MipLevel, Uint32 Slice, const Box &DstBox, const TextureSubResData &SubresData )
{
    ValidateUpdateDataParams( this->m_Desc, MipLevel, Slice, DstBox, SubresData );
}

template<class BaseInterface, class TTextureViewImpl, class TTexViewObjAllocator>
void TextureBase<BaseInterface, TTextureViewImpl, TTexViewObjAllocator> :: CopyData( IDeviceContext *pContext,
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

template<class BaseInterface, class TTextureViewImpl, class TTexViewObjAllocator>
void TextureBase<BaseInterface, TTextureViewImpl, TTexViewObjAllocator> :: Map( IDeviceContext *pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags, MappedTextureSubresource &MappedData )
{
}

template<class BaseInterface, class TTextureViewImpl, class TTexViewObjAllocator>
void TextureBase<BaseInterface, TTextureViewImpl, TTexViewObjAllocator> :: Unmap( IDeviceContext *pContext, Uint32 Subresource, MAP_TYPE MapType, Uint32 MapFlags )
{
}

}
