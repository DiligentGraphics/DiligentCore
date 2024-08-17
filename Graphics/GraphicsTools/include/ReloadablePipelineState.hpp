/*
 *  Copyright 2024 Diligent Graphics LLC
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
/// Definition of the Diligent::ReloadablePipelineState class

#include <memory>

#include "PipelineState.h"
#include "RenderStateCache.h"
#include "ObjectBase.hpp"
#include "RefCntAutoPtr.hpp"

namespace Diligent
{

class RenderStateCacheImpl;

/// Reloadable pipeline state implements the IPipelineState interface and delegates all
/// calls to the internal pipeline object, which can be replaced at run-time.
class ReloadablePipelineState final : public ObjectBase<IPipelineState>
{
public:
    using TBase = ObjectBase<IPipelineState>;

    // {1F325E25-496B-41B4-A1F9-242302ABCDD4}
    static constexpr INTERFACE_ID IID_InternalImpl =
        {0x1f325e25, 0x496b, 0x41b4, {0xa1, 0xf9, 0x24, 0x23, 0x2, 0xab, 0xcd, 0xd4}};

    ReloadablePipelineState(IReferenceCounters*            pRefCounters,
                            RenderStateCacheImpl*          pStateCache,
                            IPipelineState*                pPipeline,
                            const PipelineStateCreateInfo& CreateInfo);
    ~ReloadablePipelineState();

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final;

    // Delegate all calls to the internal pipeline object

    virtual const PipelineStateDesc& DILIGENT_CALL_TYPE GetDesc() const override final
    {
        return m_pPipeline->GetDesc();
    }

    virtual Int32 DILIGENT_CALL_TYPE GetUniqueID() const override final
    {
        return m_pPipeline->GetUniqueID();
    }

    virtual void DILIGENT_CALL_TYPE SetUserData(IObject* pUserData) override final
    {
        m_pPipeline->SetUserData(pUserData);
    }

    virtual IObject* DILIGENT_CALL_TYPE GetUserData() const override final
    {
        return m_pPipeline->GetUserData();
    }

    virtual const GraphicsPipelineDesc& DILIGENT_CALL_TYPE GetGraphicsPipelineDesc() const override final
    {
        return m_pPipeline->GetGraphicsPipelineDesc();
    }

    virtual const RayTracingPipelineDesc& DILIGENT_CALL_TYPE GetRayTracingPipelineDesc() const override final
    {
        return m_pPipeline->GetRayTracingPipelineDesc();
    }

    virtual const TilePipelineDesc& DILIGENT_CALL_TYPE GetTilePipelineDesc() const override final
    {
        return m_pPipeline->GetTilePipelineDesc();
    }

    virtual void DILIGENT_CALL_TYPE BindStaticResources(SHADER_TYPE ShaderStages, IResourceMapping* pResourceMapping, BIND_SHADER_RESOURCES_FLAGS Flags) override final
    {
        m_pPipeline->BindStaticResources(ShaderStages, pResourceMapping, Flags);
    }

    virtual Uint32 DILIGENT_CALL_TYPE GetStaticVariableCount(SHADER_TYPE ShaderType) const override final
    {
        return m_pPipeline->GetStaticVariableCount(ShaderType);
    }

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) override final
    {
        return m_pPipeline->GetStaticVariableByName(ShaderType, Name);
    }

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final
    {
        return m_pPipeline->GetStaticVariableByIndex(ShaderType, Index);
    }

    virtual void DILIGENT_CALL_TYPE CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding, bool InitStaticResources) override final
    {
        m_pPipeline->CreateShaderResourceBinding(ppShaderResourceBinding, InitStaticResources);
    }

    virtual void DILIGENT_CALL_TYPE InitializeStaticSRBResources(IShaderResourceBinding* pShaderResourceBinding) const override final
    {
        m_pPipeline->InitializeStaticSRBResources(pShaderResourceBinding);
    }

    virtual void DILIGENT_CALL_TYPE CopyStaticResources(IPipelineState* pPSO) const override final
    {
        m_pPipeline->CopyStaticResources(pPSO);
    }

    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineState* pPSO) const override final
    {
        return m_pPipeline->IsCompatibleWith(pPSO);
    }

    virtual Uint32 DILIGENT_CALL_TYPE GetResourceSignatureCount() const override final
    {
        return m_pPipeline->GetResourceSignatureCount();
    }

    virtual IPipelineResourceSignature* DILIGENT_CALL_TYPE GetResourceSignature(Uint32 Index) const override final
    {
        return m_pPipeline->GetResourceSignature(Index);
    }

    virtual PIPELINE_STATE_STATUS DILIGENT_CALL_TYPE GetStatus(bool WaitForCompletion) override final
    {
        return m_pPipeline->GetStatus(WaitForCompletion);
    }

    static void Create(RenderStateCacheImpl*          pStateCache,
                       IPipelineState*                pPipeline,
                       const PipelineStateCreateInfo& CreateInfo,
                       IPipelineState**               ppReloadablePipeline);

    bool Reload(ReloadGraphicsPipelineCallbackType ReloadGraphicsPipeline, void* pUserData);

private:
    template <typename CreateInfoType>
    bool Reload(ReloadGraphicsPipelineCallbackType ReloadGraphicsPipeline, void* pUserData);

    struct DynamicHeapObjectBase
    {
        virtual ~DynamicHeapObjectBase() {}
    };

    template <typename CreateInfoType>
    struct CreateInfoWrapper;

    RefCntAutoPtr<RenderStateCacheImpl>    m_pStateCache;
    RefCntAutoPtr<IPipelineState>          m_pPipeline;
    std::unique_ptr<DynamicHeapObjectBase> m_pCreateInfo;
    const PIPELINE_TYPE                    m_Type;
};

} // namespace Diligent
