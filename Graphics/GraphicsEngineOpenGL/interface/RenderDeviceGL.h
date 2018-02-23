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
/// Definition of the Diligent::IRenderDeviceGL interface

#include "../../GraphicsEngine/interface/RenderDevice.h"

/// Namespace for the OpenGL implementation of the graphics engine
namespace Diligent
{

// {B4B395B9-AC99-4E8A-B7E1-9DCA0D485618}
static constexpr INTERFACE_ID IID_RenderDeviceGL =
{ 0xb4b395b9, 0xac99, 0x4e8a, { 0xb7, 0xe1, 0x9d, 0xca, 0xd, 0x48, 0x56, 0x18 } };

/// Interface to the render device object implemented in OpenGL
class IRenderDeviceGL : public IRenderDevice
{
public:

    /// Creates a texture from OpenGL handle

    /// \param [in] GLHandle - OpenGL texture handle
    /// \param [in] TexDesc - Texture description. The engine can automatically 
    ///                       set texture width, height, depth, mip levels count, and format.
    ///                       Remaining fields should be set up by the app.
    /// \param [out] ppTexture - Address of the memory location where the pointer to the
    ///                          texture interface will be stored. 
    ///                          The function calls AddRef(), so that the new object will contain 
    ///                          one refernce.
    /// \note  Diligent engine texture object does not take ownership of the GL resource, 
    ///        and the application must not destroy it while it is in use by the engine.
    virtual void CreateTextureFromGLHandle(Uint32 GLHandle, const TextureDesc &TexDesc, ITexture **ppTexture) = 0;

    /// Creates a buffer from OpenGL handle

    /// \param [in] GLHandle - OpenGL buffer handle
    /// \param [in] BuffDesc - Buffer description. The engine can automatically 
    ///                       recover buffer size, but the rest of the fields need to 
    ///                       be set by the client.
    /// \param [out] ppBuffer - Address of the memory location where the pointer to the
    ///                         texture interface will be stored. 
    ///                         The function calls AddRef(), so that the new object will contain 
    ///                         one refernce.
    /// \note  Diligent engine buffer object does not take ownership of the GL resource, 
    ///        and the application must not destroy it while it is in use by the engine.
    virtual void CreateBufferFromGLHandle(Uint32 GLHandle, const BufferDesc &BuffDesc, IBuffer **ppBuffer) = 0;
};

}
