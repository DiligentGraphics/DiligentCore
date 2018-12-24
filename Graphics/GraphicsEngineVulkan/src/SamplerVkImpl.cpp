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
#include "GraphicsAccessories.h"

namespace Diligent
{

SamplerVkImpl::SamplerVkImpl(IReferenceCounters* pRefCounters, RenderDeviceVkImpl* pRenderDeviceVk, const SamplerDesc& SamplerDesc) : 
    TSamplerBase(pRefCounters, pRenderDeviceVk, SamplerDesc)
{
    const auto &LogicalDevice = pRenderDeviceVk->GetLogicalDevice();
    VkSamplerCreateInfo SamplerCI = {};
    SamplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    SamplerCI.pNext = nullptr;
    SamplerCI.flags = 0; // reserved for future use
    SamplerCI.magFilter = FilterTypeToVkFilter(m_Desc.MagFilter);
    SamplerCI.minFilter = FilterTypeToVkFilter(m_Desc.MinFilter);
    SamplerCI.mipmapMode = FilterTypeToVkMipmapMode(m_Desc.MipFilter);
    SamplerCI.addressModeU = AddressModeToVkAddressMode(m_Desc.AddressU);
    SamplerCI.addressModeV = AddressModeToVkAddressMode(m_Desc.AddressV);
    SamplerCI.addressModeW = AddressModeToVkAddressMode(m_Desc.AddressW);
    SamplerCI.mipLodBias = m_Desc.MipLODBias;
    SamplerCI.anisotropyEnable = IsAnisotropicFilter(m_Desc.MinFilter);
#ifdef DEVELOPMENT
    if( !( (SamplerCI.anisotropyEnable &&  IsAnisotropicFilter(m_Desc.MagFilter)) ||
          (!SamplerCI.anisotropyEnable && !IsAnisotropicFilter(m_Desc.MagFilter)) ) )
    {
        LOG_ERROR("Min and mag fiters must both be either anisotropic filters or non-anisotropic ones");
    }
#endif

    SamplerCI.maxAnisotropy = static_cast<float>(m_Desc.MaxAnisotropy);
    SamplerCI.compareEnable = IsComparisonFilter(m_Desc.MinFilter);
#ifdef DEVELOPMENT
    if( !( (SamplerCI.compareEnable &&  IsComparisonFilter(m_Desc.MagFilter)) ||
          (!SamplerCI.compareEnable && !IsComparisonFilter(m_Desc.MagFilter)) ) )
    {
        LOG_ERROR("Min and mag fiters must both be either comparison filters or non-comparison ones");
    }
#endif

    SamplerCI.compareOp = ComparisonFuncToVkCompareOp(m_Desc.ComparisonFunc);
    SamplerCI.minLod = m_Desc.MinLOD;
    SamplerCI.maxLod = m_Desc.MaxLOD;
    SamplerCI.borderColor = BorderColorToVkBorderColor(m_Desc.BorderColor);
    SamplerCI.unnormalizedCoordinates = VK_FALSE;

    m_VkSampler = LogicalDevice.CreateSampler(SamplerCI);
}

SamplerVkImpl::~SamplerVkImpl()
{
    m_pDevice->SafeReleaseDeviceObject(std::move(m_VkSampler), m_CommandQueueMask);
}

IMPLEMENT_QUERY_INTERFACE( SamplerVkImpl, IID_SamplerVk, TSamplerBase )

}
