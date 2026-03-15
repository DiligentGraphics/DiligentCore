/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include "SuperResolutionFactoryBase.hpp"

#include "SuperResolutionFactoryLoader.h"

#include "PlatformDebug.hpp"
#include "EngineMemory.h"

namespace Diligent
{

#if D3D12_SUPPORTED
void CreateSuperResolutionFactoryD3D12(IRenderDevice* pDevice, ISuperResolutionFactory** ppFactory);
#endif

#if METAL_SUPPORTED
void CreateSuperResolutionFactoryMtl(IRenderDevice* pDevice, ISuperResolutionFactory** ppFactory);
#endif

SuperResolutionFactoryBase::SuperResolutionFactoryBase(IReferenceCounters* pRefCounters, IRenderDevice* pDevice) :
    TBase{pRefCounters},
    m_pDevice{pDevice}
{
}

void SuperResolutionFactoryBase::EnumerateVariants(Uint32& NumVariants, SuperResolutionInfo* Variants)
{
    if (Variants == nullptr)
    {
        NumVariants = static_cast<Uint32>(m_Variants.size());
        return;
    }

    NumVariants = std::min(NumVariants, static_cast<Uint32>(m_Variants.size()));
    memcpy(Variants, m_Variants.data(), NumVariants * sizeof(SuperResolutionInfo));
}

void SuperResolutionFactoryBase::SetMessageCallback(DebugMessageCallbackType MessageCallback) const
{
    SetDebugMessageCallback(MessageCallback);
}

void SuperResolutionFactoryBase::SetBreakOnError(bool BreakOnError) const
{
    PlatformDebug::SetBreakOnError(BreakOnError);
}

void SuperResolutionFactoryBase::SetMemoryAllocator(IMemoryAllocator* pAllocator) const
{
    SetRawAllocator(pAllocator);
}


API_QUALIFIER void CreateSuperResolutionFactory(IRenderDevice* pDevice, ISuperResolutionFactory** ppFactory)
{
    if (ppFactory == nullptr)
    {
        DEV_ERROR("ppFactory must not be null");
        return;
    }
    DEV_CHECK_ERR(*ppFactory == nullptr, "ppFactory is not null. Overwriting it may cause memory leak");

    *ppFactory = nullptr;
    if (pDevice == nullptr)
    {
        DEV_ERROR("pDevice must not be null");
        return;
    }

    RENDER_DEVICE_TYPE DeviceType = pDevice->GetDeviceInfo().Type;
    try
    {
        switch (DeviceType)
        {
            case RENDER_DEVICE_TYPE_D3D12:
#if D3D12_SUPPORTED
                CreateSuperResolutionFactoryD3D12(pDevice, ppFactory);
#endif
                break;

            case RENDER_DEVICE_TYPE_METAL:
#if METAL_SUPPORTED
                CreateSuperResolutionFactoryMtl(pDevice, ppFactory);
#endif
                break;

            default:
                LOG_ERROR_MESSAGE("Super resolution is not supported on this device type: ", DeviceType);
        }
    }
    catch (...)
    {
        LOG_ERROR("Failed to create super resolution factory");
    }
}

} // namespace Diligent

extern "C"
{
    API_QUALIFIER
    void Diligent_CreateSuperResolutionFactory(Diligent::IRenderDevice*            pDevice,
                                               Diligent::ISuperResolutionFactory** ppFactory)
    {
        Diligent::CreateSuperResolutionFactory(pDevice, ppFactory);
    }
}
