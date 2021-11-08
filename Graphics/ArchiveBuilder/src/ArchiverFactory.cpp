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

#include "ArchiverFactory.h"
#include "ArchiverFactoryLoader.h"

// defined in Windows.h
#undef GetObject

#include "ArchiverImpl.hpp"
#include "SerializationDeviceImpl.hpp"
#include "EngineMemory.h"

namespace Diligent
{
namespace
{

class ArchiverFactoryImpl final : public IArchiverFactory
{
public:
    static ArchiverFactoryImpl* GetInstance()
    {
        static ArchiverFactoryImpl TheFactory;
        return &TheFactory;
    }

    ArchiverFactoryImpl() :
        m_RefCounters{*this}
    {}

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final
    {
        if (ppInterface == nullptr)
            return;

        *ppInterface = nullptr;
        if (IID == IID_Unknown || IID == IID_ArchiverFactory)
        {
            *ppInterface = this;
            (*ppInterface)->AddRef();
        }
    }

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

    virtual void DILIGENT_CALL_TYPE CreateArchiver(ISerializationDevice* pDevice, IArchiver** ppArchiver) override final
    {
        DEV_CHECK_ERR(ppArchiver != nullptr, "ppArchiver must not be null");
        if (!ppArchiver)
            return;

        *ppArchiver = nullptr;
        try
        {
            auto& RawMemAllocator = GetRawAllocator();
            auto* pArchiverImpl(NEW_RC_OBJ(RawMemAllocator, "Archiver instance", ArchiverImpl)(ClassPtrCast<SerializationDeviceImpl>(pDevice)));
            pArchiverImpl->QueryInterface(IID_Archiver, reinterpret_cast<IObject**>(ppArchiver));
        }
        catch (...)
        {
            LOG_ERROR_MESSAGE("Failed to create the archiver");
        }
    }

    virtual void DILIGENT_CALL_TYPE CreateSerializationDevice(ISerializationDevice** ppDevice) override final
    {
        DEV_CHECK_ERR(ppDevice != nullptr, "ppDevice must not be null");
        if (!ppDevice)
            return;

        *ppDevice = nullptr;
        try
        {
            auto& RawMemAllocator = GetRawAllocator();
            auto* pDeviceImpl(NEW_RC_OBJ(RawMemAllocator, "Serialization device instance", SerializationDeviceImpl)());
            pDeviceImpl->QueryInterface(IID_SerializationDevice, reinterpret_cast<IObject**>(ppDevice));
        }
        catch (...)
        {
            LOG_ERROR_MESSAGE("Failed to create the serialization device");
        }
    }

    virtual void DILIGENT_CALL_TYPE CreateDefaultShaderSourceStreamFactory(const Char* SearchDirectories, struct IShaderSourceInputStreamFactory** ppShaderSourceFactory) const override final
    {
    }

private:
    // AZ TODO: move to separate file
    class DummyReferenceCounters final : public IReferenceCounters
    {
    public:
        DummyReferenceCounters(ArchiverFactoryImpl& Factory) noexcept :
            m_Factory{Factory}
        {
            m_lNumStrongReferences = 0;
            m_lNumWeakReferences   = 0;
        }

        virtual ReferenceCounterValueType AddStrongRef() override final
        {
            return Atomics::AtomicIncrement(m_lNumStrongReferences);
        }

        virtual ReferenceCounterValueType ReleaseStrongRef() override final
        {
            return Atomics::AtomicDecrement(m_lNumStrongReferences);
        }

        virtual ReferenceCounterValueType AddWeakRef() override final
        {
            return Atomics::AtomicIncrement(m_lNumWeakReferences);
        }

        virtual ReferenceCounterValueType ReleaseWeakRef() override final
        {
            return Atomics::AtomicDecrement(m_lNumWeakReferences);
        }

        virtual void GetObject(IObject** ppObject) override final
        {
            if (ppObject != nullptr)
                m_Factory.QueryInterface(IID_Unknown, ppObject);
        }

        virtual ReferenceCounterValueType GetNumStrongRefs() const override final
        {
            return m_lNumStrongReferences;
        }

        virtual ReferenceCounterValueType GetNumWeakRefs() const override final
        {
            return m_lNumWeakReferences;
        }

    private:
        ArchiverFactoryImpl& m_Factory;
        Atomics::AtomicLong  m_lNumStrongReferences;
        Atomics::AtomicLong  m_lNumWeakReferences;
    };

    DummyReferenceCounters m_RefCounters;
};

} // namespace


API_QUALIFIER
IArchiverFactory* GetArchiverFactory()
{
    return ArchiverFactoryImpl::GetInstance();
}

} // namespace Diligent

extern "C"
{
    API_QUALIFIER
    Diligent::IArchiverFactory* Diligent_GetArchiverFactory()
    {
        return Diligent::GetArchiverFactory();
    }
}
