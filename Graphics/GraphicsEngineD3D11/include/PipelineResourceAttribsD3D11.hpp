/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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
/// Declaration of Diligent::PipelineResourceAttribsD3D11 struct

#include <array>

#include "BasicTypes.h"
#include "DebugUtilities.hpp"

namespace Diligent
{

enum DESCRIPTOR_RANGE : Uint32
{
    DESCRIPTOR_RANGE_CBV = 0,
    DESCRIPTOR_RANGE_SRV,
    DESCRIPTOR_RANGE_SAMPLER,
    DESCRIPTOR_RANGE_UAV,
    DESCRIPTOR_RANGE_COUNT,
    DESCRIPTOR_RANGE_UNKNOWN = ~0u
};
DESCRIPTOR_RANGE ShaderResourceToDescriptorRange(SHADER_RESOURCE_TYPE Type);

// sizeof(PipelineResourceAttribsD3D11) == 4, x64
struct PipelineResourceAttribsD3D11
{
private:
    static constexpr Uint32 _CacheOffsetBits     = 10;
    static constexpr Uint32 _SamplerIndBits      = 10;
    static constexpr Uint32 _SamplerAssignedBits = 1;

public:
    static constexpr Uint32 InvalidCacheOffset = (1u << _CacheOffsetBits) - 1;
    static constexpr Uint32 InvalidSamplerInd  = (1u << _SamplerIndBits) - 1;

    /// Number of different shader types (Vertex, Pixel, Geometry, Domain, Hull, Compute)
    static constexpr Uint32 NumShaderTypes = 6;

    using TBindPoints                              = std::array<Uint8, NumShaderTypes>;
    static constexpr Uint8       InvalidBindPoint  = 0xFF;
    static constexpr TBindPoints InvalidBindPoints = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // clang-format off
    const Uint32  CacheOffset          : _CacheOffsetBits;      // SRB and Signature has the same cache offsets for static resources.
    const Uint32  SamplerInd           : _SamplerIndBits;       // Index of the assigned sampler in m_Desc.Resources.
    const Uint32  ImtblSamplerAssigned : _SamplerAssignedBits;  // Immutable sampler flag.
    TBindPoints   BindPoints           = InvalidBindPoints;
    // clang-format on

    PipelineResourceAttribsD3D11(Uint32 _CacheOffset,
                                 Uint32 _SamplerInd,
                                 bool   _ImtblSamplerAssigned) noexcept :
        // clang-format off
            CacheOffset         {_CacheOffset                   },
            SamplerInd          {_SamplerInd                    },
            ImtblSamplerAssigned{_ImtblSamplerAssigned ? 1u : 0u}
    // clang-format on
    {
        VERIFY(CacheOffset == _CacheOffset, "Cache offset (", _CacheOffset, ") exceeds maximum representable value");
        VERIFY(SamplerInd == _SamplerInd, "Sampler index (", _SamplerInd, ") exceeds maximum representable value");
    }

    bool IsSamplerAssigned() const { return SamplerInd != InvalidSamplerInd; }
    bool IsImmutableSamplerAssigned() const { return ImtblSamplerAssigned != 0; }

    bool IsCompatibleWith(const PipelineResourceAttribsD3D11& rhs) const
    {
        // Ignore sampler index.
        // clang-format off
        return CacheOffset                  == rhs.CacheOffset                  &&
               IsImmutableSamplerAssigned() == rhs.IsImmutableSamplerAssigned() &&
               BindPoints                   == rhs.BindPoints;
        // clang-format on
    }

    size_t GetHash() const
    {
        Uint64 h = 0;
        for (Uint32 i = 0; i < NumShaderTypes; ++i)
        {
            h |= (BindPoints[i] << (i * 8));
        }
        return ComputeHash(h);
    }
};

} // namespace Diligent
