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

#include "D3D12/TestingEnvironmentD3D12.hpp"
#include "D3D12/TestingSwapChainD3D12.hpp"
#include "../include/d3dx12_win.h"

#include "RenderDeviceD3D12.h"
#include "TextureD3D12.h"
#include "BufferD3D12.h"
#include "CommandQueueD3D12.h"

#ifdef DILIGENT_TEST_ENABLE_D3D_NVAPI
#    include <dxgi1_4.h>
#    include "nvapi.h"
#endif

#include "SparseMemoryTest.hpp"

namespace Diligent
{

namespace Testing
{

namespace
{

CComPtr<ID3D12Resource> CreateSparseBuffer(Uint64 Size)
{
    auto* pd3d12Device = TestingEnvironmentD3D12::GetInstance()->GetD3D12Device();

    D3D12_RESOURCE_DESC Desc{};
    Desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    Desc.Alignment        = 0;
    Desc.Width            = Size;
    Desc.Height           = 1;
    Desc.DepthOrArraySize = 1;
    Desc.MipLevels        = 1;
    Desc.SampleDesc.Count = 1;
    Desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    CComPtr<ID3D12Resource> pResource;
    pd3d12Device->CreateReservedResource(&Desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&pResource));
    return pResource;
}

CComPtr<ID3D12Resource> CreateSparseTexture(const int4& Dim, Uint32 MipLevels, ID3D12Heap* pHeap)
{
    auto* pd3d12Device = TestingEnvironmentD3D12::GetInstance()->GetD3D12Device();

    MipLevels = ComputeMipLevelsCount(Dim.x, Dim.y, Dim.z);

    D3D12_RESOURCE_DESC Desc{};
    Desc.Dimension        = Dim.z > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    Desc.Alignment        = 0;
    Desc.Width            = Dim.x;
    Desc.Height           = Dim.y;
    Desc.DepthOrArraySize = static_cast<UINT16>(std::max(Dim.z, Dim.w));
    Desc.MipLevels        = static_cast<UINT16>(MipLevels);
    Desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    Desc.SampleDesc.Count = 1;
    Desc.Layout           = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
    Desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    CComPtr<ID3D12Resource> pResource;
#ifdef DILIGENT_TEST_ENABLE_D3D_NVAPI
    if (Dim.w > 1 && pHeap)
    {
        if (NvAPI_D3D12_CreateReservedResource(pd3d12Device, &Desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&pResource), true, pHeap) == NVAPI_OK)
            return pResource;
    }
#endif

    pd3d12Device->CreateReservedResource(&Desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&pResource));
    return pResource;
}

CComPtr<ID3D12Heap> CreateHeap(Uint64 NumTiles, bool Is2DArray = false)
{
    auto* pd3d12Device = TestingEnvironmentD3D12::GetInstance()->GetD3D12Device();

    D3D12_HEAP_DESC Desc{};
    Desc.SizeInBytes                     = NumTiles * D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;
    Desc.Properties.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    Desc.Properties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    Desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    Desc.Properties.CreationNodeMask     = 0; // equivalent to 1
    Desc.Properties.VisibleNodeMask      = 0; // equivalent to 1
    Desc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    Desc.Flags                           = D3D12_HEAP_FLAG_NONE;

    CComPtr<ID3D12Heap> pHeap;
#ifdef DILIGENT_TEST_ENABLE_D3D_NVAPI
    if (Is2DArray)
    {
        if (NvAPI_D3D12_CreateHeap(pd3d12Device, &Desc, IID_PPV_ARGS(&pHeap)) == NVAPI_OK)
            return pHeap;
    }
#endif
    pd3d12Device->CreateHeap(&Desc, IID_PPV_ARGS(&pHeap));
    return pHeap;
}

template <typename FnType>
void WithCmdQueue(const FnType& Fn)
{
    auto* pEnv     = TestingEnvironmentD3D12::GetInstance();
    auto* pContext = pEnv->GetDeviceContext();

    RefCntAutoPtr<ICommandQueueD3D12> pQueueD3D12{pContext->LockCommandQueue(), IID_CommandQueueD3D12};

    auto* pd3d12Queue = pQueueD3D12->GetD3D12CommandQueue();

    Fn(pd3d12Queue);

    pEnv->IdleCommandQueue(pd3d12Queue);

    pContext->UnlockCommandQueue();
}

