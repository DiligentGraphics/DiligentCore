/*     Copyright 2019 Diligent Graphics LLC
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

#pragma once

#include "DeviceObjectBase.h"
#include "RenderDeviceVkImpl.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"

namespace Diligent
{

struct SemaphoreObjectDesc : DeviceObjectAttribs
{};

class SemaphoreObject : public DeviceObjectBase<IDeviceObject, RenderDeviceVkImpl, SemaphoreObjectDesc>
{
public:
    using TDeviceObjectBase = DeviceObjectBase<IDeviceObject, RenderDeviceVkImpl, SemaphoreObjectDesc>;

    SemaphoreObject(IReferenceCounters*        pRefCounters,
                    RenderDeviceVkImpl*        pDevice,
                    const SemaphoreObjectDesc& ObjDesc,
                    bool                       bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, ObjDesc, bIsDeviceInternal}
    {
        const auto& LogicalDevice = m_pDevice->GetLogicalDevice();

        VkSemaphoreCreateInfo SemaphoreCI = {};

        SemaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        SemaphoreCI.pNext = nullptr;
        SemaphoreCI.flags = 0; // reserved for future use

        m_VkSemaphore = LogicalDevice.CreateSemaphore(SemaphoreCI, m_Desc.Name);
    }

    ~SemaphoreObject()
    {
        m_pDevice->SafeReleaseDeviceObject(std::move(m_VkSemaphore), ~Uint64{0});
    }

    static void Create(RenderDeviceVkImpl* pDevice, const char* Name, SemaphoreObject** ppSemaphore)
    {
        SemaphoreObjectDesc Desc;
        Desc.Name = Name;
        auto* pSemaphoreObj(NEW_RC_OBJ(GetRawAllocator(), "SemaphoreObject instance", SemaphoreObject)(pDevice, Desc));
        pSemaphoreObj->QueryInterface(IID_DeviceObject, reinterpret_cast<IObject**>(ppSemaphore));
    }

    VkSemaphore GetVkSemaphore() const
    {
        return m_VkSemaphore;
    }

private:
    VulkanUtilities::SemaphoreWrapper m_VkSemaphore;
};

} // namespace Diligent
