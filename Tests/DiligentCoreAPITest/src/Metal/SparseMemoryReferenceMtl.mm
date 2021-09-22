/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "RenderDeviceMtl.h"
#include "DeviceContextMtl.h"
#include "TextureMtl.h"

#include "SparseMemoryTest.hpp"

namespace Diligent
{

namespace Testing
{

namespace 
{

struct SparseTexture
{
    id<MTLTexture> mtlTexture = nil;
    id<MTLHeap>    mtlHeap    = nil;
};


SparseTexture CreateSparseTexture(const int4 &Dim) API_AVAILABLE(macos(11), ios(13))
{
    auto const    mtlDevice = TestingEnvironmentMtl::GetInstance()->GetMtlDevice();
    SparseTexture Result;

    @autoreleasepool
    {
        auto* TexDesc = [[[MTLTextureDescriptor alloc] init] autorelease];

        if (Dim.z > 1)
            TexDesc.textureType = MTLTextureType3D;
        else if (Dim.w > 1)
            TexDesc.textureType = MTLTextureType2DArray;
        else
            TexDesc.textureType = MTLTextureType2D;

        TexDesc.pixelFormat  = MTLPixelFormatRGBA8Unorm;
        TexDesc.width        = Dim.x;
        TexDesc.height       = Dim.y;
        TexDesc.depth        = Dim.z;
        TexDesc.arrayLength  = Dim.w;
        TexDesc.sampleCount  = 1;
        TexDesc.usage        = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
        TexDesc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        TexDesc.storageMode  = MTLStorageModePrivate;
        TexDesc.hazardTrackingMode = MTLHazardTrackingModeDefault;
        TexDesc.allowGPUOptimizedContents = YES;
        TexDesc.mipmapLevelCount = ComputeMipLevelsCount(Dim.x, Dim.y, Dim.z);
        
        MTLSizeAndAlign SizeAndAlign = [mtlDevice heapTextureSizeAndAlignWithDescriptor: TexDesc];

        auto* HeapDesc = [[[MTLHeapDescriptor alloc] init] autorelease];
    
        HeapDesc.type               = MTLHeapTypeSparse;
        HeapDesc.storageMode        = MTLStorageModePrivate;
        HeapDesc.cpuCacheMode       = MTLCPUCacheModeDefaultCache;
        HeapDesc.hazardTrackingMode = MTLHazardTrackingModeDefault;
        HeapDesc.size               = SizeAndAlign.size;

        Result.mtlHeap = [mtlDevice newHeapWithDescriptor: HeapDesc];

        if (Result.mtlHeap == nil)
            return {};

        Result.mtlHeap.label = @"Sparse heap";
    
        Result.mtlTexture = [Result.mtlHeap newTextureWithDescriptor: TexDesc];
        if (Result.mtlTexture == nil)
            return {};

        Result.mtlTexture.label = @"Sparse texture";
    }
    return Result;
}


RefCntAutoPtr<ITexture> CreateTextureFromMTLTexture(id<MTLTexture> mtlTexture)
{
    auto* pEnv = TestingEnvironmentMtl::GetInstance();

    RefCntAutoPtr<IRenderDeviceMtl> pDeviceMtl(pEnv->GetDevice(), IID_RenderDeviceMtl);
    if (pDeviceMtl == nullptr)
        return {};
    
    RefCntAutoPtr<ITexture> pTextureWrapper;
    pDeviceMtl->CreateTextureFromMtlResource(mtlTexture, RESOURCE_STATE_UNDEFINED, &pTextureWrapper);
    return pTextureWrapper;
}

} // namespace


void SparseMemorySparseTextureTestMtl(const SparseMemoryTestTextureHelper& Helper)
{
    if (@available(macos 11.0, ios 13.0, *))
    {
        @autoreleasepool
        {
            auto const mtlDevice = TestingEnvironmentMtl::GetInstance()->GetMtlDevice();
            const auto TexDim    = Helper.TextureSize;

            auto Texture = CreateSparseTexture(TexDim);
            ASSERT_TRUE(Texture.mtlTexture != nil);
            ASSERT_TRUE(Texture.mtlHeap != nil);

            auto* pEnv             = TestingEnvironmentMtl::GetInstance();
            auto* mtlCommandQueue  = pEnv->GetMtlCommandQueue();
            auto* mtlCommandBuffer = [mtlCommandQueue commandBuffer]; // Autoreleased
            auto* cmdEncoder       = [mtlCommandBuffer resourceStateCommandEncoder]; // Autoreleased
            ASSERT_TRUE(cmdEncoder != nil);
            
            std::vector<MTLRegion>  Regions;
            std::vector<NSUInteger> MipLevels;
            std::vector<NSUInteger> Slices;
            
            const auto FirstMipInTail = Texture.mtlTexture.firstMipmapInTail;
            const auto TileSize       = [mtlDevice sparseTileSizeWithTextureType: Texture.mtlTexture.textureType
                                                                     pixelFormat: Texture.mtlTexture.pixelFormat
                                                                     sampleCount: Texture.mtlTexture.sampleCount];

            for (Uint32 Slice = 0; Slice < static_cast<Uint32>(TexDim.w); ++Slice)
            {
                for (Uint32 Mip = 0; Mip < FirstMipInTail; ++Mip)
                {
                    const int Width  = std::max(1u, static_cast<Uint32>(TexDim.x) >> Mip);
                    const int Height = std::max(1u, static_cast<Uint32>(TexDim.y) >> Mip);
                    for (int y = 0; y < Height; y += TileSize.height)
                    {
                        for (int x = 0; x < Width; x += TileSize.width)
                        {
                            Regions.push_back(MTLRegionMake2D(x, y,
                                                              std::min(TileSize.width, static_cast<NSUInteger>(Width - x)),
                                                              std::min(TileSize.height, static_cast<NSUInteger>(Height - y))));
                            MipLevels.push_back(Mip);
                            Slices.push_back(Slice);
                        }
                    }
                }

                // Mip tail
                const int Width  = std::max(1u, static_cast<Uint32>(TexDim.x) >> FirstMipInTail);
                const int Height = std::max(1u, static_cast<Uint32>(TexDim.y) >> FirstMipInTail);
                Regions.push_back(MTLRegionMake2D(0, 0, Width, Height));
                MipLevels.push_back(FirstMipInTail);
                Slices.push_back(Slice);
            }
            
            [cmdEncoder updateTextureMappings: Texture.mtlTexture
                                         mode: MTLSparseTextureMappingModeMap
                                      regions: Regions.data()
                                    mipLevels: MipLevels.data()
                                       slices: Slices.data()
                                   numRegions: Regions.size()];
            
            [cmdEncoder endEncoding];
            [mtlCommandBuffer commit];

            auto pTextureWrapper = CreateTextureFromMTLTexture(Texture.mtlTexture);
            ASSERT_NE(pTextureWrapper, nullptr);

            Helper.FillAndDraw(pTextureWrapper);

        } // @autoreleasepool
    }
}


void SparseMemorySparseResidencyTextureTestMtl(const SparseMemoryTestTextureHelper& Helper)
{
    if (@available(macos 11.0, ios 13.0, *))
    {
        @autoreleasepool
        {
            auto const mtlDevice = TestingEnvironmentMtl::GetInstance()->GetMtlDevice();
            const auto TexDim    = Helper.TextureSize;

            auto Texture = CreateSparseTexture(TexDim);
            ASSERT_TRUE(Texture.mtlTexture != nil);
            ASSERT_TRUE(Texture.mtlHeap != nil);

            auto* pEnv             = TestingEnvironmentMtl::GetInstance();
            auto* mtlCommandQueue  = pEnv->GetMtlCommandQueue();
            auto* mtlCommandBuffer = [mtlCommandQueue commandBuffer]; // Autoreleased
            auto* cmdEncoder       = [mtlCommandBuffer resourceStateCommandEncoder]; // Autoreleased
            ASSERT_TRUE(cmdEncoder != nil);
            
            std::vector<MTLRegion>  MapRegions,   UnmapRegions;
            std::vector<NSUInteger> MapMipLevels, UnmapMipLevels;
            std::vector<NSUInteger> MapSlices,    UnmapSlices;

            const auto FirstMipInTail = Texture.mtlTexture.firstMipmapInTail;
            const auto TileSize       = [mtlDevice sparseTileSizeWithTextureType: Texture.mtlTexture.textureType
                                                                     pixelFormat: Texture.mtlTexture.pixelFormat
                                                                     sampleCount: Texture.mtlTexture.sampleCount];

            for (Uint32 Slice = 0; Slice < static_cast<Uint32>(TexDim.w); ++Slice)
            {
                for (Uint32 Mip = 0, Idx = 0; Mip < FirstMipInTail; ++Mip)
                {
                    const int Width  = std::max(1u, static_cast<Uint32>(TexDim.x) >> Mip);
                    const int Height = std::max(1u, static_cast<Uint32>(TexDim.y) >> Mip);
                    for (int y = 0; y < Height; y += TileSize.height)
                    {
                        for (int x = 0; x < Width; x += TileSize.width)
                        {
                            auto Region = MTLRegionMake2D(x, y,
                                                          std::min(TileSize.width, static_cast<NSUInteger>(Width - x)),
                                                          std::min(TileSize.height, static_cast<NSUInteger>(Height - y)));

                            if ((++Idx & 2) == 0)
                            {
                                MapRegions.push_back(Region);
                                MapMipLevels.push_back(Mip);
                                MapSlices.push_back(Slice);
                            }
                            else
                            {
                                UnmapRegions.push_back(Region);
                                UnmapMipLevels.push_back(Mip);
                                UnmapSlices.push_back(Slice);
                            }
                        }
                    }
                }

                // Mip tail
                const int Width  = std::max(1u, static_cast<Uint32>(TexDim.x) >> FirstMipInTail);
                const int Height = std::max(1u, static_cast<Uint32>(TexDim.y) >> FirstMipInTail);
                MapRegions.push_back(MTLRegionMake2D(0, 0, Width, Height));
                MapMipLevels.push_back(FirstMipInTail);
                MapSlices.push_back(Slice);
            }
            
            [cmdEncoder updateTextureMappings: Texture.mtlTexture
                                         mode: MTLSparseTextureMappingModeMap
                                      regions: MapRegions.data()
                                    mipLevels: MapMipLevels.data()
                                       slices: MapSlices.data()
                                   numRegions: MapRegions.size()];
            
            [cmdEncoder updateTextureMappings: Texture.mtlTexture
                                         mode: MTLSparseTextureMappingModeUnmap
                                      regions: UnmapRegions.data()
                                    mipLevels: UnmapMipLevels.data()
                                       slices: UnmapSlices.data()
                                   numRegions: UnmapRegions.size()];
            
            [cmdEncoder endEncoding];
            [mtlCommandBuffer commit];

            auto pTextureWrapper = CreateTextureFromMTLTexture(Texture.mtlTexture);
            ASSERT_NE(pTextureWrapper, nullptr);

            Helper.FillAndDraw(pTextureWrapper);

        } // @autoreleasepool
    }
}


void SparseMemorySparseResidencyAliasedTextureTestMtl(const SparseMemoryTestTextureHelper& Helper)
{
    UNEXPECTED("Not supported in Metal");
}


void SparseMemorySparseTexture3DTestMtl(const SparseMemoryTestTextureHelper& Helper)
{
    if (@available(macos 11.0, ios 13.0, *))
    {
        @autoreleasepool
        {
            auto const mtlDevice = TestingEnvironmentMtl::GetInstance()->GetMtlDevice();
            const auto TexDim    = Helper.TextureSize;

            auto Texture = CreateSparseTexture(TexDim);
            ASSERT_TRUE(Texture.mtlTexture != nil);
            ASSERT_TRUE(Texture.mtlHeap != nil);

            auto* pEnv             = TestingEnvironmentMtl::GetInstance();
            auto* mtlCommandQueue  = pEnv->GetMtlCommandQueue();
            auto* mtlCommandBuffer = [mtlCommandQueue commandBuffer]; // Autoreleased
            auto* cmdEncoder       = [mtlCommandBuffer resourceStateCommandEncoder]; // Autoreleased
            ASSERT_TRUE(cmdEncoder != nil);
            
            std::vector<MTLRegion>  Regions;
            std::vector<NSUInteger> MipLevels;
            std::vector<NSUInteger> Slices;
            
            const auto FirstMipInTail = Texture.mtlTexture.firstMipmapInTail;
            const auto TileSize       = [mtlDevice sparseTileSizeWithTextureType: Texture.mtlTexture.textureType
                                                                     pixelFormat: Texture.mtlTexture.pixelFormat
                                                                     sampleCount: Texture.mtlTexture.sampleCount];

            for (Uint32 Mip = 0; Mip < FirstMipInTail; ++Mip)
            {
                const int Width  = std::max(1u, static_cast<Uint32>(TexDim.x) >> Mip);
                const int Height = std::max(1u, static_cast<Uint32>(TexDim.y) >> Mip);
                const int Depth  = std::max(1u, static_cast<Uint32>(TexDim.z) >> Mip);
                for (int z = 0; z < Depth; z += TileSize.depth)
                {
                    for (int y = 0; y < Height; y += TileSize.height)
                    {
                        for (int x = 0; x < Width; x += TileSize.width)
                        {
                            Regions.push_back(MTLRegionMake3D(x, y, z,
                                                              std::min(TileSize.width,  static_cast<NSUInteger>(Width  - x)),
                                                              std::min(TileSize.height, static_cast<NSUInteger>(Height - y)),
                                                              std::min(TileSize.depth,  static_cast<NSUInteger>(Depth  - z))));
                            MipLevels.push_back(Mip);
                            Slices.push_back(0);
                        }
                    }
                }
            }

            // Mip tail
            const int Width  = std::max(1u, static_cast<Uint32>(TexDim.x) >> FirstMipInTail);
            const int Height = std::max(1u, static_cast<Uint32>(TexDim.y) >> FirstMipInTail);
            const int Depth  = std::max(1u, static_cast<Uint32>(TexDim.z) >> FirstMipInTail);
            Regions.push_back(MTLRegionMake3D(0, 0, 0, Width, Height, Depth));
            MipLevels.push_back(FirstMipInTail);
            Slices.push_back(0);
            
            [cmdEncoder updateTextureMappings: Texture.mtlTexture
                                         mode: MTLSparseTextureMappingModeMap
                                      regions: Regions.data()
                                    mipLevels: MipLevels.data()
                                       slices: Slices.data()
                                   numRegions: Regions.size()];
            
            [cmdEncoder endEncoding];
            [mtlCommandBuffer commit];

            auto pTextureWrapper = CreateTextureFromMTLTexture(Texture.mtlTexture);
            ASSERT_NE(pTextureWrapper, nullptr);

            Helper.FillAndDraw(pTextureWrapper);

        } // @autoreleasepool
    }
}

} // namespace Testing

} // namespace Diligent
