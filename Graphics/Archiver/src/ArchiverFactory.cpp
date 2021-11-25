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
#include "DefaultShaderSourceStreamFactory.h"

// defined in Windows.h
#undef GetObject

#include "DummyReferenceCounters.hpp"
#include "ArchiverImpl.hpp"
#include "SerializationDeviceImpl.hpp"
#include "EngineMemory.h"
#include "ArchiveRepacker.hpp"
#include "ArchiveMemoryImpl.hpp"

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

    virtual void DILIGENT_CALL_TYPE CreateArchiver(ISerializationDevice* pDevice, IArchiver** ppArchiver) override final;
    virtual void DILIGENT_CALL_TYPE CreateSerializationDevice(const SerializationDeviceCreateInfo& CreateInfo, ISerializationDevice** ppDevice) override final;
    virtual void DILIGENT_CALL_TYPE CreateDefaultShaderSourceStreamFactory(const Char* SearchDirectories, struct IShaderSourceInputStreamFactory** ppShaderSourceFactory) const override final;
    virtual Bool DILIGENT_CALL_TYPE RemoveDeviceData(IArchive* pSrcArchive, RENDER_DEVICE_TYPE_FLAGS DeviceFlags, IFileStream* pStream) const override final;
    virtual Bool DILIGENT_CALL_TYPE AppendDeviceData(IArchive* pSrcArchive, RENDER_DEVICE_TYPE_FLAGS DeviceFlags, IArchive* pDeviceArchive, IFileStream* pStream) const override final;
    virtual Bool DILIGENT_CALL_TYPE PrintArchiveContent(IArchive* pArchive) const override final;

private:
    DummyReferenceCounters<ArchiverFactoryImpl> m_RefCounters;
};


void ArchiverFactoryImpl::QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)
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

void ArchiverFactoryImpl::CreateArchiver(ISerializationDevice* pDevice, IArchiver** ppArchiver)
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

void ArchiverFactoryImpl::CreateSerializationDevice(const SerializationDeviceCreateInfo& CreateInfo, ISerializationDevice** ppDevice)
{
    DEV_CHECK_ERR(ppDevice != nullptr, "ppDevice must not be null");
    if (!ppDevice)
        return;

    *ppDevice = nullptr;
    try
    {
        auto& RawMemAllocator = GetRawAllocator();
        auto* pDeviceImpl(NEW_RC_OBJ(RawMemAllocator, "Serialization device instance", SerializationDeviceImpl)(CreateInfo));
        pDeviceImpl->QueryInterface(IID_SerializationDevice, reinterpret_cast<IObject**>(ppDevice));
    }
    catch (...)
    {
        LOG_ERROR_MESSAGE("Failed to create the serialization device");
    }
}

void ArchiverFactoryImpl::CreateDefaultShaderSourceStreamFactory(const Char* SearchDirectories, struct IShaderSourceInputStreamFactory** ppShaderSourceFactory) const
{
    DEV_CHECK_ERR(ppShaderSourceFactory != nullptr, "ppShaderSourceFactory must not be null");
    if (!ppShaderSourceFactory)
        return;

    Diligent::CreateDefaultShaderSourceStreamFactory(SearchDirectories, ppShaderSourceFactory);
}

Bool ArchiverFactoryImpl::RemoveDeviceData(IArchive* pSrcArchive, RENDER_DEVICE_TYPE_FLAGS DeviceFlags, IFileStream* pStream) const
{
    DEV_CHECK_ERR(pSrcArchive != nullptr, "pSrcArchive must not be null");
    DEV_CHECK_ERR(pStream != nullptr, "pStream must not be null");
    if (pStream == nullptr || pSrcArchive == nullptr)
        return false;

    try
    {
        ArchiveRepacker Repacker{pSrcArchive};

        for (; DeviceFlags != 0;)
        {
            const auto DeviceType        = static_cast<RENDER_DEVICE_TYPE>(PlatformMisc::GetLSB(ExtractLSB(DeviceFlags)));
            const auto ArchiveDeviceType = DeviceObjectArchiveBase::RenderDeviceTypeToArchiveDeviceType(DeviceType);

            Repacker.RemoveDeviceData(ArchiveDeviceType);
        }

        Repacker.Serialize(pStream);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

Bool ArchiverFactoryImpl::AppendDeviceData(IArchive* pSrcArchive, RENDER_DEVICE_TYPE_FLAGS DeviceFlags, IArchive* pDeviceArchive, IFileStream* pStream) const
{
    DEV_CHECK_ERR(pSrcArchive != nullptr, "pSrcArchive must not be null");
    DEV_CHECK_ERR(pDeviceArchive != nullptr, "pDeviceArchive must not be null");
    DEV_CHECK_ERR(pStream != nullptr, "pStream must not be null");
    if (pStream == nullptr || pDeviceArchive == nullptr || pSrcArchive == nullptr)
        return false;

    try
    {
        ArchiveRepacker       SrcRepacker{pSrcArchive};
        const ArchiveRepacker DevRepacker{pDeviceArchive};

        for (; DeviceFlags != 0;)
        {
            const auto DeviceType        = static_cast<RENDER_DEVICE_TYPE>(PlatformMisc::GetLSB(ExtractLSB(DeviceFlags)));
            const auto ArchiveDeviceType = DeviceObjectArchiveBase::RenderDeviceTypeToArchiveDeviceType(DeviceType);

            SrcRepacker.AppendDeviceData(DevRepacker, ArchiveDeviceType);
        }

        SrcRepacker.Serialize(pStream);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

Bool ArchiverFactoryImpl::PrintArchiveContent(IArchive* pArchive) const
{
    try
    {
        ArchiveRepacker Repacker{pArchive};

        Repacker.Print();
        return true;
    }
    catch (...)
    {
        return false;
    }
}

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
