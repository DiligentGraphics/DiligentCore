/*
 *  Copyright 2019-2022 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use  file except in compliance with the License.
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
 *  or consequential damages of any character arising as a result of  License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

#include <unordered_map>

#include "SerializationEngineImplTraits.hpp"

#include "PipelineState.h"
#include "Archiver.h"

#include "ObjectBase.hpp"
#include "PipelineStateBase.hpp"
#include "DeviceObjectArchiveBase.hpp"

namespace Diligent
{

class SerializableResourceSignatureImpl;

// {23DBAA36-B34E-438E-800C-D28C66237361}
static const INTERFACE_ID IID_SerializedPipelineState =
    {0x23dbaa36, 0xb34e, 0x438e, {0x80, 0xc, 0xd2, 0x8c, 0x66, 0x23, 0x73, 0x61}};

class SerializablePipelineStateImpl final : public ObjectBase<IPipelineState>
{
public:
    using TBase = ObjectBase<IPipelineState>;

    template <typename PSOCreateInfoType>
    SerializablePipelineStateImpl(IReferenceCounters*             pRefCounters,
                                  SerializationDeviceImpl*        pDevice,
                                  const PSOCreateInfoType&        CreateInfo,
                                  const PipelineStateArchiveInfo& ArchiveInfo);

    ~SerializablePipelineStateImpl() override;

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final;

    virtual const PipelineStateDesc& DILIGENT_CALL_TYPE GetDesc() const override final
    {
        return m_Desc;
    }

    virtual Int32 DILIGENT_CALL_TYPE GetUniqueID() const override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
        return 0;
    }

    virtual void DILIGENT_CALL_TYPE SetUserData(IObject* pUserData) override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
    }

    virtual IObject* DILIGENT_CALL_TYPE GetUserData() const override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
        return nullptr;
    }

    virtual const GraphicsPipelineDesc& DILIGENT_CALL_TYPE GetGraphicsPipelineDesc() const override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
        static constexpr GraphicsPipelineDesc NullDesc;
        return NullDesc;
    }

    virtual const RayTracingPipelineDesc& DILIGENT_CALL_TYPE GetRayTracingPipelineDesc() const override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
        static constexpr RayTracingPipelineDesc NullDesc;
        return NullDesc;
    }

    virtual const TilePipelineDesc& DILIGENT_CALL_TYPE GetTilePipelineDesc() const override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
        static constexpr TilePipelineDesc NullDesc;
        return NullDesc;
    }

    virtual void DILIGENT_CALL_TYPE BindStaticResources(SHADER_TYPE                 ShaderStages,
                                                        IResourceMapping*           pResourceMapping,
                                                        BIND_SHADER_RESOURCES_FLAGS Flags) override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
    }

    virtual Uint32 DILIGENT_CALL_TYPE GetStaticVariableCount(SHADER_TYPE ShaderType) const override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
        return 0;
    }

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByName(SHADER_TYPE ShaderType,
                                                                                const Char* Name) override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
        return nullptr;
    }

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByIndex(SHADER_TYPE ShaderType,
                                                                                 Uint32      Index) override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
        return nullptr;
    }

    virtual void DILIGENT_CALL_TYPE CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding, bool InitStaticResources) override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
    }

    virtual void DILIGENT_CALL_TYPE InitializeStaticSRBResources(IShaderResourceBinding* pShaderResourceBinding) const override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
    }

    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineState* pPSO) const override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
        return false;
    }

    virtual Uint32 DILIGENT_CALL_TYPE GetResourceSignatureCount() const override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
        return 0;
    }

    virtual IPipelineResourceSignature* DILIGENT_CALL_TYPE GetResourceSignature(Uint32 Index) const override final
    {
        UNSUPPORTED("This method is not supported by serializable pipeline state.");
        return nullptr;
    }


    using SerializedPSOAuxData = DeviceObjectArchiveBase::SerializedPSOAuxData;
    using DeviceType           = DeviceObjectArchiveBase::DeviceType;
    using TPRSNames            = DeviceObjectArchiveBase::TPRSNames;

    static constexpr auto DeviceDataCount = static_cast<size_t>(DeviceType::Count);

    struct Data
    {
        SerializedPSOAuxData Aux;
        SerializedData       Common;

        struct ShaderInfo
        {
            SerializedData Data;
            size_t         Hash = 0;
        };
        std::array<std::vector<ShaderInfo>, DeviceDataCount> Shaders;
    };

    const Data& GetData() const { return m_Data; }

    const SerializedData& GetCommonData() const { return m_Data.Common; };


    using RayTracingShaderMapType = std::unordered_map<const IShader*, /*Index in TShaderIndices*/ Uint32>;

    static void ExtractShadersD3D12(const RayTracingPipelineStateCreateInfo& CreateInfo, RayTracingShaderMapType& ShaderMap);
    static void ExtractShadersVk(const RayTracingPipelineStateCreateInfo& CreateInfo, RayTracingShaderMapType& ShaderMap);

    template <typename ShaderStage>
    static void GetRayTracingShaderMap(const std::vector<ShaderStage>& ShaderStages, RayTracingShaderMapType& ShaderMap)
    {
        Uint32 ShaderIndex = 0;
        for (auto& Stage : ShaderStages)
        {
            for (auto* pShader : Stage.Serializable)
            {
                if (ShaderMap.emplace(pShader, ShaderIndex).second)
                    ++ShaderIndex;
            }
        }
    }

    IRenderPass* GetRenderPass() const
    {
        return m_pRenderPass.RawPtr<IRenderPass>();
    }

    using SignaturesVector = std::vector<RefCntAutoPtr<IPipelineResourceSignature>>;
    const SignaturesVector& GetSignatures() { return m_Signatures; }

