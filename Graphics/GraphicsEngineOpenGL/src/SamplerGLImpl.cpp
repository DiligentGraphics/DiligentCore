/*     Copyright 2015-2016 Egor Yusov
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
#include "SamplerGLImpl.h"
#include "RenderDeviceGLImpl.h"
#include "GLTypeConversions.h"

namespace Diligent
{

SamplerGLImpl::SamplerGLImpl(FixedBlockMemoryAllocator &SamplerObjAllocator, class RenderDeviceGLImpl *pDeviceGL, const SamplerDesc& SamplerDesc, bool bIsDeviceInternal) : 
    TSamplerBase( SamplerObjAllocator, pDeviceGL, SamplerDesc, bIsDeviceInternal ),
    m_GlSampler(true)
{
    const auto& SamCaps = pDeviceGL->GetDeviceCaps().SamCaps;

    Bool bMinAnisotropic = False, bMagAnisotropic = False, bMipAnisotropic = False;
    Bool bMinComparison = False, bMagComparison = False, bMipComparison = False;
    GLenum GLMinFilter, GLMagFilter, GLMipFilter; 
    FilterTypeToGLFilterType(SamplerDesc.MinFilter, GLMinFilter, bMinAnisotropic, bMinComparison);
    FilterTypeToGLFilterType(SamplerDesc.MagFilter, GLMagFilter, bMagAnisotropic, bMagComparison);
    FilterTypeToGLFilterType(SamplerDesc.MipFilter, GLMipFilter, bMipAnisotropic, bMipComparison);
    VERIFY( bMinAnisotropic == bMagAnisotropic && bMagAnisotropic == bMipAnisotropic, "Incosistent anisotropy filter setting" );
    VERIFY( bMinComparison == bMagComparison && bMagComparison == bMipComparison, "Incosistent comparison filter setting" );
    
    glSamplerParameteri(m_GlSampler, GL_TEXTURE_MAG_FILTER, GLMagFilter);

    GLenum GlMinMipFilter = 0;
    if( GLMinFilter == GL_NEAREST && GLMipFilter == GL_NEAREST )
        GlMinMipFilter = GL_NEAREST_MIPMAP_NEAREST;
    else if( GLMinFilter == GL_LINEAR && GLMipFilter == GL_NEAREST )
        GlMinMipFilter = GL_LINEAR_MIPMAP_NEAREST;
    else if( GLMinFilter == GL_NEAREST && GLMipFilter == GL_LINEAR )
        GlMinMipFilter = GL_NEAREST_MIPMAP_LINEAR;
    else if( GLMinFilter == GL_LINEAR && GLMipFilter == GL_LINEAR )
        GlMinMipFilter = GL_LINEAR_MIPMAP_LINEAR;
    else
        LOG_ERROR_AND_THROW( "Unsupported min/mip filter combination" );
    glSamplerParameteri(m_GlSampler, GL_TEXTURE_MIN_FILTER, GlMinMipFilter);

    GLenum WrapModes[3] = { 0 };
    TEXTURE_ADDRESS_MODE AddressModes[] =
    {
        SamplerDesc.AddressU,
        SamplerDesc.AddressV,
        SamplerDesc.AddressW
    };
    for( int i = 0; i < _countof( AddressModes ); ++i )
    {
        auto &WrapMode = WrapModes[i];
        WrapMode = TexAddressModeToGLAddressMode( AddressModes[i] );
        if( !SamCaps.bBorderSamplingModeSupported && WrapMode == GL_CLAMP_TO_BORDER )
        {
            LOG_ERROR_MESSAGE( "GL_CLAMP_TO_BORDER filtering mode is not supported. Defaulting to GL_CLAMP_TO_EDGE.\n" );
            WrapMode = GL_CLAMP_TO_EDGE;
        }
    }
    glSamplerParameteri(m_GlSampler, GL_TEXTURE_WRAP_S, WrapModes[0]);
    glSamplerParameteri(m_GlSampler, GL_TEXTURE_WRAP_T, WrapModes[1]);
    glSamplerParameteri(m_GlSampler, GL_TEXTURE_WRAP_R, WrapModes[2]);
    
    if( SamCaps.bLODBiasSupported ) // Can be unsupported
        glSamplerParameterf(m_GlSampler, GL_TEXTURE_LOD_BIAS, SamplerDesc.MipLODBias);
    else
    {
        if( SamplerDesc.MipLODBias )
            LOG_WARNING_MESSAGE( "Texture LOD bias sampler attribute is not supported\n" );
    }
    
    if( SamCaps.bAnisotropicFilteringSupported ) // Can be unsupported
        glSamplerParameterf(m_GlSampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, bMipAnisotropic ? static_cast<float>(SamplerDesc.MaxAnisotropy) : 1.f);
    else
    {
        if( bMipAnisotropic && SamplerDesc.MaxAnisotropy != 1 )
            LOG_WARNING_MESSAGE( "Max anisotropy sampler attribute is not supported\n" );
    }

    glSamplerParameteri(m_GlSampler, GL_TEXTURE_COMPARE_MODE, bMinComparison ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
    
    if( SamCaps.bBorderSamplingModeSupported ) // Can be unsupported
        glSamplerParameterfv(m_GlSampler, GL_TEXTURE_BORDER_COLOR, SamplerDesc.BorderColor);
    else
    {
        if( SamplerDesc.BorderColor[0] != 0 || SamplerDesc.BorderColor[1] != 0 || SamplerDesc.BorderColor[2] != 0 || SamplerDesc.BorderColor[3] != 0 )
            LOG_WARNING_MESSAGE( "Border color sampler attribute is not supported\n" );
    }
    GLenum GLCompareFunc = CompareFuncToGLCompareFunc(SamplerDesc.ComparisonFunc);
    glSamplerParameteri(m_GlSampler, GL_TEXTURE_COMPARE_FUNC, GLCompareFunc);

    glSamplerParameterf(m_GlSampler, GL_TEXTURE_MAX_LOD, SamplerDesc.MaxLOD);
    glSamplerParameterf(m_GlSampler, GL_TEXTURE_MIN_LOD, SamplerDesc.MinLOD);

    CHECK_GL_ERROR_AND_THROW("Failed to create OpenGL texture sampler\n");
}

SamplerGLImpl::~SamplerGLImpl()
{

}

IMPLEMENT_QUERY_INTERFACE( SamplerGLImpl, IID_SamplerGL, TSamplerBase )

}
