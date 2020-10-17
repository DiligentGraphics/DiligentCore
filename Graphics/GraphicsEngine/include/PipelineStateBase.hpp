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

#pragma once

/// \file
/// Implementation of the Diligent::PipelineStateBase template class

#include <array>
#include <vector>

#include "PipelineState.h"
#include "DeviceObjectBase.hpp"
#include "STDAllocator.hpp"
#include "EngineMemory.h"
#include "GraphicsAccessories.hpp"
#include "LinearAllocator.hpp"

namespace Diligent
{

/// Template class implementing base functionality for a pipeline state object.

/// \tparam BaseInterface - base interface that this class will inheret
///                         (Diligent::IPipelineStateD3D11, Diligent::IPipelineStateD3D12,
///                          Diligent::IPipelineStateGL or Diligent::IPipelineStateVk).
/// \tparam RenderDeviceImplType - type of the render device implementation
///                                (Diligent::RenderDeviceD3D11Impl, Diligent::RenderDeviceD3D12Impl,
///                                 Diligent::RenderDeviceGLImpl, or Diligent::RenderDeviceVkImpl)
template <class BaseInterface, class RenderDeviceImplType>
class PipelineStateBase : public DeviceObjectBase<BaseInterface, RenderDeviceImplType, PipelineStateDesc>
{
public:
    using TDeviceObjectBase = DeviceObjectBase<BaseInterface, RenderDeviceImplType, PipelineStateDesc>;

    /// \param pRefCounters - reference counters object that controls the lifetime of this PSO
    /// \param pDevice - pointer to the device.
    /// \param CreateInfo - graphics pipeline state create info.
    /// \param bIsDeviceInternal - flag indicating if the pipeline state is an internal device object and
    ///							   must not keep a strong reference to the device.
    PipelineStateBase(IReferenceCounters*      pRefCounters,
                      RenderDeviceImplType*    pDevice,
                      const PipelineStateDesc& PSODesc,
                      bool                     bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, PSODesc, bIsDeviceInternal}
    {
        Uint64 DeviceQueuesMask = pDevice->GetCommandQueueMask();
        DEV_CHECK_ERR((this->m_Desc.CommandQueueMask & DeviceQueuesMask) != 0,
                      "No bits in the command queue mask (0x", std::hex, this->m_Desc.CommandQueueMask,
                      ") correspond to one of ", pDevice->GetCommandQueueCount(), " available device command queues");
        this->m_Desc.CommandQueueMask &= DeviceQueuesMask;
    }

    ~PipelineStateBase()
    {
        /*
        /// \note Destructor cannot directly remove the object from the registry as this may cause a  
        ///       deadlock at the point where StateObjectsRegistry::Find() locks the weak pointer: if we
        ///       are in dtor, the object is locked by Diligent::RefCountedObject::Release() and 
        ///       StateObjectsRegistry::Find() will wait for that lock to be released.
        ///       A the same time this thread will be waiting for the other thread to unlock the registry.\n
        ///       Thus destructor only notifies the registry that there is a deleted object.
        ///       The reference to the object will be removed later.
        auto &PipelineStateRegistry = static_cast<TRenderDeviceBase*>(this->GetDevice())->GetBSRegistry();
        auto &RasterizerStateRegistry = static_cast<TRenderDeviceBase*>(this->GetDevice())->GetRSRegistry();
        auto &DSSRegistry = static_cast<TRenderDeviceBase*>(this->GetDevice())->GetDSSRegistry();
        // StateObjectsRegistry::ReportDeletedObject() does not lock the registry, but only 
        // atomically increments the outstanding deleted objects counter.
        PipelineStateRegistry.ReportDeletedObject();
        RasterizerStateRegistry.ReportDeletedObject();
        DSSRegistry.ReportDeletedObject();
        */
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_PipelineState, TDeviceObjectBase)

    Uint32 GetBufferStride(Uint32 BufferSlot) const
    {
        return BufferSlot < m_BufferSlotsUsed ? m_pStrides[BufferSlot] : 0;
    }

    Uint32 GetNumBufferSlotsUsed() const
    {
        return m_BufferSlotsUsed;
    }

    SHADER_TYPE GetShaderStageType(Uint32 Stage) const { return m_ShaderStageTypes[Stage]; }
    Uint32      GetNumShaderStages() const { return m_NumShaderStages; }

