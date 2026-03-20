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
#include "SuperResolutionProvider.hpp"
#include "DebugUtilities.hpp"
#include "ObjectBase.hpp"
#include "EngineMemory.h"
#include "PlatformDebug.hpp"

#include <memory>

namespace Diligent
{

#if DILIGENT_DLSS_D3D11_SUPPORTED
std::unique_ptr<SuperResolutionProvider> CreateDLSSProviderD3D11(IRenderDevice* pDevice);
#endif

#if DILIGENT_DLSS_D3D12_SUPPORTED
std::unique_ptr<SuperResolutionProvider> CreateDLSSProviderD3D12(IRenderDevice* pDevice);
#endif

#if DILIGENT_DLSS_VK_SUPPORTED
std::unique_ptr<SuperResolutionProvider> CreateDLSSProviderVk(IRenderDevice* pDevice);
#endif

#if DILIGENT_DSR_D3D12_SUPPORTED
std::unique_ptr<SuperResolutionProvider> CreateDSRProviderD3D12(IRenderDevice* pDevice);
#endif

#if DILIGENT_METALFX_SUPPORTED
std::unique_ptr<SuperResolutionProvider> CreateMetalFXProvider(IRenderDevice* pDevice);
#endif

#if DILIGENT_FSR_SUPPORTED
std::unique_ptr<SuperResolutionProvider> CreateFSRProvider(IRenderDevice* pDevice);
#endif

namespace
{

class SuperResolutionFactory : public ObjectBase<ISuperResolutionFactory>
{
public:
    using TBase = ObjectBase<ISuperResolutionFactory>;

    SuperResolutionFactory(IReferenceCounters* pRefCounters, IRenderDevice* pDevice) :
        TBase{pRefCounters}
    {
        auto AddProvider = [this](IRenderDevice*                           pDevice,
                                  std::unique_ptr<SuperResolutionProvider> CreateProvider(IRenderDevice*),
                                  const char*                              ProviderName) {
            try
            {
                ProviderInfo ProvInfo;
                ProvInfo.Provider = CreateProvider(pDevice);
                if (ProvInfo.Provider)
                {
                    ProvInfo.Provider->EnumerateVariants(ProvInfo.Variants);
                    if (!ProvInfo.Variants.empty())
                    {
                        m_TotalVariants += static_cast<Uint32>(ProvInfo.Variants.size());
                        m_Providers.push_back(std::move(ProvInfo));
                    }
                }
            }
            catch (...)
            {
                LOG_ERROR_MESSAGE("Failed to create super resolution provider '", ProviderName, "'");
            }
        };

#ifdef DILIGENT_DLSS_D3D11_SUPPORTED
        AddProvider(pDevice, CreateDLSSProviderD3D11, "DLSS D3D11");
#endif
#ifdef DILIGENT_DLSS_D3D12_SUPPORTED
        AddProvider(pDevice, CreateDLSSProviderD3D12, "DLSS D3D12");
#endif
#ifdef DILIGENT_DLSS_VK_SUPPORTED
        AddProvider(pDevice, CreateDLSSProviderVk, "DLSS Vulkan");
#endif
#ifdef DILIGENT_DSR_D3D12_SUPPORTED
        AddProvider(pDevice, CreateDSRProviderD3D12, "DirectSR D3D12");
#endif
#ifdef DILIGENT_METALFX_SUPPORTED
        AddProvider(pDevice, CreateMetalFXProvider, "MetalFX");
#endif
#ifdef DILIGENT_FSR_SUPPORTED
        AddProvider(pDevice, CreateFSRProvider, "FSR Spatial");
#endif
        (void)AddProvider;
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_SuperResolutionFactory, TBase)

    virtual void DILIGENT_CALL_TYPE EnumerateVariants(Uint32& NumVariants, SuperResolutionInfo* Variants) override final
    {
        if (Variants == nullptr)
        {
            NumVariants = m_TotalVariants;
            return;
        }

        NumVariants = 0;
        for (const ProviderInfo& Entry : m_Providers)
        {
            for (const SuperResolutionInfo& Info : Entry.Variants)
            {
                if (NumVariants >= m_TotalVariants)
                    return;
                Variants[NumVariants++] = Info;
            }
        }
    }

    virtual void DILIGENT_CALL_TYPE GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs,
                                                      SuperResolutionSourceSettings&              Settings) const override final
    {
        Settings                   = {};
        const auto [pEntry, pInfo] = FindProvider(Attribs.VariantId);
        if (pEntry != nullptr)
        {
            pEntry->Provider->GetSourceSettings(Attribs, Settings);
        }
        else
        {
            LOG_WARNING_MESSAGE("Super resolution variant not found for the specified VariantId");
        }
    }

    virtual void DILIGENT_CALL_TYPE CreateSuperResolution(const SuperResolutionDesc& Desc, ISuperResolution** ppUpscaler) override final
    {
        DEV_CHECK_ERR(ppUpscaler != nullptr, "ppUpscaler must not be null");
        if (ppUpscaler == nullptr)
            return;

        *ppUpscaler = nullptr;

        const auto [pEntry, pInfo] = FindProvider(Desc.VariantId);
        if (pEntry == nullptr)
        {
            LOG_ERROR_MESSAGE("Super resolution variant not found for the specified VariantId. Call EnumerateVariants() to get valid variant IDs.");
            return;
        }

        try
        {
            pEntry->Provider->CreateSuperResolution(Desc, *pInfo, ppUpscaler);
        }
        catch (...)
        {
            LOG_ERROR("Failed to create super resolution upscaler '", (Desc.Name ? Desc.Name : ""), "'");
        }
    }

    virtual void DILIGENT_CALL_TYPE SetMessageCallback(DebugMessageCallbackType MessageCallback) const override final
    {
        SetDebugMessageCallback(MessageCallback);
    }

    virtual void DILIGENT_CALL_TYPE SetBreakOnError(bool BreakOnError) const override final
    {
        PlatformDebug::SetBreakOnError(BreakOnError);
    }

    virtual void DILIGENT_CALL_TYPE SetMemoryAllocator(IMemoryAllocator* pAllocator) const override final
    {
        SetRawAllocator(pAllocator);
    }

private:
    struct ProviderInfo
    {
        std::unique_ptr<SuperResolutionProvider> Provider;
        std::vector<SuperResolutionInfo>         Variants;
    };

    std::pair<const ProviderInfo*, const SuperResolutionInfo*> FindProvider(const INTERFACE_ID& VariantId) const
    {
        for (const ProviderInfo& ProvInfo : m_Providers)
        {
            for (const SuperResolutionInfo& SRInfo : ProvInfo.Variants)
            {
                if (SRInfo.VariantId == VariantId)
                    return {&ProvInfo, &SRInfo};
            }
        }
        return {nullptr, nullptr};
    }

private:
    std::vector<ProviderInfo> m_Providers;
    Uint32                    m_TotalVariants = 0;
};

} // namespace

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
        SuperResolutionFactory* pFactory = NEW_RC_OBJ(GetRawAllocator(), "SuperResolutionFactory instance", SuperResolutionFactory)(pDevice);
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
