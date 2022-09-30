/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include "RenderStateCache.h"
#include "ObjectBase.hpp"
#include "RefCntAutoPtr.hpp"
#include "DataBlobImpl.hpp"
#include "XXH128Hasher.hpp"

namespace Diligent
{

/// Implementation of IRenderStateCache
class RenderStateCacheImpl final : public ObjectBase<IRenderStateCache>
{
public:
    using TBase = ObjectBase<IRenderStateCache>;

public:
    RenderStateCacheImpl(IReferenceCounters*               pRefCounters,
                         const RenderStateCacheCreateInfo& CreateInfo) :
        TBase{pRefCounters},
        m_pDevice{CreateInfo.pDevice},
        m_pBytecodeCache{CreateInfo.pBytecodeCache}
    {
        if (CreateInfo.pDevice == nullptr)
            LOG_ERROR_AND_THROW("CreateInfo.pDevice must not be null");

        if (CreateInfo.pBytecodeCache == nullptr)
        {
            BytecodeCacheCreateInfo BytecodeCacheCI;
            BytecodeCacheCI.DeviceType = m_pDevice->GetDeviceInfo().Type;
            CreateBytecodeCache(BytecodeCacheCI, &m_pBytecodeCache);
            VERIFY_EXPR(m_pBytecodeCache);
        }
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_RenderStateCache, TBase);

    virtual bool DILIGENT_CALL_TYPE CreateShader(const ShaderCreateInfo& ShaderCI,
                                                 IShader**               ppShader) override final
    {
        RefCntAutoPtr<IDataBlob> pByteCode;
        m_pBytecodeCache->GetBytecode(ShaderCI, &pByteCode);
        if (pByteCode)
        {
            ShaderCreateInfo ShaderCI2{ShaderCI};
            ShaderCI2.Source                     = nullptr;
            ShaderCI2.FilePath                   = nullptr;
            ShaderCI2.pShaderSourceStreamFactory = nullptr;
            if (m_pDevice->GetDeviceInfo().IsGLDevice())
            {
                ShaderCI2.Source         = static_cast<const char*>(pByteCode->GetConstDataPtr());
                ShaderCI2.SourceLength   = pByteCode->GetSize();
                ShaderCI2.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM;
            }
            else
            {
                ShaderCI2.ByteCode     = pByteCode->GetConstDataPtr();
                ShaderCI2.ByteCodeSize = pByteCode->GetSize();
            }
            m_pDevice->CreateShader(ShaderCI2, ppShader);
            return true;
        }
        else
        {
            m_pDevice->CreateShader(ShaderCI, ppShader);
            if (*ppShader != nullptr)
            {
                const void* pBytecodePtr = nullptr;
                Uint64      BytecodeSize = 0;
                (*ppShader)->GetBytecode(&pBytecodePtr, BytecodeSize);
                if (pBytecodePtr != nullptr && BytecodeSize != 0)
                {
                    pByteCode = DataBlobImpl::Create(StaticCast<size_t>(BytecodeSize), pBytecodePtr);
                    m_pBytecodeCache->AddBytecode(ShaderCI, pByteCode);
                }
            }
            return false;
        }
    }

    virtual bool DILIGENT_CALL_TYPE CreateGraphicsPipelineState(
        const GraphicsPipelineStateCreateInfo& PSOCreateInfo,
        IPipelineState**                       ppPipelineState) override final
    {
        m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, ppPipelineState);
        return false;
    }

    virtual bool DILIGENT_CALL_TYPE CreateComputePipelineState(
        const ComputePipelineStateCreateInfo& PSOCreateInfo,
        IPipelineState**                      ppPipelineState) override final
    {
        m_pDevice->CreateComputePipelineState(PSOCreateInfo, ppPipelineState);
        return false;
    }

    virtual bool DILIGENT_CALL_TYPE CreateRayTracingPipelineState(
        const RayTracingPipelineStateCreateInfo& PSOCreateInfo,
        IPipelineState**                         ppPipelineState) override final
    {
        m_pDevice->CreateRayTracingPipelineState(PSOCreateInfo, ppPipelineState);
        return false;
    }

    virtual bool DILIGENT_CALL_TYPE CreateTilePipelineState(
        const TilePipelineStateCreateInfo& PSOCreateInfo,
        IPipelineState**                   ppPipelineState) override final
    {
        m_pDevice->CreateTilePipelineState(PSOCreateInfo, ppPipelineState);
        return false;
    }


    virtual IBytecodeCache* DILIGENT_CALL_TYPE GetBytecodeCache() override final
    {
        return m_pBytecodeCache;
    }

private:
    RefCntAutoPtr<IRenderDevice>  m_pDevice;
    RefCntAutoPtr<IBytecodeCache> m_pBytecodeCache;
};

void CreateRenderStateCache(const RenderStateCacheCreateInfo& CreateInfo,
                            IRenderStateCache**               ppCache)
{
    try
    {
        RefCntAutoPtr<IRenderStateCache> pCache{MakeNewRCObj<RenderStateCacheImpl>()(CreateInfo)};
        if (pCache)
            pCache->QueryInterface(IID_RenderStateCache, reinterpret_cast<IObject**>(ppCache));
    }
    catch (...)
    {
        LOG_ERROR("Failed to create the bytecode cache");
    }
}

} // namespace Diligent

extern "C"
{
    void CreateRenderStateCache(const Diligent::RenderStateCacheCreateInfo& CreateInfo,
                                Diligent::IRenderStateCache**               ppCache)
    {
        Diligent::CreateRenderStateCache(CreateInfo, ppCache);
    }
}
