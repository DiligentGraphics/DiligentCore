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
#include "PlatformDebug.hpp"
#include "DebugUtilities.hpp"

namespace Diligent
{

SuperResolutionFactoryBase::SuperResolutionFactoryBase(IReferenceCounters* pRefCounters) :
    TBase{pRefCounters}
{
}

BackendEntry* SuperResolutionFactoryBase::FindBackend(const INTERFACE_ID& VariantId) const
{
    for (const BackendEntry& Entry : m_Backends)
    {
        for (const SuperResolutionInfo& Info : Entry.Variants)
        {
            if (Info.VariantId == VariantId)
                return const_cast<BackendEntry*>(&Entry);
        }
    }
    return nullptr;
}

void SuperResolutionFactoryBase::EnumerateVariants(Uint32& NumVariants, SuperResolutionInfo* Variants)
{
    Uint32 Count = 0;
    for (const BackendEntry& Entry : m_Backends)
        Count += static_cast<Uint32>(Entry.Variants.size());

    if (Variants == nullptr)
    {
        NumVariants = Count;
        return;
    }

    const Uint32 MaxVariants = NumVariants;
    NumVariants              = 0;
    for (const BackendEntry& Entry : m_Backends)
    {
        for (const SuperResolutionInfo& Info : Entry.Variants)
        {
            if (NumVariants >= MaxVariants)
                return;
            Variants[NumVariants++] = Info;
        }
    }
}

void SuperResolutionFactoryBase::GetSourceSettings(const SuperResolutionSourceSettingsAttribs& Attribs, SuperResolutionSourceSettings& Settings) const
{
    Settings = {};

    BackendEntry* pEntry = FindBackend(Attribs.VariantId);
    if (pEntry == nullptr)
    {
        LOG_WARNING_MESSAGE("Super resolution variant not found for the specified VariantId");
        return;
    }

    pEntry->pBackend->GetSourceSettings(Attribs, Settings);
}

void SuperResolutionFactoryBase::CreateSuperResolution(const SuperResolutionDesc& Desc, ISuperResolution** ppUpscaler)
{
    DEV_CHECK_ERR(ppUpscaler != nullptr, "ppUpscaler must not be null");
    if (ppUpscaler == nullptr)
        return;

    *ppUpscaler = nullptr;

    BackendEntry* pEntry = FindBackend(Desc.VariantId);
    if (pEntry == nullptr)
    {
        LOG_ERROR_MESSAGE("Super resolution variant not found for the specified VariantId. Call EnumerateVariants() to get valid variant IDs.");
        return;
    }

    try
    {
        pEntry->pBackend->CreateSuperResolution(Desc, ppUpscaler);
    }
    catch (...)
    {
        LOG_ERROR("Failed to create super resolution upscaler '", (Desc.Name ? Desc.Name : ""), "'");
    }
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

} // namespace Diligent
