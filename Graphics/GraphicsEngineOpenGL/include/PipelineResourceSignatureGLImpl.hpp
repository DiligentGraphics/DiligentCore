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
/// Declaration of Diligent::PipelineResourceSignatureGLImpl class

#include <array>

#include "PipelineResourceSignatureBase.hpp"
#include "ShaderResourceCacheGL.hpp"
#include "ShaderResourcesGL.hpp"

namespace Diligent
{
class RenderDeviceGLImpl;
class ShaderVariableGL;

enum BINDING_RANGE : Uint32
{
    BINDING_RANGE_UNIFORM_BUFFER = 0,
    BINDING_RANGE_TEXTURE,
    BINDING_RANGE_IMAGE,
    BINDING_RANGE_STORAGE_BUFFER,
    BINDING_RANGE_COUNT,
    BINDING_RANGE_UNKNOWN = ~0u
};
BINDING_RANGE PipelineResourceToBindingRange(const PipelineResourceDesc& Desc);
const char*   GetBindingRangeName(BINDING_RANGE Range);


/// Implementation of the Diligent::PipelineResourceSignatureGLImpl class
class PipelineResourceSignatureGLImpl final : public PipelineResourceSignatureBase<IPipelineResourceSignature, RenderDeviceGLImpl>
{
public:
    using TPipelineResourceSignatureBase = PipelineResourceSignatureBase<IPipelineResourceSignature, RenderDeviceGLImpl>;

    PipelineResourceSignatureGLImpl(IReferenceCounters*                  pRefCounters,
                                    RenderDeviceGLImpl*                  pDevice,
                                    const PipelineResourceSignatureDesc& Desc,
                                    bool                                 bIsDeviceInternal = false);
    ~PipelineResourceSignatureGLImpl();

    // sizeof(ResourceAttribs) == 8, x64
    struct ResourceAttribs
    {
    private:
        static constexpr Uint32 _SamplerIndBits      = 31;
        static constexpr Uint32 _SamplerAssignedBits = 1;

    public:
        static constexpr Uint32 InvalidCacheOffset = ~0u;
        static constexpr Uint32 InvalidSamplerInd  = (1u << _SamplerIndBits) - 1;

        // clang-format off
        const Uint32  CacheOffset;                                 // SRB and Signature has the same cache offsets for static resources.
                                                                   // Binding = m_FirstBinding[Range] + CacheOffset
        const Uint32  SamplerInd           : _SamplerIndBits;      // ImtblSamplerAssigned == true:  index of the immutable sampler in m_ImmutableSamplers.
                                                                   // ImtblSamplerAssigned == false: index of the assigned sampler in m_Desc.Resources.
        const Uint32  ImtblSamplerAssigned : _SamplerAssignedBits; // Immutable sampler flag
        // clang-format on

        ResourceAttribs(Uint32 _CacheOffset,
                        Uint32 _SamplerInd,
                        bool   _ImtblSamplerAssigned) noexcept :
            // clang-format off
            CacheOffset         {_CacheOffset                   },
            SamplerInd          {_SamplerInd                    },
            ImtblSamplerAssigned{_ImtblSamplerAssigned ? 1u : 0u}
        // clang-format on
        {
            VERIFY(SamplerInd == _SamplerInd, "Sampler index (", _SamplerInd, ") exceeds maximum representable value");
            VERIFY(!_ImtblSamplerAssigned || SamplerInd != InvalidSamplerInd, "Immutable sampler assigned, but sampler index is not valid");
        }

        bool IsSamplerAssigned() const { return SamplerInd != InvalidSamplerInd; }
        bool IsImmutableSamplerAssigned() const { return ImtblSamplerAssigned != 0; }
    };

    const ResourceAttribs& GetResourceAttribs(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_pResourceAttribs[ResIndex];
    }

    const PipelineResourceDesc& GetResourceDesc(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_Desc.Resources[ResIndex];
    }

    bool HasDynamicResources() const
    {
        auto IndexRange = GetResourceIndexRange(SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);
        return IndexRange.second > IndexRange.first;
    }