    // This function only compares shader resource layout hashes, so
    // it can potentially give false negatives
    bool IsIncompatibleWith(const IPipelineState* pPSO) const
    {
        return m_ShaderResourceLayoutHash != ValidatedCast<const PipelineStateBase>(pPSO)->m_ShaderResourceLayoutHash;
    }

    virtual const GraphicsPipelineDesc& DILIGENT_CALL_TYPE GetGraphicsPipelineDesc() const override final
    {
        VERIFY_EXPR(this->m_Desc.IsAnyGraphicsPipeline());
        VERIFY_EXPR(m_pGraphicsPipelineDesc != nullptr);
        return *m_pGraphicsPipelineDesc;
    }


protected:
    size_t m_ShaderResourceLayoutHash = 0; ///< Hash computed from the shader resource layout

    Uint32* m_pStrides        = nullptr;
    Uint8   m_BufferSlotsUsed = 0;

    Uint8 m_NumShaderStages = 0; ///< Number of shader stages in this PSO

    /// Array of shader types for every shader stage used by this PSO
    std::array<SHADER_TYPE, MAX_SHADERS_IN_PIPELINE> m_ShaderStageTypes = {};

    RefCntAutoPtr<IRenderPass> m_pRenderPass; ///< Strong reference to the render pass object

    GraphicsPipelineDesc* m_pGraphicsPipelineDesc = nullptr;

protected:
#define LOG_PSO_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Description of ", GetPipelineTypeString(this->m_Desc.PipelineType), " PSO '", this->m_Desc.Name, "' is invalid: ", ##__VA_ARGS__)

    Int8 GetStaticVariableCountHelper(SHADER_TYPE ShaderType, const std::array<Int8, MAX_SHADERS_IN_PIPELINE>& ResourceLayoutIndex) const
    {
        if (!IsConsistentShaderType(ShaderType, this->m_Desc.PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to get the number of static variables in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(this->m_Desc.PipelineType), " pipeline '", this->m_Desc.Name, "'");
            return -1;
        }

        const auto ShaderTypeInd = GetShaderTypePipelineIndex(ShaderType, this->m_Desc.PipelineType);
        const auto LayoutInd     = ResourceLayoutIndex[ShaderTypeInd];
        if (LayoutInd < 0)
        {
            LOG_WARNING_MESSAGE("Unable to get the number of static variables in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is inactive in PSO '", this->m_Desc.Name, "'");
        }

        return LayoutInd;
    }

    Int8 GetStaticVariableByNameHelper(SHADER_TYPE ShaderType, const Char* Name, const std::array<Int8, MAX_SHADERS_IN_PIPELINE>& ResourceLayoutIndex) const
    {
        if (!IsConsistentShaderType(ShaderType, this->m_Desc.PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to find static variable '", Name, "' in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(this->m_Desc.PipelineType), " pipeline '", this->m_Desc.Name, "'");
            return -1;
        }

        const auto ShaderTypeInd = GetShaderTypePipelineIndex(ShaderType, this->m_Desc.PipelineType);
        const auto LayoutInd     = ResourceLayoutIndex[ShaderTypeInd];
        if (LayoutInd < 0)
        {
            LOG_WARNING_MESSAGE("Unable to find static variable '", Name, "' in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is inactive in PSO '", this->m_Desc.Name, "'");
        }

        return LayoutInd;
    }

    Int8 GetStaticVariableByIndexHelper(SHADER_TYPE ShaderType, Uint32 Index, const std::array<Int8, MAX_SHADERS_IN_PIPELINE>& ResourceLayoutIndex) const
    {
        if (!IsConsistentShaderType(ShaderType, this->m_Desc.PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to get static variable at index ", Index, " in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(this->m_Desc.PipelineType), " pipeline '", this->m_Desc.Name, "'");
            return -1;
        }

        const auto ShaderTypeInd = GetShaderTypePipelineIndex(ShaderType, this->m_Desc.PipelineType);
        const auto LayoutInd     = ResourceLayoutIndex[ShaderTypeInd];
        if (LayoutInd < 0)
        {
            LOG_WARNING_MESSAGE("Unable to get static variable at index ", Index, " in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is inactive in PSO '", this->m_Desc.Name, "'");
        }

        return LayoutInd;
    }

private:
    void CheckRasterizerStateDesc(GraphicsPipelineDesc& GraphicsPipeline) const
    {
        const auto& RSDesc = GraphicsPipeline.RasterizerDesc;
        if (RSDesc.FillMode == FILL_MODE_UNDEFINED)
            LOG_PSO_ERROR_AND_THROW("RasterizerDesc.FillMode must not be FILL_MODE_UNDEFINED");
        if (RSDesc.CullMode == CULL_MODE_UNDEFINED)
            LOG_PSO_ERROR_AND_THROW("RasterizerDesc.CullMode must not be CULL_MODE_UNDEFINED");
    }

