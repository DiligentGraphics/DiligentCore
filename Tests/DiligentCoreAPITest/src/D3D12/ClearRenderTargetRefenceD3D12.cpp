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

#include "D3D12/TestingEnvironmentD3D12.hpp"
#include "D3D12/TestingSwapChainD3D12.hpp"

#include "DeviceContextD3D12.h"

namespace Diligent
{

namespace Testing
{

void ClearRenderTargetReferenceD3D12(ISwapChain* pSwapChain, const float ClearColor[])
{
    auto* pEnv                   = TestingEnvironmentD3D12::GetInstance();
    auto* pTestingSwapChainD3D12 = ValidatedCast<TestingSwapChainD3D12>(pSwapChain);

    auto pCmdList = pEnv->CreateGraphicsCommandList();
    pTestingSwapChainD3D12->TransitionRenderTarget(pCmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

    auto RTVDescriptorHandle = pTestingSwapChainD3D12->GetRTVDescriptorHandle();

    pCmdList->ClearRenderTargetView(RTVDescriptorHandle, ClearColor, 0, nullptr);

    pCmdList->Close();
    pEnv->ExecuteCommandList(pCmdList);
}

} // namespace Testing

} // namespace Diligent
