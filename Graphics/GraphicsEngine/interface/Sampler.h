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
/// Definition of the Diligent::ISampler interface and related data structures

#include "DeviceObject.h"

namespace Diligent
{

// {595A59BF-FA81-4855-BC5E-C0E048745A95}
static const Diligent::INTERFACE_ID IID_Sampler =
{ 0x595a59bf, 0xfa81, 0x4855, { 0xbc, 0x5e, 0xc0, 0xe0, 0x48, 0x74, 0x5a, 0x95 } };

/// Sampler description

/// This structure describes the sampler state which is used in a call to 
/// IRenderDevice::CreateSampler() to create a sampler object.
///
/// To create an anisotropic filter, all three filters must either be Diligent::FILTER_TYPE_ANISOTROPIC
/// or Diligent::FILTER_TYPE_COMPARISON_ANISOTROPIC.
///
/// MipFilter cannot be comparison filter except for Diligent::FILTER_TYPE_ANISOTROPIC if all 
/// three filters have that value.
///
/// Both MinFilter and MagFilter must either be regular filters or comparison filters.
/// Mixing comparison and regular filters is an error.
struct SamplerDesc : DeviceObjectAttribs
{
    /// Texture minification filter, see Diligent::FILTER_TYPE for details.
    FILTER_TYPE MinFilter;
    
    /// Texture magnification filter, see Diligent::FILTER_TYPE for details.
    FILTER_TYPE MagFilter;

    /// Mip filter, see Diligent::FILTER_TYPE for details. 
    /// Only FILTER_TYPE_POINT, FILTER_TYPE_LINEAR, FILTER_TYPE_ANISOTROPIC, and 
    /// FILTER_TYPE_COMPARISON_ANISOTROPIC are allowed.
    FILTER_TYPE MipFilter;

    /// Texture address mode for U coordinate, see Diligent::TEXTURE_ADDRESS_MODE for details
    TEXTURE_ADDRESS_MODE AddressU;
    
    /// Texture address mode for V coordinate, see Diligent::TEXTURE_ADDRESS_MODE for details
    TEXTURE_ADDRESS_MODE AddressV;

    /// Texture address mode for W coordinate, see Diligent::TEXTURE_ADDRESS_MODE for details
    TEXTURE_ADDRESS_MODE AddressW;

    /// Offset from the calculated mipmap level. For example, if a sampler calculates that a texture 
    /// should be sampled at mipmap level 1.2 and MipLODBias is 2.3, then the texture will be sampled at 
    /// mipmap level 3.5.
    Float32 MipLODBias;

    /// Maximum anisotropy level for the anisotropic filter.
    Uint32 MaxAnisotropy;

    /// A function that compares sampled data against existing sampled data when comparsion
    /// filter is used.
    COMPARISON_FUNCTION ComparisonFunc;

    /// Border color to use if TEXTURE_ADDRESS_BORDER is specified for AddressU, AddressV, or AddressW. 
    Float32 BorderColor[4];

    /// Specifies the minimum value that LOD is clamped to before accessing the texture MIP levels.
    /// Must be less than or equal to MaxLOD.
    float MinLOD;

    /// Specifies the maximum value that LOD is clamped to before accessing the texture MIP levels.
    /// Must be greater than or equal to MinLOD.
    float MaxLOD;

    /// Initializes the structure members with default values

    /// Member              | Default value
    /// --------------------|--------------
    /// MinFilter           | FILTER_TYPE_LINEAR
    /// MagFilter           | FILTER_TYPE_LINEAR
    /// MipFilter           | FILTER_TYPE_LINEAR
    /// AddressU            | TEXTURE_ADDRESS_CLAMP
    /// AddressV            | TEXTURE_ADDRESS_CLAMP
    /// AddressW            | TEXTURE_ADDRESS_CLAMP
    /// MipLODBias          | 0
    /// MaxAnisotropy       | 0
    /// ComparisonFunc      | COMPARISON_FUNC_NEVER
    /// BorderColor         | (0,0,0,0)
    /// MinLOD              | 0
    /// MaxLOD              | +FLT_MAX
    SamplerDesc() : 
        MinFilter(FILTER_TYPE_LINEAR),
        MagFilter(FILTER_TYPE_LINEAR),
        MipFilter(FILTER_TYPE_LINEAR),
        AddressU(TEXTURE_ADDRESS_CLAMP),
        AddressV(TEXTURE_ADDRESS_CLAMP),
        AddressW(TEXTURE_ADDRESS_CLAMP),
        MipLODBias(0),
        MaxAnisotropy(0),
        ComparisonFunc(COMPARISON_FUNC_NEVER),
        MinLOD(0),
        MaxLOD(+FLT_MAX)
    {
        BorderColor[0] = BorderColor[1] = BorderColor[2] = BorderColor[3] = 0;
    }

    /// Tests if two structures are equivalent

    /// \param [in] RHS - reference to the structure to perform comparison with
    /// \return 
    /// - True if all members of the two structures are equal.
    /// - False otherwise.
    /// The operator ignores DeviceObjectAttribs::Name field as it does not affect 
    /// the sampler state.
    bool operator ==(const SamplerDesc& RHS)const
    {
                // Name is primarily used for debug purposes and does not affect the state.
                // It is ignored in comparison operation.
        return  // strcmp(Name, RHS.Name) == 0          &&
                MinFilter       == RHS.MinFilter      &&
                MagFilter       == RHS.MagFilter      && 
                MipFilter       == RHS.MipFilter      && 
                AddressU        == RHS.AddressU       && 
                AddressV        == RHS.AddressV       && 
                AddressW        == RHS.AddressW       && 
                MipLODBias      == RHS.MipLODBias     && 
                MaxAnisotropy   == RHS.MaxAnisotropy  && 
                ComparisonFunc  == RHS.ComparisonFunc && 
                BorderColor[0]  == RHS.BorderColor[0] && 
                BorderColor[1]  == RHS.BorderColor[1] &&
                BorderColor[2]  == RHS.BorderColor[2] &&
                BorderColor[3]  == RHS.BorderColor[3] &&
                MinLOD          == RHS.MinLOD         && 
                MaxLOD          == RHS.MaxLOD;
    }
};

/// Texture sampler interface.

/// The interface holds the sampler state that can be used to perform texture filtering.
/// To create a sampler, call IRenderDevice::CreateSampler(). To use a sampler,
/// call ITextureView::SetSampler().
class ISampler : public IDeviceObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface ) = 0;

    /// Returns the sampler description used to create the object
    virtual const SamplerDesc& GetDesc()const = 0;
};

}
