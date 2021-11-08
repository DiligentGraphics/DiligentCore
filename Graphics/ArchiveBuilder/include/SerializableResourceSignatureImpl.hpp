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

#pragma once

#include "PipelineResourceSignature.h"
#include "ObjectBase.hpp"
#include "STDAllocator.hpp"
#include "SerializedMemory.hpp"
#include "DeviceObjectArchiveBase.hpp"
#include "SerializationDeviceImpl.hpp"

namespace Diligent
{

#if D3D12_SUPPORTED
class PipelineResourceSignatureD3D12Impl;
#endif
#if VULKAN_SUPPORTED
class PipelineResourceSignatureVkImpl;
#endif

class SerializableResourceSignatureImpl final : public ObjectBase<IPipelineResourceSignature>
{
public:
    using TBase = ObjectBase<IPipelineResourceSignature>;

    SerializableResourceSignatureImpl(IReferenceCounters*                  pRefCounters,
                                      SerializationDeviceImpl*             pDevice,
                                      const PipelineResourceSignatureDesc& Desc,
                                      Uint32                               DeviceBits);
    ~SerializableResourceSignatureImpl() override;

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_PipelineResourceSignature, TBase)

    virtual const PipelineResourceSignatureDesc& DILIGENT_CALL_TYPE GetDesc() const override final { return *m_pDesc; }

    virtual void DILIGENT_CALL_TYPE CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                bool                     InitStaticResources) override final {}

    virtual void DILIGENT_CALL_TYPE BindStaticResources(SHADER_TYPE                 ShaderStages,
                                                        IResourceMapping*           pResourceMapping,
                                                        BIND_SHADER_RESOURCES_FLAGS Flags) override final {}

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByName(SHADER_TYPE ShaderType,
                                                                                const Char* Name) override final { return nullptr; }

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByIndex(SHADER_TYPE ShaderType,
                                                                                 Uint32      Index) override final { return nullptr; }

    virtual Uint32 DILIGENT_CALL_TYPE GetStaticVariableCount(SHADER_TYPE ShaderType) const override final { return 0; }

    virtual void DILIGENT_CALL_TYPE InitializeStaticSRBResources(IShaderResourceBinding* pShaderResourceBinding) const override final {}

    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineResourceSignature* pPRS) const override final { return false; }

    virtual Int32 DILIGENT_CALL_TYPE GetUniqueID() const override final { return 0; }

    virtual void DILIGENT_CALL_TYPE SetUserData(IObject* pUserData) override final {}

    virtual IObject* DILIGENT_CALL_TYPE GetUserData() const override final { return nullptr; }

    const SerializedMemory& GetSharedSerializedMemory() const { return m_SharedData; }

#if D3D12_SUPPORTED
    PipelineResourceSignatureD3D12Impl* GetSignatureD3D12() const;
    const SerializedMemory&             GetSerializedMemoryD3D12() const;
#endif
#if VULKAN_SUPPORTED
    PipelineResourceSignatureVkImpl* GetSignatureVk() const;
    const SerializedMemory&          GetSerializedMemoryVk() const;
#endif

private:
    const PipelineResourceSignatureDesc*           m_pDesc       = nullptr;
    const PipelineResourceSignatureSerializedData* m_pSerialized = nullptr;
    SerializedMemory                               m_DescMem;
    SerializedMemory                               m_SharedData;

    using DeviceType = DeviceObjectArchiveBase::DeviceType;

    template <typename ImplType> struct TPRS;
#if D3D12_SUPPORTED
    std::unique_ptr<TPRS<PipelineResourceSignatureD3D12Impl>> m_pPRSD3D12;
#endif
#if VULKAN_SUPPORTED
    std::unique_ptr<TPRS<PipelineResourceSignatureVkImpl>> m_pPRSVk;
#endif
};

} // namespace Diligent
