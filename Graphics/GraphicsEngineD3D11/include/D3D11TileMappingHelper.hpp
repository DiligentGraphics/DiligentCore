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

#pragma once

/// \file
/// D3D11 tile mapping helper

#include <functional>

#include "D3D11TypeDefinitions.h"
#include "D3DTileMappingHelper.hpp"

namespace Diligent
{

struct D3D11TileMappingHelper : D3DTileMappingHelper<D3D11_TILED_RESOURCE_COORDINATE, D3D11_TILE_REGION_SIZE, UINT, D3D11TileMappingHelper>
{
    UINT CalcSubresource(UINT MipSlice, UINT ArraySlice, UINT PlaneSlice, const TextureDesc& TexDesc) const
    {
        VERIFY(PlaneSlice == 0, "Plane slices are not supported in D3D11");
        return D3D11CalcSubresource(MipSlice, ArraySlice, TexDesc.MipLevels);
    }

    void SetUseBox(D3D11_TILE_REGION_SIZE& RegionSize, BOOL UseBox) const
    {
        RegionSize.bUseBox = UseBox;
    }

    void Commit(ID3D11DeviceContext2* pd3d11DeviceContext2, ID3D11Resource* pResource, ID3D11Buffer* pTilePool) const
    {
#ifdef DILIGENT_ENABLE_D3D_NVAPI
        if (UseNVApi)
        {
            // From NVAPI docs:
            //   "If any of API from this set is used, using all of them is highly recommended."
            NvAPI_D3D11_UpdateTileMappings(pd3d11DeviceContext2,
                                           pResource,
                                           static_cast<UINT>(Coordinates.size()),
                                           Coordinates.data(),
                                           RegionSizes.data(),
                                           pTilePool,
                                           static_cast<UINT>(RangeFlags.size()),
                                           RangeFlags.data(),
                                           RangeStartOffsets.data(),
                                           RangeTileCounts.data(),
                                           D3D11_TILE_MAPPING_NO_OVERWRITE);
        }
        else
#endif
        {
            pd3d11DeviceContext2->UpdateTileMappings(pResource,
                                                     static_cast<UINT>(Coordinates.size()),
                                                     Coordinates.data(),
                                                     RegionSizes.data(),
                                                     pTilePool,
                                                     static_cast<UINT>(RangeFlags.size()),
                                                     RangeFlags.data(),
                                                     RangeStartOffsets.data(),
                                                     RangeTileCounts.data(),
                                                     D3D11_TILE_MAPPING_NO_OVERWRITE);
        }
    }
};

} // namespace Diligent