    void CheckAndCorrectDepthStencilDesc(GraphicsPipelineDesc& GraphicsPipeline) const
    {
        auto& DSSDesc = GraphicsPipeline.DepthStencilDesc;
        if (DSSDesc.DepthFunc == COMPARISON_FUNC_UNKNOWN)
        {
            if (DSSDesc.DepthEnable)
                LOG_PSO_ERROR_AND_THROW("DepthStencilDesc.DepthFunc must not be COMPARISON_FUNC_UNKNOWN when depth is enabled");
            else
                DSSDesc.DepthFunc = DepthStencilStateDesc{}.DepthFunc;
        }

        auto CheckAndCorrectStencilOpDesc = [&](StencilOpDesc& OpDesc, const char* FaceName) //
        {
            if (DSSDesc.StencilEnable)
            {
                if (OpDesc.StencilFailOp == STENCIL_OP_UNDEFINED)
                    LOG_PSO_ERROR_AND_THROW("DepthStencilDesc.", FaceName, ".StencilFailOp must not be STENCIL_OP_UNDEFINED when stencil is enabled");
                if (OpDesc.StencilDepthFailOp == STENCIL_OP_UNDEFINED)
                    LOG_PSO_ERROR_AND_THROW("DepthStencilDesc.", FaceName, ".StencilDepthFailOp must not be STENCIL_OP_UNDEFINED when stencil is enabled");
                if (OpDesc.StencilPassOp == STENCIL_OP_UNDEFINED)
                    LOG_PSO_ERROR_AND_THROW("DepthStencilDesc.", FaceName, ".StencilPassOp must not be STENCIL_OP_UNDEFINED when stencil is enabled");
                if (OpDesc.StencilFunc == COMPARISON_FUNC_UNKNOWN)
                    LOG_PSO_ERROR_AND_THROW("DepthStencilDesc.", FaceName, ".StencilFunc must not be COMPARISON_FUNC_UNKNOWN when stencil is enabled");
            }
            else
            {
                if (OpDesc.StencilFailOp == STENCIL_OP_UNDEFINED)
                    OpDesc.StencilFailOp = StencilOpDesc{}.StencilFailOp;
                if (OpDesc.StencilDepthFailOp == STENCIL_OP_UNDEFINED)
                    OpDesc.StencilDepthFailOp = StencilOpDesc{}.StencilDepthFailOp;
                if (OpDesc.StencilPassOp == STENCIL_OP_UNDEFINED)
                    OpDesc.StencilPassOp = StencilOpDesc{}.StencilPassOp;
                if (OpDesc.StencilFunc == COMPARISON_FUNC_UNKNOWN)
                    OpDesc.StencilFunc = StencilOpDesc{}.StencilFunc;
            }
        };
        CheckAndCorrectStencilOpDesc(DSSDesc.FrontFace, "FrontFace");
        CheckAndCorrectStencilOpDesc(DSSDesc.BackFace, "BackFace");
    }

