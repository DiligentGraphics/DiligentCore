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

#include "ArchiveBuilderFactory.h"
#include "ArchiveBuilderFactoryLoader.h"

// defined in Windows.h
#undef GetObject

#include "ArchiveBuilderImpl.hpp"
#include "DummyRenderDevice.hpp"
#include "SerializableShaderImpl.hpp"
#include "SerializableRenderPassImpl.hpp"
#include "SerializableResourceSignatureImpl.hpp"
#include "EngineMemory.h"

namespace Diligent
{
namespace
{

class ArchiveBuilderFactoryImpl final : public IArchiveBuilderFactory
{
public:
    static ArchiveBuilderFactoryImpl* GetInstance()
    {
        static ArchiveBuilderFactoryImpl TheFactory;
        return &TheFactory;
    }

    ArchiveBuilderFactoryImpl() :
        m_RefCounters{*this}
    {}

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final
    {
        if (ppInterface == nullptr)
            return;

        *ppInterface = nullptr;
        if (IID == IID_Unknown || IID == IID_ArchiveBuilderFactory)
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

    virtual void DILIGENT_CALL_TYPE CreateArchiveBuilder(IArchiveBuilder** ppBuilder) override final
    {
        DEV_CHECK_ERR(ppBuilder != nullptr, "ppBuilder must not be null");
        if (!ppBuilder)
            return;

        *ppBuilder = nullptr;
        try
        {
            auto& RawMemAllocator = GetRawAllocator();
            auto* pBuilderImpl(NEW_RC_OBJ(RawMemAllocator, "Archive builder instance", ArchiveBuilderImpl)(&m_RenderDevice, this));
            pBuilderImpl->QueryInterface(IID_ArchiveBuilder, reinterpret_cast<IObject**>(ppBuilder));
        }
        catch (...)
        {
            LOG_ERROR_MESSAGE("Failed to create the archive builder");
        }
    }

    virtual void DILIGENT_CALL_TYPE CreateShader(const ShaderCreateInfo& ShaderCI, Uint32 DeviceBits, IShader** ppShader) override final
    {
        DEV_CHECK_ERR(ppShader != nullptr, "ppShader must not be null");
        if (!ppShader)
            return;

        *ppShader = nullptr;
        try
        {
            auto& RawMemAllocator = GetRawAllocator();
            auto* pShaderImpl(NEW_RC_OBJ(RawMemAllocator, "Shader instance", SerializableShaderImpl)(&m_RenderDevice, ShaderCI, DeviceBits));
            pShaderImpl->QueryInterface(IID_Shader, reinterpret_cast<IObject**>(ppShader));
        }
        catch (...)
        {
            LOG_ERROR_MESSAGE("Failed to create the shader");
        }
    }

    virtual void DILIGENT_CALL_TYPE CreateRenderPass(const RenderPassDesc& Desc,
                                                     IRenderPass**         ppRenderPass) override final
    {
        DEV_CHECK_ERR(ppRenderPass != nullptr, "ppRenderPass must not be null");
        if (!ppRenderPass)
            return;

        *ppRenderPass = nullptr;
        try
        {
            auto& RawMemAllocator = GetRawAllocator();
            auto* pRenderPassImpl(NEW_RC_OBJ(RawMemAllocator, "Render pass instance", SerializableRenderPassImpl)(&m_RenderDevice, Desc));
            pRenderPassImpl->QueryInterface(IID_RenderPass, reinterpret_cast<IObject**>(ppRenderPass));
        }
        catch (...)
        {
            LOG_ERROR_MESSAGE("Failed to create the render pass");
        }
    }

    virtual void DILIGENT_CALL_TYPE CreatePipelineResourceSignature(const PipelineResourceSignatureDesc& Desc,
                                                                    Uint32                               DeviceBits,
                                                                    IPipelineResourceSignature**         ppSignature) override final
    {
        DEV_CHECK_ERR(ppSignature != nullptr, "ppSignature must not be null");
        if (!ppSignature)
            return;

        *ppSignature = nullptr;
        try
        {
            auto& RawMemAllocator = GetRawAllocator();
            auto* pSignatureImpl(NEW_RC_OBJ(RawMemAllocator, "Pipeline resource signature instance", SerializableResourceSignatureImpl)(&m_RenderDevice, Desc, DeviceBits));
            pSignatureImpl->QueryInterface(IID_PipelineResourceSignature, reinterpret_cast<IObject**>(ppSignature));
        }
        catch (...)
        {
            LOG_ERROR_MESSAGE("Failed to create the resource signature");
        }
    }

private:
    // AZ TODO: move to separate file
    class DummyReferenceCounters final : public IReferenceCounters
    {
    public:
        DummyReferenceCounters(ArchiveBuilderFactoryImpl& Factory) noexcept :
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
        ArchiveBuilderFactoryImpl& m_Factory;
        Atomics::AtomicLong        m_lNumStrongReferences;
        Atomics::AtomicLong        m_lNumWeakReferences;
    };

    DummyReferenceCounters m_RefCounters;
    DummyRenderDevice      m_RenderDevice;
};

} // namespace


API_QUALIFIER
IArchiveBuilderFactory* GetArchiveBuilderFactory()
{
    return ArchiveBuilderFactoryImpl::GetInstance();
}

} // namespace Diligent

extern "C"
{
    API_QUALIFIER
    Diligent::IArchiveBuilderFactory* Diligent_GetArchiveBuilderFactory()
    {
        return Diligent::GetArchiveBuilderFactory();
    }
}
