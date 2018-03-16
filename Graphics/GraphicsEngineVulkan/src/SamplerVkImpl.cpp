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
#include "SamplerVkImpl.h"
#include "RenderDeviceVkImpl.h"
#include "VulkanTypeConversions.h"

namespace Diligent
{

SamplerVkImpl::SamplerVkImpl(IReferenceCounters *pRefCounters, class RenderDeviceVkImpl *pRenderDeviceVk, const SamplerDesc& SamplerDesc) : 
    TSamplerBase(pRefCounters, pRenderDeviceVk, SamplerDesc)
{
#if 0
    auto *pVkDevice = pRenderDeviceVk->GetVkDevice();
    Vk_SAMPLER_DESC VkSamplerDesc = 
    {
        FilterTypeToVkFilter(SamplerDesc.MinFilter, SamplerDesc.MagFilter, SamplerDesc.MipFilter),
        TexAddressModeToVkAddressMode(SamplerDesc.AddressU),
        TexAddressModeToVkAddressMode(SamplerDesc.AddressV),
        TexAddressModeToVkAddressMode(SamplerDesc.AddressW),
        SamplerDesc.MipLODBias,
        SamplerDesc.MaxAnisotropy,
        ComparisonFuncToVkComparisonFunc(SamplerDesc.ComparisonFunc),
        {SamplerDesc.BorderColor[0], SamplerDesc.BorderColor[1], SamplerDesc.BorderColor[2], SamplerDesc.BorderColor[3]},
        SamplerDesc.MinLOD,
        SamplerDesc.MaxLOD
    };

    auto CPUDescriptorAlloc = pRenderDeviceVk->AllocateDescriptor(Vk_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    m_Descriptor = std::move(CPUDescriptorAlloc);
	pVkDevice->CreateSampler(&VkSamplerDesc, m_Descriptor.GetCpuHandle());
#endif
}

SamplerVkImpl::~SamplerVkImpl()
{

}

IMPLEMENT_QUERY_INTERFACE( SamplerVkImpl, IID_SamplerVk, TSamplerBase )

}
