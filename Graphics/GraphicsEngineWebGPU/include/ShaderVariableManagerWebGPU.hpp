/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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

/// \file
/// Declaration of Diligent::ShaderVariableManagerWebGPU class

#include "EngineWebGPUImplTraits.hpp"
#include "ShaderResourceVariableBase.hpp"

namespace Diligent
{

class ShaderVariableManagerWebGPU : ShaderVariableManagerBase<EngineWebGPUImplTraits, void>
{
public:
    using TBase = ShaderVariableManagerBase<EngineWebGPUImplTraits, void>;

    ShaderVariableManagerWebGPU(IObject& Owner, ShaderResourceCacheWebGPU& ResourceCache) noexcept;

    ~ShaderVariableManagerWebGPU();

    // clang-format off
    ShaderVariableManagerWebGPU             (const ShaderVariableManagerWebGPU&)  = delete;

    ShaderVariableManagerWebGPU& operator = (const ShaderVariableManagerWebGPU&)  = delete;

    ShaderVariableManagerWebGPU             (      ShaderVariableManagerWebGPU&&) = delete;

    ShaderVariableManagerWebGPU& operator = (      ShaderVariableManagerWebGPU&&) = delete;
    // clang-format on

    void Initialize(const PipelineResourceSignatureWebGPUImpl& Signature,
                    IMemoryAllocator&                          Allocator,
                    const SHADER_RESOURCE_VARIABLE_TYPE*       AllowedVarTypes,
                    Uint32                                     NumAllowedTypes,
                    SHADER_TYPE                                ShaderType);

    void Destroy(IMemoryAllocator& Allocator);

    static size_t GetRequiredMemorySize(const PipelineResourceSignatureWebGPUImpl& Signature,
                                        const SHADER_RESOURCE_VARIABLE_TYPE*       AllowedVarTypes,
                                        Uint32                                     NumAllowedTypes,
                                        SHADER_TYPE                                ShaderType);

    using ResourceAttribs = PipelineResourceAttribsWebGPU;

    const PipelineResourceDesc& GetResourceDesc(Uint32 Index) const;

    const ResourceAttribs& GetResourceAttribs(Uint32 Index) const;

    void BindResources(IResourceMapping* pResourceMapping, BIND_SHADER_RESOURCES_FLAGS Flags);

    void CheckResources(IResourceMapping*                    pResourceMapping,
                        BIND_SHADER_RESOURCES_FLAGS          Flags,
                        SHADER_RESOURCE_VARIABLE_TYPE_FLAGS& StaleVarTypes) const;

    IShaderResourceVariable* GetVariable(const Char* Name) const;

    IShaderResourceVariable* GetVariable(Uint32 Index) const;

    IObject& GetOwner() { return m_Owner; }

    Uint32 GetVariableCount() const;

    Uint32 GetVariableIndex(const IShaderResourceVariable& Variable) const;

    Uint32 GetNumCBs() const;

    Uint32 GetNumTexSRVs() const;

    Uint32 GetNumTexUAVs() const;

    Uint32 GetNumBufSRVs() const;

    Uint32 GetNumBufUAVs() const;

    Uint32 GetNumSamplers() const;

private:
};


} // namespace Diligent
