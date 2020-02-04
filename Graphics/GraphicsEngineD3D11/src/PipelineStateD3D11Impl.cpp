/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

#include "pch.h"
#include <array>
#include "PipelineStateD3D11Impl.hpp"
#include "RenderDeviceD3D11Impl.hpp"
#include "ShaderResourceBindingD3D11Impl.hpp"
#include "EngineMemory.h"
#include "ShaderD3D11Impl.hpp"

namespace Diligent
{

PipelineStateD3D11Impl::PipelineStateD3D11Impl(IReferenceCounters*      pRefCounters,
                                               RenderDeviceD3D11Impl*   pRenderDeviceD3D11,
                                               const PipelineStateDesc& PipelineDesc) :
    // clang-format off
    TPipelineStateBase
    {
        pRefCounters,
        pRenderDeviceD3D11,
        PipelineDesc
    },
    m_SRBMemAllocator{GetRawAllocator()},
    m_StaticSamplers (STD_ALLOCATOR_RAW_MEM(StaticSamplerInfo, GetRawAllocator(), "Allocator for vector<StaticSamplerInfo>"))
// clang-format on
{
    if (m_Desc.IsComputePipeline)
    {
        auto* pCS = ValidatedCast<ShaderD3D11Impl>(m_Desc.ComputePipeline.pCS);
        m_pCS     = pCS;
        if (m_pCS == nullptr)
        {
            LOG_ERROR_AND_THROW("Compute shader is null");
        }

        if (m_pCS && m_pCS->GetDesc().ShaderType != SHADER_TYPE_COMPUTE)
        {
            LOG_ERROR_AND_THROW(GetShaderTypeLiteralName(SHADER_TYPE_COMPUTE), " shader is expeceted while ", GetShaderTypeLiteralName(m_pCS->GetDesc().ShaderType), " provided");
        }
        m_ShaderResourceLayoutHash = pCS->GetD3D11Resources()->GetHash();
    }
    else
    {

#define INIT_SHADER(ShortName, ExpectedType)                                                                                                                                         \
    do                                                                                                                                                                               \
    {                                                                                                                                                                                \
        auto* pShader  = ValidatedCast<ShaderD3D11Impl>(m_Desc.GraphicsPipeline.p##ShortName);                                                                                       \
        m_p##ShortName = pShader;                                                                                                                                                    \
        if (m_p##ShortName && m_p##ShortName->GetDesc().ShaderType != ExpectedType)                                                                                                  \
        {                                                                                                                                                                            \
            LOG_ERROR_AND_THROW(GetShaderTypeLiteralName(ExpectedType), " shader is expeceted while ", GetShaderTypeLiteralName(m_p##ShortName->GetDesc().ShaderType), " provided"); \
        }                                                                                                                                                                            \
        if (pShader != nullptr)                                                                                                                                                      \
            HashCombine(m_ShaderResourceLayoutHash, pShader->GetD3D11Resources()->GetHash());                                                                                        \
    } while (false)

        INIT_SHADER(VS, SHADER_TYPE_VERTEX);
        INIT_SHADER(PS, SHADER_TYPE_PIXEL);
        INIT_SHADER(GS, SHADER_TYPE_GEOMETRY);
        INIT_SHADER(DS, SHADER_TYPE_DOMAIN);
        INIT_SHADER(HS, SHADER_TYPE_HULL);
#undef INIT_SHADER

        if (m_pVS == nullptr)
        {
            LOG_ERROR_AND_THROW("Vertex shader is null");
        }

        auto* pDeviceD3D11 = pRenderDeviceD3D11->GetD3D11Device();

        D3D11_BLEND_DESC D3D11BSDesc = {};
        BlendStateDesc_To_D3D11_BLEND_DESC(m_Desc.GraphicsPipeline.BlendDesc, D3D11BSDesc);
        CHECK_D3D_RESULT_THROW(pDeviceD3D11->CreateBlendState(&D3D11BSDesc, &m_pd3d11BlendState),
                               "Failed to create D3D11 blend state object");

        D3D11_RASTERIZER_DESC D3D11RSDesc = {};
        RasterizerStateDesc_To_D3D11_RASTERIZER_DESC(m_Desc.GraphicsPipeline.RasterizerDesc, D3D11RSDesc);
        CHECK_D3D_RESULT_THROW(pDeviceD3D11->CreateRasterizerState(&D3D11RSDesc, &m_pd3d11RasterizerState),
                               "Failed to create D3D11 rasterizer state");

        D3D11_DEPTH_STENCIL_DESC D3D11DSSDesc = {};
        DepthStencilStateDesc_To_D3D11_DEPTH_STENCIL_DESC(m_Desc.GraphicsPipeline.DepthStencilDesc, D3D11DSSDesc);
        CHECK_D3D_RESULT_THROW(pDeviceD3D11->CreateDepthStencilState(&D3D11DSSDesc, &m_pd3d11DepthStencilState),
                               "Failed to create D3D11 depth stencil state");

        // Create input layout
        const auto& InputLayout = m_Desc.GraphicsPipeline.InputLayout;
        if (InputLayout.NumElements > 0)
        {
            std::vector<D3D11_INPUT_ELEMENT_DESC, STDAllocatorRawMem<D3D11_INPUT_ELEMENT_DESC>> d311InputElements(STD_ALLOCATOR_RAW_MEM(D3D11_INPUT_ELEMENT_DESC, GetRawAllocator(), "Allocator for vector<D3D11_INPUT_ELEMENT_DESC>"));
            LayoutElements_To_D3D11_INPUT_ELEMENT_DESCs(InputLayout, d311InputElements);

            ID3DBlob* pVSByteCode = m_pVS.RawPtr<ShaderD3D11Impl>()->GetBytecode();
            if (!pVSByteCode)
                LOG_ERROR_AND_THROW("Vertex Shader byte code does not exist");

            CHECK_D3D_RESULT_THROW(pDeviceD3D11->CreateInputLayout(d311InputElements.data(), static_cast<UINT>(d311InputElements.size()), pVSByteCode->GetBufferPointer(), pVSByteCode->GetBufferSize(), &m_pd3d11InputLayout),
                                   "Failed to create the Direct3D11 input layout");
        }
    }

    m_pStaticResourceLayouts = ALLOCATE(GetRawAllocator(), "Raw memory for ShaderResourceLayoutD3D11", ShaderResourceLayoutD3D11, m_NumShaders);
    m_pStaticResourceCaches  = ALLOCATE(GetRawAllocator(), "Raw memory for ShaderResourceCacheD3D11", ShaderResourceCacheD3D11, m_NumShaders);

    const auto& ResourceLayout = m_Desc.ResourceLayout;

#ifdef DEVELOPMENT
    {
        const ShaderResources* pResources[MAX_SHADERS_IN_PIPELINE] = {};
        for (Uint32 s = 0; s < m_NumShaders; ++s)
        {
            auto* pShader = GetShader<const ShaderD3D11Impl>(s);
            pResources[s] = &(*pShader->GetD3D11Resources());
        }
        ShaderResources::DvpVerifyResourceLayout(ResourceLayout, pResources, m_NumShaders);
    }
#endif

    decltype(m_StaticSamplers)                  StaticSamplers(STD_ALLOCATOR_RAW_MEM(StaticSamplerInfo, GetRawAllocator(), "Allocator for vector<StaticSamplerInfo>"));
    std::array<size_t, MAX_SHADERS_IN_PIPELINE> ShaderResLayoutDataSizes = {};
    std::array<size_t, MAX_SHADERS_IN_PIPELINE> ShaderResCacheDataSizes  = {};
    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        auto*       pShader         = GetShader<const ShaderD3D11Impl>(s);
        const auto& ShaderResources = *pShader->GetD3D11Resources();

        new (m_pStaticResourceCaches + s) ShaderResourceCacheD3D11;
        // Do not initialize the cache as this will be performed by the resource layout

        // Shader resource layout will only contain dynamic and mutable variables
        const SHADER_RESOURCE_VARIABLE_TYPE StaticVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_STATIC};
        // clang-format off
        new (m_pStaticResourceLayouts + s)
            ShaderResourceLayoutD3D11
            {
                *this,
                pShader->GetD3D11Resources(),
                m_Desc.ResourceLayout,
                StaticVarTypes,
                _countof(StaticVarTypes),
                m_pStaticResourceCaches[s],
                GetRawAllocator(),
                GetRawAllocator()
            };
        // clang-format on

        // Initialize static samplers
        for (Uint32 sam = 0; sam < ShaderResources.GetNumSamplers(); ++sam)
        {
            const auto&    SamplerAttribs             = ShaderResources.GetSampler(sam);
            constexpr bool LogStaticSamplerArrayError = true;
            auto           SrcStaticSamplerInd        = ShaderResources.FindStaticSampler(SamplerAttribs, ResourceLayout, LogStaticSamplerArrayError);
            if (SrcStaticSamplerInd >= 0)
            {
                const auto&             SrcStaticSamplerInfo = ResourceLayout.StaticSamplers[SrcStaticSamplerInd];
                RefCntAutoPtr<ISampler> pStaticSampler;
                pRenderDeviceD3D11->CreateSampler(SrcStaticSamplerInfo.Desc, &pStaticSampler);
                StaticSamplers.emplace_back(SamplerAttribs, std::move(pStaticSampler));
            }
        }
        m_StaticSamplerOffsets[s + 1] = static_cast<Uint16>(StaticSamplers.size());

        if (m_Desc.SRBAllocationGranularity > 1)
        {
            const SHADER_RESOURCE_VARIABLE_TYPE SRBVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC};
            ShaderResLayoutDataSizes[s]                       = ShaderResourceLayoutD3D11::GetRequiredMemorySize(ShaderResources, ResourceLayout, SRBVarTypes, _countof(SRBVarTypes));
            ShaderResCacheDataSizes[s]                        = ShaderResourceCacheD3D11::GetRequriedMemorySize(ShaderResources);
        }

        auto ShaderInd                   = GetShaderTypeIndex(pShader->GetDesc().ShaderType);
        m_ResourceLayoutIndex[ShaderInd] = static_cast<Int8>(s);
    }

    if (m_Desc.SRBAllocationGranularity > 1)
    {
        m_SRBMemAllocator.Initialize(m_Desc.SRBAllocationGranularity, m_NumShaders, ShaderResLayoutDataSizes.data(), m_NumShaders, ShaderResCacheDataSizes.data());
    }

    m_StaticSamplers.reserve(StaticSamplers.size());
    for (auto& Sam : StaticSamplers)
        m_StaticSamplers.emplace_back(std::move(Sam));

    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        // Initialize static samplers in the static resource cache to avoid warning messages
        SetStaticSamplers(m_pStaticResourceCaches[s], s);
    }
}


PipelineStateD3D11Impl::~PipelineStateD3D11Impl()
{
    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        m_pStaticResourceCaches[s].Destroy(GetRawAllocator());
        m_pStaticResourceCaches[s].~ShaderResourceCacheD3D11();
    }
    GetRawAllocator().Free(m_pStaticResourceCaches);

