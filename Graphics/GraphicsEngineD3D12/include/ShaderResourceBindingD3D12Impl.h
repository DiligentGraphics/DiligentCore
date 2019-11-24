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
/// Declaration of Diligent::ShaderResourceBindingD3D12Impl class

#include "ShaderResourceBindingD3D12.h"
#include "RenderDeviceD3D12.h"
#include "ShaderResourceBindingBase.h"
#include "ShaderBase.h"
#include "ShaderResourceCacheD3D12.h"
#include "ShaderResourceLayoutD3D12.h"
#include "ShaderVariableD3D12.h"

namespace Diligent
{

/// Implementation of the Diligent::IShaderResourceBindingD3D12 interface
// sizeof(ShaderResourceBindingD3D12Impl) == 152 (x64, msvc, Release)
class ShaderResourceBindingD3D12Impl final : public ShaderResourceBindingBase<IShaderResourceBindingD3D12>
{
public:
    using TBase = ShaderResourceBindingBase<IShaderResourceBindingD3D12>;

    ShaderResourceBindingD3D12Impl(IReferenceCounters*           pRefCounters,
                                   class PipelineStateD3D12Impl* pPSO,
                                   bool                          IsPSOInternal);
    ~ShaderResourceBindingD3D12Impl();

    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final;

    virtual void BindResources(Uint32 ShaderFlags, IResourceMapping* pResMapping, Uint32 Flags) override;

    virtual IShaderResourceVariable* GetVariableByName(SHADER_TYPE ShaderType, const char* Name) override;

    virtual Uint32 GetVariableCount(SHADER_TYPE ShaderType) const override final;

    virtual IShaderResourceVariable* GetVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    virtual void InitializeStaticResources(const IPipelineState* pPipelineState) override final;

    ShaderResourceCacheD3D12& GetResourceCache() { return m_ShaderResourceCache; }

#ifdef DEVELOPMENT
    void dvpVerifyResourceBindings(const PipelineStateD3D12Impl* pPSO) const;
#endif

    bool StaticResourcesInitialized() const
    {
        return m_bStaticResourcesInitialized;
    }

private:
    ShaderResourceCacheD3D12    m_ShaderResourceCache;
    ShaderVariableManagerD3D12* m_pShaderVarMgrs = nullptr;
    // Resource layout index in m_ResourceLayouts[] array for every shader stage
    Int8        m_ResourceLayoutIndex[6]      = {-1, -1, -1, -1, -1, -1};
    bool        m_bStaticResourcesInitialized = false;
    const Uint8 m_NumShaders                  = 0;
};

} // namespace Diligent
