/*     Copyright 2015-2017 Egor Yusov
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
/// Definition of the Diligent::ITextureD3D12 interface

#include "Texture.h"

namespace Diligent
{

// {CF5522EF-8116-4D76-ADF1-5CC8FB31FF66}
static const Diligent::INTERFACE_ID IID_TextureD3D12 =
{ 0xcf5522ef, 0x8116, 0x4d76, { 0xad, 0xf1, 0x5c, 0xc8, 0xfb, 0x31, 0xff, 0x66 } };

/// Interface to the texture object implemented in D3D11
class ITextureD3D12 : public Diligent::ITexture
{
public:

    /// Returns a pointer to the ID3D12Resource interface of the internal Direct3D12 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D12Resource* GetD3D12Texture() = 0;

    /// Sets the texture usage state

    /// \param [in] state - D3D12 resource state to be set for this texture
    virtual void SetD3D12ResourceState(D3D12_RESOURCE_STATES state) = 0;
};

}
