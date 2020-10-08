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
#include "StringPool.hpp"

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
    /// \param PSODesc - pipeline state description.
    /// \param bIsDeviceInternal - flag indicating if the pipeline state is an internal device object and
    ///							   must not keep a strong reference to the device.
    PipelineStateBase(IReferenceCounters*      pRefCounters,
                      RenderDeviceImplType*    pDevice,
                      const PipelineStateDesc& PSODesc,
                      bool                     bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, PSODesc, bIsDeviceInternal}
    {
        const auto& SrcLayout      = PSODesc.ResourceLayout;
        size_t      StringPoolSize = 0;
        if (SrcLayout.Variables != nullptr)
        {
            for (Uint32 i = 0; i < SrcLayout.NumVariables; ++i)
            {
                VERIFY(SrcLayout.Variables[i].Name != nullptr, "Variable name can't be null");
                StringPoolSize += strlen(SrcLayout.Variables[i].Name) + 1;
            }
        }

        if (SrcLayout.StaticSamplers != nullptr)
        {
            for (Uint32 i = 0; i < SrcLayout.NumStaticSamplers; ++i)
            {
                VERIFY(SrcLayout.StaticSamplers[i].SamplerOrTextureName != nullptr, "Static sampler or texture name can't be null");
                StringPoolSize += strlen(SrcLayout.StaticSamplers[i].SamplerOrTextureName) + 1;
            }
        }

        switch (PSODesc.PipelineType)
        {
            // clang-format off
            case PIPELINE_TYPE_GRAPHICS:
            case PIPELINE_TYPE_MESH:     ValidateGraphicsPipeline(  StringPoolSize); break;
            case PIPELINE_TYPE_COMPUTE:  ValidateComputePipeline(   StringPoolSize); break;
            default:                     UNEXPECTED("unknown pipeline type");
                // clang-format on
        }

        m_StringPool.Reserve(StringPoolSize, GetRawAllocator());

        auto& DstLayout = this->m_Desc.ResourceLayout;
        if (SrcLayout.Variables != nullptr)
        {
            ShaderResourceVariableDesc* Variables =
                ALLOCATE(GetRawAllocator(), "Memory for ShaderResourceVariableDesc array", ShaderResourceVariableDesc, SrcLayout.NumVariables);
            DstLayout.Variables = Variables;
            for (Uint32 i = 0; i < SrcLayout.NumVariables; ++i)
            {
                Variables[i]      = SrcLayout.Variables[i];
                Variables[i].Name = m_StringPool.CopyString(SrcLayout.Variables[i].Name);
            }
        }

        if (SrcLayout.StaticSamplers != nullptr)
        {
            StaticSamplerDesc* StaticSamplers =
                ALLOCATE(GetRawAllocator(), "Memory for StaticSamplerDesc array", StaticSamplerDesc, SrcLayout.NumStaticSamplers);
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
                StaticSamplers[i].SamplerOrTextureName = m_StringPool.CopyString(SrcLayout.StaticSamplers[i].SamplerOrTextureName);
            }
        }

        switch (PSODesc.PipelineType)
        {
            // clang-format off
            case PIPELINE_TYPE_GRAPHICS:
            case PIPELINE_TYPE_MESH:     InitGraphicsPipeline();   break;
            case PIPELINE_TYPE_COMPUTE:  InitComputePipeline();    break;
            default:                     UNEXPECTED("unknown pipeline type");
                // clang-format on
        }

        VERIFY_EXPR(m_StringPool.GetRemainingSize() == 0);

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

        auto& RawAllocator = GetRawAllocator();
        if (this->m_Desc.ResourceLayout.Variables != nullptr)
            RawAllocator.Free(const_cast<ShaderResourceVariableDesc*>(this->m_Desc.ResourceLayout.Variables));
        if (this->m_Desc.ResourceLayout.StaticSamplers != nullptr)
            RawAllocator.Free(const_cast<StaticSamplerDesc*>(this->m_Desc.ResourceLayout.StaticSamplers));
        if (this->m_Desc.GraphicsPipeline.InputLayout.LayoutElements != nullptr)
            RawAllocator.Free(const_cast<LayoutElement*>(this->m_Desc.GraphicsPipeline.InputLayout.LayoutElements));
        if (m_pStrides != nullptr)
            RawAllocator.Free(m_pStrides);
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

    SHADER_TYPE const* GetShaderTypes() const { return m_pShaderTypes.data(); }
    Uint32             GetNumShaderTypes() const { return m_NumShaderTypes; }

    // This function only compares shader resource layout hashes, so
    // it can potentially give false negatives
    bool IsIncompatibleWith(const IPipelineState* pPSO) const
    {
        return m_ShaderResourceLayoutHash != ValidatedCast<const PipelineStateBase>(pPSO)->m_ShaderResourceLayoutHash;
    }