RefCntAutoPtr<IBuffer> CreateBufferFromD3D11Resource(ID3D12Resource* pBuffer)
{
    auto* pEnv = TestingEnvironmentD3D12::GetInstance();

    RefCntAutoPtr<IRenderDeviceD3D12> pDeviceD3D12(pEnv->GetDevice(), IID_RenderDeviceD3D12);
    if (pDeviceD3D12 == nullptr)
        return {};

    BufferDesc BuffDesc;
    BuffDesc.Name              = "Sparse buffer from D3D11 resource";
    BuffDesc.Usage             = USAGE_SPARSE;
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = 4;
    BuffDesc.MiscFlags         = MISC_BUFFER_FLAG_SPARSE_ALIASING;

    RefCntAutoPtr<IBuffer> pBufferWrapper;
    pDeviceD3D12->CreateBufferFromD3DResource(pBuffer, BuffDesc, RESOURCE_STATE_UNDEFINED, &pBufferWrapper);
    return pBufferWrapper;
}

RefCntAutoPtr<ITexture> CreateTextureFromD3D11Resource(ID3D12Resource* pTexture)
{
    auto* pEnv = TestingEnvironmentD3D12::GetInstance();

    RefCntAutoPtr<IRenderDeviceD3D12> pDeviceD3D12(pEnv->GetDevice(), IID_RenderDeviceD3D12);
    if (pDeviceD3D12 == nullptr)
        return {};

    RefCntAutoPtr<ITexture> pTextureWrapper;
    pDeviceD3D12->CreateTextureFromD3DResource(pTexture, RESOURCE_STATE_UNDEFINED, &pTextureWrapper);
    return pTextureWrapper;
}

TextureSparseProperties GetTexture2DSparseProperties(ID3D12Resource* pResource)
{
    const auto d3d12Desc = pResource->GetDesc();
    VERIFY_EXPR(d3d12Desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

    TextureDesc Desc;
    Desc.Type        = d3d12Desc.DepthOrArraySize > 1 ? RESOURCE_DIM_TEX_2D_ARRAY : RESOURCE_DIM_TEX_2D;
    Desc.Width       = static_cast<Uint32>(d3d12Desc.Width);
    Desc.Height      = d3d12Desc.Height;
    Desc.ArraySize   = d3d12Desc.DepthOrArraySize;
    Desc.Format      = TEX_FORMAT_RGBA8_UNORM;
    Desc.MipLevels   = d3d12Desc.MipLevels;
    Desc.SampleCount = 1;
    Desc.Usage       = USAGE_SPARSE;

    return GetTextureSparsePropertiesForStandardBlocks(Desc);
}

void UpdateTileMappings(
    ID3D12CommandQueue*                    pd3d12Queue,
    ID3D12Resource*                        pResource,
    UINT                                   NumResourceRegions,
    const D3D12_TILED_RESOURCE_COORDINATE* pResourceRegionStartCoordinates,
    const D3D12_TILE_REGION_SIZE*          pResourceRegionSizes,
    ID3D12Heap*                            pHeap,
    UINT                                   NumRanges,
    const D3D12_TILE_RANGE_FLAGS*          pRangeFlags,
    const UINT*                            pHeapRangeStartOffsets,
    const UINT*                            pRangeTileCounts,
    D3D12_TILE_MAPPING_FLAGS               Flags,
    bool                                   Is2DArray)
{
#ifdef DILIGENT_TEST_ENABLE_D3D_NVAPI
    if (Is2DArray)
    {
        NvAPI_D3D12_UpdateTileMappings(
            pd3d12Queue,
            pResource,
            NumResourceRegions,
            pResourceRegionStartCoordinates,
            pResourceRegionSizes,
            pHeap,
            NumRanges,
            pRangeFlags,
            pHeapRangeStartOffsets,
            pRangeTileCounts,
            Flags);
    }
    else
#endif
    {
        pd3d12Queue->UpdateTileMappings(
            pResource,
            NumResourceRegions,
            pResourceRegionStartCoordinates,
            pResourceRegionSizes,
            pHeap,
            NumRanges,
            pRangeFlags,
            pHeapRangeStartOffsets,
            pRangeTileCounts,
            Flags);
    }
}

} // namespace