    void CheckAndCorrectBlendStateDesc(GraphicsPipelineDesc& GraphicsPipeline) const
    {
        auto& BlendDesc = GraphicsPipeline.BlendDesc;
        for (Uint32 rt = 0; rt < MAX_RENDER_TARGETS; ++rt)
        {
            auto& RTDesc = BlendDesc.RenderTargets[rt];
            // clang-format off
            const auto  BlendEnable   = RTDesc.BlendEnable          && (rt == 0 || (BlendDesc.IndependentBlendEnable && rt > 0));
            const auto  LogicOpEnable = RTDesc.LogicOperationEnable && (rt == 0 || (BlendDesc.IndependentBlendEnable && rt > 0));
            // clang-format on
            if (BlendEnable)
            {
                if (RTDesc.SrcBlend == BLEND_FACTOR_UNDEFINED)
                    LOG_PSO_ERROR_AND_THROW("BlendDesc.RenderTargets[", rt, "].SrcBlend must not be BLEND_FACTOR_UNDEFINED");
                if (RTDesc.DestBlend == BLEND_FACTOR_UNDEFINED)
                    LOG_PSO_ERROR_AND_THROW("BlendDesc.RenderTargets[", rt, "].DestBlend must not be BLEND_FACTOR_UNDEFINED");
                if (RTDesc.BlendOp == BLEND_OPERATION_UNDEFINED)
                    LOG_PSO_ERROR_AND_THROW("BlendDesc.RenderTargets[", rt, "].BlendOp must not be BLEND_OPERATION_UNDEFINED");

                if (RTDesc.SrcBlendAlpha == BLEND_FACTOR_UNDEFINED)
                    LOG_PSO_ERROR_AND_THROW("BlendDesc.RenderTargets[", rt, "].SrcBlendAlpha must not be BLEND_FACTOR_UNDEFINED");
                if (RTDesc.DestBlendAlpha == BLEND_FACTOR_UNDEFINED)
                    LOG_PSO_ERROR_AND_THROW("BlendDesc.RenderTargets[", rt, "].DestBlendAlpha must not be BLEND_FACTOR_UNDEFINED");
                if (RTDesc.BlendOpAlpha == BLEND_OPERATION_UNDEFINED)
                    LOG_PSO_ERROR_AND_THROW("BlendDesc.RenderTargets[", rt, "].BlendOpAlpha must not be BLEND_OPERATION_UNDEFINED");
            }
            else
            {
                if (RTDesc.SrcBlend == BLEND_FACTOR_UNDEFINED)
                    RTDesc.SrcBlend = RenderTargetBlendDesc{}.SrcBlend;
                if (RTDesc.DestBlend == BLEND_FACTOR_UNDEFINED)
                    RTDesc.DestBlend = RenderTargetBlendDesc{}.DestBlend;
                if (RTDesc.BlendOp == BLEND_OPERATION_UNDEFINED)
                    RTDesc.BlendOp = RenderTargetBlendDesc{}.BlendOp;

                if (RTDesc.SrcBlendAlpha == BLEND_FACTOR_UNDEFINED)
                    RTDesc.SrcBlendAlpha = RenderTargetBlendDesc{}.SrcBlendAlpha;
                if (RTDesc.DestBlendAlpha == BLEND_FACTOR_UNDEFINED)
                    RTDesc.DestBlendAlpha = RenderTargetBlendDesc{}.DestBlendAlpha;
                if (RTDesc.BlendOpAlpha == BLEND_OPERATION_UNDEFINED)
                    RTDesc.BlendOpAlpha = RenderTargetBlendDesc{}.BlendOpAlpha;
            }

            if (!LogicOpEnable)
                RTDesc.LogicOp = RenderTargetBlendDesc{}.LogicOp;
        }
    }

    void ValidateResourceLayout(const PipelineResourceLayoutDesc& SrcLayout, LinearAllocator& MemPool) const
    {
        if (SrcLayout.Variables != nullptr)
        {
            MemPool.AddRequiredSize<ShaderResourceVariableDesc>(SrcLayout.NumVariables);
            for (Uint32 i = 0; i < SrcLayout.NumVariables; ++i)
            {
                VERIFY(SrcLayout.Variables[i].Name != nullptr, "Variable name can't be null");
                MemPool.AddRequiredSize<Char>(strlen(SrcLayout.Variables[i].Name) + 1);
            }
        }

        if (SrcLayout.StaticSamplers != nullptr)
        {
            MemPool.AddRequiredSize<StaticSamplerDesc>(SrcLayout.NumStaticSamplers);
            for (Uint32 i = 0; i < SrcLayout.NumStaticSamplers; ++i)
            {
                VERIFY(SrcLayout.StaticSamplers[i].SamplerOrTextureName != nullptr, "Static sampler or texture name can't be null");
                MemPool.AddRequiredSize<Char>(strlen(SrcLayout.StaticSamplers[i].SamplerOrTextureName) + 1);
            }
        }
    }

