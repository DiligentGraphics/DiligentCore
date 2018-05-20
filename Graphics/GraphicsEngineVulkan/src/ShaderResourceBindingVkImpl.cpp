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

#include "pch.h"
#include "ShaderResourceBindingVkImpl.h"
#include "PipelineStateVkImpl.h"
#include "ShaderVkImpl.h"
#include "RenderDeviceVkImpl.h"

namespace Diligent
{

ShaderResourceBindingVkImpl::ShaderResourceBindingVkImpl( IReferenceCounters *pRefCounters, PipelineStateVkImpl *pPSO, bool IsPSOInternal) :
    TBase( pRefCounters, pPSO, IsPSOInternal ),
    m_ShaderResourceCache(ShaderResourceCacheVk::DbgCacheContentType::SRBResources)
{
    auto *ppShaders = pPSO->GetShaders();
    m_NumShaders = pPSO->GetNumShaders();

    auto *pRenderDeviceVkImpl = ValidatedCast<RenderDeviceVkImpl>(pPSO->GetDevice());
    pPSO->GetPipelineLayout().InitResourceCache(pRenderDeviceVkImpl, m_ShaderResourceCache, pPSO->GetResourceCacheDataAllocator());
    
    auto *pResLayoutRawMem = ALLOCATE(GetRawAllocator(), "Raw memory for ShaderResourceLayoutVk", m_NumShaders * sizeof(ShaderResourceLayoutVk));
    m_pResourceLayouts = reinterpret_cast<ShaderResourceLayoutVk*>(pResLayoutRawMem);

    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        auto *pShader = ppShaders[s];
        auto ShaderType = pShader->GetDesc().ShaderType;
        auto ShaderInd = GetShaderTypeIndex(ShaderType);
        
        auto &ShaderResLayoutDataAllocator = pPSO->GetShaderResourceLayoutDataAllocator(s);

        // http://diligentgraphics.com/diligent-engine/architecture/Vk/shader-resource-layout#Initializing-Resource-Layouts-in-a-Shader-Resource-Binding-Object
        SHADER_VARIABLE_TYPE Types[] = {SHADER_VARIABLE_TYPE_STATIC, SHADER_VARIABLE_TYPE_MUTABLE, SHADER_VARIABLE_TYPE_DYNAMIC};
        const auto &SrcLayout = pPSO->GetShaderResLayout(ShaderType);
        new (m_pResourceLayouts + s) ShaderResourceLayoutVk(*this, SrcLayout, ShaderResLayoutDataAllocator, Types, _countof(Types), m_ShaderResourceCache);

        m_ResourceLayoutIndex[ShaderInd] = static_cast<Int8>(s);
    }
}

ShaderResourceBindingVkImpl::~ShaderResourceBindingVkImpl()
{
    for(Uint32 l = 0; l < m_NumShaders; ++l)
        m_pResourceLayouts[l].~ShaderResourceLayoutVk();

    GetRawAllocator().Free(m_pResourceLayouts);
}

IMPLEMENT_QUERY_INTERFACE( ShaderResourceBindingVkImpl, IID_ShaderResourceBindingVk, TBase )

void ShaderResourceBindingVkImpl::BindResources(Uint32 ShaderFlags, IResourceMapping *pResMapping, Uint32 Flags)
{
    for (auto ShaderInd = 0; ShaderInd <= CSInd; ++ShaderInd )
    {
        if (ShaderFlags & GetShaderTypeFromIndex(ShaderInd))
        {
            auto ResLayoutInd = m_ResourceLayoutIndex[ShaderInd];
            if(ResLayoutInd >= 0)
            {
                m_pResourceLayouts[ResLayoutInd].BindResources(pResMapping, Flags, &m_ShaderResourceCache);
            }
        }
    }
}

IShaderVariable *ShaderResourceBindingVkImpl::GetVariable(SHADER_TYPE ShaderType, const char *Name)
{
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    auto ResLayoutInd = m_ResourceLayoutIndex[ShaderInd];
    if (ResLayoutInd < 0)
    {
        LOG_ERROR_MESSAGE("Failed to find shader variable \"", Name,"\" in shader resource binding: shader type ", GetShaderTypeLiteralName(ShaderType), " is not initialized");
        return ValidatedCast<PipelineStateVkImpl>(GetPipelineState())->GetDummyShaderVar();
    }
    auto *pVar = m_pResourceLayouts[ResLayoutInd].GetShaderVariable(Name);
    if(pVar->SpirvAttribs.VarType == SHADER_VARIABLE_TYPE_STATIC)
    {
        LOG_ERROR_MESSAGE("Static shader variable \"", Name, "\" must not be accessed through shader resource binding object. Static variable should be set through shader objects.");
        pVar = nullptr;
    }

    if(pVar == nullptr)
        return ValidatedCast<PipelineStateVkImpl>(GetPipelineState())->GetDummyShaderVar();
    else
        return pVar;
}

#ifdef VERIFY_SHADER_BINDINGS
void ShaderResourceBindingVkImpl::dbgVerifyResourceBindings(const PipelineStateVkImpl *pPSO)
{
    auto *pRefPSO = GetPipelineState();
    if (pPSO->IsIncompatibleWith(pRefPSO))
    {
        LOG_ERROR("Shader resource binding is incompatible with the pipeline state \"", pPSO->GetDesc().Name, '\"');
        return;
    }
    for(Uint32 l = 0; l < m_NumShaders; ++l)
        m_pResourceLayouts[l].dbgVerifyBindings();
}
#endif

void ShaderResourceBindingVkImpl::InitializeStaticResources(const PipelineStateVkImpl *pPSO)
{
    VERIFY(!StaticResourcesInitialized(), "Static resources have already been initialized");
    VERIFY(pPSO == GetPipelineState(), "Invalid pipeline state provided");

    auto NumShaders = pPSO->GetNumShaders();
    auto ppShaders = pPSO->GetShaders();
    // Copy static resources
    for (Uint32 s = 0; s < NumShaders; ++s)
    {
        auto *pShader = ValidatedCast<ShaderVkImpl>( ppShaders[s] );
#ifdef VERIFY_SHADER_BINDINGS
        pShader->DbgVerifyStaticResourceBindings();
#endif
        auto &ConstResLayout = pShader->GetConstResLayout();
        GetResourceLayout(pShader->GetDesc().ShaderType).InitializeStaticResources(ConstResLayout);
    }

    m_bStaticResourcesInitialized = true;
}

}