    for (Uint32 l = 0; l < m_NumShaders; ++l)
    {
        m_pStaticResourceLayouts[l].~ShaderResourceLayoutD3D11();
    }
    GetRawAllocator().Free(m_pStaticResourceLayouts);
}

IMPLEMENT_QUERY_INTERFACE(PipelineStateD3D11Impl, IID_PipelineStateD3D11, TPipelineStateBase)


ID3D11BlendState* PipelineStateD3D11Impl::GetD3D11BlendState()
{
    return m_pd3d11BlendState;
}

ID3D11RasterizerState* PipelineStateD3D11Impl::GetD3D11RasterizerState()
{
    return m_pd3d11RasterizerState;
}

ID3D11DepthStencilState* PipelineStateD3D11Impl::GetD3D11DepthStencilState()
{
    return m_pd3d11DepthStencilState;
}

ID3D11InputLayout* PipelineStateD3D11Impl::GetD3D11InputLayout()
{
    return m_pd3d11InputLayout;
}

void PipelineStateD3D11Impl::CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding, bool InitStaticResources)
{
    auto* pRenderDeviceD3D11 = ValidatedCast<RenderDeviceD3D11Impl>(GetDevice());
    auto& SRBAllocator       = pRenderDeviceD3D11->GetSRBAllocator();
    auto  pShaderResBinding  = NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingD3D11Impl instance", ShaderResourceBindingD3D11Impl)(this, false);
    if (InitStaticResources)
        pShaderResBinding->InitializeStaticResources(nullptr);
    pShaderResBinding->QueryInterface(IID_ShaderResourceBinding, reinterpret_cast<IObject**>(static_cast<IShaderResourceBinding**>(ppShaderResourceBinding)));
}