void SparseMemorySparseBufferTestD3D12(const SparseMemoryTestBufferHelper& Helper)
{
    auto pBuffer = CreateSparseBuffer(Helper.BufferSize);
    ASSERT_NE(pBuffer, nullptr);

    auto pHeap = CreateHeap(8);
    ASSERT_NE(pHeap, nullptr);

    WithCmdQueue([&](ID3D12CommandQueue* pd3d12Queue) {
        D3D12_TILED_RESOURCE_COORDINATE Coords[4]   = {};
        D3D12_TILE_REGION_SIZE          RegSizes[4] = {};

        Coords[0].X = 0;
        Coords[1].X = 1;
        Coords[2].X = 2;
        Coords[3].X = 3;

        RegSizes[0].NumTiles = 1;
        RegSizes[1].NumTiles = 1;
        RegSizes[2].NumTiles = 1;
        RegSizes[3].NumTiles = 1;

        UINT TilePoolStartOffsets[] = {0, 1, 3, 5};
        UINT RangeTileCounts[]      = {1, 1, 1, 1};

        pd3d12Queue->UpdateTileMappings(pBuffer,
                                        _countof(Coords),
                                        Coords,
                                        RegSizes,
                                        pHeap,
                                        _countof(TilePoolStartOffsets),
                                        nullptr,
                                        TilePoolStartOffsets,
                                        RangeTileCounts,
                                        D3D12_TILE_MAPPING_FLAG_NONE);
    });

    auto pBufferWrapper = CreateBufferFromD3D11Resource(pBuffer);
    ASSERT_NE(pBufferWrapper, nullptr);

    Helper.FillAndDraw(pBufferWrapper);
}

void SparseMemorySparseResidentBufferTestD3D12(const SparseMemoryTestBufferHelper& Helper)
{
    auto pBuffer = CreateSparseBuffer(Helper.BufferSize);
    ASSERT_NE(pBuffer, nullptr);

    auto pHeap = CreateHeap(8);
    ASSERT_NE(pHeap, nullptr);

    WithCmdQueue([&](ID3D12CommandQueue* pd3d12Queue) {
        D3D12_TILED_RESOURCE_COORDINATE Coords[4]   = {};
        D3D12_TILE_REGION_SIZE          RegSizes[4] = {};

        Coords[0].X = 0;
        Coords[1].X = 2;
        Coords[2].X = 3;
        Coords[3].X = 6;

        RegSizes[0].NumTiles = 1;
        RegSizes[1].NumTiles = 1;
        RegSizes[2].NumTiles = 1;
        RegSizes[3].NumTiles = 1;

        UINT TilePoolStartOffsets[] = {0, 1, 3, 5};
        UINT RangeTileCounts[]      = {1, 1, 1, 1};

        pd3d12Queue->UpdateTileMappings(pBuffer,
                                        _countof(Coords),
                                        Coords,
                                        RegSizes,
                                        pHeap,
                                        _countof(TilePoolStartOffsets),
                                        nullptr,
                                        TilePoolStartOffsets,
                                        RangeTileCounts,
                                        D3D12_TILE_MAPPING_FLAG_NONE);
    });

    auto pBufferWrapper = CreateBufferFromD3D11Resource(pBuffer);
    ASSERT_NE(pBufferWrapper, nullptr);

    Helper.FillAndDraw(pBufferWrapper);
}


