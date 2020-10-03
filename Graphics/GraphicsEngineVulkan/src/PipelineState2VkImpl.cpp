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
#include <array>
#include "PipelineState2VkImpl.hpp"
#include "ShaderVkImpl.hpp"
#include "VulkanTypeConversions.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "DeviceContextVkImpl.hpp"
#include "RenderPassVkImpl.hpp"
#include "ShaderResourceBindingVkImpl.hpp"
#include "EngineMemory.h"
#include "StringTools.hpp"

#if !DILIGENT_NO_HLSL
#    include "spirv-tools/optimizer.hpp"
#endif

namespace Diligent
{

static std::vector<uint32_t> StripReflection(const std::vector<uint32_t>& OriginalSPIRV)
{
#if DILIGENT_NO_HLSL
    return OriginalSPIRV;
#else
    std::vector<uint32_t> StrippedSPIRV;
    spvtools::Optimizer   SpirvOptimizer(SPV_ENV_VULKAN_1_0);
    // Decorations defined in SPV_GOOGLE_hlsl_functionality1 are the only instructions
    // removed by strip-reflect-info pass. SPIRV offsets become INVALID after this operation.
    SpirvOptimizer.RegisterPass(spvtools::CreateStripReflectInfoPass());
    auto res = SpirvOptimizer.Run(OriginalSPIRV.data(), OriginalSPIRV.size(), &StrippedSPIRV);
    if (!res)
    {
        // Optimized SPIRV may be invalid
        StrippedSPIRV.clear();
    }
    return StrippedSPIRV;
#endif
}

PipelineState2VkImpl::PipelineState2VkImpl(IReferenceCounters*            pRefCounters,
                                           RenderDeviceVkImpl*            pDeviceVk,
                                           const PipelineStateCreateInfo& CreateInfo) :
    TParent{pRefCounters, pDeviceVk, CreateInfo.PSODesc}
{
    // AZ TODO
}

PipelineState2VkImpl::~PipelineState2VkImpl()
{
    // AZ TODO
}

IMPLEMENT_QUERY_INTERFACE(PipelineState2VkImpl, IID_PipelineState2Vk, TParent)


void PipelineState2VkImpl::CreateShaderResourceBinding(IShaderResourceBinding2** ppShaderResourceBinding, bool InitStaticResources)
{
    // AZ TODO
}

bool PipelineState2VkImpl::IsCompatibleWith(const IPipelineState2* pPSO) const
{
    // AZ TODO
    return false;
}

void PipelineState2VkImpl::CommitAndTransitionShaderResources(IShaderResourceBinding2*               pShaderResourceBinding,
                                                              DeviceContextVkImpl*                   pCtxVkImpl,
                                                              bool                                   CommitResources,
                                                              RESOURCE_STATE_TRANSITION_MODE         StateTransitionMode,
                                                              PipelineLayout::DescriptorSetBindInfo* pDescrSetBindInfo) const
{
    // AZ TODO
}

void PipelineState2VkImpl::BindStaticResources(IResourceMapping* pResourceMapping, Uint32 Flags)
{
    // AZ TODO
}

Uint32 PipelineState2VkImpl::GetStaticVariableCount() const
{
    // AZ TODO
    return 0;
}

IShaderResourceVariable* PipelineState2VkImpl::GetStaticVariableByName(const Char* Name)
{
    // AZ TODO
    return nullptr;
}

IShaderResourceVariable* PipelineState2VkImpl::GetStaticVariableByIndex(Uint32 Index)
{
    // AZ TODO
    return nullptr;
}

void PipelineState2VkImpl::InitializeStaticSRBResources(ShaderResourceCacheVk& ResourceCache) const
{
    // AZ TODO
}

} // namespace Diligent
