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
#include "ShaderResourceBindingD3D12Impl.h"
#include "PipelineStateD3D12Impl.h"
#include "ShaderD3D12Impl.h"
#include "RenderDeviceD3D12Impl.h"

namespace Diligent
{

ShaderResourceBindingD3D12Impl::ShaderResourceBindingD3D12Impl(IReferenceCounters*     pRefCounters,
                                                               PipelineStateD3D12Impl* pPSO,
                                                               bool                    IsPSOInternal) :
    TBase( pRefCounters, pPSO, IsPSOInternal ),
    m_ShaderResourceCache(ShaderResourceCacheD3D12::DbgCacheContentType::SRBResources),
    m_NumShaders(static_cast<decltype(m_NumShaders)>(pPSO->GetNumShaders()))
{
    auto* ppShaders = pPSO->GetShaders();

    auto* pRenderDeviceD3D12Impl = ValidatedCast<RenderDeviceD3D12Impl>(pPSO->GetDevice());
    auto& ResCacheDataAllocator = pPSO->GetSRBMemoryAllocator().GetResourceCacheDataAllocator(0);
    pPSO->GetRootSignature().InitResourceCache(pRenderDeviceD3D12Impl, m_ShaderResourceCache, ResCacheDataAllocator);
    
    auto *pVarMgrsRawMem = ALLOCATE(GetRawAllocator(), "Raw memory for ShaderVariableManagerD3D12", m_NumShaders * sizeof(ShaderVariableManagerD3D12));
    m_pShaderVarMgrs = reinterpret_cast<ShaderVariableManagerD3D12*>(pVarMgrsRawMem);

    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        auto* pShader = ppShaders[s];
        auto ShaderType = pShader->GetDesc().ShaderType;
        auto ShaderInd = GetShaderTypeIndex(ShaderType);

        // Create shader variable manager in place
        new (m_pShaderVarMgrs + s) ShaderVariableManagerD3D12(*this);

        auto& VarDataAllocator = pPSO->GetSRBMemoryAllocator().GetShaderVariableDataAllocator(s);

        // http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-layout#Initializing-Resource-Layouts-in-a-Shader-Resource-Binding-Object
        std::array<SHADER_VARIABLE_TYPE, 2> AllowedVarTypes = { SHADER_VARIABLE_TYPE_MUTABLE, SHADER_VARIABLE_TYPE_DYNAMIC };
        const auto& SrcLayout = pPSO->GetShaderResLayout(s);
        m_pShaderVarMgrs[s].Initialize(SrcLayout, VarDataAllocator, AllowedVarTypes.data(), static_cast<Uint32>(AllowedVarTypes.size()), m_ShaderResourceCache);

        m_ResourceLayoutIndex[ShaderInd] = static_cast<Int8>(s);
    }
}

ShaderResourceBindingD3D12Impl::~ShaderResourceBindingD3D12Impl()
{
    auto* pPSO = ValidatedCast<PipelineStateD3D12Impl>(m_pPSO);
    for(Uint32 s = 0; s < m_NumShaders; ++s)
    {
        auto &VarDataAllocator = pPSO->GetSRBMemoryAllocator().GetShaderVariableDataAllocator(s);
        m_pShaderVarMgrs[s].Destroy(VarDataAllocator);
        m_pShaderVarMgrs[s].~ShaderVariableManagerD3D12();
    }

    GetRawAllocator().Free(m_pShaderVarMgrs);
}

IMPLEMENT_QUERY_INTERFACE( ShaderResourceBindingD3D12Impl, IID_ShaderResourceBindingD3D12, TBase )

void ShaderResourceBindingD3D12Impl::BindResources(Uint32 ShaderFlags, IResourceMapping* pResMapping, Uint32 Flags)
{
    for (auto ShaderInd = 0; ShaderInd <= CSInd; ++ShaderInd )
    {
        if (ShaderFlags & GetShaderTypeFromIndex(ShaderInd))
        {
            auto ResLayoutInd = m_ResourceLayoutIndex[ShaderInd];
            if(ResLayoutInd >= 0)
            {
                m_pShaderVarMgrs[ResLayoutInd].BindResources(pResMapping, Flags);
            }
        }
    }
}

