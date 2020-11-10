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
    TriangleRenderer(NSString* fragEntry, Uint32 SampleCount = 1)
    {
        auto* const pEnv      = TestingEnvironmentMtl::GetInstance();
        auto* const mtlDevice = pEnv->GetMtlDevice();

        auto* progSrc = [NSString stringWithUTF8String:MSL::DrawTestFunctions.c_str()];
        NSError *errors = nil;
        id <MTLLibrary> library = [mtlDevice newLibraryWithSource:progSrc
                                   options:nil
                                   error:&errors];
        if (library == nil)
        {
            LOG_ERROR_AND_THROW("Failed to create Metal library: ", [errors.localizedDescription cStringUsingEncoding:NSUTF8StringEncoding]);
        }
        id <MTLFunction> vertFunc = [library newFunctionWithName:@"TrisVS"];
        id <MTLFunction> fragFunc = [library newFunctionWithName:fragEntry];
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
        renderPipelineDesc.sampleCount = SampleCount;
        renderPipelineDesc.colorAttachments[0].pixelFormat = pixelFormat;
        m_MtlPipeline = [mtlDevice
                         newRenderPipelineStateWithDescriptor:renderPipelineDesc error:&errors];
    }

    ~TriangleRenderer()
    {
    }

    void Draw(id <MTLRenderCommandEncoder> renderEncoder)
    {
        [renderEncoder setRenderPipelineState:m_MtlPipeline];
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                       vertexStart:0 vertexCount:6];
        [renderEncoder endEncoding];
    }

private:
    id<MTLRenderPipelineState> m_MtlPipeline;
};

} // namespace