bool PipelineStateD3D11Impl::IsCompatibleWith(const IPipelineState* pPSO) const
{
    VERIFY_EXPR(pPSO != nullptr);

    if (pPSO == this)
        return true;

    const PipelineStateD3D11Impl* pPSOD3D11 = ValidatedCast<const PipelineStateD3D11Impl>(pPSO);
    if (m_ShaderResourceLayoutHash != pPSOD3D11->m_ShaderResourceLayoutHash)
        return false;

    if (m_NumShaders != pPSOD3D11->m_NumShaders)
        return false;

    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        auto* pShader0 = GetShader<const ShaderD3D11Impl>(s);
        auto* pShader1 = pPSOD3D11->GetShader<const ShaderD3D11Impl>(s);
        if (pShader0->GetDesc().ShaderType != pShader1->GetDesc().ShaderType)
            return false;
        const auto& Res0 = *pShader0->GetD3D11Resources();
        const auto& Res1 = *pShader1->GetD3D11Resources();
        if (!Res0.IsCompatibleWith(Res1))
            return false;
    }

    return true;
}

ID3D11VertexShader* PipelineStateD3D11Impl::GetD3D11VertexShader()
{
    if (!m_pVS) return nullptr;
    auto* pVSD3D11 = m_pVS.RawPtr<ShaderD3D11Impl>();
    return static_cast<ID3D11VertexShader*>(pVSD3D11->GetD3D11Shader());
}