    using TBindings = std::array<Uint32, BINDING_RANGE_COUNT>;

    void ApplyBindings(GLObjectWrappers::GLProgramObj& GLProgram,
                       class GLContextState&           State,
                       SHADER_TYPE                     Stages,
                       const TBindings&                Bindings) const;

    void AddBindings(TBindings& Bindings) const;

    /// Implementation of IPipelineResourceSignature::CreateShaderResourceBinding.
    virtual void DILIGENT_CALL_TYPE CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                bool                     InitStaticResources) override final;

    /// Implementation of IPipelineResourceSignature::GetStaticVariableByName.
    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) override final;

    /// Implementation of IPipelineResourceSignature::GetStaticVariableByIndex.
    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    /// Implementation of IPipelineResourceSignature::GetStaticVariableCount.
    virtual Uint32 DILIGENT_CALL_TYPE GetStaticVariableCount(SHADER_TYPE ShaderType) const override final;

    /// Implementation of IPipelineResourceSignature::BindStaticResources.
    virtual void DILIGENT_CALL_TYPE BindStaticResources(Uint32            ShaderFlags,
                                                        IResourceMapping* pResourceMapping,
                                                        Uint32            Flags) override final;

    /// Implementation of IPipelineResourceSignature::IsCompatibleWith.
    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineResourceSignature* pPRS) const override final
    {
        if (pPRS == nullptr)
        {
            return GetHash() == 0;
        }
        return IsCompatibleWith(*ValidatedCast<const PipelineResourceSignatureGLImpl>(pPRS));
    }

    /// Implementation of IPipelineResourceSignature::InitializeStaticSRBResources.
    virtual void DILIGENT_CALL_TYPE InitializeStaticSRBResources(IShaderResourceBinding* pSRB) const override final;

    bool IsCompatibleWith(const PipelineResourceSignatureGLImpl& Other) const;

    bool IsIncompatibleWith(const PipelineResourceSignatureGLImpl& Other) const
    {
        return GetHash() != Other.GetHash() || m_BindingCount != Other.m_BindingCount;
    }

    void InitSRBResourceCache(ShaderResourceCacheGL& ResourceCache) const;

#ifdef DILIGENT_DEVELOPMENT
    /// Verifies committed resource attribs using the SPIRV resource attributes from the PSO.
    bool DvpValidateCommittedResource(const ShaderResourcesGL::GLResourceAttribs& GLAttribs,
                                      RESOURCE_DIMENSION                          ResourceDim,
                                      bool                                        IsMultisample,
                                      Uint32                                      ResIndex,
                                      const ShaderResourceCacheGL&                ResourceCache,
                                      const char*                                 ShaderName,
                                      const char*                                 PSOName) const;
#endif

private:
    PipelineResourceSignatureGLImpl(IReferenceCounters*                  pRefCounters,
                                    RenderDeviceGLImpl*                  pDevice,
                                    const PipelineResourceSignatureDesc& Desc,
                                    bool                                 bIsDeviceInternal,
                                    int                                  Internal);

    // Copies static resources from the static resource cache to the destination cache
    void CopyStaticResources(ShaderResourceCacheGL& ResourceCache) const;

    void CreateLayouts();

    void Destruct();

    size_t CalculateHash() const;

private:
    TBindings m_BindingCount = {};

    ResourceAttribs* m_pResourceAttribs = nullptr; // [m_Desc.NumResources]

    // Resource cache for static resource variables only
    ShaderResourceCacheGL* m_pStaticResCache = nullptr;

    ShaderVariableGL* m_StaticVarsMgrs = nullptr; // [GetNumStaticResStages()]

    using SamplerPtr                = RefCntAutoPtr<ISampler>;
    SamplerPtr* m_ImmutableSamplers = nullptr; // [m_Desc.NumImmutableSamplers]
};


__forceinline void PipelineResourceSignatureGLImpl::AddBindings(TBindings& Bindings) const
{
    for (Uint32 i = 0; i < Bindings.size(); ++i)
    {
        Bindings[i] += m_BindingCount[i];
    }
}

} // namespace Diligent
