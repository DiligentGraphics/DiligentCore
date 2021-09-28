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

#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <d3d11_2.h>

#include "D3D11/TestingEnvironmentD3D11.hpp"
#include "D3D11/TestingSwapChainD3D11.hpp"

#include "RenderDeviceD3D11.h"
#include "TextureD3D11.h"
#include "BufferD3D11.h"

#ifdef DILIGENT_ENABLE_D3D_NVAPI
#    include "nvapi.h"
#endif

#include "SparseMemoryTest.hpp"

namespace Diligent
{

namespace Testing
{

namespace
{

CComPtr<ID3D11Buffer> CreateSparseBuffer(Uint64 Size, UINT BindFlags)
{
    auto* pd3d11Device = TestingEnvironmentD3D11::GetInstance()->GetD3D11Device();

    D3D11_BUFFER_DESC Desc{};
    Desc.ByteWidth           = static_cast<UINT>(Size);
    Desc.Usage               = D3D11_USAGE_DEFAULT;
    Desc.BindFlags           = BindFlags | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    Desc.CPUAccessFlags      = 0;
    Desc.MiscFlags           = D3D11_RESOURCE_MISC_TILED | D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    Desc.StructureByteStride = 4;

    CComPtr<ID3D11Buffer> pd3d11Buffer;
    pd3d11Device->CreateBuffer(&Desc, nullptr, &pd3d11Buffer);

    return pd3d11Buffer;
}

RefCntAutoPtr<IBuffer> CreateBufferFromD3D11Resource(ID3D11Buffer* pBuffer)
{
    auto* pEnvD3D11 = TestingEnvironmentD3D11::GetInstance();

    RefCntAutoPtr<IRenderDeviceD3D11> pDeviceD3D11(pEnvD3D11->GetDevice(), IID_RenderDeviceD3D11);
    if (pDeviceD3D11 == nullptr)
        return {};

    BufferDesc BuffDesc;
    BuffDesc.Name              = "Sparse buffer from D3D11 resource";
    BuffDesc.Usage             = USAGE_SPARSE;
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = 4;

    RefCntAutoPtr<IBuffer> pBufferWrapper;
    pDeviceD3D11->CreateBufferFromD3DResource(pBuffer, BuffDesc, RESOURCE_STATE_UNDEFINED, &pBufferWrapper);
    return pBufferWrapper;
}

CComPtr<ID3D11Resource> CreateSparseTexture(const int4& Dim, UINT BindFlags, Uint32& MipLevels)
{
    auto* pd3d11Device = TestingEnvironmentD3D11::GetInstance()->GetD3D11Device();

    MipLevels = ComputeMipLevelsCount(Dim.x, Dim.y, Dim.z);

    CComPtr<ID3D11Resource> Result;
    if (Dim.z == 1)
    {
        D3D11_TEXTURE2D_DESC Desc{};
        Desc.Width            = Dim.x;
        Desc.Height           = Dim.y;
        Desc.MipLevels        = MipLevels;
        Desc.ArraySize        = Dim.w;
        Desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        Desc.SampleDesc.Count = 1;
        Desc.Usage            = D3D11_USAGE_DEFAULT;
        Desc.BindFlags        = BindFlags | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        Desc.CPUAccessFlags   = 0;
        Desc.MiscFlags        = D3D11_RESOURCE_MISC_TILED;

        CComPtr<ID3D11Texture2D> ptex2D;
#ifdef DILIGENT_ENABLE_D3D_NVAPI
        if (Dim.w > 1)
        {
            if (NvAPI_D3D11_CreateTiledTexture2DArray(pd3d11Device, &Desc, nullptr, &ptex2D) == NVAPI_OK)
                Result = std::move(ptex2D);
        }
#endif
        if (!Result)
        {
            if (SUCCEEDED(pd3d11Device->CreateTexture2D(&Desc, nullptr, &ptex2D)))
                Result = std::move(ptex2D);
        }
    }
    else
    {
        D3D11_TEXTURE3D_DESC Desc{};
        Desc.Width          = Dim.x;
        Desc.Height         = Dim.y;
        Desc.Depth          = Dim.z;
        Desc.MipLevels      = MipLevels;
        Desc.Format         = DXGI_FORMAT_R8G8B8A8_UNORM;
        Desc.Usage          = D3D11_USAGE_DEFAULT;
        Desc.BindFlags      = BindFlags | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        Desc.CPUAccessFlags = 0;
        Desc.MiscFlags      = D3D11_RESOURCE_MISC_TILED;

        CComPtr<ID3D11Texture3D> ptex3D;
        pd3d11Device->CreateTexture3D(&Desc, nullptr, &ptex3D);
        Result = std::move(ptex3D);
    }
    return Result;
}

RefCntAutoPtr<ITexture> CreateTextureFromD3D11Resource(ID3D11Resource* pTexture, bool Is3D = false)
{
    auto* pEnvD3D11 = TestingEnvironmentD3D11::GetInstance();

    RefCntAutoPtr<IRenderDeviceD3D11> pDeviceD3D11(pEnvD3D11->GetDevice(), IID_RenderDeviceD3D11);
    if (pDeviceD3D11 == nullptr)
        return {};

    RefCntAutoPtr<ITexture> pTextureWrapper;
    if (Is3D)
        pDeviceD3D11->CreateTexture3DFromD3DResource(static_cast<ID3D11Texture3D*>(pTexture), RESOURCE_STATE_UNDEFINED, &pTextureWrapper);
    else
        pDeviceD3D11->CreateTexture2DFromD3DResource(static_cast<ID3D11Texture2D*>(pTexture), RESOURCE_STATE_UNDEFINED, &pTextureWrapper);
    return pTextureWrapper;
}

CComPtr<ID3D11Buffer> CreateTilePool(Uint32 NumTiles)
{
    auto* pd3d11Device = TestingEnvironmentD3D11::GetInstance()->GetD3D11Device();

    D3D11_BUFFER_DESC Desc{};
    Desc.ByteWidth           = NumTiles * D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES;
    Desc.Usage               = D3D11_USAGE_DEFAULT;
    Desc.BindFlags           = 0;
    Desc.CPUAccessFlags      = 0;
    Desc.MiscFlags           = D3D11_RESOURCE_MISC_TILE_POOL;
    Desc.StructureByteStride = 0;

    CComPtr<ID3D11Buffer> pd3d11Buffer;
    pd3d11Device->CreateBuffer(&Desc, nullptr, &pd3d11Buffer);

    return pd3d11Buffer;
}

TextureSparseProperties GetTexture2DSparseProperties(ID3D11Resource* pResource)
{
    D3D11_TEXTURE2D_DESC D3D11TexDesc;
    static_cast<ID3D11Texture2D*>(pResource)->GetDesc(&D3D11TexDesc);

    TextureDesc Desc;
    Desc.Type        = D3D11TexDesc.ArraySize > 1 ? RESOURCE_DIM_TEX_2D_ARRAY : RESOURCE_DIM_TEX_2D;
    Desc.Width       = D3D11TexDesc.Width;
    Desc.Height      = D3D11TexDesc.Height;
    Desc.ArraySize   = D3D11TexDesc.ArraySize;
    Desc.Format      = TEX_FORMAT_RGBA8_UNORM;
    Desc.MipLevels   = D3D11TexDesc.MipLevels;
    Desc.SampleCount = 1;
    Desc.Usage       = USAGE_SPARSE;

    return GetTextureSparsePropertiesForStandardBlocks(Desc);
}

void UpdateTileMappings(ID3D11Resource*                        pTiledResource,
                        UINT                                   NumTiledResourceRegions,
                        const D3D11_TILED_RESOURCE_COORDINATE* pTiledResourceRegionStartCoordinates,
                        const D3D11_TILE_REGION_SIZE*          pTiledResourceRegionSizes,
                        ID3D11Buffer*                          pTilePool,
                        UINT                                   NumRanges,
                        const UINT*                            pRangeFlags,
                        const UINT*                            pTilePoolStartOffsets,
                        const UINT*                            pRangeTileCounts,
                        UINT                                   Flags,
                        bool                                   Is2DArray)
{
    auto* pd3d11Context2 = static_cast<ID3D11DeviceContext2*>(TestingEnvironmentD3D11::GetInstance()->GetD3D11Context());
    bool  Updated        = false;

#ifdef DILIGENT_ENABLE_D3D_NVAPI
    if (Is2DArray)
    {
        Updated = NvAPI_D3D11_UpdateTileMappings(
                      pd3d11Context2,
                      pTiledResource,
                      NumTiledResourceRegions,
                      pTiledResourceRegionStartCoordinates,
                      pTiledResourceRegionSizes,
                      pTilePool,
                      NumRanges,
                      pRangeFlags,
                      pTilePoolStartOffsets,
                      pRangeTileCounts,
                      Flags) == NVAPI_OK;
    }
#endif
    if (!Updated)
    {
        pd3d11Context2->UpdateTileMappings(pTiledResource,
                                           NumTiledResourceRegions,
                                           pTiledResourceRegionStartCoordinates,
                                           pTiledResourceRegionSizes,
                                           pTilePool,
                                           NumRanges,
                                           pRangeFlags,
                                           pTilePoolStartOffsets,
                                           pRangeTileCounts,
                                           Flags);
    }
    pd3d11Context2->Flush();
}

} // namespace


void SparseMemorySparseBufferTestD3D11(const SparseMemoryTestBufferHelper& Helper)
{
    auto* pd3d11Context2 = static_cast<ID3D11DeviceContext2*>(TestingEnvironmentD3D11::GetInstance()->GetD3D11Context());

    const Uint32 BufferSize = D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES * 4;
    VERIFY_EXPR(BufferSize == Helper.BufferSize);

    auto pBuffer = CreateSparseBuffer(BufferSize, 0);
    ASSERT_NE(pBuffer, nullptr);

    auto pTilePool = CreateTilePool(6);
    ASSERT_NE(pTilePool, nullptr);

    D3D11_TILED_RESOURCE_COORDINATE Coords[4]   = {};
    D3D11_TILE_REGION_SIZE          RegSizes[4] = {};

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

    pd3d11Context2->UpdateTileMappings(pBuffer,
                                       _countof(Coords),
                                       Coords,
                                       RegSizes,
                                       pTilePool,
                                       _countof(TilePoolStartOffsets),
                                       nullptr,
                                       TilePoolStartOffsets,
                                       RangeTileCounts,
                                       D3D11_TILE_MAPPING_NO_OVERWRITE);
    pd3d11Context2->Flush();

    auto pBufferWrapper = CreateBufferFromD3D11Resource(pBuffer);
    ASSERT_NE(pBufferWrapper, nullptr);

    Helper.FillAndDraw(pBufferWrapper);
}


void SparseMemorySparseResidentBufferTestD3D11(const SparseMemoryTestBufferHelper& Helper)
{
    auto* pd3d11Context2 = static_cast<ID3D11DeviceContext2*>(TestingEnvironmentD3D11::GetInstance()->GetD3D11Context());

    const Uint32 BufferSize = D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES * 8;
    VERIFY_EXPR(BufferSize == Helper.BufferSize);

    auto pBuffer = CreateSparseBuffer(BufferSize, 0);
    ASSERT_NE(pBuffer, nullptr);

    auto pTilePool = CreateTilePool(6);
    ASSERT_NE(pTilePool, nullptr);

    D3D11_TILED_RESOURCE_COORDINATE Coords[4]   = {};
    D3D11_TILE_REGION_SIZE          RegSizes[4] = {};

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

    pd3d11Context2->UpdateTileMappings(pBuffer,
                                       _countof(Coords),
                                       Coords,
                                       RegSizes,
                                       pTilePool,
                                       _countof(TilePoolStartOffsets),
                                       nullptr,
                                       TilePoolStartOffsets,
                                       RangeTileCounts,
                                       D3D11_TILE_MAPPING_NO_OVERWRITE);
    pd3d11Context2->Flush();

    auto pBufferWrapper = CreateBufferFromD3D11Resource(pBuffer);
    ASSERT_NE(pBufferWrapper, nullptr);

    Helper.FillAndDraw(pBufferWrapper);
}


void SparseMemorySparseResidentAliasedBufferTestD3D11(const SparseMemoryTestBufferHelper& Helper)
{
    auto* pd3d11Context2 = static_cast<ID3D11DeviceContext2*>(TestingEnvironmentD3D11::GetInstance()->GetD3D11Context());

    const Uint32 BufferSize = D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES * 8;
    VERIFY_EXPR(BufferSize == Helper.BufferSize);

    auto pBuffer = CreateSparseBuffer(BufferSize, 0);
    ASSERT_NE(pBuffer, nullptr);

    auto pTilePool = CreateTilePool(6);
    ASSERT_NE(pTilePool, nullptr);

    D3D11_TILED_RESOURCE_COORDINATE Coords[5]   = {};
    D3D11_TILE_REGION_SIZE          RegSizes[5] = {};

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

    pd3d11Context2->UpdateTileMappings(pBuffer,
                                       _countof(Coords),
                                       Coords,
                                       RegSizes,
                                       pTilePool,
                                       _countof(TilePoolStartOffsets),
                                       nullptr,
                                       TilePoolStartOffsets,
                                       RangeTileCounts,
                                       D3D11_TILE_MAPPING_NO_OVERWRITE);
    pd3d11Context2->Flush();

    auto pBufferWrapper = CreateBufferFromD3D11Resource(pBuffer);
    ASSERT_NE(pBufferWrapper, nullptr);

    Helper.FillAndDraw(pBufferWrapper);
}


void SparseMemorySparseTextureTestD3D11(const SparseMemoryTestTextureHelper& Helper)
{
    const auto   TexDim    = Helper.TextureSize;
    Uint32       MipLevels = 0;
    const Uint32 PoolSize  = 12 * TexDim.w;

    auto pTexture = CreateSparseTexture(TexDim, 0, MipLevels);
    ASSERT_NE(pTexture, nullptr);

    auto pTilePool = CreateTilePool(PoolSize);
    ASSERT_NE(pTilePool, nullptr);

    const auto TexSparseProps = GetTexture2DSparseProperties(pTexture);

    std::vector<D3D11_TILED_RESOURCE_COORDINATE> Coordinates;
    std::vector<D3D11_TILE_REGION_SIZE>          RegionSizes;
    std::vector<UINT>                            StartOffsets;
    std::vector<UINT>                            RangeTileCounts;

    const auto NumTilesX = (TexDim.x + TexSparseProps.TileSize[0] - 1) / TexSparseProps.TileSize[0];
    const auto NumTilesY = (TexDim.y + TexSparseProps.TileSize[1] - 1) / TexSparseProps.TileSize[1];

    Uint32 MemOffsetInTiles = 0;
    for (Uint32 Slice = 0; Slice < static_cast<Uint32>(TexDim.w); ++Slice)
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
                    Coord.Subresource = D3D11CalcSubresource(Mip, Slice, MipLevels);

                    RegionSizes.emplace_back();
                    auto& Size = RegionSizes.back();

                    Size.Width    = 1;
                    Size.Height   = 1;
                    Size.Depth    = 1;
                    Size.bUseBox  = TRUE;
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
            Coord.Subresource = D3D11CalcSubresource(TexSparseProps.FirstMipInTail, Slice, MipLevels);

            RegionSizes.emplace_back();
            auto& Size = RegionSizes.back();

            Size.NumTiles = 1;

            StartOffsets.emplace_back(MemOffsetInTiles);
            RangeTileCounts.emplace_back(1);
            ++MemOffsetInTiles;
        }
    }
    VERIFY_EXPR(MemOffsetInTiles <= PoolSize);