void SparseMemorySparseResidentAliasedBufferTestD3D12(const SparseMemoryTestBufferHelper& Helper)
{
    auto pBuffer = CreateSparseBuffer(Helper.BufferSize);
    ASSERT_NE(pBuffer, nullptr);

    auto pHeap = CreateHeap(8);
    ASSERT_NE(pHeap, nullptr);

    WithCmdQueue([&](ID3D12CommandQueue* pd3d12Queue) {
        D3D12_TILED_RESOURCE_COORDINATE Coords[5]   = {};
        D3D12_TILE_REGION_SIZE          RegSizes[5] = {};

        Coords[0].X = 0;
        Coords[1].X = 1;
        Coords[2].X = 2;
        Coords[3].X = 3;
        Coords[4].X = 5;

        RegSizes[0].NumTiles = 1;
        RegSizes[1].NumTiles = 1;
        RegSizes[2].NumTiles = 1;
        RegSizes[3].NumTiles = 1;
        RegSizes[4].NumTiles = 1;

        UINT TilePoolStartOffsets[] = {0, 2, 0, 1, 5};
        UINT RangeTileCounts[]      = {1, 1, 1, 1, 1};

        pd3d12Queue->UpdateTileMappings(pBuffer,
                                        _countof(Coords),
                                        Coords,
                                        RegSizes,
                                        pHeap,
                                        _countof(TilePoolStartOffsets),
                                        nullptr,
                                        TilePoolStartOffsets,
                                        RangeTileCounts,
                                        D3D12_TILE_MAPPING_FLAG_NONE);
    });

    auto pBufferWrapper = CreateBufferFromD3D11Resource(pBuffer);
    ASSERT_NE(pBufferWrapper, nullptr);

    Helper.FillAndDraw(pBufferWrapper);
}


void SparseMemorySparseTextureTestD3D12(const SparseMemoryTestTextureHelper& Helper)
{
    const auto   TexDim         = Helper.TextureSize;
    Uint32       MipLevels      = 0;
    const Uint32 NumTilesInHeap = 8 * TexDim.w;
    const bool   IsArray        = TexDim.w > 1;

    auto pHeap = CreateHeap(NumTilesInHeap, IsArray);
    ASSERT_NE(pHeap, nullptr);

    auto pTexture = CreateSparseTexture(TexDim, MipLevels, pHeap);
    ASSERT_NE(pTexture, nullptr);

    const auto TexSparseProps = GetTexture2DSparseProperties(pTexture);

    WithCmdQueue([&](ID3D12CommandQueue* pd3d12Queue) {
        std::vector<D3D12_TILED_RESOURCE_COORDINATE> Coordinates;
        std::vector<D3D12_TILE_REGION_SIZE>          RegionSizes;
        std::vector<UINT>                            StartOffsets;
        std::vector<UINT>                            RangeTileCounts;

        const auto NumTilesX = (TexDim.x + TexSparseProps.TileSize[0] - 1) / TexSparseProps.TileSize[0];
        const auto NumTilesY = (TexDim.y + TexSparseProps.TileSize[1] - 1) / TexSparseProps.TileSize[1];
        const auto ArraySize = static_cast<Uint32>(TexDim.w);

        Uint32 MemOffsetInTiles = 0;
        for (Uint32 Slice = 0; Slice < ArraySize; ++Slice)
        {
            for (Uint32 Mip = 0; Mip < TexSparseProps.FirstMipInTail; ++Mip)
            {
                const auto TilesX = std::max(1u, NumTilesX >> Mip);
                const auto TilesY = std::max(1u, NumTilesY >> Mip);
                for (Uint32 y = 0; y < TilesY; ++y)
                {
                    for (Uint32 x = 0; x < TilesX; ++x)
                    {
                        Coordinates.emplace_back();
                        auto& Coord = Coordinates.back();

                        Coord.X           = x;
                        Coord.Y           = y;
                        Coord.Z           = 0;
                        Coord.Subresource = D3D12CalcSubresource(Mip, Slice, 0, MipLevels, ArraySize);

                        RegionSizes.emplace_back();
                        auto& Size = RegionSizes.back();

                        Size.Width    = 1;
                        Size.Height   = 1;
                        Size.Depth    = 1;
                        Size.UseBox   = TRUE;
                        Size.NumTiles = 1;

                        StartOffsets.emplace_back(MemOffsetInTiles);
                        RangeTileCounts.emplace_back(1);
                        ++MemOffsetInTiles;
                    }
                }
            }

            // Mip tail
            const auto NumTilesForPackedMips = TexSparseProps.MipTailSize / TexSparseProps.BlockSize;
            for (Uint32 OffsetInMipTail = 0; OffsetInMipTail < NumTilesForPackedMips; ++OffsetInMipTail)
            {
                Coordinates.emplace_back();
                auto& Coord = Coordinates.back();

                Coord.X           = OffsetInMipTail;
                Coord.Subresource = D3D12CalcSubresource(TexSparseProps.FirstMipInTail, Slice, 0, MipLevels, ArraySize);

                RegionSizes.emplace_back();
                auto& Size = RegionSizes.back();

                Size.NumTiles = 1;

                StartOffsets.emplace_back(MemOffsetInTiles);
                RangeTileCounts.emplace_back(1);
                ++MemOffsetInTiles;
            }
        }
        VERIFY_EXPR(MemOffsetInTiles <= NumTilesInHeap);

        UpdateTileMappings(pd3d12Queue,
                           pTexture,
                           static_cast<UINT>(Coordinates.size()),
                           Coordinates.data(),
                           RegionSizes.data(),
                           pHeap,
                           static_cast<UINT>(StartOffsets.size()),
                           nullptr,
                           StartOffsets.data(),
                           RangeTileCounts.data(),
                           D3D12_TILE_MAPPING_FLAG_NONE,
                           IsArray);
    });

    auto pTextureWrapper = CreateTextureFromD3D11Resource(pTexture);
    ASSERT_NE(pTextureWrapper, nullptr);

    Helper.FillAndDraw(pTextureWrapper);
}


