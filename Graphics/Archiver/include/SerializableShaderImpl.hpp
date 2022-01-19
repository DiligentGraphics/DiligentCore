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

#include "Shader.h"
#include "ObjectBase.hpp"
#include "RefCntAutoPtr.hpp"
#include "STDAllocator.hpp"
#include "SerializationDeviceImpl.hpp"
#include "Serializer.hpp"
#include "DeviceObjectArchiveBase.hpp"

namespace Diligent
{

#if METAL_SUPPORTED
class PipelineResourceSignatureMtlImpl;
class SPIRVShaderResources;
using MtlArchiverResourceCounters = std::array<std::array<Uint16, 4>, 2>; // same as MtlResourceCounters
#endif

class SerializableShaderImpl final : public ObjectBase<IShader>
{
public:
    using TBase      = ObjectBase<IShader>;
    using DeviceType = DeviceObjectArchiveBase::DeviceType;

    SerializableShaderImpl(IReferenceCounters*       pRefCounters,
                           SerializationDeviceImpl*  pDevice,
                           const ShaderCreateInfo&   ShaderCI,
                           ARCHIVE_DEVICE_DATA_FLAGS DeviceFlags);
    ~SerializableShaderImpl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_Shader, TBase)

    virtual Uint32 DILIGENT_CALL_TYPE GetResourceCount() const override final { return 0; }

    virtual void DILIGENT_CALL_TYPE GetResourceDesc(Uint32 Index, ShaderResourceDesc& ResourceDesc) const override final {}

    virtual const ShaderDesc& DILIGENT_CALL_TYPE GetDesc() const override final { return m_CreateInfo.Desc; }

    virtual Int32 DILIGENT_CALL_TYPE GetUniqueID() const override final { return 0; }

    virtual void DILIGENT_CALL_TYPE SetUserData(IObject* pUserData) override final {}

    virtual IObject* DILIGENT_CALL_TYPE GetUserData() const override final { return nullptr; }

    struct CompiledShader
    {
        virtual ~CompiledShader() {}
    };

    template <typename CompiledShaderType>
    CompiledShaderType* GetShader(DeviceType Type) const
    {
        return static_cast<CompiledShaderType*>(m_Shaders[static_cast<size_t>(Type)].get());
    }

#if METAL_SUPPORTED
    SerializedData PatchShaderMtl(const RefCntAutoPtr<PipelineResourceSignatureMtlImpl>* pSignatures,
                                  const MtlArchiverResourceCounters*                     pBaseBindings,
                                  const Uint32                                           SignatureCount,
                                  DeviceType                                             DevType) const noexcept(false);

    const SPIRVShaderResources* GetMtlShaderSPIRVResources() const;
#endif

    const ShaderCreateInfo& GetCreateInfo() const
    {
        return m_CreateInfo;
    }

private:
    void CopyShaderCreateInfo(const ShaderCreateInfo& ShaderCI) noexcept(false);

    SerializationDeviceImpl*                      m_pDevice;
    ShaderCreateInfo                              m_CreateInfo;
    std::unique_ptr<void, STDDeleterRawMem<void>> m_pRawMemory;

    std::array<std::unique_ptr<CompiledShader>, static_cast<size_t>(DeviceType::Count)> m_Shaders;

    template <typename ShaderType, typename... ArgTypes>
    void CreateShader(DeviceType Type, String& CompilationLog, const char* DeviceTypeName, IReferenceCounters* pRefCounters, ShaderCreateInfo ShaderCI, const ArgTypes&... Args);

#if D3D11_SUPPORTED
    void CreateShaderD3D11(IReferenceCounters* pRefCounters, const ShaderCreateInfo& ShaderCI, String& CompilationLog);
#endif

#if D3D12_SUPPORTED
    void CreateShaderD3D12(IReferenceCounters* pRefCounters, const ShaderCreateInfo& ShaderCI, String& CompilationLog);
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
    void CreateShaderGL(IReferenceCounters* pRefCounters, const ShaderCreateInfo& ShaderCI, String& CompilationLog, RENDER_DEVICE_TYPE DeviceType);
#endif

#if VULKAN_SUPPORTED
    void CreateShaderVk(IReferenceCounters* pRefCounters, const ShaderCreateInfo& ShaderCI, String& CompilationLog);
#endif

#if METAL_SUPPORTED
    struct CompiledShaderMtlImpl;
    std::unique_ptr<CompiledShader> m_pShaderMtl;

    void CreateShaderMtl(ShaderCreateInfo ShaderCI, String& CompilationLog);
#endif
};

} // namespace Diligent
