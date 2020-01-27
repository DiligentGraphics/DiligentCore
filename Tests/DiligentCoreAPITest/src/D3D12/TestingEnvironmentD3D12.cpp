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

#include "D3D12/TestingEnvironmentD3D12.hpp"
#include "RenderDeviceD3D12.h"

namespace Diligent
{

namespace Testing
{

void CreateTestingSwapChainD3D12(IRenderDevice*       pDevice,
                                 IDeviceContext*      pContext,
                                 const SwapChainDesc& SCDesc,
                                 ISwapChain**         ppSwapChain);

TestingEnvironmentD3D12::TestingEnvironmentD3D12(RENDER_DEVICE_TYPE deviceType, ADAPTER_TYPE AdapterType, const SwapChainDesc& SCDesc) :
    TestingEnvironment{deviceType, AdapterType, SCDesc},
    m_WaitForGPUEventHandle{CreateEvent(nullptr, false, false, nullptr)}
{
    RefCntAutoPtr<IRenderDeviceD3D12> pRenderDeviceD3D12{m_pDevice, IID_RenderDeviceD3D12};
    m_pd3d12Device = pRenderDeviceD3D12->GetD3D12Device();
    auto hr =
        m_pd3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(m_pd3d12CmdAllocator),
                                               reinterpret_cast<void**>(static_cast<ID3D12CommandAllocator**>(&m_pd3d12CmdAllocator)));
    VERIFY_EXPR(SUCCEEDED(hr));

    hr = m_pd3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(m_pd3d12Fence),
                                     reinterpret_cast<void**>(static_cast<ID3D12Fence**>(&m_pd3d12Fence)));
    VERIFY_EXPR(SUCCEEDED(hr));

    if (m_pSwapChain == nullptr)
    {
        CreateTestingSwapChainD3D12(m_pDevice, m_pDeviceContext, SCDesc, &m_pSwapChain);
    }
}

TestingEnvironmentD3D12::~TestingEnvironmentD3D12()
{
    CloseHandle(m_WaitForGPUEventHandle);
}

CComPtr<ID3D12GraphicsCommandList> TestingEnvironmentD3D12::CreateGraphicsCommandList()
{
    CComPtr<ID3D12GraphicsCommandList> pd3d12CmdList;
    m_pd3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pd3d12CmdAllocator, nullptr, __uuidof(pd3d12CmdList),
                                      reinterpret_cast<void**>(static_cast<ID3D12GraphicsCommandList**>(&pd3d12CmdList)));
    return pd3d12CmdList;
}

void TestingEnvironmentD3D12::IdleCommandQueue(ID3D12CommandQueue* pd3d12Queue)
{
    Uint64 LastSignaledFenceValue = m_NextFenceValue;
    ++m_NextFenceValue;

    pd3d12Queue->Signal(m_pd3d12Fence, LastSignaledFenceValue);

    if (m_pd3d12Fence->GetCompletedValue() < LastSignaledFenceValue)
    {
        m_pd3d12Fence->SetEventOnCompletion(LastSignaledFenceValue, m_WaitForGPUEventHandle);
        WaitForSingleObject(m_WaitForGPUEventHandle, INFINITE);
        VERIFY(m_pd3d12Fence->GetCompletedValue() == LastSignaledFenceValue, "Unexpected signaled fence value");
    }
}

TestingEnvironment* CreateTestingEnvironmentD3D12(RENDER_DEVICE_TYPE deviceType, ADAPTER_TYPE AdapterType, const SwapChainDesc& SCDesc)
{
    return new TestingEnvironmentD3D12{deviceType, AdapterType, SCDesc};
}

} // namespace Testing

} // namespace Diligent
