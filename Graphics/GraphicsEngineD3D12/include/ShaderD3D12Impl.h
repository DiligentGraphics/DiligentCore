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
/// Declaration of Diligent::ShaderD3D12Impl class

#include "RenderDeviceD3D12.h"
#include "ShaderD3D12.h"
#include "ShaderBase.h"
#include "ShaderD3DBase.h"
#include "ShaderResourceLayoutD3D12.h"
#include "RenderDeviceD3D12Impl.h"
#include "ShaderVariableD3D12.h"

namespace Diligent
{

class ResourceMapping;

/// Implementation of the Diligent::IShaderD3D12 interface
class ShaderD3D12Impl final : public ShaderBase<IShaderD3D12, RenderDeviceD3D12Impl>, public ShaderD3DBase
{
public:
    using TShaderBase = ShaderBase<IShaderD3D12, RenderDeviceD3D12Impl>;

    ShaderD3D12Impl(IReferenceCounters*           pRefCounters, 
                    RenderDeviceD3D12Impl*        pRenderDeviceD3D12, 
                    const ShaderCreationAttribs&  ShaderCreationAttribs);
    ~ShaderD3D12Impl();
    
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;

    virtual void BindResources( IResourceMapping* pResourceMapping, Uint32 Flags )override final
    {
       m_StaticVarsMgr.BindResources(pResourceMapping, Flags);
    }
    
    virtual IShaderVariable* GetShaderVariable(const Char* Name)override final
    {
        return m_StaticVarsMgr.GetVariable(Name);
    }

    virtual Uint32 GetVariableCount() const override final
    {
        return m_StaticVarsMgr.GetVariableCount();
    }

    virtual IShaderVariable* GetShaderVariable(Uint32 Index)override final
    {
        return m_StaticVarsMgr.GetVariable(Index);
    }

    ID3DBlob* GetShaderByteCode(){return m_pShaderByteCode;}
    const std::shared_ptr<const ShaderResourcesD3D12>& GetShaderResources()const { return m_pShaderResources; }
    const ShaderResourceLayoutD3D12&                   GetStaticResLayout()const { return m_StaticResLayout;  }
    const ShaderResourceCacheD3D12&                    GetStaticResCache() const { return m_StaticResCache;   }

#ifdef DEVELOPMENT
    bool DvpVerifyStaticResourceBindings()const;
#endif

private:
    // ShaderResources class instance must be referenced through the shared pointer, because 
    // it is referenced by ShaderResourceLayoutD3D12 class instances
    std::shared_ptr<const ShaderResourcesD3D12> m_pShaderResources;
    ShaderResourceLayoutD3D12   m_StaticResLayout;
    ShaderResourceCacheD3D12    m_StaticResCache;
    ShaderVariableManagerD3D12  m_StaticVarsMgr;
};

}
