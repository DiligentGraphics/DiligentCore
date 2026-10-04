/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include "SuperResolutionProvider.hpp"
#include "SuperResolutionBase.hpp"
#include "SuperResolutionVariants.hpp"

#include "RefCntAutoPtr.hpp"
#include "GraphicsTypesX.hpp"
#include "GraphicsUtilities.h"
#include "ShaderSourceFactoryUtils.h"
#include "CommonlyUsedStates.h"
#include "ShaderMacroHelper.hpp"
#include "BasicMath.hpp"
#include "Dearchiver.h"
#include "EngineMemory.h"

namespace Diligent
{

namespace HLSL
{
#define FFX_CPU
#include "../../ThirdParty/FSR/shaders/ffx_core.h"
#include "../../ThirdParty/FSR/shaders/ffx_fsr1.h"
#undef FFX_CPU

#include "../shaders/FSRStructures.fxh"
#include "FSRShaderList.h"
} // namespace HLSL

namespace
{

void PopulateFSRAttribs(HLSL::FSRAttribs& Attribs, float InputWidth, float InputHeight, float OutputWidth, float OutputHeight, float Sharpness)
{
    const auto ToUint4 = [](const uint32_t(&arr)[4]) {
        return uint4{arr[0], arr[1], arr[2], arr[3]};
    };

    HLSL::FfxUInt32x4 Constant0{}, Constant1{}, Constant2{}, Constant3{};
    HLSL::ffxFsrPopulateEasuConstants(Constant0, Constant1, Constant2, Constant3,
                                      InputWidth, InputHeight,
                                      InputWidth, InputHeight,
                                      OutputWidth, OutputHeight);
    Attribs.EASUConstants0 = ToUint4(Constant0);
    Attribs.EASUConstants1 = ToUint4(Constant1);
    Attribs.EASUConstants2 = ToUint4(Constant2);
    Attribs.EASUConstants3 = ToUint4(Constant3);

    HLSL::FfxUInt32x4 RCASConstant{};
    HLSL::FsrRcasCon(RCASConstant, Sharpness);
    Attribs.RCASConstants = ToUint4(RCASConstant);

    Attribs.SourceSize = float4{InputWidth, InputHeight, 1.0f / InputWidth, 1.0f / InputHeight};
}

class SuperResolutionFSR final : public SuperResolutionBase
{
public:
    SuperResolutionFSR(IReferenceCounters*        pRefCounters,
                       IRenderDevice*             pDevice,
                       const SuperResolutionDesc& Desc,
                       const SuperResolutionInfo& Info,
                       IPipelineState*            pEASU_PSO,
                       IPipelineState*            pRCAS_PSO);

