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
/// Declaration of Diligent::PipelineResourceSignatureD3D12Impl class

#include <array>

#include "EngineD3D12ImplTraits.hpp"
#include "PipelineResourceAttribsD3D12.hpp"
#include "PipelineResourceSignatureBase.hpp"
#include "SRBMemoryAllocator.hpp"
#include "RootParamsManager.hpp"
#include "ResourceBindingMap.hpp"

// ShaderVariableManagerD3D12, ShaderResourceCacheD3D12, and ShaderResourceBindingD3D12Impl
// are required by PipelineResourceSignatureBase
#include "ShaderResourceCacheD3D12.hpp"
#include "ShaderVariableManagerD3D12.hpp"
#include "ShaderResourceBindingD3D12Impl.hpp"

namespace Diligent
{

class CommandContext;
struct D3DShaderResourceAttribs;

/// Implementation of the Diligent::PipelineResourceSignatureD3D12Impl class
class PipelineResourceSignatureD3D12Impl final : public PipelineResourceSignatureBase<EngineD3D12ImplTraits>
{
public:
    using TPipelineResourceSignatureBase = PipelineResourceSignatureBase<EngineD3D12ImplTraits>;

    PipelineResourceSignatureD3D12Impl(IReferenceCounters*                  pRefCounters,
                                       RenderDeviceD3D12Impl*               pDevice,
                                       const PipelineResourceSignatureDesc& Desc,
                                       bool                                 bIsDeviceInternal = false);
    ~PipelineResourceSignatureD3D12Impl();

    using ResourceAttribs = TPipelineResourceSignatureBase::PipelineResourceAttribsType;

    struct ImmutableSamplerAttribs
    {
    private:
        static constexpr Uint32 _ShaderRegisterBits    = 24;
        static constexpr Uint32 _RegisterSpaceBits     = 8;
        static constexpr Uint32 _InvalidShaderRegister = (1u << _ShaderRegisterBits) - 1;
        static constexpr Uint32 _InvalidRegisterSpace  = (1u << _RegisterSpaceBits) - 1;

    public:
        Uint32 ArraySize = 1;
        Uint32 ShaderRegister : _ShaderRegisterBits;
        Uint32 RegisterSpace : _RegisterSpaceBits;

        ImmutableSamplerAttribs() :
            ShaderRegister{_InvalidShaderRegister},
            RegisterSpace{_InvalidRegisterSpace}
        {}

        ImmutableSamplerAttribs(Uint32 _ArraySize,
                                Uint32 _ShaderRegister,
                                Uint32 _RegisterSpace) noexcept :
            // clang-format off
            ArraySize     {_ArraySize     },
            ShaderRegister{_ShaderRegister},
            RegisterSpace {_RegisterSpace }
        // clang-format on
        {
            VERIFY(ShaderRegister == _ShaderRegister, "Shader register (", _ShaderRegister, ") exceeds maximum representable value");
            VERIFY(RegisterSpace == _RegisterSpace, "Shader register space (", _RegisterSpace, ") exceeds maximum representable value");
        }

        bool IsValid() const
        {
            return (ShaderRegister != _InvalidShaderRegister &&
                    RegisterSpace != _InvalidRegisterSpace);
        }
    };

    const ImmutableSamplerAttribs& GetImmutableSamplerAttribs(Uint32 SampIndex) const
    {
        VERIFY_EXPR(SampIndex < m_Desc.NumImmutableSamplers);
        return m_ImmutableSamplers[SampIndex];
    }

    Uint32 GetTotalRootParamsCount() const
    {
        return m_RootParams.GetNumRootTables() + m_RootParams.GetNumRootViews();
    }

    Uint32 GetNumRootTables() const
    {
        return m_RootParams.GetNumRootTables();
    }

    Uint32 GetNumRootViews() const
    {
        return m_RootParams.GetNumRootViews();
    }

    void InitSRBResourceCache(ShaderResourceCacheD3D12& ResourceCache);

    void CopyStaticResources(ShaderResourceCacheD3D12& ResourceCache) const;

    struct CommitCacheResourcesAttribs
    {
        const ShaderResourceCacheD3D12& ResourceCache;
        CommandContext&                 Ctx;
        DeviceContextD3D12Impl*         pDeviceCtx;
        DeviceContextIndex              DeviceCtxId;
        bool                            IsCompute;
        Uint32                          BaseRootIndex;
    };
    void CommitRootTables(const CommitCacheResourcesAttribs& CommitAttribs) const;

    void CommitRootViews(const CommitCacheResourcesAttribs& CommitAttribs,
                         Uint64                             BuffersMask) const;

    const RootParamsManager& GetRootParams() const { return m_RootParams; }

    // Adds resources and immutable samplers from this signature to the
    // resource binding map.
    void UpdateShaderResourceBindingMap(ResourceBinding::TMap& ResourceMap, SHADER_TYPE ShaderStage, Uint32 BaseRegisterSpace) const;

    // Returns true if there is an immutable sampler array in the given shader stage.
    bool HasImmutableSamplerArray(SHADER_TYPE ShaderStage) const;

#ifdef DILIGENT_DEVELOPMENT
    /// Verifies committed resource using the resource attributes from the PSO.
    bool DvpValidateCommittedResource(const D3DShaderResourceAttribs& D3DAttribs,
                                      Uint32                          ResIndex,
                                      const ShaderResourceCacheD3D12& ResourceCache,
                                      const char*                     ShaderName,
                                      const char*                     PSOName) const;
#endif

private:
    void AllocateRootParameters();

    void Destruct();

private:
    ImmutableSamplerAttribs* m_ImmutableSamplers = nullptr; // [m_Desc.NumImmutableSamplers]

    RootParamsManager m_RootParams;
};

} // namespace Diligent
