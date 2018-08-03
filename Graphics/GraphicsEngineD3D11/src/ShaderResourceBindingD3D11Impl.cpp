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
#include "ShaderResourceBindingD3D11Impl.h"
#include "PipelineStateD3D11Impl.h"
#include "DeviceContextD3D11Impl.h"
#include "RenderDeviceD3D11Impl.h"

namespace Diligent
{


ShaderResourceBindingD3D11Impl::ShaderResourceBindingD3D11Impl( IReferenceCounters*     pRefCounters,
                                                                PipelineStateD3D11Impl* pPSO,
                                                                bool                    IsInternal) :
    TBase( pRefCounters, pPSO, IsInternal ),
    m_bIsStaticResourcesBound(false)
{
    for(size_t s=0; s < _countof(m_ResourceLayoutIndex); ++s)
        m_ResourceLayoutIndex[s] = -1;

    auto ppShaders = pPSO->GetShaders();
    m_NumActiveShaders = static_cast<Uint8>( pPSO->GetNumShaders() );

    auto *pResLayoutRawMem = ALLOCATE(GetRawAllocator(), "Raw memory for ShaderResourceLayoutD3D11", m_NumActiveShaders * sizeof(ShaderResourceLayoutD3D11));
    m_pResourceLayouts = reinterpret_cast<ShaderResourceLayoutD3D11*>(pResLayoutRawMem);

    auto *pResCacheRawMem = ALLOCATE(GetRawAllocator(), "Raw memory for ShaderResourceCacheD3D11", m_NumActiveShaders * sizeof(ShaderResourceCacheD3D11));
    m_pBoundResourceCaches = reinterpret_cast<ShaderResourceCacheD3D11*>(pResCacheRawMem);

    // Reserve memory for resource layouts
    for (Uint8 s = 0; s < m_NumActiveShaders; ++s)
    {
        auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>(ppShaders[s]);
        auto ShaderInd = pShaderD3D11->GetShaderTypeIndex();
        VERIFY_EXPR(static_cast<Int32>(ShaderInd) == GetShaderTypeIndex(pShaderD3D11->GetDesc().ShaderType));

        auto& SRBMemAllocator = pPSO->GetSRBMemoryAllocator();
        auto& ResCacheDataAllocator = SRBMemAllocator.GetResourceCacheDataAllocator(s);
        auto& ResLayoutDataAllocator = SRBMemAllocator.GetShaderVariableDataAllocator(s);
        
        // Initialize resource cache to have enough space to contain all shader resources, including static ones
        // Static resources are copied before resources are committed
        const auto& Resources = *pShaderD3D11->GetResources();
        new (m_pBoundResourceCaches+s) ShaderResourceCacheD3D11;
        m_pBoundResourceCaches[s].Initialize(Resources, ResCacheDataAllocator);

        // Shader resource layout will only contain dynamic and mutable variables
        // http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-cache#Shader-Resource-Cache-Initialization
        SHADER_VARIABLE_TYPE VarTypes[] = {SHADER_VARIABLE_TYPE_MUTABLE, SHADER_VARIABLE_TYPE_DYNAMIC};
        new (m_pResourceLayouts + s) ShaderResourceLayoutD3D11(*this, ResLayoutDataAllocator);
        m_pResourceLayouts[s].Initialize(pShaderD3D11->GetResources(), VarTypes, _countof(VarTypes), m_pBoundResourceCaches[s], ResCacheDataAllocator, ResLayoutDataAllocator);

        Resources.InitStaticSamplers(m_pBoundResourceCaches[s]);

        m_ResourceLayoutIndex[ShaderInd] = s;
        m_ShaderTypeIndex[s] = static_cast<Int8>(ShaderInd);
    }
}

ShaderResourceBindingD3D11Impl::~ShaderResourceBindingD3D11Impl()
{
    auto *pPSOD3D11Impl = ValidatedCast<PipelineStateD3D11Impl>(m_pPSO);
    for (Uint32 s = 0; s < m_NumActiveShaders; ++s)
    {
        auto& Allocator = pPSOD3D11Impl->GetSRBMemoryAllocator().GetResourceCacheDataAllocator(s);
        m_pBoundResourceCaches[s].Destroy(Allocator);
        m_pBoundResourceCaches[s].~ShaderResourceCacheD3D11();
    }
    GetRawAllocator().Free(m_pBoundResourceCaches);

    for(Int32 l = 0; l < m_NumActiveShaders; ++l)
    {
        m_pResourceLayouts[l].~ShaderResourceLayoutD3D11();
    }
    GetRawAllocator().Free(m_pResourceLayouts);
}

IMPLEMENT_QUERY_INTERFACE( ShaderResourceBindingD3D11Impl, IID_ShaderResourceBindingD3D11, TBase )

void ShaderResourceBindingD3D11Impl::BindResources(Uint32 ShaderFlags, IResourceMapping *pResMapping, Uint32 Flags)
{
    for(Uint32 ResLayoutInd = 0; ResLayoutInd < m_NumActiveShaders; ++ResLayoutInd)
    {
        auto& ResLayout = m_pResourceLayouts[ResLayoutInd];
        if(ShaderFlags & ResLayout.GetShaderType())
        {
            ResLayout.BindResources(pResMapping, Flags, m_pBoundResourceCaches[ResLayoutInd]);
        }
    }
}

void ShaderResourceBindingD3D11Impl::BindStaticShaderResources()
{
    if (m_bIsStaticResourcesBound)
    {
        LOG_ERROR("Static resources already bound");
        return;
    }

    auto *pPSOD3D11 = ValidatedCast<PipelineStateD3D11Impl>(GetPipelineState());
    auto ppShaders = pPSOD3D11->GetShaders();
    auto NumShaders = pPSOD3D11->GetNumShaders();
    VERIFY_EXPR(NumShaders == m_NumActiveShaders);

    for (Uint32 shader = 0; shader < NumShaders; ++shader)
    {
        auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>( ppShaders[shader] );
#ifdef VERIFY_SHADER_BINDINGS
        pShaderD3D11->GetStaticResourceLayout().dbgVerifyBindings();
#endif

#ifdef _DEBUG
        auto ShaderTypeInd = pShaderD3D11->GetShaderTypeIndex();
        auto ResourceLayoutInd = m_ResourceLayoutIndex[ShaderTypeInd];
        VERIFY_EXPR(ResourceLayoutInd == static_cast<Int8>(shader) );
#endif
        pShaderD3D11->GetStaticResourceLayout().CopyResources( m_pBoundResourceCaches[shader] );
    }

    m_bIsStaticResourcesBound = true;
}

IShaderVariable* ShaderResourceBindingD3D11Impl::GetVariable(SHADER_TYPE ShaderType, const char* Name)
{
    auto Ind = GetShaderTypeIndex(ShaderType);
    VERIFY_EXPR(Ind >= 0 && Ind < _countof(m_ResourceLayoutIndex));
    if( Ind >= 0 )
    {
        auto ResLayoutIndex = m_ResourceLayoutIndex[Ind];
        auto *pVar = m_pResourceLayouts[ResLayoutIndex].GetShaderVariable(Name);
        if(pVar != nullptr)
            return pVar;
        else
        {
            auto *pPSOD3D11 = ValidatedCast<PipelineStateD3D11Impl>(GetPipelineState());
            return pPSOD3D11->GetDummyShaderVariable();
        }
    }
    else
    {
        LOG_ERROR("Shader type ", GetShaderTypeLiteralName(ShaderType)," is not active in the resource binding");
        return nullptr;
    }
}

Uint32 ShaderResourceBindingD3D11Impl::GetVariableCount(SHADER_TYPE ShaderType) const
{
    auto Ind = GetShaderTypeIndex(ShaderType);
    VERIFY_EXPR(Ind >= 0 && Ind < _countof(m_ResourceLayoutIndex));
    if( Ind >= 0 )
    {
        auto ResLayoutIndex = m_ResourceLayoutIndex[Ind];
        return m_pResourceLayouts[ResLayoutIndex].GetTotalResourceCount();
    }
    else
    {
        LOG_ERROR("Shader type ", GetShaderTypeLiteralName(ShaderType)," is not active in the resource binding");
        return 0;
    }
}

IShaderVariable* ShaderResourceBindingD3D11Impl::GetVariable(SHADER_TYPE ShaderType, Uint32 Index)
{
    auto Ind = GetShaderTypeIndex(ShaderType);
    VERIFY_EXPR(Ind >= 0 && Ind < _countof(m_ResourceLayoutIndex));
    if( Ind >= 0 )
    {
        auto ResLayoutIndex = m_ResourceLayoutIndex[Ind];
        return m_pResourceLayouts[ResLayoutIndex].GetShaderVariable(Index);
    }
    else
    {
        LOG_ERROR("Shader type ", GetShaderTypeLiteralName(ShaderType)," is not active in the resource binding");
        return nullptr;
    }
}

}