ID3D11PixelShader* PipelineStateD3D11Impl::GetD3D11PixelShader()
{
    if (!m_pPS) return nullptr;
    auto* pPSD3D11 = m_pPS.RawPtr<ShaderD3D11Impl>();
    return static_cast<ID3D11PixelShader*>(pPSD3D11->GetD3D11Shader());
}

ID3D11GeometryShader* PipelineStateD3D11Impl::GetD3D11GeometryShader()
{
    if (!m_pGS) return nullptr;
    auto* pGSD3D11 = m_pGS.RawPtr<ShaderD3D11Impl>();
    return static_cast<ID3D11GeometryShader*>(pGSD3D11->GetD3D11Shader());
}

ID3D11DomainShader* PipelineStateD3D11Impl::GetD3D11DomainShader()
{
    if (!m_pDS) return nullptr;
    auto* pDSD3D11 = m_pDS.RawPtr<ShaderD3D11Impl>();
    return static_cast<ID3D11DomainShader*>(pDSD3D11->GetD3D11Shader());
}

ID3D11HullShader* PipelineStateD3D11Impl::GetD3D11HullShader()
{
    if (!m_pHS) return nullptr;
    auto* pHSD3D11 = m_pHS.RawPtr<ShaderD3D11Impl>();
    return static_cast<ID3D11HullShader*>(pHSD3D11->GetD3D11Shader());
}

ID3D11ComputeShader* PipelineStateD3D11Impl::GetD3D11ComputeShader()
{
    if (!m_pCS) return nullptr;
    auto* pCSD3D11 = m_pCS.RawPtr<ShaderD3D11Impl>();
    return static_cast<ID3D11ComputeShader*>(pCSD3D11->GetD3D11Shader());
}


void PipelineStateD3D11Impl::BindStaticResources(Uint32 ShaderFlags, IResourceMapping* pResourceMapping, Uint32 Flags)
{
    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        auto& StaticResLayout = m_pStaticResourceLayouts[s];
        if ((ShaderFlags & StaticResLayout.GetShaderType()) != 0)
            StaticResLayout.BindResources(pResourceMapping, Flags, m_pStaticResourceCaches[s]);
    }
}

Uint32 PipelineStateD3D11Impl::GetStaticVariableCount(SHADER_TYPE ShaderType) const
{
    const auto LayoutInd = m_ResourceLayoutIndex[GetShaderTypeIndex(ShaderType)];
    if (LayoutInd < 0)
        return 0;

    return m_pStaticResourceLayouts[LayoutInd].GetTotalResourceCount();
}

IShaderResourceVariable* PipelineStateD3D11Impl::GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name)
{
    const auto LayoutInd = m_ResourceLayoutIndex[GetShaderTypeIndex(ShaderType)];
    if (LayoutInd < 0)
        return nullptr;

    return m_pStaticResourceLayouts[LayoutInd].GetShaderVariable(Name);
}

IShaderResourceVariable* PipelineStateD3D11Impl::GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    const auto LayoutInd = m_ResourceLayoutIndex[GetShaderTypeIndex(ShaderType)];
    if (LayoutInd < 0)
        return nullptr;

    return m_pStaticResourceLayouts[LayoutInd].GetShaderVariable(Index);
}

void PipelineStateD3D11Impl::SetStaticSamplers(ShaderResourceCacheD3D11& ResourceCache, Uint32 ShaderInd) const
{
    auto NumCachedSamplers = ResourceCache.GetSamplerCount();
    for (Uint32 s = m_StaticSamplerOffsets[ShaderInd]; s < m_StaticSamplerOffsets[ShaderInd + 1]; ++s)
    {
        auto&       SamplerInfo       = m_StaticSamplers[s];
        const auto& SamAttribs        = SamplerInfo.Attribs;
        auto*       pSamplerD3D11Impl = SamplerInfo.pSampler.RawPtr<SamplerD3D11Impl>();
        // Limiting EndBindPoint is required when initializing static samplers in a Shader's static cache
        auto EndBindPoint = std::min(static_cast<Uint32>(SamAttribs.BindPoint) + SamAttribs.BindCount, NumCachedSamplers);
        for (Uint32 BindPoint = SamAttribs.BindPoint; BindPoint < EndBindPoint; ++BindPoint)
            ResourceCache.SetSampler(BindPoint, pSamplerD3D11Impl);
    }
}

} // namespace Diligent
