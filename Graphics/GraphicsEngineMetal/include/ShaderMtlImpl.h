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
/// Declaration of Diligent::ShaderMtlImpl class

#include "ShaderMtl.h"
#include "RenderDeviceMtl.h"
#include "ShaderBase.hpp"
#include "RenderDeviceMtlImpl.h"

namespace Diligent
{


/// Implementation of the Diligent::IShaderMtl interface
class ShaderMtlImpl final : public ShaderBase<IShaderMtl, RenderDeviceMtlImpl>
{
public:
    using TShaderBase = ShaderBase<IShaderMtl, RenderDeviceMtlImpl>;

    ShaderMtlImpl(IReferenceCounters*        pRefCounters,
                  class RenderDeviceMtlImpl* pRenderDeviceMtl,
                  const ShaderCreateInfo&    ShaderCI);
    ~ShaderMtlImpl();

    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final;

    virtual Uint32 GetResourceCount() const override final
    {
        LOG_ERROR_MESSAGE("ShaderMtlImpl::GetResourceCount() is not implemented");
        return 0;
    }

    virtual void GetResourceDesc(Uint32 Index, ShaderResourceDesc& ResourceDesc) const override final
    {
        LOG_ERROR_MESSAGE("ShaderMtlImpl::GetResource() is not implemented");
        ResourceDesc = ShaderResourceDesc{};
    }

private:
};

} // namespace Diligent
