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

#include "SuperResolutionFactoryLoader.h"
#include "DebugUtilities.hpp"

namespace Diligent
{

void CreateSuperResolutionFactoryD3D12(IRenderDevice* pDevice, ISuperResolutionFactory** ppFactory);
void CreateSuperResolutionFactoryD3D11(IRenderDevice* pDevice, ISuperResolutionFactory** ppFactory);
void CreateSuperResolutionFactoryVk(IRenderDevice* pDevice, ISuperResolutionFactory** ppFactory);
void CreateSuperResolutionFactoryMtl(IRenderDevice* pDevice, ISuperResolutionFactory** ppFactory);

API_QUALIFIER void CreateSuperResolutionFactory(IRenderDevice* pDevice, ISuperResolutionFactory** ppFactory)
{
    DEV_CHECK_ERR(ppFactory != nullptr, "ppFactory must not be null");
    if (ppFactory == nullptr)
        return;

    *ppFactory = nullptr;

    DEV_CHECK_ERR(pDevice != nullptr, "pDevice must not be null");
    if (pDevice == nullptr)
        return;

    try
    {
        switch (pDevice->GetDeviceInfo().Type)
        {
            case RENDER_DEVICE_TYPE_D3D12:
                CreateSuperResolutionFactoryD3D12(pDevice, ppFactory);
                break;
            case RENDER_DEVICE_TYPE_D3D11:
                CreateSuperResolutionFactoryD3D11(pDevice, ppFactory);
                break;
            case RENDER_DEVICE_TYPE_VULKAN:
                CreateSuperResolutionFactoryVk(pDevice, ppFactory);
                break;
            case RENDER_DEVICE_TYPE_METAL:
                CreateSuperResolutionFactoryMtl(pDevice, ppFactory);
                break;
            default:
                break;
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
