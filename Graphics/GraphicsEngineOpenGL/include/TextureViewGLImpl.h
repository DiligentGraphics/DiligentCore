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

#include "BaseInterfacesGL.h"
#include "TextureViewGL.h"
#include "TextureViewBase.h"
#include "RenderDevice.h"
#include "GLObjectWrapper.h"
#include "RenderDeviceGLImpl.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Texture view implementation in OpenGL backend.
class TextureViewGLImpl final : public TextureViewBase<ITextureViewGL, RenderDeviceGLImpl>
{
public:
    using TTextureViewBase = TextureViewBase<ITextureViewGL, RenderDeviceGLImpl>;

    TextureViewGLImpl(IReferenceCounters*           pRefCounters,
                      RenderDeviceGLImpl*           pDevice, 
                      const struct TextureViewDesc& ViewDesc, 
                      class TextureBaseGL*          pTexture,
                      bool                          bCreateGLViewTex,
                      bool                          bIsDefaultView );
    ~TextureViewGLImpl();

    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface )override final;

    const GLObjectWrappers::GLTextureObj& GetHandle();
    GLenum GetBindTarget();
    void SetBindTarget(GLenum ViewTexBindTarget) { m_ViewTexBindTarget = ViewTexBindTarget; }

protected:
    GLObjectWrappers::GLTextureObj m_ViewTexGLHandle;
    GLenum  m_ViewTexBindTarget;
};

}
