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

#include "Metal/TestingEnvironmentMtl.hpp"
#include "Metal/TestingSwapChainMtl.hpp"

//#include "InlineShaders/DrawCommandTestMSL.h"

namespace Diligent
{

namespace Testing
{

namespace
{

class TriangleRenderer
{

public:
    TriangleRenderer(const std::string& FSSource)
    {
        auto* pEnv = TestingEnvironmentMtl::GetInstance();
    }

    ~TriangleRenderer()
    {
    }

    void Draw(Uint32 Width, Uint32 Height, const float* pClearColor)
    {
        //auto* pEnv = TestingEnvironmentGL::GetInstance();
    }

private:
};

} // namespace

void RenderDrawCommandReferenceMtl(ISwapChain* pSwapChain, const float* pClearColor)
{
    //auto* pEnv                = TestingEnvironmentMtl::GetInstance();
    //auto* pContext            = pEnv->GetDeviceContext();
    //auto* pTestingSwapChainMtl = ValidatedCast<TestingSwapChainMtl>(pSwapChain);

    //TriRenderer.Draw(SCDesc.Width, SCDesc.Height, pClearColor);

    // Make sure Diligent Engine will reset all GL states
    //pContext->InvalidateState();
}

void RenderPassMSResolveReferenceMtl(ISwapChain* pSwapChain, const float* pClearColor)
{
    //auto* pEnv                = TestingEnvironmentMtl:GetInstance();
    //auto* pContext            = pEnv->GetDeviceContext();
    //auto* pTestingSwapChainGL = ValidatedCast<TestingSwapChainMtl>(pSwapChain);

    //const auto& SCDesc = pTestingSwapChainGL->GetDesc();

    //TriangleRenderer TriRenderer{GLSL::DrawTest_FS};

    // Make sure Diligent Engine will reset all GL states
    //pContext->InvalidateState();
}

void RenderPassInputAttachmentReferenceMtl(ISwapChain* pSwapChain, const float* pClearColor)
{
    //auto* pEnv                = TestingEnvironmentMtl::GetInstance();
    //auto* pContext            = pEnv->GetDeviceContext();
    //auto* pTestingSwapChainMtl = ValidatedCast<TestingSwapChainMtl>(pSwapChain);

    //const auto& SCDesc = pTestingSwapChainMtl->GetDesc();

    // Make sure Diligent Engine will reset all GL states
    //pContext->InvalidateState();
}

} // namespace Testing

} // namespace Diligent