    void ValidateGraphicsPipeline(const GraphicsPipelineDesc& GraphicsPipeline, LinearAllocator& MemPool) const
    {
        if (GraphicsPipeline.pRenderPass != nullptr)
        {
            if (GraphicsPipeline.NumRenderTargets != 0)
                LOG_PSO_ERROR_AND_THROW("NumRenderTargets must be 0 when explicit render pass is used");
            if (GraphicsPipeline.DSVFormat != TEX_FORMAT_UNKNOWN)
                LOG_PSO_ERROR_AND_THROW("DSVFormat must be TEX_FORMAT_UNKNOWN when explicit render pass is used");

            for (Uint32 rt = 0; rt < MAX_RENDER_TARGETS; ++rt)
            {
                if (GraphicsPipeline.RTVFormats[rt] != TEX_FORMAT_UNKNOWN)
                    LOG_PSO_ERROR_AND_THROW("RTVFormats[", rt, "] must be TEX_FORMAT_UNKNOWN when explicit render pass is used");
            }

            const auto& RPDesc = GraphicsPipeline.pRenderPass->GetDesc();
            if (GraphicsPipeline.SubpassIndex >= RPDesc.SubpassCount)
                LOG_PSO_ERROR_AND_THROW("Subpass index (", Uint32{GraphicsPipeline.SubpassIndex}, ") exceeds the number of subpasses (", Uint32{RPDesc.SubpassCount}, ") in render pass '", RPDesc.Name, "'");
        }
        else
        {
            if (GraphicsPipeline.SubpassIndex != 0)
                LOG_PSO_ERROR_AND_THROW("Subpass index (", Uint32{GraphicsPipeline.SubpassIndex}, ") must be 0 when explicit render pass is not used");
        }

        const auto& InputLayout     = GraphicsPipeline.InputLayout;
        Uint32      BufferSlotsUsed = 0;
        MemPool.AddRequiredSize<LayoutElement>(InputLayout.NumElements);
        for (Uint32 i = 0; i < InputLayout.NumElements; ++i)
        {
            auto& LayoutElem = InputLayout.LayoutElements[i];
            MemPool.AddRequiredSize<Char>(strlen(LayoutElem.HLSLSemantic) + 1);
            BufferSlotsUsed = std::max(BufferSlotsUsed, LayoutElem.BufferSlot + 1);
        }

        MemPool.AddRequiredSize<Uint32>(BufferSlotsUsed);
    }

    void InitResourceLayout(const PipelineResourceLayoutDesc& SrcLayout, PipelineResourceLayoutDesc& DstLayout, LinearAllocator& MemPool) const
    {
        if (SrcLayout.Variables != nullptr)
        {
            auto* Variables     = MemPool.Allocate<ShaderResourceVariableDesc>(SrcLayout.NumVariables);
            DstLayout.Variables = Variables;
            for (Uint32 i = 0; i < SrcLayout.NumVariables; ++i)
            {
                Variables[i]      = SrcLayout.Variables[i];
                Variables[i].Name = MemPool.CopyString(SrcLayout.Variables[i].Name);
            }
        }

        if (SrcLayout.StaticSamplers != nullptr)
        {
            auto* StaticSamplers     = MemPool.Allocate<StaticSamplerDesc>(SrcLayout.NumStaticSamplers);
            DstLayout.StaticSamplers = StaticSamplers;
            for (Uint32 i = 0; i < SrcLayout.NumStaticSamplers; ++i)
            {
#ifdef DILIGENT_DEVELOPMENT
                {
                    const auto& BorderColor = SrcLayout.StaticSamplers[i].Desc.BorderColor;
                    if (!((BorderColor[0] == 0 && BorderColor[1] == 0 && BorderColor[2] == 0 && BorderColor[3] == 0) ||
                          (BorderColor[0] == 0 && BorderColor[1] == 0 && BorderColor[2] == 0 && BorderColor[3] == 1) ||
                          (BorderColor[0] == 1 && BorderColor[1] == 1 && BorderColor[2] == 1 && BorderColor[3] == 1)))
                    {
                        LOG_WARNING_MESSAGE("Static sampler for variable \"", SrcLayout.StaticSamplers[i].SamplerOrTextureName, "\" specifies border color (",
                                            BorderColor[0], ", ", BorderColor[1], ", ", BorderColor[2], ", ", BorderColor[3],
                                            "). D3D12 static samplers only allow transparent black (0,0,0,0), opaque black (0,0,0,1) or opaque white (1,1,1,1) as border colors");
                    }
                }
#endif

                StaticSamplers[i]                      = SrcLayout.StaticSamplers[i];
                StaticSamplers[i].SamplerOrTextureName = MemPool.CopyString(SrcLayout.StaticSamplers[i].SamplerOrTextureName);
            }
        }
    }

protected:
#define VALIDATE_SHADER_TYPE(Shader, ExpectedType, ShaderName)                                                                           \
    if (Shader && Shader->GetDesc().ShaderType != ExpectedType)                                                                          \
    {                                                                                                                                    \
        LOG_ERROR_AND_THROW(GetShaderTypeLiteralName(Shader->GetDesc().ShaderType), " is not a valid type for ", ShaderName, " shader"); \
    }

