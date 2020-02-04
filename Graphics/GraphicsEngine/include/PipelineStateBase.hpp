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
    /// \param bIsDeviceInternal - flag indicating if the blend state is an internal device object and
    ///							   must not keep a strong reference to the device.
    PipelineStateBase(IReferenceCounters*      pRefCounters,
                      RenderDeviceImplType*    pDevice,
                      const PipelineStateDesc& PSODesc,
                      bool                     bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, PSODesc, bIsDeviceInternal},
        m_NumShaders{0}
    {
        const auto& SrcLayout      = PSODesc.ResourceLayout;
        size_t      StringPoolSize = 0;
        if (SrcLayout.Variables != nullptr)
        {
            for (Uint32 i = 0; i < SrcLayout.NumVariables; ++i)
                StringPoolSize += strlen(SrcLayout.Variables[i].Name) + 1;
        }

        if (SrcLayout.StaticSamplers != nullptr)
        {
            for (Uint32 i = 0; i < SrcLayout.NumStaticSamplers; ++i)
                StringPoolSize += strlen(SrcLayout.StaticSamplers[i].SamplerOrTextureName) + 1;
        }

        if (!PSODesc.IsComputePipeline)
        {
            CheckAndCorrectBlendStateDesc();
            CheckRasterizerStateDesc();
            CheckAndCorrectDepthStencilDesc();

            const auto& InputLayout = PSODesc.GraphicsPipeline.InputLayout;
            for (Uint32 i = 0; i < InputLayout.NumElements; ++i)
                StringPoolSize += strlen(InputLayout.LayoutElements[i].HLSLSemantic) + 1;
        }
        else
        {
            DEV_CHECK_ERR(PSODesc.GraphicsPipeline.InputLayout.NumElements == 0, "Compute pipelines must not have input layout elements");
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
                VERIFY(SrcLayout.Variables[i].Name != nullptr, "Variable name can't be null");
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
                VERIFY(SrcLayout.StaticSamplers[i].SamplerOrTextureName != nullptr, "Static sampler or texture name can't be null");
#ifdef DEVELOPMENT
                const auto& BorderColor = SrcLayout.StaticSamplers[i].Desc.BorderColor;
                if (!((BorderColor[0] == 0 && BorderColor[1] == 0 && BorderColor[2] == 0 && BorderColor[3] == 0) ||
                      (BorderColor[0] == 0 && BorderColor[1] == 0 && BorderColor[2] == 0 && BorderColor[3] == 1) ||
                      (BorderColor[0] == 1 && BorderColor[1] == 1 && BorderColor[2] == 1 && BorderColor[3] == 1)))
                {
                    LOG_WARNING_MESSAGE("Static sampler for variable \"", SrcLayout.StaticSamplers[i].SamplerOrTextureName, "\" specifies border color (",
                                        BorderColor[0], ", ", BorderColor[1], ", ", BorderColor[2], ", ", BorderColor[3],
                                        "). D3D12 static samplers only allow transparent black (0,0,0,0), opaque black (0,0,0,1) or opaque white (1,1,1,1) as border colors");
                }
#endif

                StaticSamplers[i]                      = SrcLayout.StaticSamplers[i];
                StaticSamplers[i].SamplerOrTextureName = m_StringPool.CopyString(SrcLayout.StaticSamplers[i].SamplerOrTextureName);
            }
        }


        if (this->m_Desc.IsComputePipeline)
        {
            const auto& ComputePipeline = PSODesc.ComputePipeline;
            if (ComputePipeline.pCS == nullptr)
            {
                LOG_ERROR_AND_THROW("Compute shader is not provided");
            }

#define VALIDATE_SHADER_TYPE(Shader, ExpectedType, ShaderName)                                                                           \
    if (Shader && Shader->GetDesc().ShaderType != ExpectedType)                                                                          \
    {                                                                                                                                    \
        LOG_ERROR_AND_THROW(GetShaderTypeLiteralName(Shader->GetDesc().ShaderType), " is not a valid type for ", ShaderName, " shader"); \
    }

            VALIDATE_SHADER_TYPE(ComputePipeline.pCS, SHADER_TYPE_COMPUTE, "compute")

            m_pCS          = ComputePipeline.pCS;
            m_ppShaders[0] = ComputePipeline.pCS;
            m_NumShaders   = 1;
        }
        else
        {
            const auto& GraphicsPipeline = PSODesc.GraphicsPipeline;

            VALIDATE_SHADER_TYPE(GraphicsPipeline.pVS, SHADER_TYPE_VERTEX, "vertex")
            VALIDATE_SHADER_TYPE(GraphicsPipeline.pPS, SHADER_TYPE_PIXEL, "pixel")
            VALIDATE_SHADER_TYPE(GraphicsPipeline.pGS, SHADER_TYPE_GEOMETRY, "geometry")
            VALIDATE_SHADER_TYPE(GraphicsPipeline.pHS, SHADER_TYPE_HULL, "hull")
            VALIDATE_SHADER_TYPE(GraphicsPipeline.pDS, SHADER_TYPE_DOMAIN, "domain")
#undef VALIDATE_SHADER_TYPE

            m_pVS = GraphicsPipeline.pVS;
            m_pPS = GraphicsPipeline.pPS;
            m_pGS = GraphicsPipeline.pGS;
            m_pDS = GraphicsPipeline.pDS;
            m_pHS = GraphicsPipeline.pHS;

            if (GraphicsPipeline.pVS) m_ppShaders[m_NumShaders++] = GraphicsPipeline.pVS;
            if (GraphicsPipeline.pPS) m_ppShaders[m_NumShaders++] = GraphicsPipeline.pPS;
            if (GraphicsPipeline.pGS) m_ppShaders[m_NumShaders++] = GraphicsPipeline.pGS;
            if (GraphicsPipeline.pHS) m_ppShaders[m_NumShaders++] = GraphicsPipeline.pHS;
            if (GraphicsPipeline.pDS) m_ppShaders[m_NumShaders++] = GraphicsPipeline.pDS;

            DEV_CHECK_ERR(m_NumShaders > 0, "There must be at least one shader in the Pipeline State");

            for (Uint32 rt = GraphicsPipeline.NumRenderTargets; rt < _countof(GraphicsPipeline.RTVFormats); ++rt)
            {
                auto RTVFmt = GraphicsPipeline.RTVFormats[rt];
                if (RTVFmt != TEX_FORMAT_UNKNOWN)
                {
                    LOG_ERROR_MESSAGE("Render target format (", GetTextureFormatAttribs(RTVFmt).Name, ") of unused slot ", rt,
                                      " must be set to TEX_FORMAT_UNKNOWN");
                }
            }

            const auto&    InputLayout     = PSODesc.GraphicsPipeline.InputLayout;
            LayoutElement* pLayoutElements = nullptr;
            if (InputLayout.NumElements > 0)
            {
                pLayoutElements = ALLOCATE(GetRawAllocator(), "Raw memory for input layout elements", LayoutElement, InputLayout.NumElements);
            }
            this->m_Desc.GraphicsPipeline.InputLayout.LayoutElements = pLayoutElements;
            for (size_t Elem = 0; Elem < InputLayout.NumElements; ++Elem)
            {
                pLayoutElements[Elem]              = InputLayout.LayoutElements[Elem];
                pLayoutElements[Elem].HLSLSemantic = m_StringPool.CopyString(InputLayout.LayoutElements[Elem].HLSLSemantic);
            }


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

    IShader* GetVS() { return m_pVS; }
    IShader* GetPS() { return m_pPS; }
    IShader* GetGS() { return m_pGS; }
    IShader* GetDS() { return m_pDS; }
    IShader* GetHS() { return m_pHS; }
    IShader* GetCS() { return m_pCS; }

    IShader* const* GetShaders() const { return m_ppShaders; }
    Uint32          GetNumShaders() const { return m_NumShaders; }

    template <typename ShaderType>
    ShaderType* GetShader(Uint32 ShaderInd)
    {
        VERIFY_EXPR(ShaderInd < m_NumShaders);
        return ValidatedCast<ShaderType>(m_ppShaders[ShaderInd]);
    }
    template <typename ShaderType>
    ShaderType* GetShader(Uint32 ShaderInd) const
    {
        VERIFY_EXPR(ShaderInd < m_NumShaders);
        return ValidatedCast<ShaderType>(m_ppShaders[ShaderInd]);
    }

    // This function only compares shader resource layout hashes, so
    // it can potentially give false negatives
    bool IsIncompatibleWith(const IPipelineState* pPSO) const
    {
        return m_ShaderResourceLayoutHash != ValidatedCast<const PipelineStateBase>(pPSO)->m_ShaderResourceLayoutHash;
    }

protected:
    Uint32  m_BufferSlotsUsed = 0;
    Uint32  m_NumShaders      = 0; ///< Number of shaders that this PSO uses
    Uint32* m_pStrides        = nullptr;

    StringPool m_StringPool;

    RefCntAutoPtr<IShader> m_pVS; ///< Strong reference to the vertex shader
    RefCntAutoPtr<IShader> m_pPS; ///< Strong reference to the pixel shader
    RefCntAutoPtr<IShader> m_pGS; ///< Strong reference to the geometry shader
    RefCntAutoPtr<IShader> m_pDS; ///< Strong reference to the domain shader
    RefCntAutoPtr<IShader> m_pHS; ///< Strong reference to the hull shader
    RefCntAutoPtr<IShader> m_pCS; ///< Strong reference to the compute shader

    IShader* m_ppShaders[5]             = {}; ///< Array of pointers to the shaders used by this PSO
    size_t   m_ShaderResourceLayoutHash = 0;  ///< Hash computed from the shader resource layout

private:
    void CheckRasterizerStateDesc() const
    {
        const auto& RSDesc = this->m_Desc.GraphicsPipeline.RasterizerDesc;
        if (RSDesc.FillMode == FILL_MODE_UNDEFINED)
            LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: RasterizerDesc.FillMode must not be FILL_MODE_UNDEFINED");
        if (RSDesc.CullMode == CULL_MODE_UNDEFINED)
            LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: RasterizerDesc.CullMode must not be CULL_MODE_UNDEFINED");
    }

    void CheckAndCorrectDepthStencilDesc()
    {
        auto& DSSDesc = this->m_Desc.GraphicsPipeline.DepthStencilDesc;
        if (DSSDesc.DepthFunc == COMPARISON_FUNC_UNKNOWN)
        {
            if (DSSDesc.DepthEnable)
                LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: DepthStencilDesc.DepthFunc must not be COMPARISON_FUNC_UNKNOWN when depth is enabled");
            else
                DSSDesc.DepthFunc = DepthStencilStateDesc{}.DepthFunc;
        }

        auto CheckAndCorrectStencilOpDesc = [&](StencilOpDesc& OpDesc, const char* FaceName) //
        {
            if (DSSDesc.StencilEnable)
            {
                if (OpDesc.StencilFailOp == STENCIL_OP_UNDEFINED)
                    LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: DepthStencilDesc.", FaceName, ".StencilFailOp must not be STENCIL_OP_UNDEFINED when stencil is enabled");
                if (OpDesc.StencilDepthFailOp == STENCIL_OP_UNDEFINED)
                    LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: DepthStencilDesc.", FaceName, ".StencilDepthFailOp must not be STENCIL_OP_UNDEFINED when stencil is enabled");
                if (OpDesc.StencilPassOp == STENCIL_OP_UNDEFINED)
                    LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: DepthStencilDesc.", FaceName, ".StencilPassOp must not be STENCIL_OP_UNDEFINED when stencil is enabled");
                if (OpDesc.StencilFunc == COMPARISON_FUNC_UNKNOWN)
                    LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: DepthStencilDesc.", FaceName, ".StencilFunc must not be COMPARISON_FUNC_UNKNOWN when stencil is enabled");
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
                    LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: BlendDesc.RenderTargets[", rt, "].SrcBlend must not be BLEND_FACTOR_UNDEFINED");
                if (RTDesc.DestBlend == BLEND_FACTOR_UNDEFINED)
                    LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: BlendDesc.RenderTargets[", rt, "].DestBlend must not be BLEND_FACTOR_UNDEFINED");
                if (RTDesc.BlendOp == BLEND_OPERATION_UNDEFINED)
                    LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: BlendDesc.RenderTargets[", rt, "].BlendOp must not be BLEND_OPERATION_UNDEFINED");

                if (RTDesc.SrcBlendAlpha == BLEND_FACTOR_UNDEFINED)
                    LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: BlendDesc.RenderTargets[", rt, "].SrcBlendAlpha must not be BLEND_FACTOR_UNDEFINED");
                if (RTDesc.DestBlendAlpha == BLEND_FACTOR_UNDEFINED)
                    LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: BlendDesc.RenderTargets[", rt, "].DestBlendAlpha must not be BLEND_FACTOR_UNDEFINED");
                if (RTDesc.BlendOpAlpha == BLEND_OPERATION_UNDEFINED)
                    LOG_ERROR_AND_THROW("Description of graphics PSO '", this->m_Desc.Name, "' is invalid: BlendDesc.RenderTargets[", rt, "].BlendOpAlpha must not be BLEND_OPERATION_UNDEFINED");
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
};

} // namespace Diligent