protected:
    Uint32  m_BufferSlotsUsed = 0;
    Uint32* m_pStrides        = nullptr;

    StringPool m_StringPool;

    RefCntAutoPtr<IRenderPass> m_pRenderPass; ///< Strong reference to the render pass object

    Uint8 m_NumShaderTypes = 0; ///< Number of shader types that this PSO uses

    std::array<SHADER_TYPE, MAX_SHADERS_IN_PIPELINE> m_pShaderTypes             = {}; ///< Array of shader types used by this PSO
    size_t                                           m_ShaderResourceLayoutHash = 0;  ///< Hash computed from the shader resource layout

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

public:
    using ShaderStages_t = std::vector<std::pair<SHADER_TYPE, IShader*>>;

protected:
    void ExtractShaders(ShaderStages_t& ShaderStages)
    {
        auto& Desc = this->m_Desc;
        switch (Desc.PipelineType)
        {
            case PIPELINE_TYPE_COMPUTE:
            {
                if (Desc.ComputePipeline.pCS) ShaderStages.push_back({SHADER_TYPE_COMPUTE, Desc.ComputePipeline.pCS});

                // reset shader pointers because we don't keep strong references to shaders
                Desc.ComputePipeline.pCS = nullptr;
                break;
            }

            case PIPELINE_TYPE_GRAPHICS:
            {
                if (Desc.GraphicsPipeline.pVS) ShaderStages.push_back({SHADER_TYPE_VERTEX, Desc.GraphicsPipeline.pVS});
                if (Desc.GraphicsPipeline.pHS) ShaderStages.push_back({SHADER_TYPE_HULL, Desc.GraphicsPipeline.pHS});
                if (Desc.GraphicsPipeline.pDS) ShaderStages.push_back({SHADER_TYPE_DOMAIN, Desc.GraphicsPipeline.pDS});
                if (Desc.GraphicsPipeline.pGS) ShaderStages.push_back({SHADER_TYPE_GEOMETRY, Desc.GraphicsPipeline.pGS});
                if (Desc.GraphicsPipeline.pPS) ShaderStages.push_back({SHADER_TYPE_PIXEL, Desc.GraphicsPipeline.pPS});

                // reset shader pointers because we don't keep strong references to shaders
                Desc.GraphicsPipeline.pVS = nullptr;
                Desc.GraphicsPipeline.pHS = nullptr;
                Desc.GraphicsPipeline.pDS = nullptr;
                Desc.GraphicsPipeline.pGS = nullptr;
                Desc.GraphicsPipeline.pPS = nullptr;
                break;
            }

            case PIPELINE_TYPE_MESH:
            {
                if (Desc.GraphicsPipeline.pAS) ShaderStages.push_back({SHADER_TYPE_AMPLIFICATION, Desc.GraphicsPipeline.pAS});
                if (Desc.GraphicsPipeline.pMS) ShaderStages.push_back({SHADER_TYPE_MESH, Desc.GraphicsPipeline.pMS});
                if (Desc.GraphicsPipeline.pPS) ShaderStages.push_back({SHADER_TYPE_PIXEL, Desc.GraphicsPipeline.pPS});

                // reset shader pointers because we don't keep strong references to shaders
                Desc.GraphicsPipeline.pAS = nullptr;
                Desc.GraphicsPipeline.pMS = nullptr;
                Desc.GraphicsPipeline.pPS = nullptr;
                break;
            }

            default:
                UNEXPECTED("unknown pipeline type");
        }

#ifdef DILIGENT_DEVELOPMENT
        VERIFY_EXPR(ShaderStages.size() == m_NumShaderTypes);

        for (Uint32 s = 0; s < m_NumShaderTypes; ++s)
        {
            VERIFY_EXPR(ShaderStages[s].first == m_pShaderTypes[s]);
        }
#endif
    }


