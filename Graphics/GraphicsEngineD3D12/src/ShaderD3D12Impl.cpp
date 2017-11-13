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

#include "pch.h"

#include <D3Dcompiler.h>

#include "ShaderD3D12Impl.h"
#include "RenderDeviceD3D12Impl.h"
#include "DataBlobImpl.h"
#include "D3DShaderResourceLoader.h"

using namespace Diligent;

namespace Diligent
{


ShaderD3D12Impl::ShaderD3D12Impl(IReferenceCounters *pRefCounters, RenderDeviceD3D12Impl *pRenderDeviceD3D12, const ShaderCreationAttribs &ShaderCreationAttribs) : 
    TShaderBase(pRefCounters, pRenderDeviceD3D12, ShaderCreationAttribs.Desc),
    ShaderD3DBase(ShaderCreationAttribs),
    m_StaticResLayout(*this, GetRawAllocator()),
    m_DummyShaderVar(*this),
    m_ConstResCache(ShaderResourceCacheD3D12::DbgCacheContentType::StaticShaderResources)
{
    // Load shader resources
    auto &Allocator = GetRawAllocator();
    auto *pRawMem = ALLOCATE(Allocator, "Allocator for ShaderResources", sizeof(ShaderResourcesD3D12));
    auto *pResources = new (pRawMem) ShaderResourcesD3D12(m_pShaderByteCode, m_Desc);
    m_pShaderResources.reset(pResources, STDDeleterRawMem<ShaderResourcesD3D12>(Allocator));

    // Clone only static resources that will be set directly in the shader
    // http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-layout#Initializing-Special-Resource-Layout-for-Managing-Static-Shader-Resources
    SHADER_VARIABLE_TYPE VarTypes[] = {SHADER_VARIABLE_TYPE_STATIC};
    m_StaticResLayout.Initialize(pRenderDeviceD3D12->GetD3D12Device(), m_pShaderResources, GetRawAllocator(), VarTypes, _countof(VarTypes), &m_ConstResCache, nullptr);
}

ShaderD3D12Impl::~ShaderD3D12Impl()
{
}

void ShaderD3D12Impl::BindResources(IResourceMapping* pResourceMapping, Uint32 Flags)
{
   m_StaticResLayout.BindResources(pResourceMapping, Flags, &m_ConstResCache);
}
    
IShaderVariable* ShaderD3D12Impl::GetShaderVariable(const Char* Name)
{
    auto *pVar = m_StaticResLayout.GetShaderVariable(Name);
    if(pVar == nullptr)
        pVar = &m_DummyShaderVar;
    return pVar;
}

#ifdef VERIFY_SHADER_BINDINGS
void ShaderD3D12Impl::DbgVerifyStaticResourceBindings()
{
    m_StaticResLayout.dbgVerifyBindings();
}
#endif

}
