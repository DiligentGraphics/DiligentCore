/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include "DeviceContextMtl.h"
#include "TextureViewMtl.h"

#include "InlineShaders/MeshShaderTestMSL.h"

namespace Diligent
{

namespace Testing
{

void MeshShaderDrawReferenceMtl(ISwapChain* pSwapChain)
{
    auto* const pEnv      = TestingEnvironmentMtl::GetInstance();
    auto const  mtlDevice = pEnv->GetMtlDevice();

    if (@available(macos 13.0, ios 16.0, *))
    {
        @autoreleasepool
        {
            auto* progSrc = [NSString stringWithUTF8String:MSL::MeshShaderTest.c_str()];
            NSError* errors = nil;
            MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
            options.languageVersion = MTLLanguageVersion3_0;
            id<MTLLibrary> library = [mtlDevice newLibraryWithSource:progSrc
                                                             options:options
                                                               error:&errors];
            [options release];
            ASSERT_TRUE(library != nil);
            [library autorelease];

            auto* msFunc = [library newFunctionWithName:@"MSmain"];
            ASSERT_TRUE(msFunc != nil);
            [msFunc autorelease];
            auto* psFunc = [library newFunctionWithName:@"PSmain"];
            ASSERT_TRUE(psFunc != nil);
            [psFunc autorelease];

            auto* pTestingSwapChainMtl = ClassPtrCast<TestingSwapChainMtl>(pSwapChain);
            const auto& SCDesc = pTestingSwapChainMtl->GetDesc();
            auto* pRTV = pTestingSwapChainMtl->GetCurrentBackBufferRTV();
            auto* mtlBackBuffer = ClassPtrCast<ITextureViewMtl>(pRTV)->GetMtlTexture();

            auto* meshPipeDesc = [[MTLMeshRenderPipelineDescriptor alloc] init];
            meshPipeDesc.meshFunction                      = msFunc;
            meshPipeDesc.fragmentFunction                  = psFunc;
            meshPipeDesc.colorAttachments[0].pixelFormat   = mtlBackBuffer.pixelFormat;
            meshPipeDesc.rasterSampleCount                 = 1;
            meshPipeDesc.maxTotalThreadsPerMeshThreadgroup = 4;
            meshPipeDesc.payloadMemoryLength               = 16384;

            auto* meshPipeline = [mtlDevice newRenderPipelineStateWithMeshDescriptor:meshPipeDesc
                                                                             options:MTLPipelineOptionNone
                                                                          reflection:nil
                                                                               error:&errors];
            [meshPipeDesc release];
            ASSERT_TRUE(meshPipeline != nil);
            [meshPipeline autorelease];

            auto* renderPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
            renderPassDesc.colorAttachments[0].texture     = mtlBackBuffer;
            renderPassDesc.colorAttachments[0].loadAction  = MTLLoadActionClear;
            renderPassDesc.colorAttachments[0].clearColor  = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
            renderPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;

            auto* mtlCommandQueue = pEnv->GetMtlCommandQueue();
            auto* mtlCommandBuffer = [mtlCommandQueue commandBuffer];
            auto* renderEncoder = [mtlCommandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
            ASSERT_TRUE(renderEncoder != nil);

            [renderEncoder setViewport:MTLViewport{0, 0, (double)SCDesc.Width, (double)SCDesc.Height, 0, 1}];
            [renderEncoder setCullMode:MTLCullModeBack];
            [renderEncoder setFrontFacingWinding:MTLWindingClockwise];

            [renderEncoder setRenderPipelineState:meshPipeline];
            [renderEncoder drawMeshThreadgroups:MTLSizeMake(1, 1, 1)
                    threadsPerObjectThreadgroup:MTLSizeMake(1, 1, 1)
                      threadsPerMeshThreadgroup:MTLSizeMake(4, 1, 1)];

            [renderEncoder endEncoding];
            [mtlCommandBuffer commit];
            [mtlCommandBuffer waitUntilCompleted];
        }
    }
}


void MeshShaderIndirectDrawReferenceMtl(ISwapChain* pSwapChain)
{
    auto* const pEnv      = TestingEnvironmentMtl::GetInstance();
    auto const  mtlDevice = pEnv->GetMtlDevice();

    if (@available(macos 13.0, ios 16.0, *))
    {
        @autoreleasepool
        {
            auto* progSrc = [NSString stringWithUTF8String:MSL::MeshShaderTest.c_str()];
            NSError* errors = nil;
            MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
            options.languageVersion = MTLLanguageVersion3_0;
            id<MTLLibrary> library = [mtlDevice newLibraryWithSource:progSrc
                                                             options:options
                                                               error:&errors];
            [options release];
            ASSERT_TRUE(library != nil);
            [library autorelease];

            auto* msFunc = [library newFunctionWithName:@"MSmain"];
            ASSERT_TRUE(msFunc != nil);
            [msFunc autorelease];
            auto* psFunc = [library newFunctionWithName:@"PSmain"];
            ASSERT_TRUE(psFunc != nil);
            [psFunc autorelease];

            auto* pTestingSwapChainMtl = ClassPtrCast<TestingSwapChainMtl>(pSwapChain);
            const auto& SCDesc = pTestingSwapChainMtl->GetDesc();
            auto* pRTV = pTestingSwapChainMtl->GetCurrentBackBufferRTV();
            auto* mtlBackBuffer = ClassPtrCast<ITextureViewMtl>(pRTV)->GetMtlTexture();

            auto* meshPipeDesc = [[MTLMeshRenderPipelineDescriptor alloc] init];
            meshPipeDesc.meshFunction                      = msFunc;
            meshPipeDesc.fragmentFunction                  = psFunc;
            meshPipeDesc.colorAttachments[0].pixelFormat   = mtlBackBuffer.pixelFormat;
            meshPipeDesc.rasterSampleCount                 = 1;
            meshPipeDesc.maxTotalThreadsPerMeshThreadgroup = 4;
            meshPipeDesc.payloadMemoryLength               = 16384;

            auto* meshPipeline = [mtlDevice newRenderPipelineStateWithMeshDescriptor:meshPipeDesc
                                                                             options:MTLPipelineOptionNone
                                                                          reflection:nil
                                                                               error:&errors];
            [meshPipeDesc release];
            ASSERT_TRUE(meshPipeline != nil);
            [meshPipeline autorelease];

            // Create indirect buffer with {1, 1, 1} threadgroups
            uint32_t indirectData[3] = {1, 1, 1};
            id<MTLBuffer> indirectBuffer = [mtlDevice newBufferWithBytes:indirectData
                                                                  length:sizeof(indirectData)
                                                                 options:MTLResourceStorageModeShared];
            ASSERT_TRUE(indirectBuffer != nil);
            [indirectBuffer autorelease];

            auto* renderPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
            renderPassDesc.colorAttachments[0].texture     = mtlBackBuffer;
            renderPassDesc.colorAttachments[0].loadAction  = MTLLoadActionClear;
            renderPassDesc.colorAttachments[0].clearColor  = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
            renderPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;

            auto* mtlCommandQueue = pEnv->GetMtlCommandQueue();
            auto* mtlCommandBuffer = [mtlCommandQueue commandBuffer];
            auto* renderEncoder = [mtlCommandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
            ASSERT_TRUE(renderEncoder != nil);

            [renderEncoder setViewport:MTLViewport{0, 0, (double)SCDesc.Width, (double)SCDesc.Height, 0, 1}];
            [renderEncoder setCullMode:MTLCullModeBack];
            [renderEncoder setFrontFacingWinding:MTLWindingClockwise];

            [renderEncoder setRenderPipelineState:meshPipeline];
            [renderEncoder drawMeshThreadgroupsWithIndirectBuffer:indirectBuffer
                                            indirectBufferOffset:0
                                     threadsPerObjectThreadgroup:MTLSizeMake(1, 1, 1)
                                       threadsPerMeshThreadgroup:MTLSizeMake(4, 1, 1)];

            [renderEncoder endEncoding];
            [mtlCommandBuffer commit];
            [mtlCommandBuffer waitUntilCompleted];
        }
    }
}


void AmplificationShaderDrawReferenceMtl(ISwapChain* pSwapChain)
{
    auto* const pEnv      = TestingEnvironmentMtl::GetInstance();
    auto const  mtlDevice = pEnv->GetMtlDevice();

    if (@available(macos 13.0, ios 16.0, *))
    {
        @autoreleasepool
        {
            auto* progSrc = [NSString stringWithUTF8String:MSL::AmplificationShaderTest.c_str()];
            NSError* errors = nil;
            MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
            options.languageVersion = MTLLanguageVersion3_0;
            id<MTLLibrary> library = [mtlDevice newLibraryWithSource:progSrc
                                                             options:options
                                                               error:&errors];
            [options release];
            ASSERT_TRUE(library != nil);
            [library autorelease];

            auto* objFunc = [library newFunctionWithName:@"OBJmain"];
            ASSERT_TRUE(objFunc != nil);
            [objFunc autorelease];
            auto* msFunc = [library newFunctionWithName:@"AmpMSmain"];
            ASSERT_TRUE(msFunc != nil);
            [msFunc autorelease];
            auto* psFunc = [library newFunctionWithName:@"AmpPSmain"];
            ASSERT_TRUE(psFunc != nil);
            [psFunc autorelease];

            auto* pTestingSwapChainMtl = ClassPtrCast<TestingSwapChainMtl>(pSwapChain);
            const auto& SCDesc = pTestingSwapChainMtl->GetDesc();
            auto* pRTV = pTestingSwapChainMtl->GetCurrentBackBufferRTV();
            auto* mtlBackBuffer = ClassPtrCast<ITextureViewMtl>(pRTV)->GetMtlTexture();

            auto* meshPipeDesc = [[MTLMeshRenderPipelineDescriptor alloc] init];
            meshPipeDesc.objectFunction                      = objFunc;
            meshPipeDesc.meshFunction                        = msFunc;
            meshPipeDesc.fragmentFunction                    = psFunc;
            meshPipeDesc.colorAttachments[0].pixelFormat     = mtlBackBuffer.pixelFormat;
            meshPipeDesc.rasterSampleCount                   = 1;
            meshPipeDesc.maxTotalThreadsPerObjectThreadgroup = 8;
            meshPipeDesc.maxTotalThreadsPerMeshThreadgroup   = 1;
            meshPipeDesc.payloadMemoryLength                 = 16384;

            auto* meshPipeline = [mtlDevice newRenderPipelineStateWithMeshDescriptor:meshPipeDesc
                                                                             options:MTLPipelineOptionNone
                                                                          reflection:nil
                                                                               error:&errors];
            [meshPipeDesc release];
            ASSERT_TRUE(meshPipeline != nil);
            [meshPipeline autorelease];

            auto* renderPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
            renderPassDesc.colorAttachments[0].texture     = mtlBackBuffer;
            renderPassDesc.colorAttachments[0].loadAction  = MTLLoadActionClear;
            renderPassDesc.colorAttachments[0].clearColor  = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
            renderPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;

            auto* mtlCommandQueue = pEnv->GetMtlCommandQueue();
            auto* mtlCommandBuffer = [mtlCommandQueue commandBuffer];
            auto* renderEncoder = [mtlCommandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
            ASSERT_TRUE(renderEncoder != nil);

            [renderEncoder setViewport:MTLViewport{0, 0, (double)SCDesc.Width, (double)SCDesc.Height, 0, 1}];
            [renderEncoder setCullMode:MTLCullModeBack];
            [renderEncoder setFrontFacingWinding:MTLWindingClockwise];

            [renderEncoder setRenderPipelineState:meshPipeline];
            [renderEncoder drawMeshThreadgroups:MTLSizeMake(8, 1, 1)
                    threadsPerObjectThreadgroup:MTLSizeMake(8, 1, 1)
                      threadsPerMeshThreadgroup:MTLSizeMake(1, 1, 1)];

            [renderEncoder endEncoding];
            [mtlCommandBuffer commit];
            [mtlCommandBuffer waitUntilCompleted];
        }
    }
}

} // namespace Testing

} // namespace Diligent