    UpdateTileMappings(pTexture,
                       static_cast<UINT>(Coordinates.size()),
                       Coordinates.data(),
                       RegionSizes.data(),
                       pTilePool,
                       static_cast<UINT>(StartOffsets.size()),
                       nullptr,
                       StartOffsets.data(),
                       RangeTileCounts.data(),
                       D3D11_TILE_MAPPING_NO_OVERWRITE,
                       TexDim.w > 1);

    auto pTextureWrapper = CreateTextureFromD3D11Resource(pTexture);
    ASSERT_NE(pTextureWrapper, nullptr);

    Helper.FillAndDraw(pTextureWrapper);
}


void SparseMemorySparseResidencyTextureTestD3D11(const SparseMemoryTestTextureHelper& Helper)
{
    const auto   TexDim    = Helper.TextureSize;
    Uint32       MipLevels = 0;
    const Uint32 PoolSize  = 12 * TexDim.w;

    auto pTexture = CreateSparseTexture(TexDim, 0, MipLevels);
    ASSERT_NE(pTexture, nullptr);

    auto pTilePool = CreateTilePool(PoolSize);
    ASSERT_NE(pTilePool, nullptr);

    const auto TexSparseProps = GetTexture2DSparseProperties(pTexture);

    std::vector<D3D11_TILED_RESOURCE_COORDINATE> Coordinates;
    std::vector<D3D11_TILE_REGION_SIZE>          RegionSizes;
    std::vector<UINT>                            RangeFlags;
    std::vector<UINT>                            StartOffsets;
    std::vector<UINT>                            RangeTileCounts;

    const auto NumTilesX = (TexDim.x + TexSparseProps.TileSize[0] - 1) / TexSparseProps.TileSize[0];
    const auto NumTilesY = (TexDim.y + TexSparseProps.TileSize[1] - 1) / TexSparseProps.TileSize[1];

    Uint32 MemOffsetInTiles = 0;
    for (Uint32 Slice = 0; Slice < static_cast<Uint32>(TexDim.w); ++Slice)
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
                    Coord.Subresource = D3D11CalcSubresource(Mip, Slice, MipLevels);

                    RegionSizes.emplace_back();
                    auto& Size = RegionSizes.back();

                    Size.Width    = 1;
                    Size.Height   = 1;
                    Size.Depth    = 1;
                    Size.bUseBox  = TRUE;
                    Size.NumTiles = 1;

                    RangeTileCounts.emplace_back(1);

                    if ((++Idx & 2) == 0)
                    {
                        StartOffsets.emplace_back(MemOffsetInTiles);
                        RangeFlags.emplace_back(0);
                        ++MemOffsetInTiles;
                    }
                    else
                    {
                        StartOffsets.emplace_back(0);
                        RangeFlags.emplace_back(D3D11_TILE_RANGE_NULL);
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
            Coord.Subresource = D3D11CalcSubresource(TexSparseProps.FirstMipInTail, Slice, MipLevels);

            RegionSizes.emplace_back();
            auto& Size = RegionSizes.back();

            Size.NumTiles = 1;

            RangeFlags.emplace_back(0);
            StartOffsets.emplace_back(MemOffsetInTiles);
            RangeTileCounts.emplace_back(1);
            ++MemOffsetInTiles;
        }
    }
    VERIFY_EXPR(MemOffsetInTiles <= PoolSize);

    UpdateTileMappings(pTexture,
                       static_cast<UINT>(Coordinates.size()),
                       Coordinates.data(),
                       RegionSizes.data(),
                       pTilePool,
                       static_cast<UINT>(StartOffsets.size()),
                       RangeFlags.data(),
                       StartOffsets.data(),
                       RangeTileCounts.data(),
                       D3D11_TILE_MAPPING_NO_OVERWRITE,
                       TexDim.w > 1);

    auto pTextureWrapper = CreateTextureFromD3D11Resource(pTexture);
    ASSERT_NE(pTextureWrapper, nullptr);

    Helper.FillAndDraw(pTextureWrapper);
}