    void ValidateAndReserveSpace(const GraphicsPipelineStateCreateInfo& CreateInfo,
                                 LinearAllocator&                       MemPool) const
    {
        VALIDATE_SHADER_TYPE(CreateInfo.pVS, SHADER_TYPE_VERTEX, "vertex")
        VALIDATE_SHADER_TYPE(CreateInfo.pPS, SHADER_TYPE_PIXEL, "pixel")
        VALIDATE_SHADER_TYPE(CreateInfo.pGS, SHADER_TYPE_GEOMETRY, "geometry")
        VALIDATE_SHADER_TYPE(CreateInfo.pHS, SHADER_TYPE_HULL, "hull")
        VALIDATE_SHADER_TYPE(CreateInfo.pDS, SHADER_TYPE_DOMAIN, "domain")
        VALIDATE_SHADER_TYPE(CreateInfo.pAS, SHADER_TYPE_AMPLIFICATION, "amplification")
        VALIDATE_SHADER_TYPE(CreateInfo.pMS, SHADER_TYPE_MESH, "mesh")

        MemPool.AddRequiredSize<GraphicsPipelineDesc>(1);
        ValidateResourceLayout(CreateInfo.PSODesc.ResourceLayout, MemPool);

        ValidateGraphicsPipeline(CreateInfo.GraphicsPipeline, MemPool);
    }

    void ValidateAndReserveSpace(const ComputePipelineStateCreateInfo& CreateInfo,
                                 LinearAllocator&                      MemPool) const
    {
        if (CreateInfo.pCS == nullptr)
        {
            LOG_ERROR_AND_THROW("Compute shader is not provided");
        }
        VALIDATE_SHADER_TYPE(CreateInfo.pCS, SHADER_TYPE_COMPUTE, "compute");

        ValidateResourceLayout(CreateInfo.PSODesc.ResourceLayout, MemPool);
    }

    template <typename ShaderImplType, typename TShaderStages>
    void ExtractShaders(const GraphicsPipelineStateCreateInfo& CreateInfo,
                        TShaderStages&                         ShaderStages)
    {
        VERIFY(m_NumShaderStages == 0, "The number of shader stages is not zero! ExtractShaders must only be called once.");

        ShaderStages.clear();
        auto AddShaderStage = [&](IShader* pShader) {
            if (pShader != nullptr)
            {
                auto ShaderType = pShader->GetDesc().ShaderType;
                ShaderStages.emplace_back(ShaderType, ValidatedCast<ShaderImplType>(pShader));
                m_ShaderStageTypes[m_NumShaderStages++] = ShaderType;
            }
        };

        switch (CreateInfo.PSODesc.PipelineType)
        {
            case PIPELINE_TYPE_GRAPHICS:
            {
                AddShaderStage(CreateInfo.pVS);
                AddShaderStage(CreateInfo.pHS);
                AddShaderStage(CreateInfo.pDS);
                AddShaderStage(CreateInfo.pGS);
                AddShaderStage(CreateInfo.pPS);
                break;
            }

            case PIPELINE_TYPE_MESH:
            {
                AddShaderStage(CreateInfo.pAS);
                AddShaderStage(CreateInfo.pMS);
                AddShaderStage(CreateInfo.pPS);
                break;
            }

            default:
                UNEXPECTED("unknown pipeline type");
        }

        VERIFY_EXPR(!ShaderStages.empty() && ShaderStages.size() == m_NumShaderStages);
    }

    template <typename ShaderImplType, typename TShaderStages>
    void ExtractShaders(const ComputePipelineStateCreateInfo& CreateInfo,
                        TShaderStages&                        ShaderStages)
    {
        VERIFY(m_NumShaderStages == 0, "The number of shader stages is not zero! ExtractShaders must only be called once.");

        ShaderStages.clear();
        auto AddShaderStage = [&](IShader* pShader) {
            if (pShader != nullptr)
            {
                auto ShaderType = pShader->GetDesc().ShaderType;
                ShaderStages.emplace_back(ShaderType, ValidatedCast<ShaderImplType>(pShader));
                m_ShaderStageTypes[m_NumShaderStages++] = ShaderType;
            }
        };

        AddShaderStage(CreateInfo.pCS);

        VERIFY_EXPR(!ShaderStages.empty() && ShaderStages.size() == m_NumShaderStages);
    }


