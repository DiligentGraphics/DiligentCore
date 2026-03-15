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

#include "EngineMemory.h"
#include "DebugUtilities.hpp"

namespace Diligent
{

namespace
{

class SuperResolutionFactoryD3D12 final : public SuperResolutionFactoryBase
{
public:
    using TBase = SuperResolutionFactoryBase;

    SuperResolutionFactoryD3D12(IReferenceCounters* pRefCounters, IRenderDevice* pDevice);

    virtual void DILIGENT_CALL_TYPE GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs, SuperResolutionSourceSettings& Settings) const override final;

    virtual void DILIGENT_CALL_TYPE CreateSuperResolution(const SuperResolutionDesc& Desc, ISuperResolution** ppUpscaler) override final;

private:
    void PopulateVariants();
};


SuperResolutionFactoryD3D12::SuperResolutionFactoryD3D12(IReferenceCounters* pRefCounters, IRenderDevice* pDevice) :
    TBase{pRefCounters, pDevice}
{
    PopulateVariants();
}

void SuperResolutionFactoryD3D12::PopulateVariants()
{
}

void SuperResolutionFactoryD3D12::GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs, SuperResolutionSourceSettings& Settings) const
{
    Settings = {};
}

void SuperResolutionFactoryD3D12::CreateSuperResolution(const SuperResolutionDesc& Desc, ISuperResolution** ppUpscaler)
{
}

} // namespace

void CreateSuperResolutionFactoryD3D12(IRenderDevice* pDevice, ISuperResolutionFactory** ppFactory)
{
    VERIFY(pDevice != nullptr, "pDevice must not be null");
    VERIFY(pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D12, "Expected a D3D12 device");

    SuperResolutionFactoryD3D12* pFactory = NEW_RC_OBJ(GetRawAllocator(), "SuperResolutionFactoryD3D12 instance", SuperResolutionFactoryD3D12)(pDevice);
    pFactory->QueryInterface(IID_SuperResolutionFactory, reinterpret_cast<IObject**>(ppFactory));
}

} // namespace Diligent
