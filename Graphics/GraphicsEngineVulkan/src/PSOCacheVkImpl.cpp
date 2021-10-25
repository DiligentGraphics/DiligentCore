/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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
#include "PSOCacheVkImpl.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "VulkanTypeConversions.hpp"
#include "DataBlobImpl.hpp"

namespace Diligent
{

PSOCacheVkImpl::PSOCacheVkImpl(IReferenceCounters*       pRefCounters,
                               RenderDeviceVkImpl*       pRenderDeviceVk,
                               const PSOCacheCreateInfo& CreateInfo) :
    // clang-format off
    TPSOCacheBase
    {
        pRefCounters,
        pRenderDeviceVk,
        CreateInfo,
        false
    }
// clang-format on
{
    VkPipelineCacheCreateInfo VkPSOCacheCI{};
    VkPSOCacheCI.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    if (CreateInfo.pCacheData != nullptr && CreateInfo.CacheDataSize > 0)
    {
        VkPSOCacheCI.initialDataSize = CreateInfo.CacheDataSize;
        VkPSOCacheCI.pInitialData    = CreateInfo.pCacheData;
    }

    m_PSOCache = m_pDevice->GetLogicalDevice().CreatePipelineCache(VkPSOCacheCI, m_Desc.Name);
}

PSOCacheVkImpl::~PSOCacheVkImpl()
{
    // Vk object can only be destroyed when it is no longer used by the GPU
    if (m_PSOCache != VK_NULL_HANDLE)
        m_pDevice->SafeReleaseDeviceObject(std::move(m_PSOCache), ~Uint64{0});
}

void PSOCacheVkImpl::GetData(IDataBlob** ppBlob)
{
    DEV_CHECK_ERR(ppBlob != nullptr, "ppBlob must not be null");
    *ppBlob = nullptr;

    const auto vkDevice = m_pDevice->GetLogicalDevice().GetVkDevice();

    size_t DataSize = 0;
    if (vkGetPipelineCacheData(vkDevice, m_PSOCache, &DataSize, nullptr) != VK_SUCCESS)
        return;

    RefCntAutoPtr<DataBlobImpl> pDataBlob{MakeNewRCObj<DataBlobImpl>()(DataSize)};

    if (vkGetPipelineCacheData(vkDevice, m_PSOCache, &DataSize, pDataBlob->GetDataPtr()) != VK_SUCCESS)
        return;

    *ppBlob = pDataBlob.Detach();
}

} // namespace Diligent
