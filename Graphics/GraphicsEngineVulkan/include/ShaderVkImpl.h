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
/// Declaration of Diligent::ShaderVkImpl class

#include "RenderDeviceVk.h"
#include "ShaderVk.h"
#include "ShaderBase.h"
#include "ShaderResourceLayoutVk.h"
#include "SPIRVShaderResources.h"
#include "ShaderVariableVk.h"
#include "RenderDeviceVkImpl.h"

namespace Diligent
{

class ResourceMapping;
class FixedBlockMemoryAllocator;

/// Implementation of the Diligent::IShaderVk interface
class ShaderVkImpl : public ShaderBase<IShaderVk, RenderDeviceVkImpl>
{
public:
    using TShaderBase = ShaderBase<IShaderVk, RenderDeviceVkImpl>;

    ShaderVkImpl(IReferenceCounters* pRefCounters, RenderDeviceVkImpl* pRenderDeviceVk, const ShaderCreationAttribs &CreationAttribs);
    ~ShaderVkImpl();
    
    //virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;

    virtual void BindResources( IResourceMapping* pResourceMapping, Uint32 Flags )override;
    
    virtual IShaderVariable* GetShaderVariable(const Char* Name)override;

    virtual const std::vector<uint32_t>& GetSPIRV()const override final
    {
        return m_SPIRV;
    }

    const std::shared_ptr<const SPIRVShaderResources>& GetShaderResources()const{return m_pShaderResources;}
    const ShaderResourceLayoutVk& GetStaticResLayout()const{return m_StaticResLayout;}
    ShaderResourceCacheVk& GetStaticResCache(){return m_StaticResCache;}

#ifdef DEVELOPMENT
    void DvpVerifyStaticResourceBindings();
#endif
    
private:

    DummyShaderVariable m_DummyShaderVar; ///< Dummy shader variable
    
    // ShaderResources class instance must be referenced through the shared pointer, because 
    // it is referenced by ShaderResourceLayoutVk class instances
    std::shared_ptr<const SPIRVShaderResources> m_pShaderResources;
    ShaderResourceLayoutVk  m_StaticResLayout;
    ShaderResourceCacheVk   m_StaticResCache;
    ShaderVariableManagerVk m_StaticVarsMgr;

    std::vector<uint32_t> m_SPIRV;
};

}