void SparseMemorySparseResidencyTextureTestD3D12(const SparseMemoryTestTextureHelper& Helper)
{
    const auto   TexDim         = Helper.TextureSize;
    Uint32       MipLevels      = 0;
    const Uint32 NumTilesInHeap = 8 * TexDim.w;
    const bool   IsArray        = TexDim.w > 1;

    auto pHeap = CreateHeap(NumTilesInHeap, IsArray);
    ASSERT_NE(pHeap, nullptr);

    auto pTexture = CreateSparseTexture(TexDim, MipLevels, pHeap);
    ASSERT_NE(pTexture, nullptr);

    const auto TexSparseProps = GetTexture2DSparseProperties(pTexture);

    WithCmdQueue([&](ID3D12CommandQueue* pd3d12Queue) {
        std::vector<D3D12_TILED_RESOURCE_COORDINATE> Coordinates;
        std::vector<D3D12_TILE_REGION_SIZE>          RegionSizes;
        std::vector<D3D12_TILE_RANGE_FLAGS>          RangeFlags;
        std::vector<UINT>                            StartOffsets;
        std::vector<UINT>                            RangeTileCounts;

        const auto NumTilesX = (TexDim.x + TexSparseProps.TileSize[0] - 1) / TexSparseProps.TileSize[0];
        const auto NumTilesY = (TexDim.y + TexSparseProps.TileSize[1] - 1) / TexSparseProps.TileSize[1];
        const auto ArraySize = static_cast<Uint32>(TexDim.w);

        Uint32 MemOffsetInTiles = 0;
        for (Uint32 Slice = 0; Slice < ArraySize; ++Slice)
        {
            for (Uint32 Mip = 0, Idx = 0; Mip < TexSparseProps.FirstMipInTail; ++Mip)
            {
                const auto TilesX = std::max(1u, NumTilesX >> Mip);
                const auto TilesY = std::max(1u, NumTilesY >> Mip);
                for (Uint32 y = 0; y < TilesY; ++y)
                {
                    for (Uint32 x = 0; x < TilesX; ++x)
                    {
                        Coordinates.emplace_back();
                        auto& Coord = Coordinates.back();

                        Coord.X           = x;
                        Coord.Y           = y;
                        Coord.Z           = 0;
                        Coord.Subresource = D3D12CalcSubresource(Mip, Slice, 0, MipLevels, ArraySize);

                        RegionSizes.emplace_back();
                        auto& Size = RegionSizes.back();

                        Size.Width    = 1;
                        Size.Height   = 1;
                        Size.Depth    = 1;
                        Size.UseBox   = TRUE;
                        Size.NumTiles = 1;

                        RangeTileCounts.emplace_back(1);

                        if ((++Idx & 2) == 0)
                        {
                            StartOffsets.emplace_back(MemOffsetInTiles);
                            RangeFlags.emplace_back(D3D12_TILE_RANGE_FLAG_NONE);
                            ++MemOffsetInTiles;
                        }
                        else
                        {
                            StartOffsets.emplace_back(0);
                            RangeFlags.emplace_back(D3D12_TILE_RANGE_FLAG_NULL);
                        }
                    }
                }
            }

            // Mip tail
            const auto NumTilesForPackedMips = TexSparseProps.MipTailSize / TexSparseProps.BlockSize;
            for (Uint32 OffsetInMipTail = 0; OffsetInMipTail < NumTilesForPackedMips; ++OffsetInMipTail)
            {
                Coordinates.emplace_back();
                auto& Coord = Coordinates.back();

                Coord.X           = OffsetInMipTail;
                Coord.Subresource = D3D12CalcSubresource(TexSparseProps.FirstMipInTail, Slice, 0, MipLevels, ArraySize);

                RegionSizes.emplace_back();
                auto& Size = RegionSizes.back();

                Size.NumTiles = 1;

                RangeFlags.emplace_back(D3D12_TILE_RANGE_FLAG_NONE);
                StartOffsets.emplace_back(MemOffsetInTiles);
                RangeTileCounts.emplace_back(1);
                ++MemOffsetInTiles;
            }
        }
        VERIFY_EXPR(MemOffsetInTiles <= NumTilesInHeap);

        UpdateTileMappings(pd3d12Queue,
                           pTexture,
                           static_cast<UINT>(Coordinates.size()),
                           Coordinates.data(),
                           RegionSizes.data(),
                           pHeap,
                           static_cast<UINT>(StartOffsets.size()),
                           RangeFlags.data(),
                           StartOffsets.data(),
                           RangeTileCounts.data(),
                           D3D12_TILE_MAPPING_FLAG_NONE,
                           IsArray);
    });

    auto pTextureWrapper = CreateTextureFromD3D11Resource(pTexture);
    ASSERT_NE(pTextureWrapper, nullptr);

    Helper.FillAndDraw(pTextureWrapper);
}


