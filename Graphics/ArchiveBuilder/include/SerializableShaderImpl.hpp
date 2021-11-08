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

#include "Shader.h"
#include "ObjectBase.hpp"
#include "STDAllocator.hpp"
#include "SerializationDeviceImpl.hpp"

namespace Diligent
{

#if D3D12_SUPPORTED
class ShaderD3D12Impl;
#endif
#if VULKAN_SUPPORTED
class ShaderVkImpl;
#endif

class SerializableShaderImpl final : public ObjectBase<IShader>
{
public:
    using TBase = ObjectBase<IShader>;

    SerializableShaderImpl(IReferenceCounters*      pRefCounters,
                           SerializationDeviceImpl* pDevice,
                           const ShaderCreateInfo&  ShaderCI,
                           Uint32                   DeviceBits);
    ~SerializableShaderImpl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_Shader, TBase)

    virtual Uint32 DILIGENT_CALL_TYPE GetResourceCount() const override final { return 0; }

    virtual void DILIGENT_CALL_TYPE GetResourceDesc(Uint32 Index, ShaderResourceDesc& ResourceDesc) const override final {}

    virtual const ShaderDesc& DILIGENT_CALL_TYPE GetDesc() const override final { return m_CreateInfo.Desc; }

    virtual Int32 DILIGENT_CALL_TYPE GetUniqueID() const override final { return 0; }

    virtual void DILIGENT_CALL_TYPE SetUserData(IObject* pUserData) override final {}

    virtual IObject* DILIGENT_CALL_TYPE GetUserData() const override final { return nullptr; }

#if D3D12_SUPPORTED
    const ShaderD3D12Impl* GetShaderD3D12() const;
#endif
#if VULKAN_SUPPORTED
    const ShaderVkImpl* GetShaderVk() const;
#endif

private:
    void CopyShaderCreateInfo(const ShaderCreateInfo& ShaderCI);

    ShaderCreateInfo                              m_CreateInfo;
    std::unique_ptr<void, STDDeleterRawMem<void>> m_pRawMemory;

#if D3D12_SUPPORTED
    struct CompiledShaderD3D12;
    std::unique_ptr<CompiledShaderD3D12> m_pShaderD3D12;
#endif
#if VULKAN_SUPPORTED
    struct CompiledShaderVk;
    std::unique_ptr<CompiledShaderVk> m_pShaderVk;
#endif
};

} // namespace Diligent
