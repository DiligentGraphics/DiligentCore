/*     Copyright 2019 Diligent Graphics LLC
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "D3D11/TestingEnvironmentD3D11.h"
#include "D3D11/TestingSwapChainD3D11.h"

#include "InlineShaders/ComputeShaderTestHLSL.h"

namespace Diligent
{

namespace Testing
{

void ComputeShaderReferenceD3D11(ISwapChain* pSwapChain)
{
    auto* pEnvD3D11              = TestingEnvironmentD3D11::GetInstance();
    auto* pd3d11Context          = pEnvD3D11->GetD3D11Context();
    auto* pTestingSwapChainD3D11 = ValidatedCast<TestingSwapChainD3D11>(pSwapChain);

    pd3d11Context->ClearState();

    auto pCS = pEnvD3D11->CreateComputeShader(HLSL::FillTextureCS);
    ASSERT_NE(pCS, nullptr);

    pd3d11Context->CSSetShader(pCS, nullptr, 0);
    ID3D11UnorderedAccessView* pUAVs[] = {pTestingSwapChainD3D11->GetD3D11UAV()};
    pd3d11Context->CSSetUnorderedAccessViews(0, 1, pUAVs, nullptr);

    const auto& SCDesc = pSwapChain->GetDesc();
    pd3d11Context->Dispatch((SCDesc.Width + 15) / 16, (SCDesc.Height + 15) / 16, 1);

    pd3d11Context->ClearState();
}

} // namespace Testing

} // namespace Diligent
