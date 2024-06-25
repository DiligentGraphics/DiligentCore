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
/// Declaration of Diligent::PipelineResourceAttribsWebGPU struct

#include "HashUtils.hpp"
#include "ShaderResourceCacheCommon.hpp"

namespace Diligent
{

enum class BindGroupEntryType : Uint8
{
    UniformBuffer,
    UniformBufferDynamic,
    StorageBuffer,
    StorageBufferDynamic,
    StorageBuffer_ReadOnly,
    StorageBufferDynamic_ReadOnly,
    Texture,
    StorageTexture_WriteOnly,
    StorageTexture_ReadOnly,
    StorageTexture_ReadWrite,
    ExternalTexture,
    Sampler,
    Count
};

struct PipelineResourceAttribsWebGPU
{
private:
    static constexpr Uint32 _SamplerIndBits      = 31;
    static constexpr Uint32 _SamplerAssignedBits = 1;

public:
    static constexpr Uint32 MaxBindGroups = 2; //(1u << _DescrSetBits);

public:
    static constexpr Uint32 InvalidSamplerInd = (1u << _SamplerIndBits) - 1;

    // clang-format off
    const Uint32 SamplerInd           : _SamplerIndBits;       
    const Uint32 ImtblSamplerAssigned : _SamplerAssignedBits;
    // clang-format on

    // TODO
    Uint32 BindGroup    = ~0u;
    Uint32 BindingIndex = ~0u;

    PipelineResourceAttribsWebGPU(Uint32 _SamplerInd,
                                  bool   _ImtblSamplerAssigned) noexcept :
        // clang-format off
        SamplerInd          {_SamplerInd                    },
        ImtblSamplerAssigned{_ImtblSamplerAssigned ? 1u : 0u}
    // clang-format on
    {
        VERIFY(SamplerInd == _SamplerInd, "Sampler index (", _SamplerInd, ") exceeds maximum representable value.");
    }

    Uint32 CacheOffset(ResourceCacheContentType CacheType) const
    {
        UNSUPPORTED("Not implemented yet");
        return ~0u;
    }

    BindGroupEntryType GetBindGroupEntryType() const
    {
        UNSUPPORTED("Not implemented yet");
        return BindGroupEntryType::Count;
    }

    // Only for serialization
    PipelineResourceAttribsWebGPU() noexcept :
        PipelineResourceAttribsWebGPU{0, false}
    {}

    bool IsSamplerAssigned() const
    {
        return SamplerInd != InvalidSamplerInd;
    }

    bool IsImmutableSamplerAssigned() const
    {
        return ImtblSamplerAssigned != 0;
    }

    bool IsCompatibleWith(const PipelineResourceAttribsWebGPU& RHS) const
    {
        // Ignore assigned sampler index.
        // clang-format off
        return IsImmutableSamplerAssigned() == RHS.IsImmutableSamplerAssigned();
        // clang-format on
    }

    bool IsCombinedWithSampler() const
    {
        UNSUPPORTED("Not implemented yet");
        return false;
    }

    size_t GetHash() const
    {
        return ComputeHash(IsImmutableSamplerAssigned());
    }
};

} // namespace Diligent
