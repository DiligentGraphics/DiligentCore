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
    static constexpr Uint32 DeviceCount = static_cast<Uint32>(DeviceType::Metal_MacOS);

    SerializableResourceSignatureImpl(IReferenceCounters*                  pRefCounters,
                                      SerializationDeviceImpl*             pDevice,
                                      const PipelineResourceSignatureDesc& Desc,
                                      RENDER_DEVICE_TYPE_FLAGS             DeviceFlags,
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

    bool   IsCompatible(const SerializableResourceSignatureImpl& Rhs, RENDER_DEVICE_TYPE_FLAGS DeviceFlags) const;
    bool   operator==(const SerializableResourceSignatureImpl& Rhs) const;
    size_t CalcHash() const;

    const SerializedMemory& GetSharedSerializedMemory() const { return m_SharedData; }

    const SerializedMemory* GetSerializedMemory(DeviceType Type) const
    {
        VERIFY_EXPR(static_cast<Uint32>(Type) < DeviceCount);
        auto& Wrpr = m_pPRSWrappers[static_cast<size_t>(Type)];
        return Wrpr ? &Wrpr->Mem : nullptr;
    }

    template <typename SignatureType>
    struct SignatureTraits;

    template <typename SignatureType>
    SignatureType* GetSignature() const
    {
        constexpr auto Type = SignatureTraits<SignatureType>::Type;

        auto& Wrpr = m_pPRSWrappers[static_cast<size_t>(Type)];
        return Wrpr ? Wrpr->GetPRS<SignatureType>() : nullptr;
    }

private:
    void AddPRSDesc(const PipelineResourceSignatureDesc& Desc, const PipelineResourceSignatureSerializedData& Serialized);

    const PipelineResourceSignatureDesc*           m_pDesc       = nullptr;
    const PipelineResourceSignatureSerializedData* m_pSerialized = nullptr;
    SerializedMemory                               m_DescMem;
    SerializedMemory                               m_SharedData;

    struct PRSWapperBase
    {
        virtual ~PRSWapperBase() {}
        virtual IPipelineResourceSignature* GetPRS() = 0;

        template <typename SigType>
        SigType* GetPRS() { return ClassPtrCast<SigType>(GetPRS()); }

        SerializedMemory Mem;
    };

    template <typename ImplType> struct TPRS;

    std::array<std::unique_ptr<PRSWapperBase>, DeviceCount> m_pPRSWrappers;

    const IPipelineResourceSignature* GetPRS(DeviceType Type) const
    {
        VERIFY_EXPR(static_cast<Uint32>(Type) < DeviceCount);
        auto& Wrpr = m_pPRSWrappers[static_cast<size_t>(Type)];
        return Wrpr ? Wrpr->GetPRS() : nullptr;
    }

    template <typename SignatureImplType>
    void CreateSignature(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& Desc, SHADER_TYPE ShaderStages)
    {
        using Traits                = SignatureTraits<SignatureImplType>;
        using MeasureSerializerType = typename Traits::template PSOSerializerType<SerializerMode::Measure>;
        using WriteSerializerType   = typename Traits::template PSOSerializerType<SerializerMode::Write>;

        auto PRSWrpr = std::make_unique<TPRS<SignatureImplType>>(pRefCounters, Desc, ShaderStages);

        auto SerializedData = PRSWrpr->PRS.Serialize();
        AddPRSDesc(PRSWrpr->PRS.GetDesc(), SerializedData);

        Serializer<SerializerMode::Measure> MeasureSer;
        MeasureSerializerType::SerializePRSDesc(MeasureSer, SerializedData, nullptr);

        PRSWrpr->Mem = SerializedMemory{MeasureSer.GetSize(nullptr)};

        Serializer<SerializerMode::Write> Ser{PRSWrpr->Mem.Ptr(), PRSWrpr->Mem.Size()};
        WriteSerializerType::SerializePRSDesc(Ser, SerializedData, nullptr);
        VERIFY_EXPR(Ser.IsEnd());

        m_pPRSWrappers[static_cast<size_t>(Traits::Type)] = std::move(PRSWrpr);
    }
#if D3D11_SUPPORTED
    void CreatePRSD3D11(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& Desc, SHADER_TYPE ShaderStages);
#endif

#if D3D12_SUPPORTED
    void CreatePRSD3D12(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& Desc, SHADER_TYPE ShaderStages);
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
    void CreatePRSGL(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& Desc, SHADER_TYPE ShaderStages);
#endif

#if VULKAN_SUPPORTED
    void CreatePRSVk(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& Desc, SHADER_TYPE ShaderStages);
#endif

#if METAL_SUPPORTED
    void CreatePRSMtl(IReferenceCounters* pRefCounters, const PipelineResourceSignatureDesc& Desc, SHADER_TYPE ShaderStages);
#endif
};


#if D3D11_SUPPORTED
extern template PipelineResourceSignatureD3D11Impl* SerializableResourceSignatureImpl::GetSignature<PipelineResourceSignatureD3D11Impl>() const;
#endif

#if D3D12_SUPPORTED
extern template PipelineResourceSignatureD3D12Impl* SerializableResourceSignatureImpl::GetSignature<PipelineResourceSignatureD3D12Impl>() const;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
extern template PipelineResourceSignatureGLImpl* SerializableResourceSignatureImpl::GetSignature<PipelineResourceSignatureGLImpl>() const;
#endif

#if VULKAN_SUPPORTED
extern template PipelineResourceSignatureVkImpl* SerializableResourceSignatureImpl::GetSignature<PipelineResourceSignatureVkImpl>() const;
#endif

#if METAL_SUPPORTED
extern template PipelineResourceSignatureMtlImpl* SerializableResourceSignatureImpl::GetSignature<PipelineResourceSignatureMtlImpl>() const;
#endif

} // namespace Diligent
