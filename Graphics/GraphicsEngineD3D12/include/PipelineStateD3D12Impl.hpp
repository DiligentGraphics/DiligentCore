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
/// Declaration of Diligent::PipelineStateD3D12Impl class

#include <vector>

#include "RenderDeviceD3D12.h"
#include "PipelineStateD3D12.h"
#include "PipelineResourceSignatureD3D12Impl.hpp"
#include "PipelineStateBase.hpp"
#include "RootSignature.hpp"
#include "RenderDeviceD3D12Impl.hpp"

namespace Diligent
{

class ShaderD3D12Impl;
class ShaderResourcesD3D12;
class ShaderResourceBindingD3D12Impl;

/// Pipeline state object implementation in Direct3D12 backend.
class PipelineStateD3D12Impl final : public PipelineStateBase<IPipelineStateD3D12, RenderDeviceD3D12Impl>
{
public:
    using TPipelineStateBase = PipelineStateBase<IPipelineStateD3D12, RenderDeviceD3D12Impl>;

    PipelineStateD3D12Impl(IReferenceCounters* pRefCounters, RenderDeviceD3D12Impl* pDeviceD3D12, const GraphicsPipelineStateCreateInfo& CreateInfo);
    PipelineStateD3D12Impl(IReferenceCounters* pRefCounters, RenderDeviceD3D12Impl* pDeviceD3D12, const ComputePipelineStateCreateInfo& CreateInfo);
    PipelineStateD3D12Impl(IReferenceCounters* pRefCounters, RenderDeviceD3D12Impl* pDeviceD3D12, const RayTracingPipelineStateCreateInfo& CreateInfo);
    ~PipelineStateD3D12Impl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_PipelineStateD3D12, TPipelineStateBase)

    /// Implementation of IPipelineState::IsCompatibleWith() in Direct3D12 backend.
    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineState* pPSO) const override final;

    /// Implementation of IPipelineState::GetResourceSignatureCount() in Direct3D12 backend.
    virtual Uint32 DILIGENT_CALL_TYPE GetResourceSignatureCount() const override final { return m_RootSig->GetSignatureCount(); }

    /// Implementation of IPipelineState::GetResourceSignature() in Direct3D12 backend.
    virtual IPipelineResourceSignature* DILIGENT_CALL_TYPE GetResourceSignature(Uint32 Index) const override final { return GetSignature(Index); }

    /// Implementation of IPipelineStateD3D12::GetD3D12PipelineState().
    virtual ID3D12PipelineState* DILIGENT_CALL_TYPE GetD3D12PipelineState() const override final { return static_cast<ID3D12PipelineState*>(m_pd3d12PSO.p); }

    /// Implementation of IPipelineStateD3D12::GetD3D12StateObject().
    virtual ID3D12StateObject* DILIGENT_CALL_TYPE GetD3D12StateObject() const override final { return static_cast<ID3D12StateObject*>(m_pd3d12PSO.p); }

    /// Implementation of IPipelineStateD3D12::GetD3D12RootSignature().
    virtual ID3D12RootSignature* DILIGENT_CALL_TYPE GetD3D12RootSignature() const override final { return m_RootSig->GetD3D12RootSignature(); }

    const RootSignatureD3D12& GetRootSignature() const { return *m_RootSig; }

    PipelineResourceSignatureD3D12Impl* GetSignature(Uint32 index) const
    {
        VERIFY_EXPR(index < GetResourceSignatureCount());
        return m_ResourceSignatures[index];
    }

#ifdef DILIGENT_DEVELOPMENT
    void DvpVerifySRBResources(ShaderResourceBindingD3D12Impl* pSRBs[], Uint32 NumSRBs) const;
#endif

