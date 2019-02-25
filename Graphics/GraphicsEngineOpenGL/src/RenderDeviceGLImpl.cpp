/*     Copyright 2015-2019 Egor Yusov
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

#include "RenderDeviceGLImpl.h"

#include "BufferGLImpl.h"
#include "ShaderGLImpl.h"
#include "VAOCache.h"
#include "Texture1D_OGL.h"
#include "Texture1DArray_OGL.h"
#include "Texture2D_OGL.h"
#include "Texture2DArray_OGL.h"
#include "Texture3D_OGL.h"
#include "TextureCube_OGL.h"
#include "TextureCubeArray_OGL.h"
#include "SamplerGLImpl.h"
#include "DeviceContextGLImpl.h"
#include "GLTypeConversions.h"
#include "PipelineStateGLImpl.h"
#include "ShaderResourceBindingGLImpl.h"
#include "FenceGLImpl.h"
#include "EngineMemory.h"
#include "StringTools.h"

namespace Diligent
{

RenderDeviceGLImpl :: RenderDeviceGLImpl(IReferenceCounters *pRefCounters, IMemoryAllocator& RawMemAllocator, const EngineGLAttribs& InitAttribs):
    TRenderDeviceBase
    {
        pRefCounters,
        RawMemAllocator,
        0,
        sizeof(TextureBaseGL),
        sizeof(TextureViewGLImpl),
        sizeof(BufferGLImpl),
        sizeof(BufferViewGLImpl),
        sizeof(ShaderGLImpl),
        sizeof(SamplerGLImpl),
        sizeof(PipelineStateGLImpl),
        sizeof(ShaderResourceBindingGLImpl),
        sizeof(FenceGLImpl)
    },
    // Device caps must be filled in before the constructor of Pipeline Cache is called!
    m_GLContext(InitAttribs, m_DeviceCaps),
    m_TexRegionRender(this)
{
    GLint NumExtensions = 0;
    glGetIntegerv( GL_NUM_EXTENSIONS,& NumExtensions );
    CHECK_GL_ERROR( "Failed to get the number of extensions" );
    m_ExtensionStrings.reserve(NumExtensions);
    for( int Ext = 0; Ext < NumExtensions; ++Ext )
    {
        auto CurrExtension = glGetStringi( GL_EXTENSIONS, Ext );
        CHECK_GL_ERROR( "Failed to get extension string #", Ext );
        m_ExtensionStrings.emplace( reinterpret_cast<const Char*>(CurrExtension) );
    }

    FlagSupportedTexFormats();
    QueryDeviceCaps();

    std::basic_string<GLubyte> glstrVendor = glGetString( GL_VENDOR );
    std::string Vendor = StrToLower(std::string(glstrVendor.begin(), glstrVendor.end()));
    LOG_INFO_MESSAGE("GPU Vendor: ", Vendor);

    if( Vendor.find( "intel" ) != std::string::npos )
        m_GPUInfo.Vendor = GPU_VENDOR::INTEL;
    else if( Vendor.find( "nvidia" ) != std::string::npos )
        m_GPUInfo.Vendor = GPU_VENDOR::NVIDIA;
    else if( Vendor.find( "ati" ) != std::string::npos || 
             Vendor.find( "amd" ) != std::string::npos )
        m_GPUInfo.Vendor = GPU_VENDOR::ATI;
    else if( Vendor.find( "qualcomm" ) )
        m_GPUInfo.Vendor = GPU_VENDOR::QUALCOMM;
}

RenderDeviceGLImpl :: ~RenderDeviceGLImpl()
{
}

IMPLEMENT_QUERY_INTERFACE( RenderDeviceGLImpl, IID_RenderDeviceGL, TRenderDeviceBase )

void RenderDeviceGLImpl :: CreateBuffer(const BufferDesc& BuffDesc, const BufferData* pBuffData, IBuffer **ppBuffer, bool bIsDeviceInternal)
{
    CreateDeviceObject( "buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferGLImpl *pBufferOGL( NEW_RC_OBJ(m_BufObjAllocator, "BufferGLImpl instance", BufferGLImpl)
                                                (m_BuffViewObjAllocator, this, BuffDesc, pBuffData, bIsDeviceInternal ) );
            pBufferOGL->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferOGL->CreateDefaultViews();
            OnCreateDeviceObject( pBufferOGL );
        } 
    );
}

void RenderDeviceGLImpl :: CreateBuffer(const BufferDesc& BuffDesc, const BufferData* BuffData, IBuffer **ppBuffer)
{
	CreateBuffer(BuffDesc, BuffData, ppBuffer, false);
}

void RenderDeviceGLImpl :: CreateBufferFromGLHandle(Uint32 GLHandle, const BufferDesc& BuffDesc, RESOURCE_STATE InitialState, IBuffer **ppBuffer)
{
    VERIFY(GLHandle, "GL buffer handle must not be null");
    CreateDeviceObject( "buffer", BuffDesc, ppBuffer, 
        [&]()
        {
            BufferGLImpl *pBufferOGL( NEW_RC_OBJ(m_BufObjAllocator, "BufferGLImpl instance", BufferGLImpl)
                                                (m_BuffViewObjAllocator, this, BuffDesc, GLHandle, false ) );
            pBufferOGL->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(ppBuffer) );
            pBufferOGL->CreateDefaultViews();
            OnCreateDeviceObject( pBufferOGL );
        } 
    );
}

void RenderDeviceGLImpl :: CreateShader(const ShaderCreationAttribs& ShaderCreationAttribs, IShader** ppShader, bool bIsDeviceInternal)
{
    CreateDeviceObject( "shader", ShaderCreationAttribs.Desc, ppShader, 
        [&]()
        {
            ShaderGLImpl *pShaderOGL(NEW_RC_OBJ(m_ShaderObjAllocator, "ShaderGLImpl instance", ShaderGLImpl)
                                               (this, ShaderCreationAttribs, bIsDeviceInternal));
            pShaderOGL->QueryInterface(IID_Shader, reinterpret_cast<IObject**>(ppShader) );

            OnCreateDeviceObject( pShaderOGL );
        }
    );
}

void RenderDeviceGLImpl :: CreateShader(const ShaderCreationAttribs& ShaderCreationAttribs, IShader **ppShader)
{
	CreateShader(ShaderCreationAttribs, ppShader, false);
}

void RenderDeviceGLImpl :: CreateTexture(const TextureDesc& TexDesc, const TextureData* pData, ITexture **ppTexture, bool bIsDeviceInternal)
{
    CreateDeviceObject( "texture", TexDesc, ppTexture, 
        [&]()
        {
            auto spDeviceContext = GetImmediateContext();
            VERIFY(spDeviceContext, "Immediate device context has been destroyed");
            auto pDeviceContext = spDeviceContext.RawPtr<DeviceContextGLImpl>();
            const auto& FmtInfo = GetTextureFormatInfo( TexDesc.Format );
            if( !FmtInfo.Supported )
            {
                LOG_ERROR_AND_THROW( FmtInfo.Name, " is not supported texture format" );
            }

            TextureBaseGL *pTextureOGL = nullptr;
            switch(TexDesc.Type)
            {
                case RESOURCE_DIM_TEX_1D:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "Texture1D_OGL instance", Texture1D_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, pData, bIsDeviceInternal);
                    break;
        
                case RESOURCE_DIM_TEX_1D_ARRAY:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "Texture1DArray_OGL instance", Texture1DArray_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, pData, bIsDeviceInternal);
                    break;

                case RESOURCE_DIM_TEX_2D:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "Texture2D_OGL instance", Texture2D_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, pData, bIsDeviceInternal);
                    break;
        
                case RESOURCE_DIM_TEX_2D_ARRAY:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "Texture2DArray_OGL instance", Texture2DArray_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, pData, bIsDeviceInternal);
                    break;

                case RESOURCE_DIM_TEX_3D:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "Texture3D_OGL instance", Texture3D_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, pData, bIsDeviceInternal);
                    break;

                case RESOURCE_DIM_TEX_CUBE:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "TextureCube_OGL instance", TextureCube_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, pData, bIsDeviceInternal);
                    break;

                case RESOURCE_DIM_TEX_CUBE_ARRAY:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "TextureCubeArray_OGL instance", TextureCubeArray_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, pData, bIsDeviceInternal);
                    break;

                default: LOG_ERROR_AND_THROW( "Unknown texture type. (Did you forget to initialize the Type member of TextureDesc structure?)" );
            }
    
            pTextureOGL->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureOGL->CreateDefaultViews();
            OnCreateDeviceObject( pTextureOGL );
        }
    );
}

void RenderDeviceGLImpl::CreateTexture(const TextureDesc& TexDesc, const TextureData* Data, ITexture **ppTexture)
{
	CreateTexture(TexDesc, Data, ppTexture, false);
}

void RenderDeviceGLImpl::CreateTextureFromGLHandle(Uint32 GLHandle, const TextureDesc& TexDesc, RESOURCE_STATE InitialState, ITexture **ppTexture)
{
    VERIFY(GLHandle, "GL texture handle must not be null");
    CreateDeviceObject( "texture", TexDesc, ppTexture,
        [&]()
        {
            auto spDeviceContext = GetImmediateContext();
            VERIFY(spDeviceContext, "Immediate device context has been destroyed");
            auto pDeviceContext = spDeviceContext.RawPtr<DeviceContextGLImpl>();
            TextureBaseGL *pTextureOGL = nullptr;
            switch(TexDesc.Type)
            {
                case RESOURCE_DIM_TEX_1D:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "Texture1D_OGL instance", Texture1D_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, GLHandle);
                    break;
        
                case RESOURCE_DIM_TEX_1D_ARRAY:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "Texture1DArray_OGL instance", Texture1DArray_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, GLHandle);
                    break;

                case RESOURCE_DIM_TEX_2D:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "Texture2D_OGL instance", Texture2D_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, GLHandle);
                    break;
        
                case RESOURCE_DIM_TEX_2D_ARRAY:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "Texture2DArray_OGL instance", Texture2DArray_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, GLHandle);
                    break;

                case RESOURCE_DIM_TEX_3D:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "Texture3D_OGL instance", Texture3D_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, GLHandle);
                    break;

                case RESOURCE_DIM_TEX_CUBE:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "TextureCube_OGL instance", TextureCube_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, GLHandle);
                    break;

                case RESOURCE_DIM_TEX_CUBE_ARRAY:
                    pTextureOGL = NEW_RC_OBJ(m_TexObjAllocator, "TextureCubeArray_OGL instance", TextureCubeArray_OGL)
                                            (m_TexViewObjAllocator, this, pDeviceContext, TexDesc, GLHandle);
                    break;

                default: LOG_ERROR_AND_THROW( "Unknown texture type. (Did you forget to initialize the Type member of TextureDesc structure?)" );
            }
    
            pTextureOGL->QueryInterface( IID_Texture, reinterpret_cast<IObject**>(ppTexture) );
            pTextureOGL->CreateDefaultViews();
            OnCreateDeviceObject( pTextureOGL );
        }
    );
}

void RenderDeviceGLImpl :: CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler, bool bIsDeviceInternal)
{
    CreateDeviceObject( "sampler", SamplerDesc, ppSampler, 
        [&]()
        {
            m_SamplersRegistry.Find( SamplerDesc, reinterpret_cast<IDeviceObject**>(ppSampler) );
            if( *ppSampler == nullptr )
            {
                SamplerGLImpl *pSamplerOGL( NEW_RC_OBJ(m_SamplerObjAllocator, "SamplerGLImpl instance", SamplerGLImpl)
                                                      (this, SamplerDesc, bIsDeviceInternal ) );
                pSamplerOGL->QueryInterface( IID_Sampler, reinterpret_cast<IObject**>(ppSampler) );
                OnCreateDeviceObject( pSamplerOGL );
                m_SamplersRegistry.Add( SamplerDesc, *ppSampler );
            }
        }
    );
}

void RenderDeviceGLImpl::CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler)
{
	CreateSampler(SamplerDesc, ppSampler, false);
}


void RenderDeviceGLImpl::CreatePipelineState( const PipelineStateDesc& PipelineDesc, IPipelineState **ppPipelineState)
{
    CreatePipelineState(PipelineDesc, ppPipelineState, false);
}

void RenderDeviceGLImpl::CreatePipelineState(const PipelineStateDesc& PipelineDesc, IPipelineState **ppPipelineState, bool bIsDeviceInternal)
{
    CreateDeviceObject( "Pipeline state", PipelineDesc, ppPipelineState, 
        [&]()
        {
            PipelineStateGLImpl *pPipelineStateOGL( NEW_RC_OBJ(m_PSOAllocator, "PipelineStateGLImpl instance", PipelineStateGLImpl)
                                                              (this, PipelineDesc, bIsDeviceInternal ) );
            pPipelineStateOGL->QueryInterface( IID_PipelineState, reinterpret_cast<IObject**>(ppPipelineState) );
            OnCreateDeviceObject( pPipelineStateOGL );
        }
    );
}

void RenderDeviceGLImpl::CreateFence(const FenceDesc& Desc, IFence** ppFence)
{
    CreateDeviceObject( "Fence", Desc, ppFence, 
        [&]()
        {
            FenceGLImpl* pFenceOGL( NEW_RC_OBJ(m_FenceAllocator, "FenceGLImpl instance", FenceGLImpl)
                                              (this, Desc) );
            pFenceOGL->QueryInterface( IID_Fence, reinterpret_cast<IObject**>(ppFence) );
            OnCreateDeviceObject( pFenceOGL );
        }
    );
}

bool RenderDeviceGLImpl::CheckExtension( const Char *ExtensionString )
{
    return m_ExtensionStrings.find( ExtensionString ) != m_ExtensionStrings.end();
}

void RenderDeviceGLImpl::FlagSupportedTexFormats()
{
    const auto& DeviceCaps = GetDeviceCaps();
    bool bGL33OrAbove = DeviceCaps.DevType == DeviceType::OpenGL&&  
                        (DeviceCaps.MajorVersion >= 4 || (DeviceCaps.MajorVersion == 3&&  DeviceCaps.MinorVersion >= 3) );

    bool bRGTC = CheckExtension( "GL_ARB_texture_compression_rgtc" );
    bool bBPTC = CheckExtension( "GL_ARB_texture_compression_bptc" );
    bool bS3TC = CheckExtension( "GL_EXT_texture_compression_s3tc" );
    bool bTexNorm16 = CheckExtension( "GL_EXT_texture_norm16" ); // Only for ES3.1+
    
#define FLAG_FORMAT(Fmt, IsSupported)\
    m_TextureFormatsInfo[Fmt].Supported=IsSupported

    // The formats marked by true below are required in GL 3.3+ and GLES 3.0+
    // Note that GLES2.0 does not specify any required formats

    FLAG_FORMAT(TEX_FORMAT_RGBA32_TYPELESS,            true       );
    FLAG_FORMAT(TEX_FORMAT_RGBA32_FLOAT,               true       );
    FLAG_FORMAT(TEX_FORMAT_RGBA32_UINT,                true       );
    FLAG_FORMAT(TEX_FORMAT_RGBA32_SINT,                true       );
    FLAG_FORMAT(TEX_FORMAT_RGB32_TYPELESS,             true       );
    FLAG_FORMAT(TEX_FORMAT_RGB32_FLOAT,                true       );
    FLAG_FORMAT(TEX_FORMAT_RGB32_UINT,                 true       );
    FLAG_FORMAT(TEX_FORMAT_RGB32_SINT,                 true       );
    FLAG_FORMAT(TEX_FORMAT_RGBA16_TYPELESS,            true       );
    FLAG_FORMAT(TEX_FORMAT_RGBA16_FLOAT,               true       );
    FLAG_FORMAT(TEX_FORMAT_RGBA16_UNORM,               bGL33OrAbove || bTexNorm16 );
    FLAG_FORMAT(TEX_FORMAT_RGBA16_UINT,                true         );
    FLAG_FORMAT(TEX_FORMAT_RGBA16_SNORM,               bGL33OrAbove || bTexNorm16 );
    FLAG_FORMAT(TEX_FORMAT_RGBA16_SINT,                true         );
    FLAG_FORMAT(TEX_FORMAT_RG32_TYPELESS,              true       );
    FLAG_FORMAT(TEX_FORMAT_RG32_FLOAT,                 true       );
    FLAG_FORMAT(TEX_FORMAT_RG32_UINT,                  true       );
    FLAG_FORMAT(TEX_FORMAT_RG32_SINT,                  true       );
    FLAG_FORMAT(TEX_FORMAT_R32G8X24_TYPELESS,          true       );
    FLAG_FORMAT(TEX_FORMAT_D32_FLOAT_S8X24_UINT,       true       );
    FLAG_FORMAT(TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS,   true       );
    FLAG_FORMAT(TEX_FORMAT_X32_TYPELESS_G8X24_UINT,    false      );
    FLAG_FORMAT(TEX_FORMAT_RGB10A2_TYPELESS,           true       );
    FLAG_FORMAT(TEX_FORMAT_RGB10A2_UNORM,              true       );
    FLAG_FORMAT(TEX_FORMAT_RGB10A2_UINT,               true       );
    FLAG_FORMAT(TEX_FORMAT_R11G11B10_FLOAT,            true       );
    FLAG_FORMAT(TEX_FORMAT_RGBA8_TYPELESS,             true       );
    FLAG_FORMAT(TEX_FORMAT_RGBA8_UNORM,                true       );
    FLAG_FORMAT(TEX_FORMAT_RGBA8_UNORM_SRGB,           true       );
    FLAG_FORMAT(TEX_FORMAT_RGBA8_UINT,                 true       );
    FLAG_FORMAT(TEX_FORMAT_RGBA8_SNORM,                true       );
    FLAG_FORMAT(TEX_FORMAT_RGBA8_SINT,                 true       );
    FLAG_FORMAT(TEX_FORMAT_RG16_TYPELESS,              true       );
    FLAG_FORMAT(TEX_FORMAT_RG16_FLOAT,                 true       );
    FLAG_FORMAT(TEX_FORMAT_RG16_UNORM,                 bGL33OrAbove || bTexNorm16 );
    FLAG_FORMAT(TEX_FORMAT_RG16_UINT,                  true         );
    FLAG_FORMAT(TEX_FORMAT_RG16_SNORM,                 bGL33OrAbove || bTexNorm16 );
    FLAG_FORMAT(TEX_FORMAT_RG16_SINT,                  true         );
    FLAG_FORMAT(TEX_FORMAT_R32_TYPELESS,               true       );
    FLAG_FORMAT(TEX_FORMAT_D32_FLOAT,                  true       );
    FLAG_FORMAT(TEX_FORMAT_R32_FLOAT,                  true       );
    FLAG_FORMAT(TEX_FORMAT_R32_UINT,                   true       );
    FLAG_FORMAT(TEX_FORMAT_R32_SINT,                   true       );
    FLAG_FORMAT(TEX_FORMAT_R24G8_TYPELESS,             true       );
    FLAG_FORMAT(TEX_FORMAT_D24_UNORM_S8_UINT,          true       );
    FLAG_FORMAT(TEX_FORMAT_R24_UNORM_X8_TYPELESS,      true       );
    FLAG_FORMAT(TEX_FORMAT_X24_TYPELESS_G8_UINT,       false      );
    FLAG_FORMAT(TEX_FORMAT_RG8_TYPELESS,               true       );
    FLAG_FORMAT(TEX_FORMAT_RG8_UNORM,                  true       );
    FLAG_FORMAT(TEX_FORMAT_RG8_UINT,                   true       );
    FLAG_FORMAT(TEX_FORMAT_RG8_SNORM,                  true       );
    FLAG_FORMAT(TEX_FORMAT_RG8_SINT,                   true       );
    FLAG_FORMAT(TEX_FORMAT_R16_TYPELESS,               true       );
    FLAG_FORMAT(TEX_FORMAT_R16_FLOAT,                  true       );
    FLAG_FORMAT(TEX_FORMAT_D16_UNORM,                  true       );
    FLAG_FORMAT(TEX_FORMAT_R16_UNORM,                  bGL33OrAbove || bTexNorm16 );
    FLAG_FORMAT(TEX_FORMAT_R16_UINT,                   true       );
    FLAG_FORMAT(TEX_FORMAT_R16_SNORM,                  bGL33OrAbove || bTexNorm16 );
    FLAG_FORMAT(TEX_FORMAT_R16_SINT,                   true       );
    FLAG_FORMAT(TEX_FORMAT_R8_TYPELESS,                true       );
    FLAG_FORMAT(TEX_FORMAT_R8_UNORM,                   true       );
    FLAG_FORMAT(TEX_FORMAT_R8_UINT,                    true       );
    FLAG_FORMAT(TEX_FORMAT_R8_SNORM,                   true       );
    FLAG_FORMAT(TEX_FORMAT_R8_SINT,                    true       );
    FLAG_FORMAT(TEX_FORMAT_A8_UNORM,                   false      ); // Not supported in OpenGL
    FLAG_FORMAT(TEX_FORMAT_R1_UNORM,                   false      ); // Not supported in OpenGL
    FLAG_FORMAT(TEX_FORMAT_RGB9E5_SHAREDEXP,           true       );
    FLAG_FORMAT(TEX_FORMAT_RG8_B8G8_UNORM,             false      ); // Not supported in OpenGL
    FLAG_FORMAT(TEX_FORMAT_G8R8_G8B8_UNORM,            false      ); // Not supported in OpenGL

    FLAG_FORMAT(TEX_FORMAT_BC1_TYPELESS,               bS3TC );
    FLAG_FORMAT(TEX_FORMAT_BC1_UNORM,                  bS3TC );
    FLAG_FORMAT(TEX_FORMAT_BC1_UNORM_SRGB,             bS3TC );
    FLAG_FORMAT(TEX_FORMAT_BC2_TYPELESS,               bS3TC );
    FLAG_FORMAT(TEX_FORMAT_BC2_UNORM,                  bS3TC );
    FLAG_FORMAT(TEX_FORMAT_BC2_UNORM_SRGB,             bS3TC );
    FLAG_FORMAT(TEX_FORMAT_BC3_TYPELESS,               bS3TC );
    FLAG_FORMAT(TEX_FORMAT_BC3_UNORM,                  bS3TC );
    FLAG_FORMAT(TEX_FORMAT_BC3_UNORM_SRGB,             bS3TC );

    FLAG_FORMAT(TEX_FORMAT_BC4_TYPELESS,               bRGTC );
    FLAG_FORMAT(TEX_FORMAT_BC4_UNORM,                  bRGTC );
    FLAG_FORMAT(TEX_FORMAT_BC4_SNORM,                  bRGTC );
    FLAG_FORMAT(TEX_FORMAT_BC5_TYPELESS,               bRGTC );
    FLAG_FORMAT(TEX_FORMAT_BC5_UNORM,                  bRGTC );
    FLAG_FORMAT(TEX_FORMAT_BC5_SNORM,                  bRGTC );

    FLAG_FORMAT(TEX_FORMAT_B5G6R5_UNORM,               false       ); // Not supported in OpenGL
    FLAG_FORMAT(TEX_FORMAT_B5G5R5A1_UNORM,             false       ); // Not supported in OpenGL
    FLAG_FORMAT(TEX_FORMAT_BGRA8_UNORM,                false       ); // Not supported in OpenGL
    FLAG_FORMAT(TEX_FORMAT_BGRX8_UNORM,                false       ); // Not supported in OpenGL
    FLAG_FORMAT(TEX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, false       ); // Not supported in OpenGL
    FLAG_FORMAT(TEX_FORMAT_BGRA8_TYPELESS,             false       ); // Not supported in OpenGL
    FLAG_FORMAT(TEX_FORMAT_BGRA8_UNORM_SRGB,           false       ); // Not supported in OpenGL
    FLAG_FORMAT(TEX_FORMAT_BGRX8_TYPELESS,             false       ); // Not supported in OpenGL
    FLAG_FORMAT(TEX_FORMAT_BGRX8_UNORM_SRGB,           false       ); // Not supported in OpenGL
    
    FLAG_FORMAT(TEX_FORMAT_BC6H_TYPELESS,              bBPTC );
    FLAG_FORMAT(TEX_FORMAT_BC6H_UF16,                  bBPTC );
    FLAG_FORMAT(TEX_FORMAT_BC6H_SF16,                  bBPTC );
    FLAG_FORMAT(TEX_FORMAT_BC7_TYPELESS,               bBPTC );
    FLAG_FORMAT(TEX_FORMAT_BC7_UNORM,                  bBPTC );
    FLAG_FORMAT(TEX_FORMAT_BC7_UNORM_SRGB,             bBPTC );

#ifdef _DEBUG
    bool bGL43OrAbove = DeviceCaps.DevType == DeviceType::OpenGL &&  
                        (DeviceCaps.MajorVersion >= 5 || (DeviceCaps.MajorVersion == 4 && DeviceCaps.MinorVersion >= 3) );

    constexpr int TestTextureDim = 8;
    constexpr int MaxTexelSize   = 16;
    std::vector<Uint8> ZeroData(TestTextureDim * TestTextureDim * MaxTexelSize);
    
    // Go through all formats and try to create small 2D texture to check if the format is supported
    for( auto FmtInfo = m_TextureFormatsInfo.begin(); FmtInfo != m_TextureFormatsInfo.end(); ++FmtInfo )
    {
        if( FmtInfo->Format == TEX_FORMAT_UNKNOWN )
            continue;

        auto GLFmt = TexFormatToGLInternalTexFormat(FmtInfo->Format);
        if( GLFmt == 0 )
        {
            VERIFY( !FmtInfo->Supported, "Format should be marked as unsupported" );
            continue;
        }

#if GL_ARB_internalformat_query2
        // Only works on GL4.3+
        if( bGL43OrAbove )
        {
            GLint params = 0;
            glGetInternalformativ( GL_TEXTURE_2D, GLFmt, GL_INTERNALFORMAT_SUPPORTED, 1, &params );
            CHECK_GL_ERROR( "glGetInternalformativ() failed" );
            VERIFY( FmtInfo->Supported == (params == GL_TRUE), "This internal format should be supported" );
        }
#endif

        // Check that the format is indeed supported
        if (FmtInfo->Supported)
        {
            GLObjectWrappers::GLTextureObj TestGLTex( true );
            // Immediate context has not been created yet, so use raw GL functions
            glBindTexture( GL_TEXTURE_2D, TestGLTex );
            CHECK_GL_ERROR( "Failed to bind texture" );
            glTexStorage2D( GL_TEXTURE_2D, 1, GLFmt, TestTextureDim, TestTextureDim );
            if (glGetError() == GL_NO_ERROR)
            {
                // It turned out it is not enough to only allocate texture storage
                // For some reason glTexStorage2D() may succeed, but upload operation
                // will later fail. So we need to additionally try to upload some
                // data to the texture
                const auto& TransferAttribs = GetNativePixelTransferAttribs(FmtInfo->Format);
                if (TransferAttribs.IsCompressed)
                {
                    const auto& FmtAttribs = GetTextureFormatAttribs(FmtInfo->Format);
                    static_assert( (TestTextureDim & (TestTextureDim-1)) == 0, "Test texture dim must be power of two!");
                    auto BlockBytesInRow = (TestTextureDim/int{FmtAttribs.BlockWidth}) * int{FmtAttribs.ComponentSize};
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0,  // mip level
                        0, 0, TestTextureDim, TestTextureDim,
                        GLFmt,
                        (TestTextureDim/int{FmtAttribs.BlockHeight}) * BlockBytesInRow,
                        ZeroData.data() );
                }
                else
                {
                    glTexSubImage2D( GL_TEXTURE_2D, 0,  // mip level
                        0, 0, TestTextureDim, TestTextureDim,
                        TransferAttribs.PixelFormat, TransferAttribs.DataType,
                        ZeroData.data() );
                }

                if (glGetError() != GL_NO_ERROR)
                {
                    LOG_WARNING_MESSAGE("Failed to upload data to a test ", TestTextureDim, "x", TestTextureDim, " ", FmtInfo->Name, " texture. "
                                        "This likely indicates that the format is not supported despite being reported so by the device.");
                    FmtInfo->Supported = false;
                }
            }
            else
            {
                LOG_WARNING_MESSAGE( "Failed to allocate storage for a test ", TestTextureDim, "x", TestTextureDim, " ", FmtInfo->Name, " texture. "
                                     "This likely indicates that the format is not supported despite being reported so by the device.");
                FmtInfo->Supported = false;
            }
            glBindTexture( GL_TEXTURE_2D, 0 );
        }
    }
#endif
}

template<typename CreateFuncType>
bool CreateTestGLTexture(GLContextState& GlCtxState, GLenum BindTarget, const GLObjectWrappers::GLTextureObj& GLTexObj, CreateFuncType CreateFunc)
{
    GlCtxState.BindTexture(-1, BindTarget, GLTexObj);
    CreateFunc();
    bool bSuccess = glGetError() == GL_NO_ERROR;
    GlCtxState.BindTexture(-1, BindTarget, GLObjectWrappers::GLTextureObj(false) );
    return bSuccess;
}

void RenderDeviceGLImpl::TestTextureFormat( TEXTURE_FORMAT TexFormat )
{
    auto& TexFormatInfo = m_TextureFormatsInfo[TexFormat];
    VERIFY( TexFormatInfo.Supported, "Texture format is not supported" );

    auto GLFmt = TexFormatToGLInternalTexFormat(TexFormat);
    VERIFY( GLFmt != 0, "Incorrect internal GL format" );

    auto spDeviceContext = GetImmediateContext();
    VERIFY(spDeviceContext, "Immediate device context has been destroyed");
    auto *pContextGL = spDeviceContext.RawPtr<DeviceContextGLImpl>();
    auto& ContextState = pContextGL->GetContextState();

    const int TestTextureDim = 32;
    const int TestTextureDepth = 8;

    // Create test texture 1D
    TexFormatInfo.Tex1DFmt = false;
    if( m_DeviceCaps.TexCaps.bTexture1DSupported && 
        TexFormatInfo.ComponentType != COMPONENT_TYPE_COMPRESSED )
    {
        GLObjectWrappers::GLTextureObj TestGLTex( true );
        TexFormatInfo.Tex1DFmt = CreateTestGLTexture( ContextState, GL_TEXTURE_1D, TestGLTex, [&]()
        {
            glTexStorage1D(GL_TEXTURE_1D, 1, GLFmt, TestTextureDim);
        } );
    }

    // Create test texture 2D
    TexFormatInfo.Tex2DFmt = false;
    TexFormatInfo.TexCubeFmt = false;
    TexFormatInfo.ColorRenderable = false;
    TexFormatInfo.DepthRenderable = false;
    {
        GLObjectWrappers::GLTextureObj TestGLTex( true );
        TexFormatInfo.Tex2DFmt = CreateTestGLTexture( ContextState, GL_TEXTURE_2D, TestGLTex, [&]()
        {
            glTexStorage2D(GL_TEXTURE_2D, 1, GLFmt, TestTextureDim, TestTextureDim);
        } );

        if( TexFormatInfo.Tex2DFmt )
        {
            {
                GLObjectWrappers::GLTextureObj TestGLCubeTex( true );
                TexFormatInfo.TexCubeFmt = CreateTestGLTexture( ContextState, GL_TEXTURE_CUBE_MAP, TestGLCubeTex, [&]()
                {
                    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GLFmt, TestTextureDim, TestTextureDim);
                } );
            }

            bool bTestDepthAttachment = 
                TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH ||
                TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL;
            bool bTestColorAttachment = !bTestDepthAttachment && TexFormatInfo.ComponentType != COMPONENT_TYPE_COMPRESSED;

            GLObjectWrappers::GLFrameBufferObj NewFBO(false);
            
            GLint CurrentFramebuffer = -1;
            if( bTestColorAttachment || bTestDepthAttachment )
            {
                glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &CurrentFramebuffer );
                CHECK_GL_ERROR( "Failed to get current framebuffer");

                NewFBO.Create();
                glBindFramebuffer( GL_DRAW_FRAMEBUFFER, NewFBO );
                CHECK_GL_ERROR( "Failed to bind the framebuffer");
            }

            if( bTestDepthAttachment )
            {
                GLenum Attachment = TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH ? GL_DEPTH_ATTACHMENT : GL_DEPTH_STENCIL_ATTACHMENT;
                glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, Attachment, GL_TEXTURE_2D, TestGLTex, 0 );
                if( glGetError() == GL_NO_ERROR )
                {
                    // Create dummy texture2D since some older version do not allow depth only
                    // attachments
                    GLObjectWrappers::GLTextureObj ColorTex( true );
                    bool Success = CreateTestGLTexture( ContextState, GL_TEXTURE_2D, ColorTex, [&]()
                    {
                        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, TestTextureDim, TestTextureDim);
                    } );
                    VERIFY( Success, "Failed to create dummy render target texture" ); (void)Success;
                    glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ColorTex, 0 );
                    CHECK_GL_ERROR( "Failed to set bind dummy render target to framebuffer" );

                    static const GLenum DrawBuffers[] = { GL_COLOR_ATTACHMENT0 };
                    glDrawBuffers( _countof( DrawBuffers ), DrawBuffers );
                    CHECK_GL_ERROR( "Failed to set draw buffers via glDrawBuffers()" );

                    GLenum Status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
                    TexFormatInfo.DepthRenderable = (glGetError() == GL_NO_ERROR) && (Status == GL_FRAMEBUFFER_COMPLETE);
                }
            }
            else if( bTestColorAttachment )
            {
                glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, TestGLTex, 0 );
                if( glGetError() == GL_NO_ERROR )
                {
                    static const GLenum DrawBuffers[] = { GL_COLOR_ATTACHMENT0 };
                    glDrawBuffers( _countof( DrawBuffers ), DrawBuffers );
                    CHECK_GL_ERROR( "Failed to set draw buffers via glDrawBuffers()" );

                    GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                    TexFormatInfo.ColorRenderable = (glGetError() == GL_NO_ERROR) && (Status == GL_FRAMEBUFFER_COMPLETE);
                }
            }

            if( bTestColorAttachment || bTestDepthAttachment )
            {
                glBindFramebuffer( GL_DRAW_FRAMEBUFFER, CurrentFramebuffer );
                CHECK_GL_ERROR( "Failed to bind the framebuffer");
            }
        }
    }

    TexFormatInfo.SampleCounts = 0x01;
    if( TexFormatInfo.ComponentType != COMPONENT_TYPE_COMPRESSED &&  
        m_DeviceCaps.TexCaps.bTexture2DMSSupported )
    {
#if GL_ARB_texture_storage_multisample
        for (GLsizei SampleCount = 2; SampleCount <= 8; SampleCount *= 2)
        {
            GLObjectWrappers::GLTextureObj TestGLTex( true );
            auto SampleCountSupported = CreateTestGLTexture( ContextState, GL_TEXTURE_2D_MULTISAMPLE, TestGLTex, [&]()
            {
                glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, SampleCount, GLFmt, TestTextureDim, TestTextureDim, GL_TRUE);
            } );
            if (SampleCountSupported)
                TexFormatInfo.SampleCounts |= SampleCount;
        }
#endif
    }

    // Create test texture 3D
    TexFormatInfo.Tex3DFmt = false;
    // 3D textures do not support depth formats
    if( !(TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH ||
          TexFormatInfo.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL) )
    {
        GLObjectWrappers::GLTextureObj TestGLTex( true );
        TexFormatInfo.Tex3DFmt = CreateTestGLTexture( ContextState, GL_TEXTURE_3D, TestGLTex, [&]()
        {
            glTexStorage3D(GL_TEXTURE_3D, 1, GLFmt, TestTextureDim, TestTextureDim, TestTextureDepth);
        } );
    }
}

void RenderDeviceGLImpl :: QueryDeviceCaps()
{
    if(glPolygonMode == nullptr)
        m_DeviceCaps.bWireframeFillSupported = false;
        
    if(m_DeviceCaps.bWireframeFillSupported)
    {
        // Test glPolygonMode() function to check if it fails
        // (It does fail on NVidia Shield tablet, but works fine 
        // on Intel hw)
        VERIFY( glGetError() == GL_NO_ERROR, "Unhandled gl error encountered" );
        m_DeviceCaps.bWireframeFillSupported = True;
        glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
        if( glGetError() != GL_NO_ERROR )
            m_DeviceCaps.bWireframeFillSupported = False;
        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
        if( glGetError() != GL_NO_ERROR )
            m_DeviceCaps.bWireframeFillSupported = False;
    }
}


FBOCache& RenderDeviceGLImpl::GetFBOCache(GLContext::NativeGLContextType Context)
{
    ThreadingTools::LockHelper FBOCacheLock(m_FBOCacheLockFlag);
    return m_FBOCache[Context];
}

void RenderDeviceGLImpl::OnReleaseTexture(ITexture *pTexture)
{
    ThreadingTools::LockHelper FBOCacheLock(m_FBOCacheLockFlag);
    for (auto& FBOCacheIt : m_FBOCache)
        FBOCacheIt.second.OnReleaseTexture(pTexture);
}

VAOCache& RenderDeviceGLImpl::GetVAOCache(GLContext::NativeGLContextType Context)
{
    ThreadingTools::LockHelper VAOCacheLock(m_VAOCacheLockFlag);
    return m_VAOCache[Context];
}

void RenderDeviceGLImpl::OnDestroyPSO(IPipelineState *pPSO)
{
    ThreadingTools::LockHelper VAOCacheLock(m_VAOCacheLockFlag);
    for (auto& VAOCacheIt : m_VAOCache)
        VAOCacheIt.second.OnDestroyPSO(pPSO);
}

void RenderDeviceGLImpl::OnDestroyBuffer(IBuffer *pBuffer)
{
    ThreadingTools::LockHelper VAOCacheLock(m_VAOCacheLockFlag);
    for (auto& VAOCacheIt : m_VAOCache)
        VAOCacheIt.second.OnDestroyBuffer(pBuffer);
}

}
