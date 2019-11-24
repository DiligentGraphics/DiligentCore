/*     Copyright 2019 Diligent Graphics LLC
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
/// Definition of the Diligent::ITextureGL interface

#include "../../GraphicsEngine/interface/Texture.h"

namespace Diligent
{

// {D7BC9FF0-28F0-4636-9732-710C204D1D63}
static constexpr INTERFACE_ID IID_TextureGL =
    {0xd7bc9ff0, 0x28f0, 0x4636, {0x97, 0x32, 0x71, 0xc, 0x20, 0x4d, 0x1d, 0x63}};

/// Exposes OpenGL-specific functionality of a texture object.
class ITextureGL : public ITexture
{
public:
    /// Returns OpenGL texture handle
    virtual GLuint GetGLTextureHandle() = 0;

    /// Returns bind target of the native OpenGL texture
    virtual GLenum GetBindTarget() const = 0;
};

} // namespace Diligent
