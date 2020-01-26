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

#include "pch.h"

#include <D3Dcompiler.h>

#include "ShaderD3D12Impl.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "DataBlobImpl.hpp"

namespace Diligent
{

static const std::string GetD3D12ShaderModel(RenderDeviceD3D12Impl* /*pDevice*/, const ShaderVersion& HLSLVersion)
{
    if (HLSLVersion.Major == 0 && HLSLVersion.Minor == 0)
    {
        //auto d3dDeviceFeatureLevel = pDevice->GetD3DFeatureLevel();

        // Direct3D12 supports shader model 5.1 on all feature levels.
        // https://docs.microsoft.com/en-us/windows/win32/direct3d12/hardware-feature-levels#feature-level-support
        return "5_1";
    }
    else
    {
        return std::to_string(Uint32{HLSLVersion.Major}) + '_' + std::to_string(Uint32{HLSLVersion.Minor});
    }
}

ShaderD3D12Impl::ShaderD3D12Impl(IReferenceCounters*     pRefCounters,
                                 RenderDeviceD3D12Impl*  pRenderDeviceD3D12,
                                 const ShaderCreateInfo& ShaderCI) :
    // clang-format off
    TShaderBase
    {
        pRefCounters,
        pRenderDeviceD3D12,
        ShaderCI.Desc
    },
    ShaderD3DBase{ShaderCI, GetD3D12ShaderModel(pRenderDeviceD3D12, ShaderCI.HLSLVersion).c_str()}
// clang-format on
{
    // Load shader resources
    auto& Allocator  = GetRawAllocator();
    auto* pRawMem    = ALLOCATE(Allocator, "Allocator for ShaderResources", ShaderResourcesD3D12, 1);
    auto* pResources = new (pRawMem) ShaderResourcesD3D12(m_pShaderByteCode, m_Desc, ShaderCI.UseCombinedTextureSamplers ? ShaderCI.CombinedSamplerSuffix : nullptr);
    m_pShaderResources.reset(pResources, STDDeleterRawMem<ShaderResourcesD3D12>(Allocator));
}

ShaderD3D12Impl::~ShaderD3D12Impl()
{
}

void ShaderD3D12Impl::QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)
{
    if (ppInterface == nullptr)
        return;
    if (IID == IID_ShaderD3D || IID == IID_ShaderD3D12)
    {
        *ppInterface = this;
        (*ppInterface)->AddRef();
    }
    else
    {
        TShaderBase::QueryInterface(IID, ppInterface);
    }
}

} // namespace Diligent
