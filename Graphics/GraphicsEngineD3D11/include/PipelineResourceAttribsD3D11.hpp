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


// sizeof(BindPointsD3D11) == 8, x64
struct BindPointsD3D11
{
    /// Number of different shader types (Vertex, Pixel, Geometry, Domain, Hull, Compute)
    static constexpr Uint32 NumShaderTypes = 6;

    static constexpr Uint8 InvalidBindPoint = 0xFF;

    BindPointsD3D11() noexcept {}
    BindPointsD3D11(const BindPointsD3D11&) noexcept = default;

    // clang-format off
    Uint32 GetActiveBits()          const { return m_ActiveBits; }
    bool   IsValid(Uint32 index)    const { return m_Bindings[index] != InvalidBindPoint; }
    Uint8  operator[](Uint32 index) const { return m_Bindings[index]; }
    // clang-format on

    void Set(Uint32 Index, Uint32 BindPoint)
    {
        VERIFY_EXPR(Index < NumShaderTypes);
        VERIFY_EXPR(BindPoint < InvalidBindPoint);
        m_ActiveBits      = static_cast<Uint8>(m_ActiveBits | (1u << Index));
        m_Bindings[Index] = static_cast<Uint8>(BindPoint);
    }

    size_t GetHash() const
    {
        size_t Hash = 0;
        for (Uint32 i = 0; i < NumShaderTypes; ++i)
            HashCombine(Hash, m_Bindings[i]);
        return Hash;
    }

    bool operator==(const BindPointsD3D11& rhs) const
    {
        return m_Bindings == rhs.m_Bindings;
    }

    BindPointsD3D11 operator+(Uint32 value) const
    {
        BindPointsD3D11 Result{*this};
        for (Uint32 Bits = Result.m_ActiveBits; Bits != 0;)
        {
            auto Index = PlatformMisc::GetLSB(Bits);
            Bits &= ~(1u << Index);

            auto NewBindPoint = Result.m_Bindings[Index] + value;
            VERIFY_EXPR(NewBindPoint < InvalidBindPoint);
            Result.m_Bindings[Index] = static_cast<Uint8>(NewBindPoint);
        }
        return Result;
    }

private:
    Uint8                             m_ActiveBits = 0;
    std::array<Uint8, NumShaderTypes> m_Bindings   = {InvalidBindPoint, InvalidBindPoint, InvalidBindPoint, InvalidBindPoint, InvalidBindPoint, InvalidBindPoint};
};


// sizeof(PipelineResourceAttribsD3D11) == 12, x64
struct PipelineResourceAttribsD3D11
{
private:
    static constexpr Uint32 _CacheOffsetBits     = 10;
    static constexpr Uint32 _SamplerIndBits      = 10;
    static constexpr Uint32 _SamplerAssignedBits = 1;

public:
    static constexpr Uint32 InvalidCacheOffset = (1u << _CacheOffsetBits) - 1;
    static constexpr Uint32 InvalidSamplerInd  = (1u << _SamplerIndBits) - 1;

    // clang-format off
    const Uint32    CacheOffset          : _CacheOffsetBits;      // SRB and Signature has the same cache offsets for static resources.
    const Uint32    SamplerInd           : _SamplerIndBits;       // Index of the assigned sampler in m_Desc.Resources.
    const Uint32    ImtblSamplerAssigned : _SamplerAssignedBits;  // Immutable sampler flag.
    BindPointsD3D11 BindPoints;
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
        // Ignore cache offset and sampler index.
        // clang-format off
        return IsImmutableSamplerAssigned() == rhs.IsImmutableSamplerAssigned() &&
               BindPoints                   == rhs.BindPoints;
        // clang-format on
    }

    size_t GetHash() const
    {
        return ComputeHash(IsImmutableSamplerAssigned(), BindPoints.GetHash());
    }
};

} // namespace Diligent