private:
    template <typename CreateInfoType>
    void PatchShadersVk(const CreateInfoType& CreateInfo) noexcept(false);

    template <typename CreateInfoType>
    void PatchShadersD3D12(const CreateInfoType& CreateInfo) noexcept(false);

    template <typename CreateInfoType>
    void PatchShadersD3D11(const CreateInfoType& CreateInfo) noexcept(false);

    template <typename CreateInfoType>
    void PatchShadersGL(const CreateInfoType& CreateInfo) noexcept(false);

    template <typename CreateInfoType>
    void PatchShadersMtl(const CreateInfoType& CreateInfo, DeviceType DevType) noexcept(false);

    // Default signatures in OpenGL are not serialized and require special handling.
    template <typename CreateInfoType>
    void PrepareDefaultSignatureGL(const CreateInfoType& CreateInfo) noexcept(false);


    void SerializeShaderBytecode(DeviceType              Type,
                                 const ShaderCreateInfo& CI,
                                 const void*             Bytecode,
                                 size_t                  BytecodeSize);
    void SerializeShaderSource(DeviceType DeType, const ShaderCreateInfo& CI);


    template <typename PipelineStateImplType, typename SignatureImplType, typename ShaderStagesArrayType, typename... ExtraArgsType>
    void CreateDefaultResourceSignature(DeviceType                   Type,
                                        const PipelineStateDesc&     PSODesc,
                                        SHADER_TYPE                  ActiveShaderStageFlags,
                                        const ShaderStagesArrayType& ShaderStages,
                                        const ExtraArgsType&... ExtraArgs);

protected:
    SerializationDeviceImpl* const m_pSerializationDevice;

    Data m_Data;

    const std::string       m_Name;
    const PipelineStateDesc m_Desc;

    RefCntAutoPtr<IRenderPass>                       m_pRenderPass;
    RefCntAutoPtr<SerializableResourceSignatureImpl> m_pDefaultSignature;
    SignaturesVector                                 m_Signatures;
};

#define INSTANTIATE_SERIALIZED_PSO_CTOR(CreateInfoType)                                                                 \
    template SerializablePipelineStateImpl::SerializablePipelineStateImpl(IReferenceCounters*             pRefCounters, \
                                                                          SerializationDeviceImpl*        pDevice,      \
                                                                          const CreateInfoType&           CreateInfo,   \
                                                                          const PipelineStateArchiveInfo& ArchiveInfo)
