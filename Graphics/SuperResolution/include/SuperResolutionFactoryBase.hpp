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

#pragma once

#include "SuperResolutionFactory.h"
#include "SuperResolution.h"
#include "ObjectBase.hpp"

#include <vector>
#include <memory>

namespace Diligent
{

struct BackendEntry
{
    struct IHolder
    {
        virtual ~IHolder() = default;

        virtual void GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs, SuperResolutionSourceSettings& Settings) = 0;

        virtual void CreateSuperResolution(const SuperResolutionDesc& Desc, ISuperResolution** ppUpscaler) = 0;
    };

    template <typename T>
    struct Holder final : IHolder
    {
        T Instance;
        template <typename... Args>
        explicit Holder(Args&&... args) :
            Instance(std::forward<Args>(args)...) {}

        void GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs, SuperResolutionSourceSettings& Settings) override { Instance.GetSourceSettings(Attribs, Settings); }

        void CreateSuperResolution(const SuperResolutionDesc& Desc, ISuperResolution** ppUpscaler) override { Instance.CreateSuperResolution(Desc, ppUpscaler); }
    };

    std::unique_ptr<IHolder>         pBackend;
    std::vector<SuperResolutionInfo> Variants;
};

class SuperResolutionFactoryBase : public ObjectBase<ISuperResolutionFactory>
{
public:
    using TBase = ObjectBase<ISuperResolutionFactory>;

    SuperResolutionFactoryBase(IReferenceCounters* pRefCounters);

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_SuperResolutionFactory, TBase)

    virtual void DILIGENT_CALL_TYPE EnumerateVariants(Uint32& NumVariants, SuperResolutionInfo* Variants) override final;

    virtual void DILIGENT_CALL_TYPE GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs, SuperResolutionSourceSettings& Settings) const override final;

    virtual void DILIGENT_CALL_TYPE CreateSuperResolution(const SuperResolutionDesc& Desc, ISuperResolution** ppUpscaler) override final;

    virtual void DILIGENT_CALL_TYPE SetMessageCallback(DebugMessageCallbackType MessageCallback) const override final;

    virtual void DILIGENT_CALL_TYPE SetBreakOnError(bool BreakOnError) const override final;

    virtual void DILIGENT_CALL_TYPE SetMemoryAllocator(IMemoryAllocator* pAllocator) const override final;

    template <typename BackendType, typename... Args>
    void AddBackend(Args&&... args);

private:
    BackendEntry* FindBackend(const INTERFACE_ID& VariantId) const;

    std::vector<BackendEntry> m_Backends;
};

template <typename BackendType, typename... Args>
void SuperResolutionFactoryBase::AddBackend(Args&&... args)
{
    try
    {
        auto pHolder = std::make_unique<BackendEntry::Holder<BackendType>>(std::forward<Args>(args)...);

        BackendEntry Entry;
        pHolder->Instance.EnumerateVariants(Entry.Variants);
        if (Entry.Variants.empty())
            return;

        Entry.pBackend = std::move(pHolder);
        m_Backends.push_back(std::move(Entry));
    }
    catch (...)
    {
    }
}

} // namespace Diligent