void SparseMemorySparseResidencyAliasedTextureTestD3D12(const SparseMemoryTestTextureHelper& Helper)
{
    const auto   TexDim         = Helper.TextureSize;
    Uint32       MipLevels      = 0;
    const Uint32 NumTilesInHeap = 8 * TexDim.w;
    const bool   IsArray        = TexDim.w > 1;

    auto pHeap = CreateHeap(NumTilesInHeap, IsArray);
    ASSERT_NE(pHeap, nullptr);

    auto pTexture = CreateSparseTexture(TexDim, MipLevels, pHeap);
    ASSERT_NE(pTexture, nullptr);

    const auto TexSparseProps = GetTexture2DSparseProperties(pTexture);

    WithCmdQueue([&](ID3D12CommandQueue* pd3d12Queue) {
        std::vector<D3D12_TILED_RESOURCE_COORDINATE> Coordinates;
        std::vector<D3D12_TILE_REGION_SIZE>          RegionSizes;
        std::vector<UINT>                            StartOffsets;
        std::vector<UINT>                            RangeTileCounts;

        const auto NumTilesX = (TexDim.x + TexSparseProps.TileSize[0] - 1) / TexSparseProps.TileSize[0];
        const auto NumTilesY = (TexDim.y + TexSparseProps.TileSize[1] - 1) / TexSparseProps.TileSize[1];
        const auto ArraySize = static_cast<Uint32>(TexDim.w);

        // Mip tail - must not alias with other tiles
        Uint32 InitialOffsetInTiles = 0;
        for (Uint32 Slice = 0; Slice < ArraySize; ++Slice)
        {
            const auto NumTilesForPackedMips = TexSparseProps.MipTailSize / TexSparseProps.BlockSize;
            for (Uint32 OffsetInMipTail = 0; OffsetInMipTail < NumTilesForPackedMips; ++OffsetInMipTail)
            {
                Coordinates.emplace_back();
                auto& Coord = Coordinates.back();

                Coord.X           = OffsetInMipTail;
                Coord.Subresource = D3D12CalcSubresource(TexSparseProps.FirstMipInTail, Slice, 0, MipLevels, ArraySize);

                RegionSizes.emplace_back();
                auto& Size = RegionSizes.back();

                Size.NumTiles = 1;

                StartOffsets.emplace_back(InitialOffsetInTiles);
                RangeTileCounts.emplace_back(1);
                ++InitialOffsetInTiles;
            }
        }

        // tiles may alias
        for (Uint32 Slice = 0; Slice < static_cast<Uint32>(TexDim.w); ++Slice)
        {
            Uint32 MemOffsetInTiles = InitialOffsetInTiles;
            for (Uint32 Mip = 0, Idx = 0; Mip < TexSparseProps.FirstMipInTail; ++Mip)
            {
                const auto TilesX = std::max(1u, NumTilesX >> Mip);
                const auto TilesY = std::max(1u, NumTilesY >> Mip);
                for (Uint32 y = 0; y < TilesY; ++y)
                {
                    for (Uint32 x = 0; x < TilesX; ++x)
                    {
                        if (++Idx > 3)
                        {
                            Idx              = 0;
                            MemOffsetInTiles = InitialOffsetInTiles;
                        }

                        Coordinates.emplace_back();
                        auto& Coord = Coordinates.back();

                        Coord.X           = x;
                        Coord.Y           = y;
                        Coord.Z           = 0;
                        Coord.Subresource = D3D12CalcSubresource(Mip, Slice, 0, MipLevels, ArraySize);

                        RegionSizes.emplace_back();
                        auto& Size = RegionSizes.back();

                        Size.Width    = 1;
                        Size.Height   = 1;
                        Size.Depth    = 1;
                        Size.UseBox   = TRUE;
                        Size.NumTiles = 1;

                        StartOffsets.emplace_back(MemOffsetInTiles);
                        RangeTileCounts.emplace_back(1);

                        ++MemOffsetInTiles;
                        VERIFY_EXPR(MemOffsetInTiles <= NumTilesInHeap);
                    }
                }
            }
            InitialOffsetInTiles += 3;
        }

        UpdateTileMappings(pd3d12Queue,
                           pTexture,
                           static_cast<UINT>(Coordinates.size()),
                           Coordinates.data(),
                           RegionSizes.data(),
                           pHeap,
                           static_cast<UINT>(StartOffsets.size()),
                           nullptr,
                           StartOffsets.data(),
                           RangeTileCounts.data(),
                           D3D12_TILE_MAPPING_FLAG_NONE,
                           IsArray);
    });

    auto pTextureWrapper = CreateTextureFromD3D11Resource(pTexture);
    ASSERT_NE(pTextureWrapper, nullptr);

    Helper.FillAndDraw(pTextureWrapper);
}


