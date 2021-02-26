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

#include <vector>
#include "PipelineStateGL.h"
#include "PipelineStateBase.hpp"
#include "RenderDevice.h"
#include "GLObjectWrapper.hpp"
#include "GLContext.hpp"
#include "RenderDeviceGLImpl.hpp"
#include "ShaderVariableGL.hpp"
#include "ShaderGLImpl.hpp"
#include "PipelineResourceSignatureGLImpl.hpp"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Pipeline state object implementation in OpenGL backend.
class PipelineStateGLImpl final : public PipelineStateBase<IPipelineStateGL, RenderDeviceGLImpl>
{
public:
    using TPipelineStateBase = PipelineStateBase<IPipelineStateGL, RenderDeviceGLImpl>;

    PipelineStateGLImpl(IReferenceCounters*                    pRefCounters,
                        RenderDeviceGLImpl*                    pDeviceGL,
                        const GraphicsPipelineStateCreateInfo& CreateInfo,
                        bool                                   IsDeviceInternal = false);
    PipelineStateGLImpl(IReferenceCounters*                   pRefCounters,
                        RenderDeviceGLImpl*                   pDeviceGL,
                        const ComputePipelineStateCreateInfo& CreateInfo,
                        bool                                  IsDeviceInternal = false);
    ~PipelineStateGLImpl();

    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override;

    /// Implementation of IPipelineState::GetResourceSignatureCount() in OpenGL backend.
    virtual Uint32 DILIGENT_CALL_TYPE GetResourceSignatureCount() const override final { return m_SignatureCount; }

    /// Implementation of IPipelineState::GetResourceSignature() in OpenGL backend.
    virtual IPipelineResourceSignature* DILIGENT_CALL_TYPE GetResourceSignature(Uint32 Index) const override final
    {
        VERIFY_EXPR(Index < m_SignatureCount);
        return m_Signatures[Index].RawPtr<IPipelineResourceSignature>();
    }

    /// Implementation of IPipelineState::IsCompatibleWith() in OpenGL backend.
    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineState* pPSO) const override final;

    void CommitProgram(GLContextState& State);

    Uint32 GetSignatureCount() const { return m_SignatureCount; }

    PipelineResourceSignatureGLImpl* GetSignature(Uint32 index) const
    {
        VERIFY_EXPR(index < m_SignatureCount);
        return m_Signatures[index].RawPtr<PipelineResourceSignatureGLImpl>();
    }

#ifdef DILIGENT_DEVELOPMENT
    using TBindings = PipelineResourceSignatureGLImpl::TBindings;
    void DvpVerifySRBResources(class ShaderResourceBindingGLImpl* pSRBs[], const TBindings BoundResOffsets[], Uint32 NumSRBs) const;
#endif

private:
    using ShaderStageInfo = ShaderGLImpl::ShaderStageInfo;
    using TShaderStages   = std::vector<ShaderStageInfo>;

    GLObjectWrappers::GLPipelineObj& GetGLProgramPipeline(GLContext::NativeGLContextType Context);

    template <typename PSOCreateInfoType>
    void InitInternalObjects(const PSOCreateInfoType& CreateInfo, const TShaderStages& ShaderStages);

    void InitResourceLayouts(const PipelineStateCreateInfo& CreateInfo,
                             const TShaderStages&           ShaderStages,
                             SHADER_TYPE                    ActiveStages);

    void CreateDefaultSignature(const PipelineStateCreateInfo& CreateInfo,
                                const TShaderStages&           ShaderStages,
                                SHADER_TYPE                    ActiveStages,
                                IPipelineResourceSignature**   ppSignature);

    void Destruct();

    SHADER_TYPE GetShaderStageType(Uint32 Index) const;
    Uint32      GetNumShaderStages() const { return m_NumPrograms; }

#ifdef DILIGENT_DEVELOPMENT
    struct ResourceAttribution
    {
        static constexpr Uint32 InvalidSignatureIndex = ~0u;
        static constexpr Uint32 InvalidResourceIndex  = PipelineResourceSignatureGLImpl::InvalidResourceIndex;
        static constexpr Uint32 InvalidSamplerIndex   = InvalidImmutableSamplerIndex;

        const PipelineResourceSignatureGLImpl* pSignature = nullptr;

        Uint32 SignatureIndex        = InvalidSignatureIndex;
        Uint32 ResourceIndex         = InvalidResourceIndex;
        Uint32 ImmutableSamplerIndex = InvalidSamplerIndex;

        ResourceAttribution() noexcept {}
        ResourceAttribution(const PipelineResourceSignatureGLImpl* _pSignature,
                            Uint32                                 _SignatureIndex,
                            Uint32                                 _ResourceIndex,
                            Uint32                                 _ImmutableSamplerIndex = InvalidResourceIndex) noexcept :
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

    void DvpValidateShaderResources(const std::shared_ptr<const ShaderResourcesGL>& pShaderResources, const char* ShaderName, SHADER_TYPE ShaderStages);
#endif

private:
    // Linked GL programs for every shader stage. Every pipeline needs to have its own programs
    // because resource bindings assigned by PipelineResourceSignatureGLImpl::ApplyBindings depend on other
    // shader stages.
    using GLProgramObj         = GLObjectWrappers::GLProgramObj;
    GLProgramObj* m_GLPrograms = nullptr; // [m_NumPrograms]

    ThreadingTools::LockFlag m_ProgPipelineLockFlag;

    std::vector<std::pair<GLContext::NativeGLContextType, GLObjectWrappers::GLPipelineObj>> m_GLProgPipelines;

    using SignatureArrayType            = std::array<RefCntAutoPtr<PipelineResourceSignatureGLImpl>, MAX_RESOURCE_SIGNATURES>;
    SignatureArrayType m_Signatures     = {};
    Uint8              m_SignatureCount = 0;

    Uint8                      m_NumPrograms                = 0;
    bool                       m_IsProgramPipelineSupported = false;
    std::array<SHADER_TYPE, 5> m_ShaderTypes                = {};

#ifdef DILIGENT_DEVELOPMENT
    // Shader resources for all shaders in all shader stages in the pipeline.
    std::vector<std::shared_ptr<const ShaderResourcesGL>> m_ShaderResources;
    std::vector<String>                                   m_ShaderNames;

    // Shader resource attributions for every resource in m_ShaderResources, in the same order.
    std::vector<ResourceAttribution> m_ResourceAttibutions;
#endif
};

} // namespace Diligent
