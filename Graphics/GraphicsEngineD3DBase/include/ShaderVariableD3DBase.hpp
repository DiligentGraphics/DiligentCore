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

#include "ShaderResourceVariableBase.hpp"
#include "ShaderResourceVariableD3D.h"

/// \file
/// Declaration of Diligent::ShaderVariableD3DBase class

namespace Diligent
{

struct D3DVariableIDComparator
{
    bool operator()(const INTERFACE_ID& IID) const
    {
        return IID == IID_ShaderResourceVariableD3D || IID == IID_ShaderResourceVariable || IID == IID_Unknown;
    }
};

template <typename TShaderResourceLayout>
struct ShaderVariableD3DBase : public ShaderVariableBase<TShaderResourceLayout, IShaderResourceVariableD3D, D3DVariableIDComparator>
{
    using TBase = ShaderVariableBase<TShaderResourceLayout, IShaderResourceVariableD3D, D3DVariableIDComparator>;

    ShaderVariableD3DBase(TShaderResourceLayout&          ParentResLayout,
                          const D3DShaderResourceAttribs& Attribs,
                          SHADER_RESOURCE_VARIABLE_TYPE   VariableType) :
        // clang-format off
        TBase          {ParentResLayout},
        m_Attribs      {Attribs        },
        m_VariableType {VariableType   }
    // clang-format on
    {
    }

    virtual SHADER_RESOURCE_VARIABLE_TYPE DILIGENT_CALL_TYPE GetType() const override final
    {
        return m_VariableType;
    }

    virtual void DILIGENT_CALL_TYPE GetResourceDesc(ShaderResourceDesc& ResourceDesc) const override final
    {
        ResourceDesc = GetHLSLResourceDesc();
    }

    virtual HLSLShaderResourceDesc DILIGENT_CALL_TYPE GetHLSLResourceDesc() const override final
    {
        return m_Attribs.GetHLSLResourceDesc();
    }

    virtual Uint32 DILIGENT_CALL_TYPE GetIndex() const override final
    {
        return m_ParentResLayout.GetVariableIndex(*this);
    }

    const D3DShaderResourceAttribs& m_Attribs;

protected:
    const SHADER_RESOURCE_VARIABLE_TYPE m_VariableType;
};

} // namespace Diligent
