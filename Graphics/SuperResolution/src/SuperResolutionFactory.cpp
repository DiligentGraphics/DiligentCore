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
#include "DummyReferenceCounters.hpp"
#include "EngineMemory.h"
#include "PlatformDebug.hpp"

namespace Diligent
{

namespace
{

class SuperResolutionFactoryImpl final : public ISuperResolutionFactory
{
public:
    static SuperResolutionFactoryImpl* GetInstance()
    {
        static SuperResolutionFactoryImpl TheFactory;
        return &TheFactory;
    }

    SuperResolutionFactoryImpl() :
        m_RefCounters{*this}
    {}

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final;

    virtual ReferenceCounterValueType DILIGENT_CALL_TYPE AddRef() override final
    {
        return m_RefCounters.AddStrongRef();
    }

    virtual ReferenceCounterValueType DILIGENT_CALL_TYPE Release() override final
    {
        return m_RefCounters.ReleaseStrongRef();
    }

    virtual IReferenceCounters* DILIGENT_CALL_TYPE GetReferenceCounters() const override final
    {
        return const_cast<IReferenceCounters*>(static_cast<const IReferenceCounters*>(&m_RefCounters));
    }

    virtual void DILIGENT_CALL_TYPE EnumerateVariants(IRenderDevice* pDevice, Uint32& NumVariants, SuperResolutionInfo* Variants) override final;

    virtual void DILIGENT_CALL_TYPE GetSourceSettings(IRenderDevice*                              pDevice,
                                                      const SuperResolutionSourceSettingsAttribs& Attribs,
                                                      SuperResolutionSourceSettings&              Settings) const override final;

    virtual void DILIGENT_CALL_TYPE CreateSuperResolution(IRenderDevice*             pDevice,
                                                          const SuperResolutionDesc& Desc,
                                                          ISuperResolution**         ppUpscaler) override final;

    virtual void DILIGENT_CALL_TYPE
    SetMessageCallback(DebugMessageCallbackType MessageCallback) const override final;

    virtual void DILIGENT_CALL_TYPE SetBreakOnError(bool BreakOnError) const override final;

    virtual void DILIGENT_CALL_TYPE SetMemoryAllocator(IMemoryAllocator* pAllocator) const override final;

private:
    DummyReferenceCounters<SuperResolutionFactoryImpl> m_RefCounters;
};


void SuperResolutionFactoryImpl::QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)
{
    if (ppInterface == nullptr)
        return;

    *ppInterface = nullptr;
    if (IID == IID_Unknown || IID == IID_SuperResolutionFactory)
    {
        *ppInterface = this;
        (*ppInterface)->AddRef();
    }
}

void SuperResolutionFactoryImpl::EnumerateVariants(IRenderDevice* pDevice, Uint32& NumVariants, SuperResolutionInfo* Variants)
{
    NumVariants = 0;
}

void SuperResolutionFactoryImpl::GetSourceSettings(IRenderDevice*                              pDevice,
                                                   const SuperResolutionSourceSettingsAttribs& Attribs,
                                                   SuperResolutionSourceSettings&              Settings) const
{
    Settings = {};
}

void SuperResolutionFactoryImpl::CreateSuperResolution(IRenderDevice*             pDevice,
                                                       const SuperResolutionDesc& Desc,
                                                       ISuperResolution**         ppUpscaler)
{
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


API_QUALIFIER
ISuperResolutionFactory* GetSuperResolutionFactory()
{
    return SuperResolutionFactoryImpl::GetInstance();
}

} // namespace Diligent

extern "C"
{
    API_QUALIFIER
    Diligent::ISuperResolutionFactory* Diligent_GetSuperResolutionFactory()
    {
        return Diligent::GetSuperResolutionFactory();
    }
}