void SparseMemorySparseTexture3DTestD3D12(const SparseMemoryTestTextureHelper& Helper)
{
    auto* pd3d12Device = TestingEnvironmentD3D12::GetInstance()->GetD3D12Device();

    const auto   TexDim         = Helper.TextureSize;
    Uint32       MipLevels      = 0;
    const Uint32 NumTilesInHeap = 8 * TexDim.z;

    auto pHeap = CreateHeap(NumTilesInHeap);
    ASSERT_NE(pHeap, nullptr);

    auto pTexture = CreateSparseTexture(TexDim, MipLevels, pHeap);
    ASSERT_NE(pTexture, nullptr);

    UINT                  NumTilesForEntireResource = 0;
    D3D12_PACKED_MIP_INFO PackedMipDesc;
    D3D12_TILE_SHAPE      StandardTileShapeForNonPackedMips;
    UINT                  NumSubresourceTilings = 0;
    pd3d12Device->GetResourceTiling(pTexture,
                                    &NumTilesForEntireResource,
                                    &PackedMipDesc,
                                    &StandardTileShapeForNonPackedMips,
                                    &NumSubresourceTilings,
                                    0,
                                    nullptr);
    ASSERT_GT(StandardTileShapeForNonPackedMips.WidthInTexels, 0u);
    ASSERT_GT(StandardTileShapeForNonPackedMips.HeightInTexels, 0u);
    ASSERT_GT(StandardTileShapeForNonPackedMips.DepthInTexels, 0u);

    WithCmdQueue([&](ID3D12CommandQueue* pd3d12Queue) {
        std::vector<D3D12_TILED_RESOURCE_COORDINATE> Coordinates;
        std::vector<D3D12_TILE_REGION_SIZE>          RegionSizes;
        std::vector<UINT>                            StartOffsets;
        std::vector<UINT>                            RangeTileCounts;

        const auto NumTilesX = (TexDim.x + StandardTileShapeForNonPackedMips.WidthInTexels - 1) / StandardTileShapeForNonPackedMips.WidthInTexels;
        const auto NumTilesY = (TexDim.y + StandardTileShapeForNonPackedMips.HeightInTexels - 1) / StandardTileShapeForNonPackedMips.HeightInTexels;
        const auto NumTilesZ = (TexDim.z + StandardTileShapeForNonPackedMips.DepthInTexels - 1) / StandardTileShapeForNonPackedMips.DepthInTexels;

        Uint32 MemOffsetInTiles = 0;
        for (Uint32 Mip = 0; Mip < PackedMipDesc.NumStandardMips; ++Mip)
        {
            const auto TilesX = std::max(1u, NumTilesX >> Mip);
            const auto TilesY = std::max(1u, NumTilesY >> Mip);
            const auto TilesZ = std::max(1u, NumTilesZ >> Mip);
            for (Uint32 z = 0; z < TilesZ; ++z)
            {
                for (Uint32 y = 0; y < TilesY; ++y)
                {
                    for (Uint32 x = 0; x < TilesX; ++x)
                    {
                        Coordinates.emplace_back();
                        auto& Coord = Coordinates.back();

                        Coord.X           = x;
                        Coord.Y           = y;
                        Coord.Z           = z;
                        Coord.Subresource = D3D12CalcSubresource(Mip, 0, 0, MipLevels, 1);

                        RegionSizes.emplace_back();
                        auto& Size = RegionSizes.back();

                        Size.Width    = 1;
                        Size.Height   = 1;
                        Size.Depth    = 1;
                        Size.UseBox   = TRUE;
                        Size.NumTiles = 1;

                        StartOffsets.emplace_back(MemOffsetInTiles);
                        RangeTileCounts.emplace_back(1);
                        ++MemOffsetInTiles;
                    }
                }
            }
        }

        // Mip tail
        for (Uint32 OffsetInMipTail = 0; OffsetInMipTail < PackedMipDesc.NumTilesForPackedMips; ++OffsetInMipTail)
        {
            Coordinates.emplace_back();
            auto& Coord = Coordinates.back();

            Coord.X           = OffsetInMipTail;
            Coord.Subresource = D3D12CalcSubresource(PackedMipDesc.NumStandardMips, 0, 0, MipLevels, 1);

            RegionSizes.emplace_back();
            auto& Size = RegionSizes.back();

            Size.NumTiles = 1;

            StartOffsets.emplace_back(MemOffsetInTiles);
            RangeTileCounts.emplace_back(1);
            ++MemOffsetInTiles;
        }
        VERIFY_EXPR(MemOffsetInTiles <= NumTilesInHeap);

        pd3d12Queue->UpdateTileMappings(pTexture,
                                        static_cast<UINT>(Coordinates.size()),
                                        Coordinates.data(),
                                        RegionSizes.data(),
                                        pHeap,
                                        static_cast<UINT>(StartOffsets.size()),
                                        nullptr,
                                        StartOffsets.data(),
                                        RangeTileCounts.data(),
                                        D3D12_TILE_MAPPING_FLAG_NONE);
    });

    auto pTextureWrapper = CreateTextureFromD3D11Resource(pTexture);
    ASSERT_NE(pTextureWrapper, nullptr);

    Helper.FillAndDraw(pTextureWrapper);
}

} // namespace Testing

} // namespace Diligent
