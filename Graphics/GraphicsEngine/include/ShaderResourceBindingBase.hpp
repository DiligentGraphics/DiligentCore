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
/// Implementation of the Diligent::ShaderResourceBindingBase template class

#include <array>

#include "PrivateConstants.h"
#include "ShaderResourceBinding.h"
#include "ObjectBase.hpp"
#include "GraphicsTypes.h"
#include "Constants.h"
#include "RefCntAutoPtr.hpp"
#include "GraphicsAccessories.hpp"

namespace Diligent
{

/// Template class implementing base functionality of the shader resource binding

/// \tparam BaseInterface - Base interface that this class will inheret
///                         (Diligent::IShaderResourceBindingGL, Diligent::IShaderResourceBindingD3D11,
///                          Diligent::IShaderResourceBindingD3D12 or Diligent::IShaderResourceBindingVk).
/// \tparam ResourceSignatureType - Type of the pipeline resource signature implementation
///                                 (Diligent::PipelineResourceSignatureD3D12Impl, Diligent::PipelineResourceSignatureVkImpl, etc.)
template <class BaseInterface, class ResourceSignatureType>
class ShaderResourceBindingBase : public ObjectBase<BaseInterface>
{
public:
    typedef ObjectBase<BaseInterface> TObjectBase;

    /// \param pRefCounters - Reference counters object that controls the lifetime of this SRB.
    /// \param pPRS         - Pipeline resource signature that this SRB belongs to.
    /// \param IsInternal   - Flag indicating if the shader resource binding is an internal object and
    ///                       must not keep a strong reference to the pipeline resource signature.
    ShaderResourceBindingBase(IReferenceCounters* pRefCounters, ResourceSignatureType* pPRS) :
        TObjectBase{pRefCounters},
        m_pPRS{pPRS}
    {
        m_ActiveShaderStageIndex.fill(-1);

        const auto NumShaders   = GetNumShaders();
        const auto PipelineType = GetPipelineType();
        for (Uint32 s = 0; s < NumShaders; ++s)
        {
            const auto ShaderType = pPRS->GetActiveShaderStageType(s);
            const auto ShaderInd  = GetShaderTypePipelineIndex(ShaderType, PipelineType);

            m_ActiveShaderStageIndex[ShaderInd] = static_cast<Int8>(s);
        }
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_ShaderResourceBinding, TObjectBase)

    Uint32 GetBindingIndex() const
    {
        return m_pPRS->GetDesc().BindingIndex;
    }

    PIPELINE_TYPE GetPipelineType() const
    {
        return m_pPRS->GetPipelineType();
    }

    Uint32 GetNumShaders() const
    {
        return m_pPRS->GetNumActiveShaderStages();
    }

    /// Implementation of IShaderResourceBinding::GetPipelineResourceSignature().
    virtual IPipelineResourceSignature* DILIGENT_CALL_TYPE GetPipelineResourceSignature() override final
    {
        return m_pPRS;
    }

    virtual void DILIGENT_CALL_TYPE InitializeStaticResources(const IPipelineState* pPipelineState) override
    {
        if (StaticResourcesInitialized())
        {
            LOG_WARNING_MESSAGE("Static resources have already been initialized in this shader resource binding object. The operation will be ignored.");
            return;
        }

        const IPipelineResourceSignature* pResourceSignature = nullptr;
        if (pPipelineState != nullptr)
        {
            pResourceSignature = pPipelineState->GetResourceSignature(GetBindingIndex());
            if (pResourceSignature == nullptr)
            {
                LOG_ERROR_MESSAGE("Shader resource binding is not compatible with pipeline state.");
                return;
            }

#ifdef DILIGENT_DEVELOPMENT
            if (!pResourceSignature->IsCompatibleWith(GetSignature()))
            {
                LOG_ERROR_MESSAGE("Shader resource binding is not compatible with pipeline state.");
                return;
            }
#endif
        }

        InitializeStaticResourcesWithSignature(pResourceSignature);
    }

    ResourceSignatureType* GetSignature() const
    {
        return m_pPRS.RawPtr<ResourceSignatureType>();
    }