void SparseMemorySparseResidencyAliasedTextureTestD3D11(const SparseMemoryTestTextureHelper& Helper)
{
    const auto   TexDim    = Helper.TextureSize;
    Uint32       MipLevels = 0;
    const Uint32 PoolSize  = 12 * TexDim.w;

    auto pTexture = CreateSparseTexture(TexDim, 0, MipLevels);
    ASSERT_NE(pTexture, nullptr);

    auto pTilePool = CreateTilePool(PoolSize);
    ASSERT_NE(pTilePool, nullptr);

    const auto TexSparseProps = GetTexture2DSparseProperties(pTexture);

    std::vector<D3D11_TILED_RESOURCE_COORDINATE> Coordinates;
    std::vector<D3D11_TILE_REGION_SIZE>          RegionSizes;
    std::vector<UINT>                            StartOffsets;
    std::vector<UINT>                            RangeTileCounts;

    const auto NumTilesX = (TexDim.x + TexSparseProps.TileSize[0] - 1) / TexSparseProps.TileSize[0];
    const auto NumTilesY = (TexDim.y + TexSparseProps.TileSize[1] - 1) / TexSparseProps.TileSize[1];

    // Mip tail - must not alias with other tiles
    Uint32 InitialOffsetInTiles = 0;
    for (Uint32 Slice = 0; Slice < static_cast<Uint32>(TexDim.w); ++Slice)
    {
        const auto NumTilesForPackedMips = TexSparseProps.MipTailSize / TexSparseProps.BlockSize;
        for (Uint32 OffsetInMipTail = 0; OffsetInMipTail < NumTilesForPackedMips; ++OffsetInMipTail)
        {
            Coordinates.emplace_back();
            auto& Coord = Coordinates.back();

            Coord.X           = OffsetInMipTail;
            Coord.Subresource = D3D11CalcSubresource(TexSparseProps.FirstMipInTail, Slice, MipLevels);

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
                    Coord.Subresource = D3D11CalcSubresource(Mip, Slice, MipLevels);

                    RegionSizes.emplace_back();
                    auto& Size = RegionSizes.back();

                    Size.Width    = 1;
                    Size.Height   = 1;
                    Size.Depth    = 1;
                    Size.bUseBox  = TRUE;
                    Size.NumTiles = 1;

                    StartOffsets.emplace_back(MemOffsetInTiles);
                    RangeTileCounts.emplace_back(1);

                    ++MemOffsetInTiles;
                    VERIFY_EXPR(MemOffsetInTiles <= PoolSize);
                }
            }
        }
        InitialOffsetInTiles += 3;
    }

    UpdateTileMappings(pTexture,
                       static_cast<UINT>(Coordinates.size()),
                       Coordinates.data(),
                       RegionSizes.data(),
                       pTilePool,
                       static_cast<UINT>(StartOffsets.size()),
                       nullptr,
                       StartOffsets.data(),
                       RangeTileCounts.data(),
                       D3D11_TILE_MAPPING_NO_OVERWRITE,
                       TexDim.w > 1);

    auto pTextureWrapper = CreateTextureFromD3D11Resource(pTexture);
    ASSERT_NE(pTextureWrapper, nullptr);

    Helper.FillAndDraw(pTextureWrapper);
}