void RenderDrawCommandReferenceMtl(ISwapChain* pSwapChain, const float* pClearColor)
{
    auto* const pEnv            = TestingEnvironmentMtl::GetInstance();
    auto* const mtlCommandQueue = pEnv->GetMtlCommandQueue();

    auto* pTestingSwapChainMtl = ValidatedCast<TestingSwapChainMtl>(pSwapChain);
    const auto& SCDesc = pTestingSwapChainMtl->GetDesc();

    auto* pRTV = pTestingSwapChainMtl->GetCurrentBackBufferRTV();
    auto* mtlBackBuffer = ValidatedCast<ITextureViewMtl>(pRTV)->GetMtlTexture();

    id <MTLCommandBuffer> mtlCommandBuffer = [mtlCommandQueue commandBuffer];

    constexpr float Zero[4] = {};
    if (pClearColor == nullptr)
        pClearColor = Zero;

    MTLRenderPassDescriptor* renderPassDesc =
        [MTLRenderPassDescriptor renderPassDescriptor];
    renderPassDesc.colorAttachments[0].texture     = mtlBackBuffer;
    renderPassDesc.colorAttachments[0].loadAction  = MTLLoadActionClear;
    renderPassDesc.colorAttachments[0].clearColor  = MTLClearColorMake(pClearColor[0], pClearColor[1], pClearColor[2], pClearColor[3]);
    renderPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
    id <MTLRenderCommandEncoder> renderEncoder =
        [mtlCommandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
    [renderEncoder setViewport:MTLViewport{0, 0, (double) SCDesc.Width, (double) SCDesc.Height, 0, 1}];

    TriangleRenderer TriRenderer{@"TrisFS"};
    TriRenderer.Draw(renderEncoder);

    [mtlCommandBuffer commit];
}

void RenderPassMSResolveReferenceMtl(ISwapChain* pSwapChain, const float* pClearColor)
{
    auto* const pEnv            = TestingEnvironmentMtl::GetInstance();
    auto* const mtlCommandQueue = pEnv->GetMtlCommandQueue();
    auto* const mtlDevice       = pEnv->GetMtlDevice();

    auto* pTestingSwapChainMtl = ValidatedCast<TestingSwapChainMtl>(pSwapChain);
    const auto& SCDesc = pTestingSwapChainMtl->GetDesc();

    auto* pRTV = pTestingSwapChainMtl->GetCurrentBackBufferRTV();
    auto* mtlBackBuffer = ValidatedCast<ITextureViewMtl>(pRTV)->GetMtlTexture();

    constexpr Uint32 SampleCount = 4;

    MTLTextureDescriptor* msTextureDescriptor = [[MTLTextureDescriptor alloc] init];
    msTextureDescriptor.textureType = MTLTextureType2DMultisample;
    msTextureDescriptor.width       = SCDesc.Width;
    msTextureDescriptor.height      = SCDesc.Height;
    msTextureDescriptor.sampleCount = SampleCount;
    msTextureDescriptor.pixelFormat = mtlBackBuffer.pixelFormat;
    msTextureDescriptor.arrayLength = 1;
    msTextureDescriptor.mipmapLevelCount = 1;
    msTextureDescriptor.storageMode = MTLStorageModePrivate;
    msTextureDescriptor.allowGPUOptimizedContents = true;
    msTextureDescriptor.usage = MTLTextureUsageRenderTarget;
    auto mtlMSTexture = [mtlDevice newTextureWithDescriptor:msTextureDescriptor];
    ASSERT_TRUE(mtlMSTexture != nil);

    id <MTLCommandBuffer> mtlCommandBuffer = [mtlCommandQueue commandBuffer];

    MTLRenderPassDescriptor* renderPassDesc =
        [MTLRenderPassDescriptor renderPassDescriptor];
    renderPassDesc.colorAttachments[0].texture        = mtlMSTexture;
    renderPassDesc.colorAttachments[0].loadAction     = MTLLoadActionClear;
    renderPassDesc.colorAttachments[0].clearColor     = MTLClearColorMake(pClearColor[0], pClearColor[1], pClearColor[2], pClearColor[3]);
    renderPassDesc.colorAttachments[0].storeAction    = MTLStoreActionMultisampleResolve;
    renderPassDesc.colorAttachments[0].resolveTexture = mtlBackBuffer;
    id <MTLRenderCommandEncoder> renderEncoder =
        [mtlCommandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
    [renderEncoder setViewport:MTLViewport{0, 0, (double) SCDesc.Width, (double) SCDesc.Height, 0, 1}];

    TriangleRenderer TriRenderer{@"TrisFS", SampleCount};
    TriRenderer.Draw(renderEncoder);

    [mtlCommandBuffer commit];
}

void RenderPassInputAttachmentReferenceMtl(ISwapChain* pSwapChain, const float* pClearColor)
{
    auto* const pEnv            = TestingEnvironmentMtl::GetInstance();
    auto* const mtlCommandQueue = pEnv->GetMtlCommandQueue();
    auto* const mtlDevice       = pEnv->GetMtlDevice();

    auto* pTestingSwapChainMtl = ValidatedCast<TestingSwapChainMtl>(pSwapChain);
    const auto& SCDesc = pTestingSwapChainMtl->GetDesc();

    auto* pRTV = pTestingSwapChainMtl->GetCurrentBackBufferRTV();
    auto* mtlBackBuffer = ValidatedCast<ITextureViewMtl>(pRTV)->GetMtlTexture();

    MTLTextureDescriptor* inptAttTextureDescriptor = [[MTLTextureDescriptor alloc] init];
    inptAttTextureDescriptor.textureType = MTLTextureType2D;
    inptAttTextureDescriptor.width       = SCDesc.Width;
    inptAttTextureDescriptor.height      = SCDesc.Height;
    inptAttTextureDescriptor.pixelFormat = mtlBackBuffer.pixelFormat;
    inptAttTextureDescriptor.arrayLength = 1;
    inptAttTextureDescriptor.mipmapLevelCount = 1;
    inptAttTextureDescriptor.storageMode = MTLStorageModePrivate;
    inptAttTextureDescriptor.allowGPUOptimizedContents = true;
    inptAttTextureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    auto mtlInputAttachment = [mtlDevice newTextureWithDescriptor:inptAttTextureDescriptor];
    ASSERT_TRUE(mtlInputAttachment != nil);

    id <MTLCommandBuffer> mtlCommandBuffer = [mtlCommandQueue commandBuffer];

    MTLRenderPassDescriptor* subpass0Desc =
        [MTLRenderPassDescriptor renderPassDescriptor];
    subpass0Desc.colorAttachments[0].texture     = mtlInputAttachment;
    subpass0Desc.colorAttachments[0].loadAction  = MTLLoadActionClear;
    subpass0Desc.colorAttachments[0].clearColor  = MTLClearColorMake(0, 0, 0, 0);
    subpass0Desc.colorAttachments[0].storeAction = MTLStoreActionStore;
    id <MTLRenderCommandEncoder> renderEncoder =
        [mtlCommandBuffer renderCommandEncoderWithDescriptor:subpass0Desc];
    [renderEncoder setViewport:MTLViewport{0, 0, (double) SCDesc.Width, (double) SCDesc.Height, 0, 1}];

    TriangleRenderer TriRenderer{@"TrisFS"};
    TriRenderer.Draw(renderEncoder);

    MTLRenderPassDescriptor* subpass1Desc =
        [MTLRenderPassDescriptor renderPassDescriptor];
    subpass1Desc.colorAttachments[0].texture     = mtlBackBuffer;
    subpass1Desc.colorAttachments[0].loadAction  = MTLLoadActionClear;
    subpass1Desc.colorAttachments[0].clearColor  = MTLClearColorMake(pClearColor[0], pClearColor[1], pClearColor[2], pClearColor[3]);
    subpass1Desc.colorAttachments[0].storeAction = MTLStoreActionStore;
    renderEncoder = [mtlCommandBuffer renderCommandEncoderWithDescriptor:subpass1Desc];
    [renderEncoder setViewport:MTLViewport{0, 0, (double) SCDesc.Width, (double) SCDesc.Height, 0, 1}];

    TriangleRenderer TriRendererInptAtt{@"InptAttFS"};
    [renderEncoder setFragmentTexture:mtlInputAttachment atIndex:0];
    TriRendererInptAtt.Draw(renderEncoder);
    
    [mtlCommandBuffer commit];
}

} // namespace Testing

} // namespace Diligent