    virtual void DILIGENT_CALL_TYPE Execute(const ExecuteSuperResolutionAttribs& Attribs) override final;

private:
    RefCntAutoPtr<IPipelineState>         m_pEASU_PSO;
    RefCntAutoPtr<IPipelineState>         m_pRCAS_PSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pEASU_SRB;
    RefCntAutoPtr<IShaderResourceBinding> m_pRCAS_SRB;
    RefCntAutoPtr<IBuffer>                m_pConstantBuffer;
    RefCntAutoPtr<ITexture>               m_pIntermediateTexture;
    float                                 m_LastSharpness = -1.0f;
};


SuperResolutionFSR::SuperResolutionFSR(IReferenceCounters*        pRefCounters,
                                       IRenderDevice*             pDevice,
                                       const SuperResolutionDesc& Desc,
                                       const SuperResolutionInfo& Info,
                                       IPipelineState*            pEASU_PSO,
                                       IPipelineState*            pRCAS_PSO) :
    SuperResolutionBase{pRefCounters, Desc, Info},
    m_pEASU_PSO{pEASU_PSO},
    m_pRCAS_PSO{pRCAS_PSO}
{
    VERIFY_SUPER_RESOLUTION(m_Desc.Name, pDevice != nullptr, "Render device must not be null");
    VERIFY_SUPER_RESOLUTION(m_Desc.Name, m_pEASU_PSO != nullptr, "FSR EASU pipeline state must not be null");
    VERIFY_SUPER_RESOLUTION(m_Desc.Name, m_pRCAS_PSO != nullptr, "FSR RCAS pipeline state must not be null");

    {
        const float InputWidth   = static_cast<float>(Desc.InputWidth);
        const float InputHeight  = static_cast<float>(Desc.InputHeight);
        const float OutputWidth  = static_cast<float>(Desc.OutputWidth);
        const float OutputHeight = static_cast<float>(Desc.OutputHeight);

        HLSL::FSRAttribs DefaultAttribs{};
        PopulateFSRAttribs(DefaultAttribs, InputWidth, InputHeight, OutputWidth, OutputHeight, 1.0f);

        CreateUniformBuffer(pDevice, sizeof(HLSL::FSRAttribs), "FSR::ConstantBuffer", &m_pConstantBuffer, USAGE_DEFAULT, BIND_UNIFORM_BUFFER, CPU_ACCESS_NONE, &DefaultAttribs);
    }

    const bool SharpeningEnabled = (m_Desc.Flags & SUPER_RESOLUTION_FLAG_ENABLE_SHARPENING) != 0;

    if (SharpeningEnabled)
    {
        TextureDesc TexDesc;
        TexDesc.Name      = "FSR::EASU Output";
        TexDesc.Type      = RESOURCE_DIM_TEX_2D;
        TexDesc.Width     = Desc.OutputWidth;
        TexDesc.Height    = Desc.OutputHeight;
        TexDesc.Format    = m_Desc.OutputFormat;
        TexDesc.MipLevels = 1;
        TexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
        pDevice->CreateTexture(TexDesc, nullptr, &m_pIntermediateTexture);
    }

    // Initialize SRBs and bind mutable resources
    {
        m_pEASU_PSO->CreateShaderResourceBinding(&m_pEASU_SRB, true);
        ShaderResourceVariableX{m_pEASU_SRB, SHADER_TYPE_PIXEL, "cbFSRAttribs"}.Set(m_pConstantBuffer);

        if (SharpeningEnabled)
        {
            m_pRCAS_PSO->CreateShaderResourceBinding(&m_pRCAS_SRB, true);
            ShaderResourceVariableX{m_pRCAS_SRB, SHADER_TYPE_PIXEL, "cbFSRAttribs"}.Set(m_pConstantBuffer);
            ShaderResourceVariableX{m_pRCAS_SRB, SHADER_TYPE_PIXEL, "g_TextureSource"}.Set(m_pIntermediateTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        }
    }
}

void SuperResolutionFSR::Execute(const ExecuteSuperResolutionAttribs& Attribs)
{
    ValidateExecuteSuperResolutionAttribs(m_Desc, m_Info, Attribs);

    IDeviceContext* pContext = Attribs.pContext;

    const bool SharpeningEnabled = (m_Desc.Flags & SUPER_RESOLUTION_FLAG_ENABLE_SHARPENING) != 0;

    if (SharpeningEnabled && m_LastSharpness != Attribs.Sharpness)
    {
        m_LastSharpness = Attribs.Sharpness;

        HLSL::FSRAttribs FSRAttribs{};
        PopulateFSRAttribs(FSRAttribs,
                           static_cast<float>(m_Desc.InputWidth), static_cast<float>(m_Desc.InputHeight),
                           static_cast<float>(m_Desc.OutputWidth), static_cast<float>(m_Desc.OutputHeight),
                           Attribs.Sharpness);
        pContext->UpdateBuffer(m_pConstantBuffer, 0, sizeof(HLSL::FSRAttribs), &FSRAttribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    {
        ITextureView* pEASU_RTV = SharpeningEnabled ? m_pIntermediateTexture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) : Attribs.pOutputTextureView;
        ITextureView* pRTVs[]   = {pEASU_RTV};
        pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->SetPipelineState(m_pEASU_PSO);

        ShaderResourceVariableX{m_pEASU_SRB, SHADER_TYPE_PIXEL, "g_TextureSource"}.Set(Attribs.pColorTextureSRV);

        pContext->CommitShaderResources(m_pEASU_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
    }

    if (SharpeningEnabled)
    {
        ITextureView* pRTVs[] = {Attribs.pOutputTextureView};
        pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->SetPipelineState(m_pRCAS_PSO);
        pContext->CommitShaderResources(m_pRCAS_SRB, Attribs.StateTransitionMode);
        pContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
    }

    pContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
}


class FSRProvider final : public SuperResolutionProvider
{
public:
    FSRProvider(IRenderDevice* pDevice, const SuperResolutionFSRCreateInfo* pCreateInfo);

    virtual void EnumerateVariants(std::vector<SuperResolutionInfo>& Variants) override final
    {
        SuperResolutionInfo Info{};
        Info.VariantId = VariantId_FSRSpatial;
        snprintf(Info.Name, sizeof(Info.Name), "Software: FSR Spatial");
        Info.Type            = SUPER_RESOLUTION_TYPE_SPATIAL;
        Info.SpatialCapFlags = SUPER_RESOLUTION_SPATIAL_CAP_FLAG_SHARPNESS;
        Variants.push_back(Info);
    }

    virtual void CreateSuperResolution(const SuperResolutionDesc& Desc, const SuperResolutionInfo& Info, ISuperResolution** ppUpscaler) override final
    {
        DEV_CHECK_ERR(ppUpscaler != nullptr, "ppUpscaler must not be null");

        const auto* pPipelines = GetOrCreatePipelines(Desc.OutputFormat);
        if (pPipelines == nullptr)
            return;

        auto* pUpscaler = NEW_RC_OBJ(GetRawAllocator(), "SuperResolutionFSR instance", SuperResolutionFSR)(m_pDevice, Desc, Info, pPipelines->pEASU_PSO, pPipelines->pRCAS_PSO);
        pUpscaler->QueryInterface(IID_SuperResolution, reinterpret_cast<IObject**>(ppUpscaler));
    }

private:
    struct PipelineData
    {
        RefCntAutoPtr<IPipelineState> pEASU_PSO;
        RefCntAutoPtr<IPipelineState> pRCAS_PSO;
    };

    struct ArchivedPipelineInfo
    {
        explicit ArchivedPipelineInfo(const SuperResolutionFSRCreateInfo& CI) :
            pDearchiver{CI.pDearchiver},
            EASUPSOName{CI.EASUPSOName},
            RCASPSOName{CI.RCASPSOName}
        {}

        RefCntAutoPtr<IDearchiver> pDearchiver;
        std::string                EASUPSOName;
        std::string                RCASPSOName;
    };

    PipelineData* GetOrCreatePipelines(TEXTURE_FORMAT OutputFormat);

    RefCntAutoPtr<IRenderDevice>                                        m_pDevice;
    RefCntAutoPtr<IShaderSourceInputStreamFactory>                      m_pShaderSourceFactory;
    RefCntAutoPtr<IShader>                                              m_pVS;
    RefCntAutoPtr<IShader>                                              m_pEASU_PS;
    RefCntAutoPtr<IShader>                                              m_pRCAS_PS;
    std::unique_ptr<ArchivedPipelineInfo>                               m_pArchivedPipelineInfo;
    std::unordered_map<TEXTURE_FORMAT, PipelineData, std::hash<Uint32>> m_PipelineCache;
};


FSRProvider::FSRProvider(IRenderDevice* pDevice, const SuperResolutionFSRCreateInfo* pCreateInfo) :
    m_pDevice{pDevice}
{
    if (pDevice == nullptr)
        LOG_ERROR_AND_THROW("Device must not be null");

    if (pCreateInfo != nullptr)
    {
        if (pCreateInfo->pDearchiver == nullptr)
            LOG_ERROR_AND_THROW("FSR dearchiver must not be null");
        if (pCreateInfo->EASUPSOName == nullptr || pCreateInfo->EASUPSOName[0] == '\0')
            LOG_ERROR_AND_THROW("FSR EASU pipeline state name must not be null or empty");
        if (pCreateInfo->RCASPSOName == nullptr || pCreateInfo->RCASPSOName[0] == '\0')
            LOG_ERROR_AND_THROW("FSR RCAS pipeline state name must not be null or empty");

        m_pArchivedPipelineInfo = std::make_unique<ArchivedPipelineInfo>(*pCreateInfo);
        return;
    }

    MemoryShaderSourceFactoryCreateInfo CI{HLSL::g_Shaders, _countof(HLSL::g_Shaders)};
    CreateMemoryShaderSourceFactory(CI, &m_pShaderSourceFactory);

    ShaderMacroHelper Macros;
    if (m_pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_GLES)
        Macros.AddShaderMacro("FSR_FEATURE_TEXTURE_GATHER", 1);

    auto CreateShader = [&](SHADER_TYPE Type, const char* Name, const char* EntryPoint, const char* FilePath, const ShaderMacroArray& ShaderMacros = {}) {
        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.Desc.ShaderType                 = Type;
        ShaderCI.Desc.Name                       = Name;
        ShaderCI.Desc.UseCombinedTextureSamplers = true;
        ShaderCI.EntryPoint                      = EntryPoint;
        ShaderCI.FilePath                        = FilePath;
        ShaderCI.Macros                          = ShaderMacros;
        ShaderCI.pShaderSourceStreamFactory      = m_pShaderSourceFactory;
        RefCntAutoPtr<IShader> pShader;
        m_pDevice->CreateShader(ShaderCI, &pShader);
        return pShader;
    };

    m_pVS      = CreateShader(SHADER_TYPE_VERTEX, "FSR FullQuad VS", "FSR_FullQuadVS", "FSR_FullQuad.fx");
    m_pEASU_PS = CreateShader(SHADER_TYPE_PIXEL, "FSR EASU PS", "ComputeEdgeAdaptiveUpsamplingPS", "FSR_EdgeAdaptiveUpsampling.fx", Macros);
    m_pRCAS_PS = CreateShader(SHADER_TYPE_PIXEL, "FSR RCAS PS", "ComputeContrastAdaptiveSharpeningPS", "FSR_ContrastAdaptiveSharpening.fx");

    if (!m_pVS || !m_pEASU_PS || !m_pRCAS_PS)
        LOG_ERROR_AND_THROW("Failed to create FSR shaders");
}

FSRProvider::PipelineData* FSRProvider::GetOrCreatePipelines(TEXTURE_FORMAT OutputFormat)
{
    auto It = m_PipelineCache.find(OutputFormat);
    if (It != m_PipelineCache.end())
        return &It->second;

    PipelineData Data;

    if (m_pArchivedPipelineInfo)
    {
        auto ModifyPipelineStateCreateInfo = [](PipelineStateCreateInfo& PipelineCI, void* pUserData) {
            auto& GraphicsPipelineCI                          = static_cast<GraphicsPipelineStateCreateInfo&>(PipelineCI);
            GraphicsPipelineCI.GraphicsPipeline.RTVFormats[0] = *static_cast<const TEXTURE_FORMAT*>(pUserData);
        };

        PipelineStateUnpackInfo UnpackInfo;
        UnpackInfo.pDevice                       = m_pDevice;
        UnpackInfo.PipelineType                  = PIPELINE_TYPE_GRAPHICS;
        UnpackInfo.ModifyPipelineStateCreateInfo = ModifyPipelineStateCreateInfo;
        UnpackInfo.pUserData                     = &OutputFormat;

        UnpackInfo.Name = m_pArchivedPipelineInfo->EASUPSOName.c_str();
        m_pArchivedPipelineInfo->pDearchiver->UnpackPipelineState(UnpackInfo, &Data.pEASU_PSO);

        UnpackInfo.Name = m_pArchivedPipelineInfo->RCASPSOName.c_str();
        m_pArchivedPipelineInfo->pDearchiver->UnpackPipelineState(UnpackInfo, &Data.pRCAS_PSO);
    }
    else
    {
        {
            PipelineResourceLayoutDescX ResourceLayout;
            ResourceLayout
                .SetDefaultVariableType(SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                .AddVariable(SHADER_TYPE_PIXEL, "cbFSRAttribs", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
                .AddImmutableSampler(SHADER_TYPE_PIXEL, "g_TextureSource", Sam_PointClamp);

            GraphicsPipelineStateCreateInfoX PSOCreateInfo{"FSR::EASU PSO"};
            PSOCreateInfo
                .AddShader(m_pVS)
                .AddShader(m_pEASU_PS)
                .AddRenderTarget(OutputFormat)
                .SetRasterizerDesc(RasterizerStateDesc{FILL_MODE_SOLID, CULL_MODE_NONE})
                .SetDepthStencilDesc(DepthStencilStateDesc{False, False})
                .SetResourceLayout(ResourceLayout);

            m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &Data.pEASU_PSO);
        }

        {
            PipelineResourceLayoutDescX ResourceLayout;
            ResourceLayout
                .SetDefaultVariableType(SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                .AddVariable(SHADER_TYPE_PIXEL, "cbFSRAttribs", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
                .AddVariable(SHADER_TYPE_PIXEL, "g_TextureSource", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE);

            GraphicsPipelineStateCreateInfoX PSOCreateInfo{"FSR::RCAS PSO"};
            PSOCreateInfo
                .AddShader(m_pVS)
                .AddShader(m_pRCAS_PS)
                .AddRenderTarget(OutputFormat)
                .SetRasterizerDesc(RasterizerStateDesc{FILL_MODE_SOLID, CULL_MODE_NONE})
                .SetDepthStencilDesc(DepthStencilStateDesc{False, False})
                .SetResourceLayout(ResourceLayout);

            m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &Data.pRCAS_PSO);
        }
    }

    if (!Data.pEASU_PSO || !Data.pRCAS_PSO)
    {
        LOG_ERROR_MESSAGE("Failed to create FSR pipeline states");
        return nullptr;
    }

    return &m_PipelineCache.emplace(OutputFormat, std::move(Data)).first->second;
}

} // anonymous namespace


std::unique_ptr<SuperResolutionProvider> CreateFSRProvider(IRenderDevice* pDevice, const SuperResolutionFSRCreateInfo* pCreateInfo)
{
    return std::make_unique<FSRProvider>(pDevice, pCreateInfo);
}

} // namespace Diligent
