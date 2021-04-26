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

#include "EngineD3D12ImplTraits.hpp"
#include "PipelineStateBase.hpp"
#include "PipelineResourceSignatureD3D12Impl.hpp" // Required by PipelineStateBase
#include "RootSignature.hpp"

namespace Diligent
{

class ShaderResourcesD3D12;

/// Pipeline state object implementation in Direct3D12 backend.
class PipelineStateD3D12Impl final : public PipelineStateBase<EngineD3D12ImplTraits>
{
public:
    using TPipelineStateBase = PipelineStateBase<EngineD3D12ImplTraits>;

    PipelineStateD3D12Impl(IReferenceCounters* pRefCounters, RenderDeviceD3D12Impl* pDeviceD3D12, const GraphicsPipelineStateCreateInfo& CreateInfo);
    PipelineStateD3D12Impl(IReferenceCounters* pRefCounters, RenderDeviceD3D12Impl* pDeviceD3D12, const ComputePipelineStateCreateInfo& CreateInfo);
    PipelineStateD3D12Impl(IReferenceCounters* pRefCounters, RenderDeviceD3D12Impl* pDeviceD3D12, const RayTracingPipelineStateCreateInfo& CreateInfo);
    ~PipelineStateD3D12Impl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_PipelineStateD3D12, TPipelineStateBase)

    /// Implementation of IPipelineState::IsCompatibleWith() in Direct3D12 backend.
    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineState* pPSO) const override final;

    /// Implementation of IPipelineStateD3D12::GetD3D12PipelineState().
    virtual ID3D12PipelineState* DILIGENT_CALL_TYPE GetD3D12PipelineState() const override final { return static_cast<ID3D12PipelineState*>(m_pd3d12PSO.p); }

    /// Implementation of IPipelineStateD3D12::GetD3D12StateObject().
    virtual ID3D12StateObject* DILIGENT_CALL_TYPE GetD3D12StateObject() const override final { return static_cast<ID3D12StateObject*>(m_pd3d12PSO.p); }

    /// Implementation of IPipelineStateD3D12::GetD3D12RootSignature().
    virtual ID3D12RootSignature* DILIGENT_CALL_TYPE GetD3D12RootSignature() const override final { return m_RootSig->GetD3D12RootSignature(); }

    const RootSignatureD3D12& GetRootSignature() const { return *m_RootSig; }

#ifdef DILIGENT_DEVELOPMENT
    using ShaderResourceCacheArrayType = std::array<ShaderResourceCacheD3D12*, MAX_RESOURCE_SIGNATURES>;
    void DvpVerifySRBResources(const ShaderResourceCacheArrayType& ResourceCaches) const;
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

        friend SHADER_TYPE GetShaderStageType(const ShaderStageInfo& Stage) { return Stage.Type; }
    };
    using TShaderStages = std::vector<ShaderStageInfo>;

    template <typename PSOCreateInfoType>
    void InitInternalObjects(const PSOCreateInfoType& CreateInfo,
                             TShaderStages&           ShaderStages,
                             LocalRootSignatureD3D12* pLocalRootSig = nullptr);

    void InitRootSignature(TShaderStages&           ShaderStages,
                           LocalRootSignatureD3D12* pLocalRootSig);

    RefCntAutoPtr<PipelineResourceSignatureD3D12Impl> CreateDefaultResourceSignature(
        TShaderStages&           ShaderStages,
        LocalRootSignatureD3D12* pLocalRootSig);

    void Destruct();

    void ValidateShaderResources(const ShaderD3D12Impl* pShader, const LocalRootSignatureD3D12* pLocalRootSig);

private:
    CComPtr<ID3D12DeviceChild>        m_pd3d12PSO;
    RefCntAutoPtr<RootSignatureD3D12> m_RootSig;

    // NB:  Pipeline resource signatures used to create the PSO may NOT be the same as
    //      pipeline resource signatures in m_RootSig, because the latter may be used from the
    //      cache. While the two signatures may be compatible, they resource names may not be identical.

#ifdef DILIGENT_DEVELOPMENT
    // Shader resources for all shaders in all shader stages in the pipeline.
    std::vector<std::shared_ptr<const ShaderResourcesD3D12>> m_ShaderResources;

    // Shader resource attributions for every resource in m_ShaderResources, in the same order.
    std::vector<ResourceAttribution> m_ResourceAttibutions;
#endif
};

} // namespace Diligent
