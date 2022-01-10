/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#if D3D11_SUPPORTED
class PipelineResourceSignatureD3D11Impl;
#endif
#if D3D12_SUPPORTED
class PipelineResourceSignatureD3D12Impl;
#endif
#if GL_SUPPORTED || GLES_SUPPORTED
class PipelineResourceSignatureGLImpl;
#endif
#if VULKAN_SUPPORTED
class PipelineResourceSignatureVkImpl;
#endif
#if METAL_SUPPORTED
class PipelineResourceSignatureMtlImpl;
#endif


class SerializableResourceSignatureImpl final : public ObjectBase<IPipelineResourceSignature>
{
public:
    using TBase = ObjectBase<IPipelineResourceSignature>;

    using DeviceType                    = DeviceObjectArchiveBase::DeviceType;
    static constexpr Uint32 DeviceCount = static_cast<Uint32>(DeviceType::Count);

    SerializableResourceSignatureImpl(IReferenceCounters*                  pRefCounters,
                                      SerializationDeviceImpl*             pDevice,
                                      const PipelineResourceSignatureDesc& Desc,
                                      ARCHIVE_DEVICE_DATA_FLAGS            DeviceFlags,
                                      SHADER_TYPE                          ShaderStages = SHADER_TYPE_UNKNOWN);
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

    bool   IsCompatible(const SerializableResourceSignatureImpl& Rhs, ARCHIVE_DEVICE_DATA_FLAGS DeviceFlags) const;
    bool   operator==(const SerializableResourceSignatureImpl& Rhs) const;
    size_t CalcHash() const;

    const SerializedMemory& GetCommonData() const { return m_CommonData; }

    const SerializedMemory* GetDeviceData(DeviceType Type) const
    {
        VERIFY_EXPR(static_cast<Uint32>(Type) < DeviceCount);
        auto& Wrpr = m_pDeviceSignatures[static_cast<size_t>(Type)];
        return Wrpr ? &Wrpr->Mem : nullptr;
    }

    template <typename SignatureType>
    struct SignatureTraits;

    template <typename SignatureType>
    SignatureType* GetDeviceSignature(DeviceType Type) const
    {
        constexpr auto TraitsType = SignatureTraits<SignatureType>::Type;
        VERIFY_EXPR(Type == TraitsType || (Type == DeviceType::Metal_iOS && TraitsType == DeviceType::Metal_MacOS));

        auto& Wrpr = m_pDeviceSignatures[static_cast<size_t>(Type)];
        return Wrpr ? Wrpr->GetPRS<SignatureType>() : nullptr;
    }


    template <typename SignatureImplType>
    void CreateDeviceSignature(DeviceType                           Type,
                               const PipelineResourceSignatureDesc& Desc,
                               SHADER_TYPE                          ShaderStages);

private:
    const IPipelineResourceSignature* GetPRS(DeviceType Type) const
    {
        VERIFY_EXPR(static_cast<Uint32>(Type) < DeviceCount);
        auto& Wrpr = m_pDeviceSignatures[static_cast<size_t>(Type)];
        return Wrpr ? Wrpr->GetPRS() : nullptr;
    }

private:
    const PipelineResourceSignatureDesc*          m_pDesc = nullptr;
    std::unique_ptr<void, STDDeleterRawMem<void>> m_pRawMemory;

    SerializedMemory m_CommonData;

    struct PRSWapperBase
    {
        virtual ~PRSWapperBase() {}
        virtual IPipelineResourceSignature* GetPRS() = 0;

        template <typename SigType>
        SigType* GetPRS() { return ClassPtrCast<SigType>(GetPRS()); }

        SerializedMemory Mem;
    };

    template <typename ImplType> struct TPRS;

    std::array<std::unique_ptr<PRSWapperBase>, DeviceCount> m_pDeviceSignatures;
};

#if D3D11_SUPPORTED
extern template PipelineResourceSignatureD3D11Impl* SerializableResourceSignatureImpl::GetDeviceSignature<PipelineResourceSignatureD3D11Impl>(DeviceType Type) const;

extern template void SerializableResourceSignatureImpl::CreateDeviceSignature<PipelineResourceSignatureD3D11Impl>(
    DeviceType                           Type,
    const PipelineResourceSignatureDesc& Desc,
    SHADER_TYPE                          ShaderStages);
#endif

#if D3D12_SUPPORTED
extern template PipelineResourceSignatureD3D12Impl* SerializableResourceSignatureImpl::GetDeviceSignature<PipelineResourceSignatureD3D12Impl>(DeviceType Type) const;

extern template void SerializableResourceSignatureImpl::CreateDeviceSignature<PipelineResourceSignatureD3D12Impl>(
    DeviceType                           Type,
    const PipelineResourceSignatureDesc& Desc,
    SHADER_TYPE                          ShaderStages);
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
extern template PipelineResourceSignatureGLImpl* SerializableResourceSignatureImpl::GetDeviceSignature<PipelineResourceSignatureGLImpl>(DeviceType Type) const;

extern template void SerializableResourceSignatureImpl::CreateDeviceSignature<PipelineResourceSignatureGLImpl>(
    DeviceType                           Type,
    const PipelineResourceSignatureDesc& Desc,
    SHADER_TYPE                          ShaderStages);
#endif

#if VULKAN_SUPPORTED
extern template PipelineResourceSignatureVkImpl* SerializableResourceSignatureImpl::GetDeviceSignature<PipelineResourceSignatureVkImpl>(DeviceType Type) const;

extern template void SerializableResourceSignatureImpl::CreateDeviceSignature<PipelineResourceSignatureVkImpl>(
    DeviceType                           Type,
    const PipelineResourceSignatureDesc& Desc,
    SHADER_TYPE                          ShaderStages);
#endif

#if METAL_SUPPORTED
extern template PipelineResourceSignatureMtlImpl* SerializableResourceSignatureImpl::GetDeviceSignature<PipelineResourceSignatureMtlImpl>(DeviceType Type) const;

extern template void SerializableResourceSignatureImpl::CreateDeviceSignature<PipelineResourceSignatureMtlImpl>(
    DeviceType                           Type,
    const PipelineResourceSignatureDesc& Desc,
    SHADER_TYPE                          ShaderStages);
#endif

} // namespace Diligent
