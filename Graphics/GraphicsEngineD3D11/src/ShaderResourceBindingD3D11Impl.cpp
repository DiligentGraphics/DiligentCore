/*
 *  Copyright 2019-2020 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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
#include "ShaderResourceBindingD3D11Impl.hpp"
#include "PipelineStateD3D11Impl.hpp"
#include "DeviceContextD3D11Impl.hpp"
#include "RenderDeviceD3D11Impl.hpp"

namespace Diligent
{


ShaderResourceBindingD3D11Impl::ShaderResourceBindingD3D11Impl(IReferenceCounters*     pRefCounters,
                                                               PipelineStateD3D11Impl* pPSO,
                                                               bool                    IsInternal) :
    // clang-format off
    TBase
    {
        pRefCounters,
        pPSO,
        IsInternal
    },
    m_bIsStaticResourcesBound{false}
// clang-format on
{
    m_NumActiveShaders = static_cast<Uint8>(pPSO->GetNumShaders());

    // clang-format off
    m_pResourceLayouts     = ALLOCATE(GetRawAllocator(), "Raw memory for ShaderResourceLayoutD3D11", ShaderResourceLayoutD3D11, m_NumActiveShaders);
    m_pBoundResourceCaches = ALLOCATE(GetRawAllocator(), "Raw memory for ShaderResourceCacheD3D11",  ShaderResourceCacheD3D11,  m_NumActiveShaders);
    // clang-format on

    const auto& PSODesc = pPSO->GetDesc();

    // Reserve memory for resource layouts
    for (Uint8 s = 0; s < m_NumActiveShaders; ++s)
    {
        auto* pShaderD3D11 = pPSO->GetShader<ShaderD3D11Impl>(s);
        auto  ShaderInd    = GetShaderTypeIndex(pShaderD3D11->GetDesc().ShaderType);

        auto& SRBMemAllocator        = pPSO->GetSRBMemoryAllocator();
        auto& ResCacheDataAllocator  = SRBMemAllocator.GetResourceCacheDataAllocator(s);
        auto& ResLayoutDataAllocator = SRBMemAllocator.GetShaderVariableDataAllocator(s);

        // Initialize resource cache to have enough space to contain all shader resources, including static ones
        // Static resources are copied before resources are committed
        const auto& Resources = *pShaderD3D11->GetD3D11Resources();
        new (m_pBoundResourceCaches + s) ShaderResourceCacheD3D11;
        m_pBoundResourceCaches[s].Initialize(Resources, ResCacheDataAllocator);

        // Shader resource layout will only contain dynamic and mutable variables
        // http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-cache#Shader-Resource-Cache-Initialization
        SHADER_RESOURCE_VARIABLE_TYPE VarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC};
        // clang-format off
        new (m_pResourceLayouts + s)
            ShaderResourceLayoutD3D11
            {
                *this,
                pShaderD3D11->GetD3D11Resources(),
                PSODesc.ResourceLayout,
                VarTypes,
                _countof(VarTypes),
                m_pBoundResourceCaches[s],
                ResCacheDataAllocator,
                ResLayoutDataAllocator
            };
        // clang-format on

        m_ResourceLayoutIndex[ShaderInd] = s;
        m_ShaderTypeIndex[s]             = static_cast<Int8>(ShaderInd);
    }
}

ShaderResourceBindingD3D11Impl::~ShaderResourceBindingD3D11Impl()
{
    auto* pPSOD3D11Impl = ValidatedCast<PipelineStateD3D11Impl>(m_pPSO);
    for (Uint32 s = 0; s < m_NumActiveShaders; ++s)
    {
        auto& Allocator = pPSOD3D11Impl->GetSRBMemoryAllocator().GetResourceCacheDataAllocator(s);
        m_pBoundResourceCaches[s].Destroy(Allocator);
        m_pBoundResourceCaches[s].~ShaderResourceCacheD3D11();
    }
    GetRawAllocator().Free(m_pBoundResourceCaches);

    for (Int32 l = 0; l < m_NumActiveShaders; ++l)
    {
        m_pResourceLayouts[l].~ShaderResourceLayoutD3D11();
    }
    GetRawAllocator().Free(m_pResourceLayouts);
}

IMPLEMENT_QUERY_INTERFACE(ShaderResourceBindingD3D11Impl, IID_ShaderResourceBindingD3D11, TBase)

void ShaderResourceBindingD3D11Impl::BindResources(Uint32 ShaderFlags, IResourceMapping* pResMapping, Uint32 Flags)
{
    for (Uint32 ResLayoutInd = 0; ResLayoutInd < m_NumActiveShaders; ++ResLayoutInd)
    {
        auto& ResLayout = m_pResourceLayouts[ResLayoutInd];
        if (ShaderFlags & ResLayout.GetShaderType())
        {
            ResLayout.BindResources(pResMapping, Flags, m_pBoundResourceCaches[ResLayoutInd]);
        }
    }
}

void ShaderResourceBindingD3D11Impl::InitializeStaticResources(const IPipelineState* pPipelineState)
{
    if (m_bIsStaticResourcesBound)
    {
        LOG_WARNING_MESSAGE("Static resources have already been initialized in this shader resource binding object. The operation will be ignored.");
        return;
    }

    if (pPipelineState == nullptr)
    {
        pPipelineState = GetPipelineState();
    }
    else
    {
        DEV_CHECK_ERR(pPipelineState->IsCompatibleWith(GetPipelineState()), "The pipeline state is not compatible with this SRB");
    }

    const auto* pPSOD3D11  = ValidatedCast<const PipelineStateD3D11Impl>(pPipelineState);
    auto        ppShaders  = pPSOD3D11->GetShaders();
    auto        NumShaders = pPSOD3D11->GetNumShaders();
    VERIFY_EXPR(NumShaders == m_NumActiveShaders);

    for (Uint32 shader = 0; shader < NumShaders; ++shader)
    {
        const auto& StaticResLayout = pPSOD3D11->GetStaticResourceLayout(shader);
        auto*       pShaderD3D11    = ValidatedCast<ShaderD3D11Impl>(ppShaders[shader]);
#ifdef DEVELOPMENT
        if (!StaticResLayout.dvpVerifyBindings())
        {
            LOG_ERROR_MESSAGE("Static resources in SRB of PSO '", pPSOD3D11->GetDesc().Name,
                              "' will not be successfully initialized because not all static resource bindings in shader '",
                              pShaderD3D11->GetDesc().Name,
                              "' are valid. Please make sure you bind all static resources to PSO before calling InitializeStaticResources() "
                              "directly or indirectly by passing InitStaticResources=true to CreateShaderResourceBinding() method.");
        }
#endif

#ifdef _DEBUG
        auto ShaderTypeInd     = GetShaderTypeIndex(pShaderD3D11->GetDesc().ShaderType);
        auto ResourceLayoutInd = m_ResourceLayoutIndex[ShaderTypeInd];
        VERIFY_EXPR(ResourceLayoutInd == static_cast<Int8>(shader));
#endif
        StaticResLayout.CopyResources(m_pBoundResourceCaches[shader]);
        pPSOD3D11->SetStaticSamplers(m_pBoundResourceCaches[shader], shader);
    }

    m_bIsStaticResourcesBound = true;
}

IShaderResourceVariable* ShaderResourceBindingD3D11Impl::GetVariableByName(SHADER_TYPE ShaderType, const char* Name)
{
    auto Ind = GetShaderTypeIndex(ShaderType);
    VERIFY_EXPR(Ind >= 0 && Ind < _countof(m_ResourceLayoutIndex));
    auto ResLayoutIndex = m_ResourceLayoutIndex[Ind];
    if (ResLayoutIndex < 0)
    {
        LOG_WARNING_MESSAGE("Unable to find mutable/dynamic variable '", Name, "': shader stage ",
                            GetShaderTypeLiteralName(ShaderType), " is inactive in Pipeline State '",
                            m_pPSO->GetDesc().Name, "'");
        return nullptr;
    }

    return m_pResourceLayouts[ResLayoutIndex].GetShaderVariable(Name);
}

Uint32 ShaderResourceBindingD3D11Impl::GetVariableCount(SHADER_TYPE ShaderType) const
{
    auto Ind = GetShaderTypeIndex(ShaderType);
    VERIFY_EXPR(Ind >= 0 && Ind < _countof(m_ResourceLayoutIndex));
    auto ResLayoutIndex = m_ResourceLayoutIndex[Ind];
    if (ResLayoutIndex < 0)
    {
        LOG_WARNING_MESSAGE("Unable to get the number of mutable/dynamic variables: shader stage ",
                            GetShaderTypeLiteralName(ShaderType), " is inactive in Pipeline State '",
                            m_pPSO->GetDesc().Name, "'");
        return 0;
    }

    return m_pResourceLayouts[ResLayoutIndex].GetTotalResourceCount();
}

IShaderResourceVariable* ShaderResourceBindingD3D11Impl::GetVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    auto Ind = GetShaderTypeIndex(ShaderType);
    VERIFY_EXPR(Ind >= 0 && Ind < _countof(m_ResourceLayoutIndex));
    auto ResLayoutIndex = m_ResourceLayoutIndex[Ind];
    if (ResLayoutIndex < 0)
    {
        LOG_WARNING_MESSAGE("Unable to get mutable/dynamic variable at index ", Index, ": shader stage ",
                            GetShaderTypeLiteralName(ShaderType), " is inactive in Pipeline State '",
                            m_pPSO->GetDesc().Name, "'");
        return nullptr;
    }

    return m_pResourceLayouts[ResLayoutIndex].GetShaderVariable(Index);
}

} // namespace Diligent
