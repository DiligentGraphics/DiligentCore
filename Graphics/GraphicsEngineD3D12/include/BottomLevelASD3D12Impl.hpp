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
/// Declaration of Diligent::BottomLevelASD3D12Impl class

#include "BottomLevelASD3D12.h"
#include "RenderDeviceD3D12.h"
#include "BottomLevelASBase.hpp"
#include "D3D12ResourceBase.hpp"
#include "RenderDeviceD3D12Impl.hpp"

namespace Diligent
{

/// Bottom-level acceleration structure object implementation in Direct3D12 backend.
class BottomLevelASD3D12Impl final : public BottomLevelASBase<IBottomLevelASD3D12, RenderDeviceD3D12Impl>, public D3D12ResourceBase
{
public:
    using TBottomLevelASBase = BottomLevelASBase<IBottomLevelASD3D12, RenderDeviceD3D12Impl>;

    BottomLevelASD3D12Impl(IReferenceCounters*          pRefCounters,
                           class RenderDeviceD3D12Impl* pDeviceD3D12,
                           const BottomLevelASDesc&     Desc,
                           bool                         bIsDeviceInternal = false);
    ~BottomLevelASD3D12Impl();

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final;

    /// Implementation of IBottomLevelAS::GetScratchBufferSizes() in DirectX 12 backend.
    virtual ScratchBufferSizes DILIGENT_CALL_TYPE GetScratchBufferSizes() const override { return m_ScratchSize; }

    /// Implementation of IBottomLevelASD3D12::GetD3D12BLAS().
    virtual ID3D12Resource* DILIGENT_CALL_TYPE GetD3D12BLAS() override final { return GetD3D12Resource(); }

    /// Implementation of IBottomLevelAS::GetNativeHandle() in Direct3D12 backend.
    virtual void* DILIGENT_CALL_TYPE GetNativeHandle() override final { return GetD3D12BLAS(); }

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress()
    {
        return GetD3D12Resource()->GetGPUVirtualAddress();
    }

private:
    ScratchBufferSizes m_ScratchSize;
};

} // namespace Diligent
