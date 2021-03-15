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
/// Declaration of Diligent::PipelineResourceSignatureD3D11Impl class

#include <array>

#include "ResourceBindingMap.hpp"

#include "EngineD3D11ImplTraits.hpp"
#include "PipelineResourceSignatureBase.hpp"
#include "PipelineResourceAttribsD3D11.hpp"

// ShaderVariableManagerD3D11, ShaderResourceCacheD3D11, and ShaderResourceBindingD3D11Impl
// are required by PipelineResourceSignatureBase
#include "ShaderVariableManagerD3D11.hpp"
#include "ShaderResourceBindingD3D11Impl.hpp"
#include "ShaderResourceCacheD3D11.hpp"

namespace Diligent
{

/// Implementation of the Diligent::PipelineResourceSignatureD3D11Impl class
class PipelineResourceSignatureD3D11Impl final : public PipelineResourceSignatureBase<EngineD3D11ImplTraits>
{
public:
    using TPipelineResourceSignatureBase = PipelineResourceSignatureBase<EngineD3D11ImplTraits>;

    PipelineResourceSignatureD3D11Impl(IReferenceCounters*                  pRefCounters,
                                       RenderDeviceD3D11Impl*               pDevice,
                                       const PipelineResourceSignatureDesc& Desc,
                                       bool                                 bIsDeviceInternal = false);
    ~PipelineResourceSignatureD3D11Impl();

    using ResourceAttribs                = PipelineResourceAttribsD3D11;
    static constexpr auto NumShaderTypes = BindPointsD3D11::NumShaderTypes;

    const ResourceAttribs& GetResourceAttribs(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_pResourceAttribs[ResIndex];
    }

    // sizeof(ImmutableSamplerAttribs) == 24, x64
    struct ImmutableSamplerAttribs
    {
    public:
        RefCntAutoPtr<ISampler> pSampler;
        Uint32                  ArraySize = 0;
        BindPointsD3D11         BindPoints;

        ImmutableSamplerAttribs() noexcept {}

        bool              IsAllocated() const { return !BindPoints.IsEmpty(); }
        SamplerD3D11Impl* GetSamplerD3D11() const { return ValidatedCast<SamplerD3D11Impl>(pSampler.RawPtr<ISampler>()); }
    };

    const ImmutableSamplerAttribs& GetImmutableSamplerAttribs(Uint32 SampIndex) const
    {
        VERIFY_EXPR(SampIndex < m_Desc.NumImmutableSamplers);
        return m_ImmutableSamplers[SampIndex];
    }

    using TResourceCount    = std::array<Uint8, D3D11_RESOURCE_RANGE_COUNT>;
    using TBindingsPerStage = std::array<std::array<Uint8, NumShaderTypes>, D3D11_RESOURCE_RANGE_COUNT>;

    __forceinline void ShiftBindings(TBindingsPerStage& Bindings) const
    {
        for (Uint32 r = 0; r < Bindings.size(); ++r)
        {
            for (Uint32 s = 0; s < Bindings[r].size(); ++s)
            {
                Uint32 Count = Bindings[r][s] + m_BindingCountPerStage[r][s];
                VERIFY_EXPR(Count < std::numeric_limits<Uint8>::max());
                Bindings[r][s] = static_cast<Uint8>(Count);
            }
        }
    }

    void InitSRBResourceCache(ShaderResourceCacheD3D11& ResourceCache);

    void UpdateShaderResourceBindingMap(ResourceBinding::TMap& ResourceMap, SHADER_TYPE ShaderStage, const TBindingsPerStage& BaseBindings) const;

    // Copies static resources from the static resource cache to the destination cache
    void CopyStaticResources(ShaderResourceCacheD3D11& ResourceCache) const;

#ifdef DILIGENT_DEVELOPMENT
    /// Verifies committed resource attribs using the SPIRV resource attributes from the PSO.
    bool DvpValidateCommittedResource(const D3DShaderResourceAttribs& D3DAttribs,
                                      Uint32                          ResIndex,
                                      const ShaderResourceCacheD3D11& ResourceCache,
                                      const char*                     ShaderName,
                                      const char*                     PSOName) const;
#endif

private:
    void CreateLayout();

    void Destruct();

private:
    TBindingsPerStage        m_BindingCountPerStage = {};
    ResourceAttribs*         m_pResourceAttribs     = nullptr; // [m_Desc.NumResources]
    ImmutableSamplerAttribs* m_ImmutableSamplers    = nullptr; // [m_Desc.NumImmutableSamplers]
};

} // namespace Diligent
