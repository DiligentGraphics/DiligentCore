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

/// \file
/// Definition of the Diligent::ITextureView interface and related data structures

#include "DeviceObject.h"

namespace Diligent
{

// {5B2EA04E-8128-45E4-AA4D-6DC7E70DC424}
static const Diligent::INTERFACE_ID IID_TextureView =
{ 0x5b2ea04e, 0x8128, 0x45e4, { 0xaa, 0x4d, 0x6d, 0xc7, 0xe7, 0xd, 0xc4, 0x24 } };

/// Describes allowed unordered access view mode
enum UAV_ACCESS_FLAG : Int32
{
    /// Access mode is unspecified
    UAV_ACCESS_UNSPECIFIED = 0x00,
    
    /// Allow read operations on the UAV
    UAV_ACCESS_FLAG_READ   = 0x01,

    /// Allow write operations on the UAV
    UAV_ACCESS_FLAG_WRITE  = 0x02,

    /// Allow read and write operations on the UAV
    UAV_ACCESS_FLAG_READ_WRITE = UAV_ACCESS_FLAG_READ | UAV_ACCESS_FLAG_WRITE
};

/// Texture view description
struct TextureViewDesc : DeviceObjectAttribs
{
    /// Describes the texture view type, see Diligent::TEXTURE_VIEW_TYPE for details.
    TEXTURE_VIEW_TYPE ViewType;

    /// View interpretation of the original texture. For instance, 
    /// one slice of a 2D texture array can be viewed as a 2D texture.
    /// See Diligent::RESOURCE_DIMENSION for a list of texture types.
    /// If default value Diligent::RESOURCE_DIM_UNDEFINED is provided,
    /// the view type will match the type of the referenced texture.
    RESOURCE_DIMENSION TextureDim;

    /// View format. If default value Diligent::TEX_FORMAT_UNKNOWN is provided,
    /// the view format will match the referenced texture format.
    TEXTURE_FORMAT Format;
    
    /// Most detailed mip level to use
    Uint32 MostDetailedMip;

    /// Total number of mip levels for the view of the texture.
    /// Render target and depth stencil views can address only one mip level.
    /// If 0 is provided, then for a shader resource view all mip levels will be
    /// referenced, and for a render target or a depth stencil view, one mip level 
    /// will be referenced.
    Uint32 NumMipLevels;

    union
    {
        /// For a texture array, first array slice to address in the view
        Uint32 FirstArraySlice;

        /// For a 3D texture, first depth slice to address the view
        Uint32 FirstDepthSlice;
    };
    
    union
    {
        /// For a texture array, number of array slices to address in the view.
        /// Set to 0 to address all array slices.
        Uint32 NumArraySlices;
        
        /// For a 3D texture, number of depth slices to address in the view
        /// Set to 0 to address all depth slices.
        Uint32 NumDepthSlices;
    };

    /// For an unordered access view, allowed access flags. See Diligent::UAV_ACCESS_FLAG
    /// for details.
    Uint32 AccessFlags;


    /// Initializes the structure members with default values

    /// Default values:
    /// Member              | Default value
    /// --------------------|--------------
    /// ViewType            | TEXTURE_VIEW_UNDEFINED
    /// TextureDim         | RESOURCE_DIM_UNDEFINED
    /// Format              | TEX_FORMAT_UNKNOWN
    /// MostDetailedMip     | 0
    /// NumMipLevels        | 0
    /// FirstArraySlice     | 0
    /// NumArraySlices      | 0
    /// AccessFlags         | 0
    TextureViewDesc() : 
        ViewType( TEXTURE_VIEW_UNDEFINED ),
        TextureDim( RESOURCE_DIM_UNDEFINED ),
        Format(TEX_FORMAT_UNKNOWN),
        MostDetailedMip(0),
        NumMipLevels(0),
        FirstArraySlice(0),
        NumArraySlices(0),
        AccessFlags(0)
    {
    }

    /// Tests if two structures are equivalent

    /// \param [in] RHS - reference to the structure to perform comparison with
    /// \return 
    /// - True if all members of the two structures are equal.
    /// - False otherwise
    bool operator == (const TextureViewDesc& RHS)const
    {
               // Name is primarily used for debug purposes and does not affect the view.
               // It is ignored in comparison operation.
        return //strcmp(Name, RHS.Name) == 0      &&
               ViewType     == RHS.ViewType     &&
               TextureDim  == RHS.TextureDim  &&
               Format       == RHS.Format       &&
               MostDetailedMip == RHS.MostDetailedMip &&
               NumMipLevels    == RHS.NumMipLevels    &&
               FirstArraySlice == RHS.FirstArraySlice &&
               FirstDepthSlice == RHS.FirstDepthSlice &&
               NumArraySlices  == RHS.NumArraySlices  &&
               NumDepthSlices  == RHS.NumDepthSlices &&
               AccessFlags     == RHS.AccessFlags;
    }
};

/// Texture view interface

/// \remarks
/// To create a texture view, call ITexture::CreateView().
/// Texture view holds strong references to the texture. The texture
/// will not be destroyed until all views are released.
/// The texture view will also keep a strong reference to the texture sampler,
/// if any is set.
class ITextureView : public IDeviceObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface ) = 0;

    /// Returns the texture view description used to create the object
    virtual const TextureViewDesc& GetDesc()const = 0;

    /// Sets the texture sampler to use for filtering operations
    /// when accessing a texture from shaders. Only
    /// shader resource views can be assigned a sampler.
    /// The view will keep strong reference to the sampler.
    virtual void SetSampler( class ISampler *pSampler ) = 0;

    /// Returns the pointer to the sampler object set by the ITextureView::SetSampler().

    /// The method does *NOT* call AddRef() on the returned interface, 
    /// so Release() must not be called.
    virtual ISampler* GetSampler() = 0;
    

    /// Returns the pointer to the referenced texture object.

    /// The method does *NOT* call AddRef() on the returned interface, 
    /// so Release() must not be called.
    virtual class ITexture* GetTexture() = 0;

    /// Generates a mipmap chain 

    /// \remarks This function can only be called for a shader resource view
    ///          The texture must be created with MISC_TEXTURE_FLAG_GENERATE_MIPS flag
    virtual void GenerateMips(IDeviceContext *pContext ) = 0;
};

}
