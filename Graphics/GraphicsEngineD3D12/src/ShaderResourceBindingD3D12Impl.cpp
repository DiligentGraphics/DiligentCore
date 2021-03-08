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
#include "PipelineResourceSignatureD3D12Impl.hpp"
#include "FixedLinearAllocator.hpp"

namespace Diligent
{

ShaderResourceBindingD3D12Impl::ShaderResourceBindingD3D12Impl(IReferenceCounters*                 pRefCounters,
                                                               PipelineResourceSignatureD3D12Impl* pPRS) :
    // clang-format off
    TBase
    {
        pRefCounters,
        pPRS
    },
    m_ShaderResourceCache{ResourceCacheContentType::SRB}
// clang-format on
{
    try
    {
        const auto NumShaders = GetNumShaders();

        FixedLinearAllocator MemPool{GetRawAllocator()};
        MemPool.AddSpace<ShaderVariableManagerD3D12>(NumShaders);
        MemPool.Reserve();
        // Constructor of ShaderVariableManagerD3D12 is noexcept, so we can safely construct all managers.
        m_pShaderVarMgrs = MemPool.ConstructArray<ShaderVariableManagerD3D12>(NumShaders, std::ref(*this), std::ref(m_ShaderResourceCache));

        // The memory is now owned by ShaderResourceBindingD3D12Impl and will be freed by Destruct().
        auto* Ptr = MemPool.ReleaseOwnership();
        VERIFY_EXPR(Ptr == m_pShaderVarMgrs);
        (void)Ptr;

        // It is important to construct all objects before initializing them because if an exception is thrown,
        // destructors will be called for all objects

        auto& SRBMemAllocator            = pPRS->GetSRBMemoryAllocator();
        auto& ResourceCacheDataAllocator = SRBMemAllocator.GetResourceCacheDataAllocator(0);
        pPRS->InitSRBResourceCache(m_ShaderResourceCache, ResourceCacheDataAllocator, pPRS->GetDesc().Name);

        for (Uint32 s = 0; s < NumShaders; ++s)
        {
            const auto ShaderType = pPRS->GetActiveShaderStageType(s);
            const auto ShaderInd  = GetShaderTypePipelineIndex(ShaderType, pPRS->GetPipelineType());
            const auto MgrInd     = m_ActiveShaderStageIndex[ShaderInd];
            VERIFY_EXPR(MgrInd >= 0 && MgrInd < static_cast<int>(NumShaders));

            auto& VarDataAllocator = SRBMemAllocator.GetShaderVariableDataAllocator(s);

            // It is important that initialization is separated from construction because it provides exception safety.
            constexpr SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC};
            m_pShaderVarMgrs[MgrInd].Initialize(
                *pPRS,
                VarDataAllocator,
                AllowedVarTypes,
                _countof(AllowedVarTypes),
                ShaderType //
            );
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
        for (Uint32 s = 0; s < GetNumShaders(); ++s)
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
    BindResourcesImpl(ShaderFlags, pResMapping, Flags, m_pShaderVarMgrs);
}

IShaderResourceVariable* ShaderResourceBindingD3D12Impl::GetVariableByName(SHADER_TYPE ShaderType, const char* Name)
{
    return GetVariableByNameImpl(ShaderType, Name, m_pShaderVarMgrs);
}

Uint32 ShaderResourceBindingD3D12Impl::GetVariableCount(SHADER_TYPE ShaderType) const
{
    return GetVariableCountImpl(ShaderType, m_pShaderVarMgrs);
}

IShaderResourceVariable* ShaderResourceBindingD3D12Impl::GetVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    return GetVariableByIndexImpl(ShaderType, Index, m_pShaderVarMgrs);
}

} // namespace Diligent
