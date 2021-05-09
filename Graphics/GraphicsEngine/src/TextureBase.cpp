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

#include "Texture.h"

#include <algorithm>

#include "DeviceContext.h"
#include "GraphicsAccessories.hpp"

namespace Diligent
{

void ValidateTextureDesc(const TextureDesc& Desc) noexcept(false)
{
#define LOG_TEXTURE_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Texture '", (Desc.Name ? Desc.Name : ""), "': ", ##__VA_ARGS__)

    const auto& FmtAttribs = GetTextureFormatAttribs(Desc.Format);

    if (Desc.Type == RESOURCE_DIM_UNDEFINED)
    {
        LOG_TEXTURE_ERROR_AND_THROW("Resource dimension is undefined.");
    }

    if (!(Desc.Type >= RESOURCE_DIM_TEX_1D && Desc.Type <= RESOURCE_DIM_TEX_CUBE_ARRAY))
    {
        LOG_TEXTURE_ERROR_AND_THROW("Unexpected resource dimension.");
    }

    if (Desc.Width == 0)
    {
        LOG_TEXTURE_ERROR_AND_THROW("Texture width cannot be zero.");
    }

    // Perform some parameter correctness check
    if (Desc.Type == RESOURCE_DIM_TEX_1D || Desc.Type == RESOURCE_DIM_TEX_1D_ARRAY)
    {
        if (Desc.Height != FmtAttribs.BlockHeight)
        {
            if (FmtAttribs.BlockHeight == 1)
            {
                LOG_TEXTURE_ERROR_AND_THROW("Height (", Desc.Height, ") of a Texture 1D/Texture 1D Array must be 1.");
            }
            else
            {
                LOG_TEXTURE_ERROR_AND_THROW("For block-compressed formats, the height (", Desc.Height,
                                            ") of a Texture 1D/Texture 1D Array must be equal to the compressed block height (",
                                            Uint32{FmtAttribs.BlockHeight}, ").");
            }
        }
    }
    else
    {
        if (Desc.Height == 0)
            LOG_TEXTURE_ERROR_AND_THROW("Texture height cannot be zero.");
    }

    if (Desc.Type == RESOURCE_DIM_TEX_3D && Desc.Depth == 0)
    {
        LOG_TEXTURE_ERROR_AND_THROW("3D texture depth cannot be zero.");
    }

    if (Desc.Type == RESOURCE_DIM_TEX_1D || Desc.Type == RESOURCE_DIM_TEX_2D)
    {
        if (Desc.ArraySize != 1)
            LOG_TEXTURE_ERROR_AND_THROW("Texture 1D/2D must have one array slice (", Desc.ArraySize, " provided). Use Texture 1D/2D array if you need more than one slice.");
    }

    if (Desc.Type == RESOURCE_DIM_TEX_CUBE || Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
    {
        if (Desc.Width != Desc.Height)
            LOG_TEXTURE_ERROR_AND_THROW("For cube map textures, texture width (", Desc.Width, " provided) must match texture height (", Desc.Height, " provided).");

        if (Desc.ArraySize < 6)
            LOG_TEXTURE_ERROR_AND_THROW("Texture cube/cube array must have at least 6 slices (", Desc.ArraySize, " provided).");
    }

    Uint32 MaxDim = 0;
    if (Desc.Type == RESOURCE_DIM_TEX_1D || Desc.Type == RESOURCE_DIM_TEX_1D_ARRAY)
        MaxDim = Desc.Width;
    else if (Desc.Type == RESOURCE_DIM_TEX_2D || Desc.Type == RESOURCE_DIM_TEX_2D_ARRAY || Desc.Type == RESOURCE_DIM_TEX_CUBE || Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
        MaxDim = std::max(Desc.Width, Desc.Height);
    else if (Desc.Type == RESOURCE_DIM_TEX_3D)
        MaxDim = std::max(std::max(Desc.Width, Desc.Height), Desc.Depth);
    VERIFY(MaxDim >= (1U << (Desc.MipLevels - 1)), "Texture '", Desc.Name ? Desc.Name : "", "': Incorrect number of Mip levels (", Desc.MipLevels, ").");

    if (Desc.SampleCount > 1)
    {
        if (!(Desc.Type == RESOURCE_DIM_TEX_2D || Desc.Type == RESOURCE_DIM_TEX_2D_ARRAY))
            LOG_TEXTURE_ERROR_AND_THROW("Only Texture 2D/Texture 2D Array can be multisampled");

        if (Desc.MipLevels != 1)
            LOG_TEXTURE_ERROR_AND_THROW("Multisampled textures must have one mip level (", Desc.MipLevels, " levels specified).");

        if (Desc.BindFlags & BIND_UNORDERED_ACCESS)
            LOG_TEXTURE_ERROR_AND_THROW("UAVs are not allowed for multisampled resources");
    }

    if ((Desc.BindFlags & BIND_RENDER_TARGET) &&
        (Desc.Format == TEX_FORMAT_R8_SNORM || Desc.Format == TEX_FORMAT_RG8_SNORM || Desc.Format == TEX_FORMAT_RGBA8_SNORM ||
         Desc.Format == TEX_FORMAT_R16_SNORM || Desc.Format == TEX_FORMAT_RG16_SNORM || Desc.Format == TEX_FORMAT_RGBA16_SNORM))
    {
        const auto* FmtName = GetTextureFormatAttribs(Desc.Format).Name;
        LOG_WARNING_MESSAGE(FmtName, " texture is created with BIND_RENDER_TARGET flag set.\n"
                                     "There might be an issue in OpenGL driver on NVidia hardware: when rendering to SNORM textures, all negative values are clamped to zero.\n"
                                     "Use UNORM format instead.");
    }

    if (Desc.Usage == USAGE_STAGING)
    {
        if (Desc.BindFlags != 0)
            LOG_TEXTURE_ERROR_AND_THROW("Staging textures cannot be bound to any GPU pipeline stage.");

        if (Desc.MiscFlags & MISC_TEXTURE_FLAG_GENERATE_MIPS)
            LOG_TEXTURE_ERROR_AND_THROW("Mipmaps cannot be autogenerated for staging textures.");

        if (Desc.CPUAccessFlags == 0)
            LOG_TEXTURE_ERROR_AND_THROW("Staging textures must specify CPU access flags.");

        if ((Desc.CPUAccessFlags & (CPU_ACCESS_READ | CPU_ACCESS_WRITE)) == (CPU_ACCESS_READ | CPU_ACCESS_WRITE))
            LOG_TEXTURE_ERROR_AND_THROW("Staging textures must use exactly one of ACESS_READ or ACCESS_WRITE flags.");
    }
    else if (Desc.Usage == USAGE_UNIFIED)
    {
        LOG_TEXTURE_ERROR_AND_THROW("USAGE_UNIFIED textures are currently not supported.");
    }

    if (Desc.Usage == USAGE_DYNAMIC &&
        PlatformMisc::CountOneBits(Desc.ImmediateContextMask) > 1)
    {
        // Dynamic textures always use backing resource that requires implicit state transitions
        // in map/unamp operations, which is not safe in multiple contexts.
        LOG_TEXTURE_ERROR_AND_THROW("USAGE_DYNAMIC textures may only be used in one immediate device context.");
    }
}


void ValidateTextureRegion(const TextureDesc& TexDesc, Uint32 MipLevel, Uint32 Slice, const Box& Box)
{
#define VERIFY_TEX_PARAMS(Expr, ...)                                                          \
    do                                                                                        \
    {                                                                                         \
        if (!(Expr))                                                                          \
        {                                                                                     \
            LOG_ERROR("Texture '", (TexDesc.Name ? TexDesc.Name : ""), "': ", ##__VA_ARGS__); \
        }                                                                                     \
    } while (false)

#ifdef DILIGENT_DEVELOPMENT
    VERIFY_TEX_PARAMS(MipLevel < TexDesc.MipLevels, "Mip level (", MipLevel, ") is out of allowed range [0, ", TexDesc.MipLevels - 1, "]");
    VERIFY_TEX_PARAMS(Box.MinX < Box.MaxX, "Invalid X range: ", Box.MinX, "..", Box.MaxX);
    VERIFY_TEX_PARAMS(Box.MinY < Box.MaxY, "Invalid Y range: ", Box.MinY, "..", Box.MaxY);
    VERIFY_TEX_PARAMS(Box.MinZ < Box.MaxZ, "Invalid Z range: ", Box.MinZ, "..", Box.MaxZ);

    if (TexDesc.Type == RESOURCE_DIM_TEX_1D_ARRAY ||
        TexDesc.Type == RESOURCE_DIM_TEX_2D_ARRAY ||
        TexDesc.Type == RESOURCE_DIM_TEX_CUBE ||
        TexDesc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
    {
        VERIFY_TEX_PARAMS(Slice < TexDesc.ArraySize, "Array slice (", Slice, ") is out of range [0,", TexDesc.ArraySize - 1, "].");
    }
    else
    {
        VERIFY_TEX_PARAMS(Slice == 0, "Array slice (", Slice, ") must be 0 for non-array textures.");
    }

    const auto& FmtAttribs = GetTextureFormatAttribs(TexDesc.Format);

    Uint32 MipWidth = std::max(TexDesc.Width >> MipLevel, 1U);
    if (FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED)
    {
        VERIFY_EXPR((FmtAttribs.BlockWidth & (FmtAttribs.BlockWidth - 1)) == 0);
        Uint32 BlockAlignedMipWidth = (MipWidth + (FmtAttribs.BlockWidth - 1)) & ~(FmtAttribs.BlockWidth - 1);
        VERIFY_TEX_PARAMS(Box.MaxX <= BlockAlignedMipWidth, "Region max X coordinate (", Box.MaxX, ") is out of allowed range [0, ", BlockAlignedMipWidth, "].");
        VERIFY_TEX_PARAMS((Box.MinX % FmtAttribs.BlockWidth) == 0, "For compressed formats, the region min X coordinate (", Box.MinX, ") must be a multiple of block width (", Uint32{FmtAttribs.BlockWidth}, ").");
        VERIFY_TEX_PARAMS((Box.MaxX % FmtAttribs.BlockWidth) == 0 || Box.MaxX == MipWidth, "For compressed formats, the region max X coordinate (", Box.MaxX, ") must be a multiple of block width (", Uint32{FmtAttribs.BlockWidth}, ") or equal the mip level width (", MipWidth, ").");
    }
    else
        VERIFY_TEX_PARAMS(Box.MaxX <= MipWidth, "Region max X coordinate (", Box.MaxX, ") is out of allowed range [0, ", MipWidth, "].");

    if (TexDesc.Type != RESOURCE_DIM_TEX_1D &&
        TexDesc.Type != RESOURCE_DIM_TEX_1D_ARRAY)
    {
        Uint32 MipHeight = std::max(TexDesc.Height >> MipLevel, 1U);
        if (FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED)
        {
            VERIFY_EXPR((FmtAttribs.BlockHeight & (FmtAttribs.BlockHeight - 1)) == 0);
            Uint32 BlockAlignedMipHeight = (MipHeight + (FmtAttribs.BlockHeight - 1)) & ~(FmtAttribs.BlockHeight - 1);
            VERIFY_TEX_PARAMS(Box.MaxY <= BlockAlignedMipHeight, "Region max Y coordinate (", Box.MaxY, ") is out of allowed range [0, ", BlockAlignedMipHeight, "].");
            VERIFY_TEX_PARAMS((Box.MinY % FmtAttribs.BlockHeight) == 0, "For compressed formats, the region min Y coordinate (", Box.MinY, ") must be a multiple of block height (", Uint32{FmtAttribs.BlockHeight}, ").");
            VERIFY_TEX_PARAMS((Box.MaxY % FmtAttribs.BlockHeight) == 0 || Box.MaxY == MipHeight, "For compressed formats, the region max Y coordinate (", Box.MaxY, ") must be a multiple of block height (", Uint32{FmtAttribs.BlockHeight}, ") or equal the mip level height (", MipHeight, ").");
        }
        else
            VERIFY_TEX_PARAMS(Box.MaxY <= MipHeight, "Region max Y coordinate (", Box.MaxY, ") is out of allowed range [0, ", MipHeight, "].");
    }

    if (TexDesc.Type == RESOURCE_DIM_TEX_3D)
    {
        Uint32 MipDepth = std::max(TexDesc.Depth >> MipLevel, 1U);
        VERIFY_TEX_PARAMS(Box.MaxZ <= MipDepth, "Region max Z coordinate (", Box.MaxZ, ") is out of allowed range  [0, ", MipDepth, "].");
    }
    else
    {
        VERIFY_TEX_PARAMS(Box.MinZ == 0, "Region min Z (", Box.MinZ, ") must be 0 for all but 3D textures.");
        VERIFY_TEX_PARAMS(Box.MaxZ == 1, "Region max Z (", Box.MaxZ, ") must be 1 for all but 3D textures.");
    }
#endif
}

void ValidateUpdateTextureParams(const TextureDesc& TexDesc, Uint32 MipLevel, Uint32 Slice, const Box& DstBox, const TextureSubResData& SubresData)
{
    VERIFY((SubresData.pData != nullptr) ^ (SubresData.pSrcBuffer != nullptr), "Either CPU data pointer (pData) or GPU buffer (pSrcBuffer) must not be null, but not both.");
    ValidateTextureRegion(TexDesc, MipLevel, Slice, DstBox);

#ifdef DILIGENT_DEVELOPMENT
    VERIFY_TEX_PARAMS(TexDesc.SampleCount == 1, "Only non-multisampled textures can be updated with UpdateData().");
    VERIFY_TEX_PARAMS((SubresData.Stride & 0x03) == 0, "Texture data stride (", SubresData.Stride, ") must be at least 32-bit aligned.");
    VERIFY_TEX_PARAMS((SubresData.DepthStride & 0x03) == 0, "Texture data depth stride (", SubresData.DepthStride, ") must be at least 32-bit aligned.");

    auto        UpdateRegionWidth  = DstBox.MaxX - DstBox.MinX;
    auto        UpdateRegionHeight = DstBox.MaxY - DstBox.MinY;
    auto        UpdateRegionDepth  = DstBox.MaxZ - DstBox.MinZ;
    const auto& FmtAttribs         = GetTextureFormatAttribs(TexDesc.Format);
    Uint32      RowSize            = 0;
    Uint32      RowCount           = 0;
    if (FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED)
    {
        // Align update region size by the block size. This is only necessary when updating
        // coarse mip levels. Otherwise UpdateRegionWidth/Height should be multiples of block size
        VERIFY_EXPR((FmtAttribs.BlockWidth & (FmtAttribs.BlockWidth - 1)) == 0);
        VERIFY_EXPR((FmtAttribs.BlockHeight & (FmtAttribs.BlockHeight - 1)) == 0);
        UpdateRegionWidth  = (UpdateRegionWidth + (FmtAttribs.BlockWidth - 1)) & ~(FmtAttribs.BlockWidth - 1);
        UpdateRegionHeight = (UpdateRegionHeight + (FmtAttribs.BlockHeight - 1)) & ~(FmtAttribs.BlockHeight - 1);
        RowSize            = UpdateRegionWidth / Uint32{FmtAttribs.BlockWidth} * Uint32{FmtAttribs.ComponentSize};
        RowCount           = UpdateRegionHeight / FmtAttribs.BlockHeight;
    }
    else
    {
        RowSize  = UpdateRegionWidth * Uint32{FmtAttribs.ComponentSize} * Uint32{FmtAttribs.NumComponents};
        RowCount = UpdateRegionHeight;
    }
    DEV_CHECK_ERR(SubresData.Stride >= RowSize, "Source data stride (", SubresData.Stride, ") is below the image row size (", RowSize, ").");
    const Uint32 PlaneSize = SubresData.Stride * RowCount;
    DEV_CHECK_ERR(UpdateRegionDepth == 1 || SubresData.DepthStride >= PlaneSize, "Source data depth stride (", SubresData.DepthStride, ") is below the image plane size (", PlaneSize, ").");
#endif
}

void ValidateCopyTextureParams(const CopyTextureAttribs& CopyAttribs)
{
    VERIFY_EXPR(CopyAttribs.pSrcTexture != nullptr && CopyAttribs.pDstTexture != nullptr);
    Box         SrcBox;
    const auto& SrcTexDesc = CopyAttribs.pSrcTexture->GetDesc();
    const auto& DstTexDesc = CopyAttribs.pDstTexture->GetDesc();
    auto        pSrcBox    = CopyAttribs.pSrcBox;
    if (pSrcBox == nullptr)
    {
        auto MipLevelAttribs = GetMipLevelProperties(SrcTexDesc, CopyAttribs.SrcMipLevel);
        SrcBox.MaxX          = MipLevelAttribs.LogicalWidth;
        SrcBox.MaxY          = MipLevelAttribs.LogicalHeight;
        SrcBox.MaxZ          = MipLevelAttribs.Depth;
        pSrcBox              = &SrcBox;
    }
    ValidateTextureRegion(SrcTexDesc, CopyAttribs.SrcMipLevel, CopyAttribs.SrcSlice, *pSrcBox);

    Box DstBox;
    DstBox.MinX = CopyAttribs.DstX;
    DstBox.MinY = CopyAttribs.DstY;
    DstBox.MinZ = CopyAttribs.DstZ;
    DstBox.MaxX = DstBox.MinX + (pSrcBox->MaxX - pSrcBox->MinX);
    DstBox.MaxY = DstBox.MinY + (pSrcBox->MaxY - pSrcBox->MinY);
    DstBox.MaxZ = DstBox.MinZ + (pSrcBox->MaxZ - pSrcBox->MinZ);
    ValidateTextureRegion(DstTexDesc, CopyAttribs.DstMipLevel, CopyAttribs.DstSlice, DstBox);
}

void ValidateMapTextureParams(const TextureDesc& TexDesc,
                              Uint32             MipLevel,
                              Uint32             ArraySlice,
                              MAP_TYPE           MapType,
                              Uint32             MapFlags,
                              const Box*         pMapRegion)
{
    VERIFY_TEX_PARAMS(MipLevel < TexDesc.MipLevels, "Mip level (", MipLevel, ") is out of allowed range [0, ", TexDesc.MipLevels - 1, "].");
    if (TexDesc.Type == RESOURCE_DIM_TEX_1D_ARRAY ||
        TexDesc.Type == RESOURCE_DIM_TEX_2D_ARRAY ||
        TexDesc.Type == RESOURCE_DIM_TEX_CUBE ||
        TexDesc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
    {
        VERIFY_TEX_PARAMS(ArraySlice < TexDesc.ArraySize, "Array slice (", ArraySlice, ") is out of range [0,", TexDesc.ArraySize - 1, "].");
    }
    else
    {
        VERIFY_TEX_PARAMS(ArraySlice == 0, "Array slice (", ArraySlice, ") must be 0 for non-array textures.");
    }

    if (pMapRegion != nullptr)
    {
        ValidateTextureRegion(TexDesc, MipLevel, ArraySlice, *pMapRegion);
    }
}

void ValidatedAndCorrectTextureViewDesc(const TextureDesc& TexDesc, TextureViewDesc& ViewDesc) noexcept(false)
{
#define TEX_VIEW_VALIDATION_ERROR(...) LOG_ERROR_AND_THROW("\n                 Failed to create texture view '", (ViewDesc.Name ? ViewDesc.Name : ""), "' for texture '", TexDesc.Name, "': ", ##__VA_ARGS__)

    if (!(ViewDesc.ViewType > TEXTURE_VIEW_UNDEFINED && ViewDesc.ViewType < TEXTURE_VIEW_NUM_VIEWS))
        TEX_VIEW_VALIDATION_ERROR("Texture view type is not specified.");

    if (ViewDesc.MostDetailedMip >= TexDesc.MipLevels)
        TEX_VIEW_VALIDATION_ERROR("Most detailed mip (", ViewDesc.MostDetailedMip, ") is out of range. The texture has only ", TexDesc.MipLevels, " mip ", (TexDesc.MipLevels > 1 ? "levels." : "level."));

    if (ViewDesc.NumMipLevels != REMAINING_MIP_LEVELS && ViewDesc.MostDetailedMip + ViewDesc.NumMipLevels > TexDesc.MipLevels)
        TEX_VIEW_VALIDATION_ERROR("Most detailed mip (", ViewDesc.MostDetailedMip, ") and number of mip levels in the view (", ViewDesc.NumMipLevels, ") is out of range. The texture has only ", TexDesc.MipLevels, " mip ", (TexDesc.MipLevels > 1 ? "levels." : "level."));

    if (ViewDesc.Format == TEX_FORMAT_UNKNOWN)
        ViewDesc.Format = GetDefaultTextureViewFormat(TexDesc.Format, ViewDesc.ViewType, TexDesc.BindFlags);

    if (ViewDesc.TextureDim == RESOURCE_DIM_UNDEFINED)
    {
        if (TexDesc.Type == RESOURCE_DIM_TEX_CUBE || TexDesc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
        {
            switch (ViewDesc.ViewType)
            {
                case TEXTURE_VIEW_SHADER_RESOURCE:
                    ViewDesc.TextureDim = TexDesc.Type;
                    break;

                case TEXTURE_VIEW_RENDER_TARGET:
                case TEXTURE_VIEW_DEPTH_STENCIL:
                case TEXTURE_VIEW_UNORDERED_ACCESS:
                    ViewDesc.TextureDim = RESOURCE_DIM_TEX_2D_ARRAY;
                    break;

                default: UNEXPECTED("Unexpected view type");
            }
        }
        else
        {
            ViewDesc.TextureDim = TexDesc.Type;
        }
    }

    switch (TexDesc.Type)
    {
        case RESOURCE_DIM_TEX_1D:
            if (ViewDesc.TextureDim != RESOURCE_DIM_TEX_1D)
            {
                TEX_VIEW_VALIDATION_ERROR("Incorrect texture type for Texture 1D view: only Texture 1D is allowed.");
            }
            break;

        case RESOURCE_DIM_TEX_1D_ARRAY:
            if (ViewDesc.TextureDim != RESOURCE_DIM_TEX_1D_ARRAY &&
                ViewDesc.TextureDim != RESOURCE_DIM_TEX_1D)
            {
                TEX_VIEW_VALIDATION_ERROR("Incorrect view type for Texture 1D Array: only Texture 1D or Texture 1D Array are allowed.");
            }
            break;

        case RESOURCE_DIM_TEX_2D:
            if (ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D_ARRAY &&
                ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D)
            {
                TEX_VIEW_VALIDATION_ERROR("Incorrect texture type for Texture 2D view: only Texture 2D or Texture 2D Array are allowed.");
            }
            break;

        case RESOURCE_DIM_TEX_2D_ARRAY:
            if (ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D_ARRAY &&
                ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D)
            {
                TEX_VIEW_VALIDATION_ERROR("Incorrect texture type for Texture 2D Array view: only Texture 2D or Texture 2D Array are allowed.");
            }
            break;

        case RESOURCE_DIM_TEX_3D:
            if (ViewDesc.TextureDim != RESOURCE_DIM_TEX_3D)
            {
                TEX_VIEW_VALIDATION_ERROR("Incorrect texture type for Texture 3D view: only Texture 3D is allowed.");
            }
            break;

        case RESOURCE_DIM_TEX_CUBE:
            if (ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE)
            {
                if (ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D &&
                    ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D_ARRAY &&
                    ViewDesc.TextureDim != RESOURCE_DIM_TEX_CUBE)
                {
                    TEX_VIEW_VALIDATION_ERROR("Incorrect texture type for Texture cube SRV: Texture 2D, Texture 2D array or Texture Cube is allowed.");
                }
            }
            else
            {
                if (ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D &&
                    ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D_ARRAY)
                {
                    TEX_VIEW_VALIDATION_ERROR("Incorrect texture type for Texture cube non-shader resource view: Texture 2D or Texture 2D array is allowed.");
                }
            }
            break;

        case RESOURCE_DIM_TEX_CUBE_ARRAY:
            if (ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE)
            {
                if (ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D &&
                    ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D_ARRAY &&
                    ViewDesc.TextureDim != RESOURCE_DIM_TEX_CUBE &&
                    ViewDesc.TextureDim != RESOURCE_DIM_TEX_CUBE_ARRAY)
                {
                    TEX_VIEW_VALIDATION_ERROR("Incorrect texture type for Texture cube array SRV: Texture 2D, Texture 2D array, Texture Cube or Texture Cube Array is allowed.");
                }
            }
            else
            {
                if (ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D &&
                    ViewDesc.TextureDim != RESOURCE_DIM_TEX_2D_ARRAY)
                {
                    TEX_VIEW_VALIDATION_ERROR("Incorrect texture type for Texture cube array non-shader resource view: Texture 2D or Texture 2D array is allowed.");
                }
            }
            break;

        default:
            UNEXPECTED("Unexpected texture type");
            break;
    }

    if (ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE)
    {
        if (ViewDesc.ViewType != TEXTURE_VIEW_SHADER_RESOURCE)
            TEX_VIEW_VALIDATION_ERROR("Unexpected view type: SRV is expected.");
        if (ViewDesc.NumArraySlices != 6 && ViewDesc.NumArraySlices != 0 && ViewDesc.NumArraySlices != REMAINING_ARRAY_SLICES)
            TEX_VIEW_VALIDATION_ERROR("Texture cube SRV is expected to have 6 array slices, while ", ViewDesc.NumArraySlices, " is provided.");
        if (ViewDesc.FirstArraySlice != 0)
            TEX_VIEW_VALIDATION_ERROR("First slice (", ViewDesc.FirstArraySlice, ") must be 0 for non-array texture cube SRV.");
    }
    if (ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE_ARRAY)
    {
        if (ViewDesc.ViewType != TEXTURE_VIEW_SHADER_RESOURCE)
            TEX_VIEW_VALIDATION_ERROR("Unexpected view type: SRV is expected.");
        if (ViewDesc.NumArraySlices != REMAINING_ARRAY_SLICES && (ViewDesc.NumArraySlices % 6) != 0)
            TEX_VIEW_VALIDATION_ERROR("Number of slices in texture cube array SRV is expected to be multiple of 6. ", ViewDesc.NumArraySlices, " slices is provided.");
    }

    if (ViewDesc.TextureDim == RESOURCE_DIM_TEX_1D ||
        ViewDesc.TextureDim == RESOURCE_DIM_TEX_2D)
    {
        if (ViewDesc.FirstArraySlice != 0)
            TEX_VIEW_VALIDATION_ERROR("First slice (", ViewDesc.FirstArraySlice, ") must be 0 for non-array texture 1D/2D views.");

        if (ViewDesc.NumArraySlices != REMAINING_ARRAY_SLICES && ViewDesc.NumArraySlices > 1)
            TEX_VIEW_VALIDATION_ERROR("Number of slices in the view (", ViewDesc.NumArraySlices, ") must be 1 (or 0) for non-array texture 1D/2D views.");
    }
    else if (ViewDesc.TextureDim == RESOURCE_DIM_TEX_1D_ARRAY ||
             ViewDesc.TextureDim == RESOURCE_DIM_TEX_2D_ARRAY ||
             ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE ||
             ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE_ARRAY)
    {
        if (ViewDesc.FirstArraySlice >= TexDesc.ArraySize)
            TEX_VIEW_VALIDATION_ERROR("First array slice (", ViewDesc.FirstArraySlice, ") exceeds the number of slices in the texture array (", TexDesc.ArraySize, ").");

        if (ViewDesc.NumArraySlices != REMAINING_ARRAY_SLICES && ViewDesc.FirstArraySlice + ViewDesc.NumArraySlices > TexDesc.ArraySize)
            TEX_VIEW_VALIDATION_ERROR("First slice (", ViewDesc.FirstArraySlice, ") and number of slices in the view (", ViewDesc.NumArraySlices, ") specify more slices than target texture has (", TexDesc.ArraySize, ").");
    }
    else if (ViewDesc.TextureDim == RESOURCE_DIM_TEX_3D)
    {
        auto MipDepth = TexDesc.Depth >> ViewDesc.MostDetailedMip;
        if (ViewDesc.FirstDepthSlice + ViewDesc.NumDepthSlices > MipDepth)
            TEX_VIEW_VALIDATION_ERROR("First slice (", ViewDesc.FirstDepthSlice, ") and number of slices in the view (", ViewDesc.NumDepthSlices, ") specify more slices than target 3D texture mip level has (", MipDepth, ").");
    }
    else
    {
        UNEXPECTED("Unexpected texture dimension");
    }

    if (GetTextureFormatAttribs(ViewDesc.Format).IsTypeless)
    {
        TEX_VIEW_VALIDATION_ERROR("Texture view format (", GetTextureFormatAttribs(ViewDesc.Format).Name, ") cannot be typeless.");
    }

    if ((ViewDesc.Flags & TEXTURE_VIEW_FLAG_ALLOW_MIP_MAP_GENERATION) != 0)
    {
        if ((TexDesc.MiscFlags & MISC_TEXTURE_FLAG_GENERATE_MIPS) == 0)
            TEX_VIEW_VALIDATION_ERROR("TEXTURE_VIEW_FLAG_ALLOW_MIP_MAP_GENERATION flag can only set if the texture was created with MISC_TEXTURE_FLAG_GENERATE_MIPS flag.");

        if (ViewDesc.ViewType != TEXTURE_VIEW_SHADER_RESOURCE)
            TEX_VIEW_VALIDATION_ERROR("TEXTURE_VIEW_FLAG_ALLOW_MIP_MAP_GENERATION flag can only be used with TEXTURE_VIEW_SHADER_RESOURCE view type.");
    }

#undef TEX_VIEW_VALIDATION_ERROR

    if (ViewDesc.NumMipLevels == 0 || ViewDesc.NumMipLevels == REMAINING_MIP_LEVELS)
    {
        if (ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE)
            ViewDesc.NumMipLevels = TexDesc.MipLevels - ViewDesc.MostDetailedMip;
        else
            ViewDesc.NumMipLevels = 1;
    }

    if (ViewDesc.NumArraySlices == 0 || ViewDesc.NumArraySlices == REMAINING_ARRAY_SLICES)
    {
        if (ViewDesc.TextureDim == RESOURCE_DIM_TEX_1D_ARRAY ||
            ViewDesc.TextureDim == RESOURCE_DIM_TEX_2D_ARRAY ||
            ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE ||
            ViewDesc.TextureDim == RESOURCE_DIM_TEX_CUBE_ARRAY)
            ViewDesc.NumArraySlices = TexDesc.ArraySize - ViewDesc.FirstArraySlice;
        else if (ViewDesc.TextureDim == RESOURCE_DIM_TEX_3D)
        {
            auto MipDepth           = TexDesc.Depth >> ViewDesc.MostDetailedMip;
            ViewDesc.NumDepthSlices = MipDepth - ViewDesc.FirstDepthSlice;
        }
        else
            ViewDesc.NumArraySlices = 1;
    }

    if ((ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET) &&
        (ViewDesc.Format == TEX_FORMAT_R8_SNORM || ViewDesc.Format == TEX_FORMAT_RG8_SNORM || ViewDesc.Format == TEX_FORMAT_RGBA8_SNORM ||
         ViewDesc.Format == TEX_FORMAT_R16_SNORM || ViewDesc.Format == TEX_FORMAT_RG16_SNORM || ViewDesc.Format == TEX_FORMAT_RGBA16_SNORM))
    {
        const auto* FmtName = GetTextureFormatAttribs(ViewDesc.Format).Name;
        LOG_WARNING_MESSAGE(FmtName, " render target view is created.\n"
                                     "There might be an issue in OpenGL driver on NVidia hardware: when rendering to SNORM textures, all negative values are clamped to zero.\n"
                                     "Use UNORM format instead.");
    }
}

} // namespace Diligent