    void InitGraphicsPipeline(const GraphicsPipelineStateCreateInfo& CreateInfo,
                              LinearAllocator&                       MemPool)
    {
        this->m_pGraphicsPipelineDesc = MemPool.CopyArray(&CreateInfo.GraphicsPipeline, 1);

        InitResourceLayout(CreateInfo.PSODesc.ResourceLayout, this->m_Desc.ResourceLayout, MemPool);

        auto&       GraphicsPipeline = *this->m_pGraphicsPipelineDesc;
        const auto& PSODesc          = this->m_Desc;

        CheckAndCorrectBlendStateDesc(GraphicsPipeline);
        CheckRasterizerStateDesc(GraphicsPipeline);
        CheckAndCorrectDepthStencilDesc(GraphicsPipeline);

        if (PSODesc.PipelineType == PIPELINE_TYPE_GRAPHICS)
        {
            DEV_CHECK_ERR(CreateInfo.pVS, "Vertex shader must be defined");
            DEV_CHECK_ERR(!CreateInfo.pAS && !CreateInfo.pMS, "Mesh shaders are not supported in graphics pipeline");
        }
        else if (PSODesc.PipelineType == PIPELINE_TYPE_MESH)
        {
            DEV_CHECK_ERR(CreateInfo.pMS, "Mesh shader must be defined");
            DEV_CHECK_ERR(!CreateInfo.pVS && !CreateInfo.pGS && !CreateInfo.pDS && !CreateInfo.pHS,
                          "Vertex, geometry and tessellation shaders are not supported in a mesh pipeline");
            DEV_CHECK_ERR(GraphicsPipeline.InputLayout.NumElements == 0, "Input layout ignored in mesh shader");
            DEV_CHECK_ERR(GraphicsPipeline.PrimitiveTopology == PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ||
                              GraphicsPipeline.PrimitiveTopology == PRIMITIVE_TOPOLOGY_UNDEFINED,
                          "Primitive topology is ignored in a mesh pipeline, set it to undefined or keep default value (triangle list)");
        }

        m_pRenderPass = GraphicsPipeline.pRenderPass;

        for (Uint32 rt = GraphicsPipeline.NumRenderTargets; rt < _countof(GraphicsPipeline.RTVFormats); ++rt)
        {
            auto RTVFmt = GraphicsPipeline.RTVFormats[rt];
            if (RTVFmt != TEX_FORMAT_UNKNOWN)
            {
                LOG_ERROR_MESSAGE("Render target format (", GetTextureFormatAttribs(RTVFmt).Name, ") of unused slot ", rt,
                                  " must be set to TEX_FORMAT_UNKNOWN");
            }
        }

        if (m_pRenderPass)
        {
            const auto& RPDesc = m_pRenderPass->GetDesc();
            VERIFY_EXPR(GraphicsPipeline.SubpassIndex < RPDesc.SubpassCount);
            const auto& Subpass = RPDesc.pSubpasses[GraphicsPipeline.SubpassIndex];

            GraphicsPipeline.NumRenderTargets = static_cast<Uint8>(Subpass.RenderTargetAttachmentCount);
            for (Uint32 rt = 0; rt < Subpass.RenderTargetAttachmentCount; ++rt)
            {
                const auto& RTAttachmentRef = Subpass.pRenderTargetAttachments[rt];
                if (RTAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED)
                {
                    VERIFY_EXPR(RTAttachmentRef.AttachmentIndex < RPDesc.AttachmentCount);
                    GraphicsPipeline.RTVFormats[rt] = RPDesc.pAttachments[RTAttachmentRef.AttachmentIndex].Format;
                }
            }

            if (Subpass.pDepthStencilAttachment != nullptr)
            {
                const auto& DSAttachmentRef = *Subpass.pDepthStencilAttachment;
                if (DSAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED)
                {
                    VERIFY_EXPR(DSAttachmentRef.AttachmentIndex < RPDesc.AttachmentCount);
                    GraphicsPipeline.DSVFormat = RPDesc.pAttachments[DSAttachmentRef.AttachmentIndex].Format;
                }
            }
        }

        const auto&    InputLayout     = GraphicsPipeline.InputLayout;
        LayoutElement* pLayoutElements = MemPool.Allocate<LayoutElement>(InputLayout.NumElements);
        for (size_t Elem = 0; Elem < InputLayout.NumElements; ++Elem)
        {
            pLayoutElements[Elem]              = InputLayout.LayoutElements[Elem];
            pLayoutElements[Elem].HLSLSemantic = MemPool.CopyString(InputLayout.LayoutElements[Elem].HLSLSemantic);
        }
        GraphicsPipeline.InputLayout.LayoutElements = pLayoutElements;


        // Correct description and compute offsets and tight strides
        std::array<Uint32, MAX_BUFFER_SLOTS> Strides, TightStrides = {};
        // Set all strides to an invalid value because an application may want to use 0 stride
        for (auto& Stride : Strides)
            Stride = LAYOUT_ELEMENT_AUTO_STRIDE;

        for (Uint32 i = 0; i < InputLayout.NumElements; ++i)
        {
            auto& LayoutElem = pLayoutElements[i];

            if (LayoutElem.ValueType == VT_FLOAT32 || LayoutElem.ValueType == VT_FLOAT16)
                LayoutElem.IsNormalized = false; // Floating point values cannot be normalized

            auto BuffSlot = LayoutElem.BufferSlot;
            if (BuffSlot >= Strides.size())
            {
                UNEXPECTED("Buffer slot (", BuffSlot, ") exceeds maximum allowed value (", Strides.size() - 1, ")");
                continue;
            }
            m_BufferSlotsUsed = static_cast<Uint8>(std::max<Uint32>(m_BufferSlotsUsed, BuffSlot + 1));

            auto& CurrAutoStride = TightStrides[BuffSlot];
            // If offset is not explicitly specified, use current auto stride value
            if (LayoutElem.RelativeOffset == LAYOUT_ELEMENT_AUTO_OFFSET)
            {
                LayoutElem.RelativeOffset = CurrAutoStride;
            }

            // If stride is explicitly specified, use it for the current buffer slot
            if (LayoutElem.Stride != LAYOUT_ELEMENT_AUTO_STRIDE)
            {
                // Verify that the value is consistent with the previously specified stride, if any
                if (Strides[BuffSlot] != LAYOUT_ELEMENT_AUTO_STRIDE && Strides[BuffSlot] != LayoutElem.Stride)
                {
                    LOG_ERROR_MESSAGE("Inconsistent strides are specified for buffer slot ", BuffSlot,
                                      ". Input element at index ", LayoutElem.InputIndex, " explicitly specifies stride ",
                                      LayoutElem.Stride, ", while current value is ", Strides[BuffSlot],
                                      ". Specify consistent strides or use LAYOUT_ELEMENT_AUTO_STRIDE to allow "
                                      "the engine compute strides automatically.");
                }
                Strides[BuffSlot] = LayoutElem.Stride;
            }

            CurrAutoStride = std::max(CurrAutoStride, LayoutElem.RelativeOffset + LayoutElem.NumComponents * GetValueSize(LayoutElem.ValueType));
        }

        for (Uint32 i = 0; i < InputLayout.NumElements; ++i)
        {
            auto& LayoutElem = pLayoutElements[i];

            auto BuffSlot = LayoutElem.BufferSlot;
            // If no input elements explicitly specified stride for this buffer slot, use automatic stride
            if (Strides[BuffSlot] == LAYOUT_ELEMENT_AUTO_STRIDE)
            {
                Strides[BuffSlot] = TightStrides[BuffSlot];
            }
            else
            {
                if (Strides[BuffSlot] < TightStrides[BuffSlot])
                {
                    LOG_ERROR_MESSAGE("Stride ", Strides[BuffSlot], " explicitly specified for slot ", BuffSlot,
                                      " is smaller than the minimum stride ", TightStrides[BuffSlot],
                                      " required to accomodate all input elements.");
                }
            }
            if (LayoutElem.Stride == LAYOUT_ELEMENT_AUTO_STRIDE)
                LayoutElem.Stride = Strides[BuffSlot];
        }

        m_pStrides = MemPool.Allocate<Uint32>(m_BufferSlotsUsed);

        // Set strides for all unused slots to 0
        for (Uint32 i = 0; i < m_BufferSlotsUsed; ++i)
        {
            auto Stride   = Strides[i];
            m_pStrides[i] = Stride != LAYOUT_ELEMENT_AUTO_STRIDE ? Stride : 0;
        }
    }

    void InitComputePipeline(const ComputePipelineStateCreateInfo& CreateInfo,
                             LinearAllocator&                      MemPool)
    {
        InitResourceLayout(CreateInfo.PSODesc.ResourceLayout, this->m_Desc.ResourceLayout, MemPool);
    }

#undef VALIDATE_SHADER_TYPE
#undef LOG_PSO_ERROR_AND_THROW
};

} // namespace Diligent
