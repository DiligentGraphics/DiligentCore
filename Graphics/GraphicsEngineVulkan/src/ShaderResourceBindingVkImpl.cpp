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
#include "ShaderResourceBindingVkImpl.hpp"
#include "PipelineStateVkImpl.hpp"
#include "ShaderVkImpl.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "FixedLinearAllocator.hpp"

namespace Diligent
{

ShaderResourceBindingVkImpl::ShaderResourceBindingVkImpl(IReferenceCounters*              pRefCounters,
                                                         PipelineResourceSignatureVkImpl* pPRS,
                                                         bool                             IsPSOInternal) :
    // clang-format off
    TBase
    {
        pRefCounters,
        pPRS,
        IsPSOInternal
    },
    m_ShaderResourceCache{ShaderResourceCacheVk::DbgCacheContentType::SRBResources}
// clang-format on
{
    try
    {
        m_ShaderVarIndex.fill(-1);

        m_NumShaders = static_cast<decltype(m_NumShaders)>(pPRS->GetNumShaderStages());

        FixedLinearAllocator MemPool{GetRawAllocator()};
        MemPool.AddSpace<ShaderVariableManagerVk>(m_NumShaders);
        MemPool.Reserve();
        m_pShaderVarMgrs = MemPool.ConstructArray<ShaderVariableManagerVk>(m_NumShaders, std::ref(*this), std::ref(m_ShaderResourceCache));

        // The memory is now owned by ShaderResourceBindingVkImpl and will be freed by Destruct().
        auto* Ptr = MemPool.ReleaseOwnership();
        VERIFY_EXPR(Ptr == m_pShaderVarMgrs);
        (void)Ptr;

        // It is important to construct all objects before initializing them because if an exception is thrown,
        // destructors will be called for all objects

        // This will only allocate memory and initialize descriptor sets in the resource cache
        // Resources will be initialized by InitializeResourceMemoryInCache()
        auto& SRBMemAllocator            = pPRS->GetSRBMemoryAllocator();
        auto& ResourceCacheDataAllocator = SRBMemAllocator.GetResourceCacheDataAllocator(0);
        pPRS->InitResourceCache(m_ShaderResourceCache, ResourceCacheDataAllocator, pPRS->GetDesc().Name);

        // Use resource signature to initialize resource memory in the cache
        pPRS->InitializeResourceMemoryInCache(m_ShaderResourceCache);

        for (Uint32 s = 0; s < m_NumShaders; ++s)
        {
            const auto ShaderType = pPRS->GetShaderStageType(s);
            const auto ShaderInd  = GetShaderTypePipelineIndex(ShaderType, pPRS->GetPipelineType());

            m_ShaderVarIndex[ShaderInd] = static_cast<Int8>(s);

            auto& VarDataAllocator = SRBMemAllocator.GetShaderVariableDataAllocator(s);

            // Create shader variable manager in place
            // Initialize vars manager to reference mutable and dynamic variables
            // Note that the cache has space for all variable types
            const SHADER_RESOURCE_VARIABLE_TYPE VarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC};
            m_pShaderVarMgrs[s].Initialize(*pPRS, VarDataAllocator, VarTypes, _countof(VarTypes), ShaderType);
        }
#ifdef DILIGENT_DEBUG
        m_ShaderResourceCache.DbgVerifyResourceInitialization();
#endif
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

ShaderResourceBindingVkImpl::~ShaderResourceBindingVkImpl()
{
    Destruct();
}

void ShaderResourceBindingVkImpl::Destruct()
{
    if (m_pShaderVarMgrs != nullptr)
    {
        auto& SRBMemAllocator = GetSignature()->GetSRBMemoryAllocator();
        for (Uint32 s = 0; s < m_NumShaders; ++s)
        {
            auto& VarDataAllocator = SRBMemAllocator.GetShaderVariableDataAllocator(s);
            m_pShaderVarMgrs[s].DestroyVariables(VarDataAllocator);
            m_pShaderVarMgrs[s].~ShaderVariableManagerVk();
        }

        GetRawAllocator().Free(m_pShaderVarMgrs);
    }
}

void ShaderResourceBindingVkImpl::BindResources(Uint32 ShaderFlags, IResourceMapping* pResMapping, Uint32 Flags)
{
    const auto PipelineType = GetPipelineType();
    for (Uint32 ShaderInd = 0; ShaderInd < m_ShaderVarIndex.size(); ++ShaderInd)
    {
        const auto VarMngrInd = m_ShaderVarIndex[ShaderInd];
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

Uint32 ShaderResourceBindingVkImpl::GetVariableCount(SHADER_TYPE ShaderType) const
{
    const auto VarMngrInd = GetVariableCountHelper(ShaderType, m_ShaderVarIndex);
    if (VarMngrInd < 0)
        return 0;

    auto& ShaderVarMgr = m_pShaderVarMgrs[VarMngrInd];
    return ShaderVarMgr.GetVariableCount();
}

IShaderResourceVariable* ShaderResourceBindingVkImpl::GetVariableByName(SHADER_TYPE ShaderType, const Char* Name)
{
    const auto VarMngrInd = GetVariableByNameHelper(ShaderType, Name, m_ShaderVarIndex);
    if (VarMngrInd < 0)
        return nullptr;

    auto& ShaderVarMgr = m_pShaderVarMgrs[VarMngrInd];
    return ShaderVarMgr.GetVariable(Name);
}

IShaderResourceVariable* ShaderResourceBindingVkImpl::GetVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    const auto VarMngrInd = GetVariableByIndexHelper(ShaderType, Index, m_ShaderVarIndex);
    if (VarMngrInd < 0)
        return nullptr;

    const auto& ShaderVarMgr = m_pShaderVarMgrs[VarMngrInd];
    return ShaderVarMgr.GetVariable(Index);
}

void ShaderResourceBindingVkImpl::InitializeStaticResources(const IPipelineState* pPipelineState)
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

void ShaderResourceBindingVkImpl::InitializeStaticResourcesWithSignature(const IPipelineResourceSignature* pResourceSignature)
{
    if (pResourceSignature == nullptr)
        pResourceSignature = GetPipelineResourceSignature();

    auto* pPRSVk = ValidatedCast<const PipelineResourceSignatureVkImpl>(pResourceSignature);
    pPRSVk->InitializeStaticSRBResources(m_ShaderResourceCache);
    m_bStaticResourcesInitialized = true;
}

} // namespace Diligent