private:
    void CheckRasterizerStateDesc() const
    {
        const auto& RSDesc = this->m_Desc.GraphicsPipeline.RasterizerDesc;
        if (RSDesc.FillMode == FILL_MODE_UNDEFINED)
            LOG_PSO_ERROR_AND_THROW("RasterizerDesc.FillMode must not be FILL_MODE_UNDEFINED");
        if (RSDesc.CullMode == CULL_MODE_UNDEFINED)
            LOG_PSO_ERROR_AND_THROW("RasterizerDesc.CullMode must not be CULL_MODE_UNDEFINED");
    }

    void CheckAndCorrectDepthStencilDesc()
    {
        auto& DSSDesc = this->m_Desc.GraphicsPipeline.DepthStencilDesc;
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

    void CheckAndCorrectBlendStateDesc()
    {
        auto& BlendDesc = this->m_Desc.GraphicsPipeline.BlendDesc;
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

    void ValidateGraphicsPipeline(size_t& StringPoolSize)
    {
        const auto& GraphicsPipeline = this->m_Desc.GraphicsPipeline;
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

        CheckAndCorrectBlendStateDesc();
        CheckRasterizerStateDesc();
        CheckAndCorrectDepthStencilDesc();

        const auto& InputLayout = this->m_Desc.GraphicsPipeline.InputLayout;
        for (Uint32 i = 0; i < InputLayout.NumElements; ++i)
            StringPoolSize += strlen(InputLayout.LayoutElements[i].HLSLSemantic) + 1;
    }

    void ValidateComputePipeline(size_t& StringPoolSize)
    {
        if (this->m_Desc.GraphicsPipeline.pRenderPass != nullptr)
        {
            LOG_PSO_ERROR_AND_THROW("GraphicsPipeline.pRenderPass must be null for compute pipelines");
        }
        DEV_CHECK_ERR(this->m_Desc.GraphicsPipeline.InputLayout.NumElements == 0, "Compute pipelines must not have input layout elements");
    }

#define VALIDATE_SHADER_TYPE(Shader, ExpectedType, ShaderName)                                                                           \
    if (Shader && Shader->GetDesc().ShaderType != ExpectedType)                                                                          \
    {                                                                                                                                    \
        LOG_ERROR_AND_THROW(GetShaderTypeLiteralName(Shader->GetDesc().ShaderType), " is not a valid type for ", ShaderName, " shader"); \
    }

    void InitGraphicsPipeline()
    {
        const auto& PSODesc          = this->m_Desc;
        const auto& GraphicsPipeline = PSODesc.GraphicsPipeline;

        VALIDATE_SHADER_TYPE(GraphicsPipeline.pVS, SHADER_TYPE_VERTEX, "vertex")
        VALIDATE_SHADER_TYPE(GraphicsPipeline.pPS, SHADER_TYPE_PIXEL, "pixel")
        VALIDATE_SHADER_TYPE(GraphicsPipeline.pGS, SHADER_TYPE_GEOMETRY, "geometry")
        VALIDATE_SHADER_TYPE(GraphicsPipeline.pHS, SHADER_TYPE_HULL, "hull")
        VALIDATE_SHADER_TYPE(GraphicsPipeline.pDS, SHADER_TYPE_DOMAIN, "domain")
        VALIDATE_SHADER_TYPE(GraphicsPipeline.pAS, SHADER_TYPE_AMPLIFICATION, "amplification")
        VALIDATE_SHADER_TYPE(GraphicsPipeline.pMS, SHADER_TYPE_MESH, "mesh")

        if (PSODesc.PipelineType == PIPELINE_TYPE_GRAPHICS)
        {
            DEV_CHECK_ERR(GraphicsPipeline.pVS, "Vertex shader must be defined");
            DEV_CHECK_ERR(!GraphicsPipeline.pAS && !GraphicsPipeline.pMS, "Mesh shaders are not supported in graphics pipeline");
        }
        else if (PSODesc.PipelineType == PIPELINE_TYPE_MESH)
        {
            DEV_CHECK_ERR(GraphicsPipeline.pMS, "Mesh shader must be defined");
            DEV_CHECK_ERR(!GraphicsPipeline.pVS && !GraphicsPipeline.pGS && !GraphicsPipeline.pDS && !GraphicsPipeline.pHS,
                          "Vertex, geometry and tessellation shaders are not supported in a mesh pipeline");
            DEV_CHECK_ERR(GraphicsPipeline.InputLayout.NumElements == 0, "Input layout ignored in mesh shader");
            DEV_CHECK_ERR(GraphicsPipeline.PrimitiveTopology == PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ||
                              GraphicsPipeline.PrimitiveTopology == PRIMITIVE_TOPOLOGY_UNDEFINED,
                          "Primitive topology is ignored in a mesh pipeline, set it to undefined or keep default value (triangle list)");
        }

        if (GraphicsPipeline.pVS) m_pShaderTypes[m_NumShaderTypes++] = SHADER_TYPE_VERTEX;
        if (GraphicsPipeline.pHS) m_pShaderTypes[m_NumShaderTypes++] = SHADER_TYPE_HULL;
        if (GraphicsPipeline.pDS) m_pShaderTypes[m_NumShaderTypes++] = SHADER_TYPE_DOMAIN;
        if (GraphicsPipeline.pGS) m_pShaderTypes[m_NumShaderTypes++] = SHADER_TYPE_GEOMETRY;
        if (GraphicsPipeline.pAS) m_pShaderTypes[m_NumShaderTypes++] = SHADER_TYPE_AMPLIFICATION;
        if (GraphicsPipeline.pMS) m_pShaderTypes[m_NumShaderTypes++] = SHADER_TYPE_MESH;
        if (GraphicsPipeline.pPS) m_pShaderTypes[m_NumShaderTypes++] = SHADER_TYPE_PIXEL;

        DEV_CHECK_ERR(m_NumShaderTypes > 0, "There must be at least one shader in the Pipeline State");

        m_pRenderPass = PSODesc.GraphicsPipeline.pRenderPass;

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

            this->m_Desc.GraphicsPipeline.NumRenderTargets = static_cast<Uint8>(Subpass.RenderTargetAttachmentCount);
            for (Uint32 rt = 0; rt < Subpass.RenderTargetAttachmentCount; ++rt)
            {
                const auto& RTAttachmentRef = Subpass.pRenderTargetAttachments[rt];
                if (RTAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED)
                {
                    VERIFY_EXPR(RTAttachmentRef.AttachmentIndex < RPDesc.AttachmentCount);
                    this->m_Desc.GraphicsPipeline.RTVFormats[rt] = RPDesc.pAttachments[RTAttachmentRef.AttachmentIndex].Format;
                }
            }

            if (Subpass.pDepthStencilAttachment != nullptr)
            {
                const auto& DSAttachmentRef = *Subpass.pDepthStencilAttachment;
                if (DSAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED)
                {
                    VERIFY_EXPR(DSAttachmentRef.AttachmentIndex < RPDesc.AttachmentCount);
                    this->m_Desc.GraphicsPipeline.DSVFormat = RPDesc.pAttachments[DSAttachmentRef.AttachmentIndex].Format;
                }
            }
        }

        const auto&    InputLayout     = PSODesc.GraphicsPipeline.InputLayout;
        LayoutElement* pLayoutElements = nullptr;
        if (InputLayout.NumElements > 0)
        {
            pLayoutElements = ALLOCATE(GetRawAllocator(), "Raw memory for input layout elements", LayoutElement, InputLayout.NumElements);
        }
        for (size_t Elem = 0; Elem < InputLayout.NumElements; ++Elem)
        {
            pLayoutElements[Elem]              = InputLayout.LayoutElements[Elem];
            pLayoutElements[Elem].HLSLSemantic = m_StringPool.CopyString(InputLayout.LayoutElements[Elem].HLSLSemantic);
        }
        this->m_Desc.GraphicsPipeline.InputLayout.LayoutElements = pLayoutElements;


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
            m_BufferSlotsUsed = std::max(m_BufferSlotsUsed, BuffSlot + 1);

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

        if (m_BufferSlotsUsed > 0)
        {
            m_pStrides = ALLOCATE(GetRawAllocator(), "Raw memory for buffer strides", Uint32, m_BufferSlotsUsed);

            // Set strides for all unused slots to 0
            for (Uint32 i = 0; i < m_BufferSlotsUsed; ++i)
            {
                auto Stride   = Strides[i];
                m_pStrides[i] = Stride != LAYOUT_ELEMENT_AUTO_STRIDE ? Stride : 0;
            }
        }
    }

    void InitComputePipeline()
    {
        const auto& ComputePipeline = this->m_Desc.ComputePipeline;
        if (ComputePipeline.pCS == nullptr)
        {
            LOG_ERROR_AND_THROW("Compute shader is not provided");
        }

        VALIDATE_SHADER_TYPE(ComputePipeline.pCS, SHADER_TYPE_COMPUTE, "compute");

        m_pShaderTypes[m_NumShaderTypes++] = SHADER_TYPE_COMPUTE;
    }

#undef VALIDATE_SHADER_TYPE
#undef LOG_PSO_ERROR_AND_THROW
};

} // namespace Diligent
