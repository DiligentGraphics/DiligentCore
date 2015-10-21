/*     Copyright 2015 Egor Yusov
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

#include "pch.h"
#include "Texture.h"

namespace Diligent
{

void ValidateTextureDesc( const TextureDesc& Desc )
{
#define LOG_TEXTURE_ERROR_AND_THROW(...) LOG_ERROR_AND_THROW("Texture \"", Desc.Name ? Desc.Name : "", "\": ", ##__VA_ARGS__)

    // Perform some parameter correctness check
    if( Desc.Type == TEXTURE_TYPE_1D || Desc.Type == TEXTURE_TYPE_1D_ARRAY )
    {
        if( Desc.Height != 1 )
            LOG_TEXTURE_ERROR_AND_THROW("Height (", Desc.Height,") of Texture 1D/Texture 1D Array must be 1");
    }

    if(  Desc.Type == TEXTURE_TYPE_1D || Desc.Type == TEXTURE_TYPE_2D )
    {
        if( Desc.ArraySize != 1 )
            LOG_TEXTURE_ERROR_AND_THROW("Texture 1D/2D must have one array slice (", Desc.ArraySize, " provided). Use Texture 1D/2D array if you need more than one slice.");
    }

    Uint32 MaxDim = 0;
    if( Desc.Type == TEXTURE_TYPE_1D || Desc.Type == TEXTURE_TYPE_1D_ARRAY )
        MaxDim = Desc.Width;
    else if( Desc.Type == TEXTURE_TYPE_2D || Desc.Type == TEXTURE_TYPE_2D_ARRAY )
        MaxDim = std::max(Desc.Width, Desc.Height);
    else if( Desc.Type == TEXTURE_TYPE_3D )
        MaxDim = std::max( std::max(Desc.Width, Desc.Height), Desc.Depth );
    VERIFY( MaxDim >= (1U << (Desc.MipLevels-1)), "Texture \"", Desc.Name ? Desc.Name : "", "\": Incorrect number of Mip levels (", Desc.MipLevels, ")" )

    if( Desc.SampleCount > 1 )
    {
        if( !(Desc.Type == TEXTURE_TYPE_2D || Desc.Type == TEXTURE_TYPE_2D_ARRAY) )
            LOG_TEXTURE_ERROR_AND_THROW("Only Texture 2D/Texture 2D Array can be multisampled");

        if( Desc.MipLevels != 1 )
            LOG_TEXTURE_ERROR_AND_THROW("Multisampled textures must have one mip level (", Desc.MipLevels, " levels specified)");

        if( Desc.BindFlags & BIND_UNORDERED_ACCESS )
            LOG_TEXTURE_ERROR_AND_THROW("UAVs are not allowed for multisampled resources");
    }

    if( (Desc.BindFlags & BIND_RENDER_TARGET)  && 
        ( Desc.Format == TEX_FORMAT_R8_SNORM  || Desc.Format == TEX_FORMAT_RG8_SNORM  || Desc.Format == TEX_FORMAT_RGBA8_SNORM ||
          Desc.Format == TEX_FORMAT_R16_SNORM || Desc.Format == TEX_FORMAT_RG16_SNORM || Desc.Format == TEX_FORMAT_RGBA16_SNORM ) )
    {
        const auto *FmtName = GetTextureFormatAttribs( Desc.Format ).Name;
        LOG_WARNING_MESSAGE( FmtName, " texture is created with BIND_RENDER_TARGET flag set.\n" 
                             "There might be an issue in OpenGL driver on NVidia hardware: when rendering to SNORM textures, all negative values are clamped to zero.\n"
                             "Use UNORM format instead." );
    }
}


void ValidateTextureRegion(const TextureDesc &TexDesc, Uint32 MipLevel, Uint32 Slice, const Box &Box)
{
#define VERIFY_TEX_PARAMS(Expr, ...) VERIFY(Expr, "Texture \"", TexDesc.Name ? TexDesc.Name : "", "\": ", ##__VA_ARGS__)

    VERIFY_TEX_PARAMS( MipLevel < TexDesc.MipLevels, "Mip level (", MipLevel, ") is out of allowed range [0, ", TexDesc.MipLevels-1, "]" );
    VERIFY_TEX_PARAMS( Box.MinX < Box.MaxX, "Incorrect X range [",Box.MinX, ", ", Box.MaxX, ")" );
    VERIFY_TEX_PARAMS( Box.MinY < Box.MaxY, "Incorrect Y range [",Box.MinY, ", ", Box.MaxY, ")" );
    VERIFY_TEX_PARAMS( Box.MinZ < Box.MaxZ, "Incorrect Z range [",Box.MinZ, ", ", Box.MaxZ, ")" );
    
    if( TexDesc.Type == TEXTURE_TYPE_1D_ARRAY ||
        TexDesc.Type == TEXTURE_TYPE_2D_ARRAY )
    {
        VERIFY_TEX_PARAMS( Slice < TexDesc.ArraySize, "Array slice (", Slice, ") is out of range [0,", TexDesc.ArraySize-1, "]" );
    }
    else
    {
        VERIFY_TEX_PARAMS( Slice == 0, "Array slice (", Slice, ") must be 0 for non-array textures" );
    }

    Uint32 MipWidth = std::max(TexDesc.Width >> MipLevel, 1U);
    VERIFY_TEX_PARAMS( Box.MaxX <= MipWidth, "Region max X coordinate (", Box.MaxX, ") is out of allowed range [0, ", MipWidth, "]" );
    if( TexDesc.Type != TEXTURE_TYPE_1D && 
        TexDesc.Type != TEXTURE_TYPE_1D_ARRAY )
    {
        Uint32 MipHeight = std::max(TexDesc.Height >> MipLevel, 1U);
        VERIFY_TEX_PARAMS( Box.MaxY <= MipHeight, "Region max Y coordinate (", Box.MaxY, ") is out of allowed range [0, ", MipHeight, "]" );
    }

    if( TexDesc.Type == TEXTURE_TYPE_3D )
    {
        Uint32 MipDepth = std::max(TexDesc.Depth >> MipLevel, 1U);
        VERIFY_TEX_PARAMS( Box.MaxZ <= MipDepth, "Region max Z coordinate (", Box.MaxZ, ") is out of allowed range  [0, ", MipDepth, "]" );
    }
}

void ValidateUpdateDataParams( const TextureDesc &TexDesc, Uint32 MipLevel, Uint32 Slice, const Box &DstBox, const TextureSubResData &SubresData )
{
    ValidateTextureRegion(TexDesc, MipLevel, Slice, DstBox);

    VERIFY_TEX_PARAMS( (SubresData.Stride & 0x03) == 0, "Texture data stride (", SubresData.Stride, ") must be at least 32-bit aligned" );
    VERIFY_TEX_PARAMS( (SubresData.DepthStride & 0x03) == 0, "Texture data depth stride (", SubresData.DepthStride, ") must be at least 32-bit aligned" );
}

void VliadateCopyTextureDataParams( const TextureDesc &SrcTexDesc, Uint32 SrcMipLevel, Uint32 SrcSlice, const Box *pSrcBox,
                                    const TextureDesc &DstTexDesc, Uint32 DstMipLevel, Uint32 DstSlice,
                                    Uint32 DstX, Uint32 DstY, Uint32 DstZ )
{
    Box SrcBox;
    if( pSrcBox == nullptr )
    {
        SrcBox.MaxX = std::max( SrcTexDesc.Width >> SrcMipLevel, 1u );
        if( SrcTexDesc.Type == TEXTURE_TYPE_1D || 
            SrcTexDesc.Type == TEXTURE_TYPE_1D_ARRAY )
            SrcBox.MaxY = 1;
        else
            SrcBox.MaxY = std::max( SrcTexDesc.Height >> SrcMipLevel, 1u );

        if( SrcTexDesc.Type == TEXTURE_TYPE_3D )
            SrcBox.MaxZ = std::max( SrcTexDesc.Depth >> SrcMipLevel, 1u );
        else
            SrcBox.MaxZ = 1;

        pSrcBox = &SrcBox;
    }
    ValidateTextureRegion(SrcTexDesc, SrcMipLevel, SrcSlice, *pSrcBox);

    Box DstBox;
    DstBox.MinX = DstX;
    DstBox.MaxX = DstBox.MinX + (pSrcBox->MaxX - pSrcBox->MinX);
    DstBox.MinY = DstY;
    DstBox.MaxY = DstBox.MinY + (pSrcBox->MaxY - pSrcBox->MinY);
    DstBox.MinZ = DstZ;
    DstBox.MaxZ = DstBox.MinZ + (pSrcBox->MaxZ - pSrcBox->MinZ);
    ValidateTextureRegion(DstTexDesc, DstMipLevel, DstSlice, DstBox);
}


//void CTexture :: Map(MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData)
//{
    //switch(MapType)
    //{
    //    case MAP_READ:
    //        VERIFY( "Only buffers with usage USAGE_CPU_ACCESSIBLE can be read from" && m_Desc.Usage == USAGE_CPU_ACCESSIBLE);
    //        VERIFY( "Buffer being mapped for reading was not created with CPU_ACCESS_READ flag" && (m_Desc.CPUAccessFlags & CPU_ACCESS_READ));
    //    break;

    //    case MAP_WRITE:
    //        VERIFY( "Only buffers with usage USAGE_CPU_ACCESSIBLE can be written to" && m_Desc.Usage == USAGE_CPU_ACCESSIBLE );
    //        VERIFY( "Buffer being mapped for writing was not created with CPU_ACCESS_WRITE flag" && (m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE));
    //    break;

    //    case MAP_READ_WRITE:
    //        VERIFY( "Only buffers with usage USAGE_CPU_ACCESSIBLE can be read and written" && m_Desc.Usage == USAGE_CPU_ACCESSIBLE );
    //        VERIFY( "Buffer being mapped for reading & writing was not created with CPU_ACCESS_WRITE flag" && (m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE));
    //        VERIFY( "Buffer being mapped for reading & writing was not created with CPU_ACCESS_READ flag" && (m_Desc.CPUAccessFlags & CPU_ACCESS_READ));
    //    break;

    //    case MAP_WRITE_DISCARD:
    //        VERIFY( "Only dynamic buffers can be mapped with write discard flag" && m_Desc.Usage == USAGE_DYNAMIC );
    //        VERIFY( "Dynamic buffer must be created with CPU_ACCESS_WRITE flag" && (m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE) );
    //    break;

    //    case MAP_WRITE_NO_OVERWRITE:
    //        VERIFY( "Only dynamic buffers can be mapped with write no overwrite flag" && m_Desc.Usage == USAGE_DYNAMIC );
    //        VERIFY( "Dynamic buffer must be created with CPU_ACCESS_WRITE flag" && (m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE) );
    //    break;

    //    default: VERIFY("Unknown map type" && false);
    //}
//}

}