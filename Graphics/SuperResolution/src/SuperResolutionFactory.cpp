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

#include "SuperResolutionFactory.h"
#include "SuperResolutionFactoryLoader.h"
#include "ObjectBase.hpp"
#include "RefCntAutoPtr.hpp"
#include "EngineMemory.h"
#include "PlatformDebug.hpp"
#include "DebugUtilities.hpp"
#include "SuperResolutionInternal.hpp"

#if D3D12_SUPPORTED
#    include "SuperResolution_D3D12.hpp"
#endif

namespace Diligent
{

namespace
{

class SuperResolutionFactoryImpl final : public ObjectBase<ISuperResolutionFactory>
{
public:
    using TBase = ObjectBase<ISuperResolutionFactory>;

    SuperResolutionFactoryImpl(IReferenceCounters* pRefCounters, IRenderDevice* pDevice);

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_SuperResolutionFactory, TBase)

    virtual void DILIGENT_CALL_TYPE EnumerateVariants(Uint32& NumVariants, SuperResolutionInfo* Variants) override final;

    virtual void DILIGENT_CALL_TYPE GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs, SuperResolutionSourceSettings& Settings) const override final;

    virtual void DILIGENT_CALL_TYPE CreateSuperResolution(const SuperResolutionDesc& Desc, ISuperResolution** ppUpscaler) override final;

    virtual void DILIGENT_CALL_TYPE SetMessageCallback(DebugMessageCallbackType MessageCallback) const override final;

    virtual void DILIGENT_CALL_TYPE SetBreakOnError(bool BreakOnError) const override final;

    virtual void DILIGENT_CALL_TYPE SetMemoryAllocator(IMemoryAllocator* pAllocator) const override final;

private:
    void PopulateVariants();

private:
    SUPER_RESOLUTION_BACKEND FindVariant(const INTERFACE_ID& VariantId) const;

    RefCntAutoPtr<IRenderDevice> m_pDevice;
    SuperResolutionVariants      m_Variants{};

#if D3D12_SUPPORTED
    CComPtr<IDSRDevice> m_pDSRDevice;
#endif
};


SuperResolutionFactoryImpl::SuperResolutionFactoryImpl(IReferenceCounters* pRefCounters, IRenderDevice* pDevice) :
    TBase{pRefCounters},
    m_pDevice{pDevice}
{
#if D3D12_SUPPORTED
    if (m_pDevice != nullptr && m_pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D12)
        m_pDSRDevice = CreateDSRDeviceD3D12(m_pDevice);
#endif

    PopulateVariants();
}

void SuperResolutionFactoryImpl::PopulateVariants()
{
#if D3D12_SUPPORTED
    if (m_pDSRDevice)
        EnumerateVariantsD3D12(m_pDSRDevice, m_Variants[SUPER_RESOLUTION_BACKEND_D3D12_DSR]);
#endif
}

SUPER_RESOLUTION_BACKEND SuperResolutionFactoryImpl::FindVariant(const INTERFACE_ID& VariantId) const
{
    for (Uint32 BackendIdx = 0; BackendIdx < SUPER_RESOLUTION_BACKEND_COUNT; ++BackendIdx)
    {
        for (const auto& Info : m_Variants[BackendIdx])
        {
            if (Info.VariantId == VariantId)
                return static_cast<SUPER_RESOLUTION_BACKEND>(BackendIdx);
        }
    }
    return SUPER_RESOLUTION_BACKEND_COUNT;
}

void SuperResolutionFactoryImpl::EnumerateVariants(Uint32& NumVariants, SuperResolutionInfo* Variants)
{
    Uint32 Count = 0;
    for (Uint32 BackendIdx = 0; BackendIdx < SUPER_RESOLUTION_BACKEND_COUNT; ++BackendIdx)
        Count += static_cast<Uint32>(m_Variants[BackendIdx].size());

    if (Variants == nullptr)
    {
        NumVariants = Count;
        return;
    }

    const Uint32 MaxVariants = NumVariants;
    NumVariants              = 0;
    for (Uint32 BackendIdx = 0; BackendIdx < SUPER_RESOLUTION_BACKEND_COUNT; ++BackendIdx)
    {
        for (const auto& Info : m_Variants[BackendIdx])
        {
            if (NumVariants >= MaxVariants)
                break;
            Variants[NumVariants++] = Info;
        }
    }
}

void SuperResolutionFactoryImpl::GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs, SuperResolutionSourceSettings& Settings) const
{
    Settings = {};

    const auto Backend = FindVariant(Attribs.VariantId);
    if (Backend == SUPER_RESOLUTION_BACKEND_COUNT)
    {
        LOG_WARNING_MESSAGE("Super resolution variant not found for the specified VariantId");
        return;
    }

    switch (Backend)
    {
#if D3D12_SUPPORTED
        case SUPER_RESOLUTION_BACKEND_D3D12_DSR:
            GetSourceSettingsD3D12(m_pDSRDevice, Attribs, Settings);
            break;
#endif
        default:
            LOG_WARNING_MESSAGE("Unknown super resolution backend");
            break;
    }
}

void SuperResolutionFactoryImpl::CreateSuperResolution(const SuperResolutionDesc& Desc, ISuperResolution** ppUpscaler)
{
    DEV_CHECK_ERR(ppUpscaler != nullptr, "ppUpscaler must not be null");
    if (ppUpscaler == nullptr)
        return;

    *ppUpscaler = nullptr;

    const auto Backend = FindVariant(Desc.VariantId);
    if (Backend == SUPER_RESOLUTION_BACKEND_COUNT)
    {
        LOG_ERROR_MESSAGE("Super resolution variant not found for the specified VariantId. Call EnumerateVariants() to get valid variant IDs.");
        return;
    }

    try
    {
        switch (Backend)
        {
#if D3D12_SUPPORTED
            case SUPER_RESOLUTION_BACKEND_D3D12_DSR:
                CreateSuperResolutionD3D12(m_pDevice, m_pDSRDevice, Desc, ppUpscaler);
                break;
#endif
            default:
                LOG_ERROR_MESSAGE("Unknown super resolution backend");
                break;
        }
    }
    catch (...)
    {
        LOG_ERROR("Failed to create super resolution upscaler '", (Desc.Name ? Desc.Name : ""), "'");
    }
}

void SuperResolutionFactoryImpl::SetMessageCallback(DebugMessageCallbackType MessageCallback) const
{
    SetDebugMessageCallback(MessageCallback);
}

void SuperResolutionFactoryImpl::SetBreakOnError(bool BreakOnError) const
{
    PlatformDebug::SetBreakOnError(BreakOnError);
}

void SuperResolutionFactoryImpl::SetMemoryAllocator(IMemoryAllocator* pAllocator) const
{
    SetRawAllocator(pAllocator);
}

} // namespace


API_QUALIFIER void CreateSuperResolutionFactory(IRenderDevice* pDevice, ISuperResolutionFactory** ppFactory)
{
    DEV_CHECK_ERR(ppFactory != nullptr, "ppFactory must not be null");
    if (ppFactory == nullptr)
        return;

    *ppFactory = nullptr;

    try
    {
        auto* pFactory = NEW_RC_OBJ(GetRawAllocator(), "SuperResolutionFactoryImpl instance", SuperResolutionFactoryImpl)(pDevice);
        pFactory->QueryInterface(IID_SuperResolutionFactory, reinterpret_cast<IObject**>(ppFactory));
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
