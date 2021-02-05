/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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
#include "ShaderResourceBindingD3D12Impl.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "FixedLinearAllocator.hpp"

namespace Diligent
{

ShaderResourceBindingD3D12Impl::ShaderResourceBindingD3D12Impl(IReferenceCounters*                 pRefCounters,
                                                               PipelineResourceSignatureD3D12Impl* pPRS,
                                                               bool                                IsDeviceInternal) :
    // clang-format off
    TBase
    {
        pRefCounters,
        pPRS,
        IsDeviceInternal
    },
    m_ShaderResourceCache{ShaderResourceCacheD3D12::CacheContentType::SRB},
    m_NumShaders         {static_cast<decltype(m_NumShaders)>(pPRS->GetNumActiveShaderStages())}
// clang-format on
{
    try
    {
        m_ShaderVarIndex.fill(-1);

        FixedLinearAllocator MemPool{GetRawAllocator()};
        MemPool.AddSpace<ShaderVariableManagerD3D12>(m_NumShaders);
        MemPool.Reserve();
        m_pShaderVarMgrs = MemPool.ConstructArray<ShaderVariableManagerD3D12>(m_NumShaders, std::ref(*this), std::ref(m_ShaderResourceCache));

        // The memory is now owned by ShaderResourceBindingD3D12Impl and will be freed by Destruct().
        auto* Ptr = MemPool.ReleaseOwnership();
        VERIFY_EXPR(Ptr == m_pShaderVarMgrs);
        (void)Ptr;

        // It is important to construct all objects before initializing them because if an exception is thrown,
        // destructors will be called for all objects

        auto& SRBMemAllocator            = pPRS->GetSRBMemoryAllocator();
        auto& ResourceCacheDataAllocator = SRBMemAllocator.GetResourceCacheDataAllocator(0);
        pPRS->InitSRBResourceCache(m_ShaderResourceCache, ResourceCacheDataAllocator, pPRS->GetDesc().Name);

        for (Uint32 s = 0; s < m_NumShaders; ++s)
        {
            const auto ShaderType = pPRS->GetActiveShaderStageType(s);
            const auto ShaderInd  = GetShaderTypePipelineIndex(ShaderType, pPRS->GetPipelineType());

            auto& VarDataAllocator = SRBMemAllocator.GetShaderVariableDataAllocator(s);

            // http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-layout#Initializing-Resource-Layouts-in-a-Shader-Resource-Binding-Object
            const SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC};
            m_pShaderVarMgrs[s].Initialize(
                *pPRS,
                VarDataAllocator,
                AllowedVarTypes,
                _countof(AllowedVarTypes),
                ShaderType //
            );

            m_ShaderVarIndex[ShaderInd] = static_cast<Int8>(s);
        }
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}


ShaderResourceBindingD3D12Impl::~ShaderResourceBindingD3D12Impl()
{
    Destruct();
}

void ShaderResourceBindingD3D12Impl::Destruct()
{
    if (m_pShaderVarMgrs != nullptr)
    {
        auto& SRBMemAllocator = GetSignature()->GetSRBMemoryAllocator();
        for (Uint32 s = 0; s < m_NumShaders; ++s)
        {
            auto& VarDataAllocator = SRBMemAllocator.GetShaderVariableDataAllocator(s);
            m_pShaderVarMgrs[s].Destroy(VarDataAllocator);
            m_pShaderVarMgrs[s].~ShaderVariableManagerD3D12();
        }
        GetRawAllocator().Free(m_pShaderVarMgrs);
    }
}

void ShaderResourceBindingD3D12Impl::BindResources(Uint32 ShaderFlags, IResourceMapping* pResMapping, Uint32 Flags)
{
    const auto PipelineType = GetPipelineType();
    for (Int32 ShaderInd = 0; ShaderInd < static_cast<Int32>(m_ShaderVarIndex.size()); ++ShaderInd)
    {
        auto VarMngrInd = m_ShaderVarIndex[ShaderInd];
        if (VarMngrInd >= 0)
        {
            // ShaderInd is the shader type pipeline index here
            const auto ShaderType = GetShaderTypeFromPipelineIndex(ShaderInd, PipelineType);
            if (ShaderFlags & ShaderType)
            {
                m_pShaderVarMgrs[VarMngrInd].BindResources(pResMapping, Flags);
            }
        }
    }
}

IShaderResourceVariable* ShaderResourceBindingD3D12Impl::GetVariableByName(SHADER_TYPE ShaderType, const char* Name)
{
    auto VarMngrInd = GetVariableByNameHelper(ShaderType, Name, m_ShaderVarIndex);
    if (VarMngrInd < 0)
        return nullptr;

    VERIFY_EXPR(static_cast<Uint32>(VarMngrInd) < Uint32{m_NumShaders});
    return m_pShaderVarMgrs[VarMngrInd].GetVariable(Name);
}

Uint32 ShaderResourceBindingD3D12Impl::GetVariableCount(SHADER_TYPE ShaderType) const
{
    auto VarMngrInd = GetVariableCountHelper(ShaderType, m_ShaderVarIndex);
    if (VarMngrInd < 0)
        return 0;

    VERIFY_EXPR(static_cast<Uint32>(VarMngrInd) < Uint32{m_NumShaders});
    return m_pShaderVarMgrs[VarMngrInd].GetVariableCount();
}

IShaderResourceVariable* ShaderResourceBindingD3D12Impl::GetVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    auto VarMngrInd = GetVariableByIndexHelper(ShaderType, Index, m_ShaderVarIndex);
    if (VarMngrInd < 0)
        return nullptr;

    VERIFY_EXPR(static_cast<Uint32>(VarMngrInd) < Uint32{m_NumShaders});
    return m_pShaderVarMgrs[VarMngrInd].GetVariable(Index);
}

void ShaderResourceBindingD3D12Impl::InitializeStaticResources(const IPipelineState* pPipelineState)
{
    if (StaticResourcesInitialized())
    {
        LOG_WARNING_MESSAGE("Static resources have already been initialized in this shader resource binding object. The operation will be ignored.");
        return;
    }

    if (pPipelineState == nullptr)
    {
        InitializeStaticResourcesWithSignature(nullptr);
    }
    else
    {
        auto* pSign = pPipelineState->GetResourceSignature(GetBindingIndex());
        if (pSign == nullptr)
        {
            LOG_ERROR_MESSAGE("Shader resource binding is not compatible with pipeline state.");
            return;
        }

        InitializeStaticResourcesWithSignature(pSign);
    }
}

void ShaderResourceBindingD3D12Impl::InitializeStaticResourcesWithSignature(const IPipelineResourceSignature* pResourceSignature)
{
    if (pResourceSignature == nullptr)
        pResourceSignature = GetPipelineResourceSignature();

    auto* pPRSD3D12 = ValidatedCast<const PipelineResourceSignatureD3D12Impl>(pResourceSignature);
    pPRSD3D12->InitializeStaticSRBResources(m_ShaderResourceCache);
    m_bStaticResourcesInitialized = true;
}

} // namespace Diligent
