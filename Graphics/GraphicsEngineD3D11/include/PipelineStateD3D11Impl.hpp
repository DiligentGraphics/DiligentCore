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
/// Declaration of Diligent::PipelineStateD3D11Impl class

#include "PipelineStateD3D11.h"
#include "RenderDeviceD3D11.h"
#include "PipelineStateBase.hpp"

#include "PipelineResourceSignatureD3D11Impl.hpp" // Required by PipelineStateBase
#include "ShaderD3D11Impl.hpp"

namespace Diligent
{

/// Pipeline state object implementation in Direct3D11 backend.
class PipelineStateD3D11Impl final : public PipelineStateBase<EngineD3D11ImplTraits>
{
public:
    using TPipelineStateBase = PipelineStateBase<EngineD3D11ImplTraits>;

    PipelineStateD3D11Impl(IReferenceCounters*                    pRefCounters,
                           class RenderDeviceD3D11Impl*           pDeviceD3D11,
                           const GraphicsPipelineStateCreateInfo& CreateInfo);
    PipelineStateD3D11Impl(IReferenceCounters*                   pRefCounters,
                           class RenderDeviceD3D11Impl*          pDeviceD3D11,
                           const ComputePipelineStateCreateInfo& CreateInfo);
    ~PipelineStateD3D11Impl();

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final;

    /// Implementation of IPipelineState::IsCompatibleWith() in Direct3D11 backend.
    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineState* pPSO) const override final;

    /// Implementation of IPipelineStateD3D11::GetD3D11BlendState() method.
    virtual ID3D11BlendState* DILIGENT_CALL_TYPE GetD3D11BlendState() override final { return m_pd3d11BlendState; }

    /// Implementation of IPipelineStateD3D11::GetD3D11RasterizerState() method.
    virtual ID3D11RasterizerState* DILIGENT_CALL_TYPE GetD3D11RasterizerState() override final { return m_pd3d11RasterizerState; }

    /// Implementation of IPipelineStateD3D11::GetD3D11DepthStencilState() method.
    virtual ID3D11DepthStencilState* DILIGENT_CALL_TYPE GetD3D11DepthStencilState() override final { return m_pd3d11DepthStencilState; }

    /// Implementation of IPipelineStateD3D11::GetD3D11InputLayout() method.
    virtual ID3D11InputLayout* DILIGENT_CALL_TYPE GetD3D11InputLayout() override final { return m_pd3d11InputLayout; }

    /// Implementation of IPipelineStateD3D11::GetD3D11VertexShader() method.
    virtual ID3D11VertexShader* DILIGENT_CALL_TYPE GetD3D11VertexShader() override final { return m_pVS; }

    /// Implementation of IPipelineStateD3D11::GetD3D11PixelShader() method.
    virtual ID3D11PixelShader* DILIGENT_CALL_TYPE GetD3D11PixelShader() override final { return m_pPS; }

    /// Implementation of IPipelineStateD3D11::GetD3D11GeometryShader() method.
    virtual ID3D11GeometryShader* DILIGENT_CALL_TYPE GetD3D11GeometryShader() override final { return m_pGS; }

    /// Implementation of IPipelineStateD3D11::GetD3D11DomainShader() method.
    virtual ID3D11DomainShader* DILIGENT_CALL_TYPE GetD3D11DomainShader() override final { return m_pDS; }

    /// Implementation of IPipelineStateD3D11::GetD3D11HullShader() method.
    virtual ID3D11HullShader* DILIGENT_CALL_TYPE GetD3D11HullShader() override final { return m_pHS; }

    /// Implementation of IPipelineStateD3D11::GetD3D11ComputeShader() method.
    virtual ID3D11ComputeShader* DILIGENT_CALL_TYPE GetD3D11ComputeShader() override final { return m_pCS; }

    Uint32      GetNumShaders() const { return m_NumShaders; }
    SHADER_TYPE GetShaderStageType(Uint32 Index) const;

private:
    template <typename PSOCreateInfoType>
    void InitInternalObjects(const PSOCreateInfoType&        CreateInfo,
                             std::vector<CComPtr<ID3DBlob>>& ByteCodes);

    void InitResourceLayouts(const PipelineStateCreateInfo&       CreateInfo,
                             const std::vector<ShaderD3D11Impl*>& Shaders,
                             std::vector<CComPtr<ID3DBlob>>&      ByteCodes);

    RefCntAutoPtr<PipelineResourceSignatureD3D11Impl> CreateDefaultResourceSignature(
        const PipelineStateCreateInfo&       CreateInfo,
        const std::vector<ShaderD3D11Impl*>& Shaders);

    void Destruct();

    void ValidateShaderResources(const ShaderD3D11Impl* pShader);

private:
    using SignaturePtr = RefCntAutoPtr<PipelineResourceSignatureD3D11Impl>;

    std::array<Uint8, 5> m_ShaderTypes = {};
    Uint8                m_NumShaders  = 0;

    CComPtr<ID3D11BlendState>        m_pd3d11BlendState;
    CComPtr<ID3D11RasterizerState>   m_pd3d11RasterizerState;
    CComPtr<ID3D11DepthStencilState> m_pd3d11DepthStencilState;
    CComPtr<ID3D11InputLayout>       m_pd3d11InputLayout;
    CComPtr<ID3D11VertexShader>      m_pVS;
    CComPtr<ID3D11PixelShader>       m_pPS;
    CComPtr<ID3D11GeometryShader>    m_pGS;
    CComPtr<ID3D11DomainShader>      m_pDS;
    CComPtr<ID3D11HullShader>        m_pHS;
    CComPtr<ID3D11ComputeShader>     m_pCS;

#ifdef DILIGENT_DEVELOPMENT
    // Shader resources for all shaders in all shader stages in the pipeline.
    std::vector<std::shared_ptr<const ShaderResourcesD3D11>> m_ShaderResources;

    // Shader resource attributions for every resource in m_ShaderResources, in the same order.
    std::vector<ResourceAttribution> m_ResourceAttibutions;
#endif
};

__forceinline SHADER_TYPE GetShaderStageType(const ShaderD3D11Impl* pShader)
{
    return pShader->GetDesc().ShaderType;
}

} // namespace Diligent
