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
/// Implementation of the Diligent::ShaderResourceBindingBase template class

#include "ShaderResourceBinding.h"
#include "ObjectBase.hpp"
#include "GraphicsTypes.h"
#include "RefCntAutoPtr.hpp"

namespace Diligent
{

/// Template class implementing base functionality for a shader resource binding

/// \tparam BaseInterface - base interface that this class will inheret
///                         (Diligent::IShaderResourceBindingGL, Diligent::IShaderResourceBindingD3D11,
///                          Diligent::IShaderResourceBindingD3D12 or Diligent::IShaderResourceBindingVk).
template <class BaseInterface>
class ShaderResourceBindingBase : public ObjectBase<BaseInterface>
{
public:
    typedef ObjectBase<BaseInterface> TObjectBase;

    /// \param pRefCounters - reference counters object that controls the lifetime of this SRB.
    /// \param pPSO - pipeline state that this SRB belongs to.
    /// \param IsInternal - flag indicating if the shader resource binding is an internal PSO object and
    ///						must not keep a strong reference to the PSO.
    ShaderResourceBindingBase(IReferenceCounters* pRefCounters, IPipelineState* pPSO, bool IsInternal = false) :
        TObjectBase{pRefCounters},
        m_spPSO{IsInternal ? nullptr : pPSO},
        m_pPSO{pPSO}
    {}

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_ShaderResourceBinding, TObjectBase)

    /// Implementation of IShaderResourceBinding::GetPipelineState().
    virtual IPipelineState* DILIGENT_CALL_TYPE GetPipelineState() override final
    {
        return m_pPSO;
    }

    template <typename PSOType>
    PSOType* GetPipelineState()
    {
        return ValidatedCast<PSOType>(m_pPSO);
    }

    template <typename PSOType>
    PSOType* GetPipelineState() const
    {
        return ValidatedCast<PSOType>(m_pPSO);
    }

protected:
    /// Strong reference to PSO. We must use strong reference, because
    /// shader resource binding uses PSO's memory allocator to allocate
    /// memory for shader resource cache.
    RefCntAutoPtr<IPipelineState> m_spPSO;
    IPipelineState* const         m_pPSO;
};

} // namespace Diligent