#define DECLARE_SERIALIZED_PSO_CTOR(CreateInfoType) extern INSTANTIATE_SERIALIZED_PSO_CTOR(CreateInfoType)

DECLARE_SERIALIZED_PSO_CTOR(GraphicsPipelineStateCreateInfo);
DECLARE_SERIALIZED_PSO_CTOR(ComputePipelineStateCreateInfo);
DECLARE_SERIALIZED_PSO_CTOR(TilePipelineStateCreateInfo);
DECLARE_SERIALIZED_PSO_CTOR(RayTracingPipelineStateCreateInfo);


#define INSTANTIATE_PATCH_SHADER(MethodName, CreateInfoType, ...) template void SerializablePipelineStateImpl::MethodName<CreateInfoType>(const CreateInfoType& CreateInfo, ##__VA_ARGS__) noexcept(false)
#define DECLARE_PATCH_SHADER(MethodName, CreateInfoType, ...)     extern INSTANTIATE_PATCH_SHADER(MethodName, CreateInfoType, ##__VA_ARGS__)

#define DECLARE_PATCH_METHODS(MethodName, ...)                                        \
    DECLARE_PATCH_SHADER(MethodName, GraphicsPipelineStateCreateInfo, ##__VA_ARGS__); \
    DECLARE_PATCH_SHADER(MethodName, ComputePipelineStateCreateInfo, ##__VA_ARGS__);  \
    DECLARE_PATCH_SHADER(MethodName, TilePipelineStateCreateInfo, ##__VA_ARGS__);     \
    DECLARE_PATCH_SHADER(MethodName, RayTracingPipelineStateCreateInfo, ##__VA_ARGS__);

#define INSTANTIATE_PATCH_SHADER_METHODS(MethodName, ...)                                 \
    INSTANTIATE_PATCH_SHADER(MethodName, GraphicsPipelineStateCreateInfo, ##__VA_ARGS__); \
    INSTANTIATE_PATCH_SHADER(MethodName, ComputePipelineStateCreateInfo, ##__VA_ARGS__);  \
    INSTANTIATE_PATCH_SHADER(MethodName, TilePipelineStateCreateInfo, ##__VA_ARGS__);     \
    INSTANTIATE_PATCH_SHADER(MethodName, RayTracingPipelineStateCreateInfo, ##__VA_ARGS__);

#define INSTANTIATE_PREPARE_DEF_SIGNATURE_GL(CreateInfoType) template void SerializablePipelineStateImpl::PrepareDefaultSignatureGL<CreateInfoType>(const CreateInfoType& CreateInfo) noexcept(false)
#define DECLARE_PREPARE_DEF_SIGNATURE_GL(CreateInfoType)     extern INSTANTIATE_PREPARE_DEF_SIGNATURE_GL(CreateInfoType)
#define DECLARE_PREPARE_DEF_SIGNATURE_GL_METHODS()                     \
    DECLARE_PREPARE_DEF_SIGNATURE_GL(GraphicsPipelineStateCreateInfo); \
    DECLARE_PREPARE_DEF_SIGNATURE_GL(ComputePipelineStateCreateInfo);  \
    DECLARE_PREPARE_DEF_SIGNATURE_GL(TilePipelineStateCreateInfo);     \
    DECLARE_PREPARE_DEF_SIGNATURE_GL(RayTracingPipelineStateCreateInfo);


#if D3D11_SUPPORTED
DECLARE_PATCH_METHODS(PatchShadersD3D11)
#endif

#if D3D12_SUPPORTED
DECLARE_PATCH_METHODS(PatchShadersD3D12)
#endif

#if GL_SUPPORTED
DECLARE_PATCH_METHODS(PatchShadersGL)
DECLARE_PREPARE_DEF_SIGNATURE_GL_METHODS()
#endif

#if VULKAN_SUPPORTED
DECLARE_PATCH_METHODS(PatchShadersVk)
#endif

#if METAL_SUPPORTED
DECLARE_PATCH_METHODS(PatchShadersMtl, DeviceType DevType)
#endif

} // namespace Diligent
