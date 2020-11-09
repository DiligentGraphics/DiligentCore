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

#include <Metal/Metal.h>

#include "Metal/TestingEnvironmentMtl.hpp"
#include "Metal/TestingSwapChainMtl.hpp"

#include "TextureViewMtl.h"

#include "InlineShaders/DrawCommandTestMSL.h"

namespace Diligent
{

namespace Testing
{

namespace
{

class TriangleRenderer
{

public:
    TriangleRenderer(const std::string& ProgramSource)
    {
        auto* const pEnv      = TestingEnvironmentMtl::GetInstance();
        auto const mtlDevice = pEnv->GetMtlDevice();

        auto* progSrc = [NSString stringWithUTF8String:MSL::DrawTestFunctions.c_str()];
        NSError *errors = nil;
        id <MTLLibrary> library = [mtlDevice newLibraryWithSource:progSrc
                                   options:nil
                                   error:&errors];
        id <MTLFunction> vertFunc = [library newFunctionWithName:@"QuadVS"];
        id <MTLFunction> fragFunc = [library newFunctionWithName:@"QuadPS"];
        MTLRenderPipelineDescriptor* renderPipelineDesc =
            [[MTLRenderPipelineDescriptor alloc] init];
        renderPipelineDesc.vertexFunction   = vertFunc;
        renderPipelineDesc.fragmentFunction = fragFunc;
        const auto& SCDesc = pEnv->GetSwapChain()->GetDesc();
        MTLPixelFormat pixelFormat = MTLPixelFormatInvalid;
        switch (SCDesc.ColorBufferFormat)
        {
            case TEX_FORMAT_RGBA8_UNORM:
                pixelFormat = MTLPixelFormatRGBA8Unorm;
                break;

            default:
                UNSUPPORTED("Unexpected swap chain color format");
        }
        renderPipelineDesc.colorAttachments[0].pixelFormat = pixelFormat;
        m_MtlPipeline = [mtlDevice
                         newRenderPipelineStateWithDescriptor:renderPipelineDesc error:&errors];
    }

    ~TriangleRenderer()
    {
    }

    void Draw(const float* pClearColor)
    {
        auto* const pEnv            = TestingEnvironmentMtl::GetInstance();
        auto const mtlCommandQueue = pEnv->GetMtlCommandQueue();

        auto* const pSwapChain = pEnv->GetSwapChain();
        auto* pTestingSwapChainMtl = ValidatedCast<TestingSwapChainMtl>(pSwapChain);
        const auto& SCDesc = pTestingSwapChainMtl->GetDesc();

        auto* pRTV = pTestingSwapChainMtl->GetCurrentBackBufferRTV();
        auto* mtlTexture = ValidatedCast<ITextureViewMtl>(pRTV)->GetMtlTexture();

        id <MTLCommandBuffer> mtlCommandBuffer = [mtlCommandQueue commandBuffer];
         
        MTLRenderPassDescriptor* renderPassDesc =
            [MTLRenderPassDescriptor renderPassDescriptor];
        renderPassDesc.colorAttachments[0].texture = mtlTexture;
        renderPassDesc.colorAttachments[0].loadAction  = MTLLoadActionClear;
        renderPassDesc.colorAttachments[0].clearColor  = MTLClearColorMake(0.0,0.0,0.0,0.0);
        renderPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
        id <MTLRenderCommandEncoder> renderEncoder =
            [mtlCommandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
        [renderEncoder setViewport:MTLViewport{0, 0, (double) SCDesc.Width, (double) SCDesc.Height, 0, 1}];
        [renderEncoder setRenderPipelineState:m_MtlPipeline];
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                       vertexStart:0 vertexCount:6];
        [renderEncoder endEncoding];
        [mtlCommandBuffer commit];
    }

private:
    id<MTLRenderPipelineState> m_MtlPipeline;
};

} // namespace

void RenderDrawCommandReferenceMtl(ISwapChain* pSwapChain, const float* pClearColor)
{
    auto* pEnv                = TestingEnvironmentMtl::GetInstance();
    auto* pContext            = pEnv->GetDeviceContext();

    TriangleRenderer TriRenderer{MSL::DrawTestFunctions};
    TriRenderer.Draw(pClearColor);

    // Make sure Diligent Engine will reset all GL states
    pContext->InvalidateState();
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