IShaderVariable *ShaderResourceBindingD3D12Impl::GetVariable(SHADER_TYPE ShaderType, const char *Name)
{
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    auto ResLayoutInd = m_ResourceLayoutIndex[ShaderInd];
    if (ResLayoutInd < 0)
    {
        LOG_ERROR("Unable to find mutable/dynamic variable '", Name, "': shader stage ", GetShaderTypeLiteralName(ShaderType), " is inactive");
        return nullptr;
    }
    return m_pShaderVarMgrs[ResLayoutInd].GetVariable(Name);
}

Uint32 ShaderResourceBindingD3D12Impl::GetVariableCount(SHADER_TYPE ShaderType) const 
{
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    auto ResLayoutInd = m_ResourceLayoutIndex[ShaderInd];
    if (ResLayoutInd < 0)
    {
        LOG_ERROR("Unable to get the number of mutable/dynamic variables: shader stage ", GetShaderTypeLiteralName(ShaderType), " is inactive");
        return 0;
    }

    return m_pShaderVarMgrs[ResLayoutInd].GetVariableCount();
}

IShaderVariable* ShaderResourceBindingD3D12Impl::GetVariable(SHADER_TYPE ShaderType, Uint32 Index)
{
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    auto ResLayoutInd = m_ResourceLayoutIndex[ShaderInd];
    if (ResLayoutInd < 0)
    {
        LOG_ERROR("Unable to get mutable/dynamic variable at index ", Index, ": shader stage ", GetShaderTypeLiteralName(ShaderType), " is inactive");
        return nullptr;
    }

    return m_pShaderVarMgrs[ResLayoutInd].GetVariable(Index);
}


#ifdef DEVELOPMENT
void ShaderResourceBindingD3D12Impl::dvpVerifyResourceBindings(const PipelineStateD3D12Impl* pPSO)
{
    auto* pRefPSO = ValidatedCast<PipelineStateD3D12Impl>(GetPipelineState());
    if (pPSO->IsIncompatibleWith(pRefPSO))
    {
        LOG_ERROR("Shader resource binding is incompatible with the pipeline state \"", pPSO->GetDesc().Name, '\"');
        return;
    }
    for(Uint32 l = 0; l < m_NumShaders; ++l)
    {
        // Use reference layout from pipeline state that contains all shader resource types
        const auto& ShaderResLayout = pRefPSO->GetShaderResLayout(l);
        ShaderResLayout.dvpVerifyBindings(m_ShaderResourceCache);
    }
}
#endif


void ShaderResourceBindingD3D12Impl::InitializeStaticResources(const PipelineStateD3D12Impl* pPSO)
{
    VERIFY(!StaticResourcesInitialized(), "Static resources have already been initialized");
    VERIFY(pPSO == GetPipelineState(), "Invalid pipeline state provided");

    auto NumShaders = pPSO->GetNumShaders();
    auto ppShaders = pPSO->GetShaders();
    // Copy static resources
    for (Uint32 s = 0; s < NumShaders; ++s)
    {
        auto* pShader = ValidatedCast<ShaderD3D12Impl>( ppShaders[s] );
#ifdef DEVELOPMENT
        pShader->DvpVerifyStaticResourceBindings();
#endif
        const auto& ShaderResLayout = pPSO->GetShaderResLayout(s);
        auto& StaticResLayout = pShader->GetStaticResLayout();
        auto& StaticResCache = pShader->GetStaticResCache();
        StaticResLayout.CopyStaticResourceDesriptorHandles(StaticResCache, ShaderResLayout, m_ShaderResourceCache);
    }

    m_bStaticResourcesInitialized = true;
}

}