    bool StaticResourcesInitialized() const
    {
        return m_bStaticResourcesInitialized;
    }

protected:
    template <typename ShaderVarManagerType>
    IShaderResourceVariable* GetVariableByNameImpl(SHADER_TYPE                ShaderType,
                                                   const char*                Name,
                                                   const ShaderVarManagerType ShaderVarMgrs[]) const
    {
        const auto PipelineType = GetPipelineType();
        if (!IsConsistentShaderType(ShaderType, PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to find mutable/dynamic variable '", Name, "' in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(PipelineType), " pipeline resource signature '", m_pPRS->GetDesc().Name, "'.");
            return nullptr;
        }

        const auto ShaderInd = GetShaderTypePipelineIndex(ShaderType, PipelineType);
        const auto MgrInd    = m_ActiveShaderStageIndex[ShaderInd];
        if (MgrInd < 0)
            return nullptr;

        VERIFY_EXPR(static_cast<Uint32>(MgrInd) < GetNumShaders());
        return ShaderVarMgrs[MgrInd].GetVariable(Name);
    }

    template <typename ShaderVarManagerType>
    Uint32 GetVariableCountImpl(SHADER_TYPE ShaderType, const ShaderVarManagerType ShaderVarMgrs[]) const
    {
        const auto PipelineType = GetPipelineType();
        if (!IsConsistentShaderType(ShaderType, PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to get the number of mutable/dynamic variables in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(PipelineType), " pipeline resource signature '", m_pPRS->GetDesc().Name, "'.");
            return 0;
        }

        const auto ShaderInd = GetShaderTypePipelineIndex(ShaderType, PipelineType);
        const auto MgrInd    = m_ActiveShaderStageIndex[ShaderInd];
        if (MgrInd < 0)
            return 0;

        VERIFY_EXPR(static_cast<Uint32>(MgrInd) < GetNumShaders());
        return ShaderVarMgrs[MgrInd].GetVariableCount();
    }

    template <typename ShaderVarManagerType>
    IShaderResourceVariable* GetVariableByIndexImpl(SHADER_TYPE                ShaderType,
                                                    Uint32                     Index,
                                                    const ShaderVarManagerType ShaderVarMgrs[]) const

    {
        const auto PipelineType = GetPipelineType();
        if (!IsConsistentShaderType(ShaderType, PipelineType))
        {
            LOG_WARNING_MESSAGE("Unable to get mutable/dynamic variable at index ", Index, " in shader stage ", GetShaderTypeLiteralName(ShaderType),
                                " as the stage is invalid for ", GetPipelineTypeString(PipelineType), " pipeline resource signature '", m_pPRS->GetDesc().Name, "'.");
            return nullptr;
        }

        const auto ShaderInd = GetShaderTypePipelineIndex(ShaderType, PipelineType);
        const auto MgrInd    = m_ActiveShaderStageIndex[ShaderInd];
        if (MgrInd < 0)
            return nullptr;

        VERIFY_EXPR(static_cast<Uint32>(MgrInd) < GetNumShaders());
        return ShaderVarMgrs[MgrInd].GetVariable(Index);
    }

    template <typename ShaderVarManagerType>
    void BindResourcesImpl(Uint32               ShaderFlags,
                           IResourceMapping*    pResMapping,
                           Uint32               Flags,
                           ShaderVarManagerType ShaderVarMgrs[]) const
    {
        const auto PipelineType = GetPipelineType();
        for (Int32 ShaderInd = 0; ShaderInd < static_cast<Int32>(m_ActiveShaderStageIndex.size()); ++ShaderInd)
        {
            auto VarMngrInd = m_ActiveShaderStageIndex[ShaderInd];
            if (VarMngrInd >= 0)
            {
                // ShaderInd is the shader type pipeline index here
                const auto ShaderType = GetShaderTypeFromPipelineIndex(ShaderInd, PipelineType);
                if ((ShaderFlags & ShaderType) != 0)
                {
                    ShaderVarMgrs[VarMngrInd].BindResources(pResMapping, Flags);
                }
            }
        }
    }

protected:
    /// Strong reference to pipeline resource signature. We must use strong reference, because
    /// shader resource binding uses pipeline resource signature's memory allocator to allocate
    /// memory for shader resource cache.
    RefCntAutoPtr<ResourceSignatureType> m_pPRS;

    // Index of the active shader stage that has resources, for every shader
    // type in the pipeline (given by GetShaderTypePipelineIndex(ShaderType, m_PipelineType)).
    std::array<Int8, MAX_SHADERS_IN_PIPELINE> m_ActiveShaderStageIndex = {-1, -1, -1, -1, -1, -1};
    static_assert(MAX_SHADERS_IN_PIPELINE == 6, "Please update the initializer list above");

    bool m_bStaticResourcesInitialized = false;
};

} // namespace Diligent