void SparseMemorySparseTexture3DTestD3D11(const SparseMemoryTestTextureHelper& Helper)
{
    auto* pEnvD3D11      = TestingEnvironmentD3D11::GetInstance();
    auto* pd3d11Device2  = static_cast<ID3D11Device2*>(pEnvD3D11->GetD3D11Device());
    auto* pd3d11Context2 = static_cast<ID3D11DeviceContext2*>(pEnvD3D11->GetD3D11Context());

    const auto   TexDim    = Helper.TextureSize;
    Uint32       MipLevels = 0;
    const Uint32 PoolSize  = 16;

    auto pTexture = CreateSparseTexture(TexDim, 0, MipLevels);
    ASSERT_NE(pTexture, nullptr);

    auto pTilePool = CreateTilePool(PoolSize);
    ASSERT_NE(pTilePool, nullptr);

    UINT                  NumTilesForEntireResource = 0;
    D3D11_PACKED_MIP_DESC PackedMipDesc;
    D3D11_TILE_SHAPE      StandardTileShapeForNonPackedMips;
    UINT                  NumSubresourceTilings = 0;
    pd3d11Device2->GetResourceTiling(pTexture,
                                     &NumTilesForEntireResource,
                                     &PackedMipDesc,
                                     &StandardTileShapeForNonPackedMips,
                                     &NumSubresourceTilings,
                                     0,
                                     nullptr);

    std::vector<D3D11_TILED_RESOURCE_COORDINATE> Coordinates;
    std::vector<D3D11_TILE_REGION_SIZE>          RegionSizes;
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
                    Coord.Subresource = D3D11CalcSubresource(Mip, 0, MipLevels);

                    RegionSizes.emplace_back();
                    auto& Size = RegionSizes.back();

                    Size.Width    = 1;
                    Size.Height   = 1;
                    Size.Depth    = 1;
                    Size.bUseBox  = TRUE;
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
        Coord.Subresource = D3D11CalcSubresource(PackedMipDesc.NumStandardMips, 0, MipLevels);

        RegionSizes.emplace_back();
        auto& Size = RegionSizes.back();

        Size.NumTiles = 1;

        StartOffsets.emplace_back(MemOffsetInTiles);
        RangeTileCounts.emplace_back(1);
        ++MemOffsetInTiles;
    }

    VERIFY_EXPR(MemOffsetInTiles <= PoolSize);

    pd3d11Context2->UpdateTileMappings(pTexture,
                                       static_cast<UINT>(Coordinates.size()),
                                       Coordinates.data(),
                                       RegionSizes.data(),
                                       pTilePool,
                                       static_cast<UINT>(StartOffsets.size()),
                                       nullptr,
                                       StartOffsets.data(),
                                       RangeTileCounts.data(),
                                       D3D11_TILE_MAPPING_NO_OVERWRITE);
    pd3d11Context2->Flush();

    auto pTextureWrapper = CreateTextureFromD3D11Resource(pTexture);
    ASSERT_NE(pTextureWrapper, nullptr);

    Helper.FillAndDraw(pTextureWrapper);
}


} // namespace Testing

} // namespace Diligent
