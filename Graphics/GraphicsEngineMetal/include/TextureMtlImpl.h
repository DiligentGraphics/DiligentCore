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

#pragma once

/// \file
/// Declaration of Diligent::TextureMtlImpl class

#include "TextureMtl.h"
#include "RenderDeviceMtl.h"
#include "TextureBase.hpp"
#include "TextureViewMtlImpl.h"
#include "RenderDeviceMtlImpl.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Base implementation of the Diligent::ITextureMtl interface
class TextureMtlImpl : public TextureBase<ITextureMtl, RenderDeviceMtlImpl, TextureViewMtlImpl, FixedBlockMemoryAllocator>
{
public:
    using TTextureBase = TextureBase<ITextureMtl, RenderDeviceMtlImpl, TextureViewMtlImpl, FixedBlockMemoryAllocator>;
    using ViewImplType = TextureViewMtlImpl;

    TextureMtlImpl(IReferenceCounters*        pRefCounters,
                   FixedBlockMemoryAllocator& TexViewObjAllocator,
                   class RenderDeviceMtlImpl* pDeviceMtl,
                   const TextureDesc&         TexDesc,
                   const TextureData*         pInitData = nullptr);
    ~TextureMtlImpl();

    virtual void QueryInterface(const Diligent::INTERFACE_ID& IID, IObject** ppInterface) override final;

    virtual void* GetNativeHandle() override final
    {
        LOG_ERROR_MESSAGE("TextureMtlImpl::GetNativeHandle() is not implemented");
        return nullptr;
    }

protected:
    void CreateViewInternal(const struct TextureViewDesc& ViewDesc, ITextureView** ppView, bool bIsDefaultView) override final;
};

} // namespace Diligent
