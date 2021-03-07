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
/// Declaration of Diligent::ShaderResourceBindingD3D12Impl class

#include "EngineD3D12ImplTraits.hpp"
#include "ShaderResourceBindingBase.hpp"
#include "ShaderResourceBindingD3D12.h"

// ShaderVariableManagerD3D12 and ShaderResourceCacheD3D12 are required by ShaderResourceBindingBase
#include "ShaderVariableManagerD3D12.hpp"
#include "ShaderResourceCacheD3D12.hpp"

namespace Diligent
{

class PipelineResourceSignatureD3D12Impl;

/// Implementation of the Diligent::IShaderResourceBindingD3D12 interface
// sizeof(ShaderResourceBindingD3D12Impl) == 96 (x64, msvc, Release)
class ShaderResourceBindingD3D12Impl final : public ShaderResourceBindingBase<EngineD3D12ImplTraits>
{
public:
    using TBase = ShaderResourceBindingBase<EngineD3D12ImplTraits>;

    ShaderResourceBindingD3D12Impl(IReferenceCounters*                 pRefCounters,
                                   PipelineResourceSignatureD3D12Impl* pPRS);
    ~ShaderResourceBindingD3D12Impl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_ShaderResourceBindingD3D12, TBase)

    virtual void DILIGENT_CALL_TYPE BindResources(Uint32 ShaderFlags, IResourceMapping* pResMapping, Uint32 Flags) override;

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetVariableByName(SHADER_TYPE ShaderType, const char* Name) override;

    virtual Uint32 DILIGENT_CALL_TYPE GetVariableCount(SHADER_TYPE ShaderType) const override final;

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    ShaderResourceCacheD3D12& GetResourceCache() { return m_ShaderResourceCache; }

private:
    void Destruct();

    ShaderResourceCacheD3D12    m_ShaderResourceCache;
    ShaderVariableManagerD3D12* m_pShaderVarMgrs = nullptr; // [GetNumShaders()]
};

} // namespace Diligent