private:
    struct ShaderStageInfo
    {
        ShaderStageInfo() {}
        ShaderStageInfo(ShaderD3D12Impl* _pShader);

        void   Append(ShaderD3D12Impl* pShader);
        size_t Count() const;

        SHADER_TYPE                    Type = SHADER_TYPE_UNKNOWN;
        std::vector<ShaderD3D12Impl*>  Shaders;
        std::vector<CComPtr<ID3DBlob>> ByteCodes;
    };
    using TShaderStages = std::vector<ShaderStageInfo>;

    template <typename PSOCreateInfoType>
    void InitInternalObjects(const PSOCreateInfoType& CreateInfo,
                             TShaderStages&           ShaderStages,
                             LocalRootSignatureD3D12* pLocalRootSig = nullptr);

    void InitRootSignature(const PipelineStateCreateInfo& CreateInfo,
                           TShaderStages&                 ShaderStages,
                           LocalRootSignatureD3D12*       pLocalRootSig);

    static RefCntAutoPtr<IPipelineResourceSignature> CreateDefaultResourceSignature(
        RenderDeviceD3D12Impl*         pDevice,
        const PipelineStateCreateInfo& CreateInfo,
        TShaderStages&                 ShaderStages,
        LocalRootSignatureD3D12*       pLocalRootSig);

    void Destruct();

#ifdef DILIGENT_DEVELOPMENT
    struct ResourceAttribution
    {
        static constexpr Uint32 InvalidSignatureIndex = ~0u;
        static constexpr Uint32 InvalidResourceIndex  = PipelineResourceSignatureD3D12Impl::InvalidResourceIndex;
        static constexpr Uint32 InvalidSamplerIndex   = InvalidImmutableSamplerIndex;

        const PipelineResourceSignatureD3D12Impl* pSignature = nullptr;

        Uint32 SignatureIndex        = InvalidSignatureIndex;
        Uint32 ResourceIndex         = InvalidResourceIndex;
        Uint32 ImmutableSamplerIndex = InvalidSamplerIndex;

        ResourceAttribution() noexcept {}
        ResourceAttribution(const PipelineResourceSignatureD3D12Impl* _pSignature,
                            Uint32                                    _SignatureIndex,
                            Uint32                                    _ResourceIndex,
                            Uint32                                    _ImmutableSamplerIndex = InvalidResourceIndex) noexcept :
            pSignature{_pSignature},
            SignatureIndex{_SignatureIndex},
            ResourceIndex{_ResourceIndex},
            ImmutableSamplerIndex{_ImmutableSamplerIndex}
        {
            VERIFY_EXPR(pSignature == nullptr || pSignature->GetDesc().BindingIndex == SignatureIndex);
            VERIFY_EXPR((ResourceIndex == InvalidResourceIndex) || (ImmutableSamplerIndex == InvalidSamplerIndex));
        }

        explicit operator bool() const
        {
            return SignatureIndex != InvalidSignatureIndex && (ResourceIndex != InvalidResourceIndex || ImmutableSamplerIndex != InvalidSamplerIndex);
        }

        bool IsImmutableSampler() const
        {
            return operator bool() && ImmutableSamplerIndex != InvalidSamplerIndex;
        }
    };
    ResourceAttribution GetResourceAttribution(const char* Name, SHADER_TYPE Stage) const;

    void DvpValidateShaderResources(const ShaderD3D12Impl* pShader, const LocalRootSignatureD3D12* pLocalRootSig);
#endif

private:
    CComPtr<ID3D12DeviceChild>        m_pd3d12PSO;
    RefCntAutoPtr<RootSignatureD3D12> m_RootSig;

    // NB:  Pipeline resource signatures used to create the PSO may NOT be the same as
    //      pipeline resource signatures in m_RootSig, because the latter may be used from the
    //      cache. While the two signatures may be compatible, they resource names may not be identical.
    std::unique_ptr<RefCntAutoPtr<PipelineResourceSignatureD3D12Impl>[]> m_ResourceSignatures;

#ifdef DILIGENT_DEVELOPMENT
    // Shader resources for all shaders in all shader stages in the pipeline.
    std::vector<std::shared_ptr<const ShaderResourcesD3D12>> m_ShaderResources;

    // Shader resource attributions for every resource in m_ShaderResources, in the same order.
    std::vector<ResourceAttribution> m_ResourceAttibutions;
#endif
};

} // namespace Diligent
