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

#pragma once

#include "RenderDeviceBase.h"

#ifdef _WINDOWS
#include "GLContextWindows.h"
#endif

#ifdef ANDROID
#include "GLContextAndroid.h"
#endif

#include "VAOCache.h"
#include "ProgramPipelineCache.h"
#include "BaseInterfacesGL.h"
#include "FBOCache.h"
#include "TexRegionRender.h"

enum class GPU_VENDOR
{
    UNKNOWN, 
    INTEL,
    ATI,
    NVIDIA
};

struct GPUInfo
{
    GPU_VENDOR Vendor;
    GPUInfo() :
        Vendor( GPU_VENDOR::UNKNOWN )
    {}
};

namespace Diligent
{

/// Implementation of the render device interface in OpenGL
class RenderDeviceGLImpl : public RenderDeviceBase<IGLDeviceBaseInterface>
{
public:
    typedef RenderDeviceBase<IGLDeviceBaseInterface> TRenderDeviceBase;

    RenderDeviceGLImpl( const ContextInitInfo &InitInfo );
    ~RenderDeviceGLImpl();
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;
    
	void CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBufferLayout, bool bIsDeviceInternal);
    virtual void CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBufferLayout)override;

	void CreateVertexDescription( const LayoutDesc& LayoutDesc, IShader *pVertexShader, IVertexDescription **ppVertexDesc, bool bIsDeviceInternal );
    virtual void CreateVertexDescription( const LayoutDesc& LayoutDesc, IShader *pVertexShader, IVertexDescription **ppVertexDesc )override;

	void CreateShader(const ShaderCreationAttribs &ShaderCreationAttribs, IShader **ppShader, bool bIsDeviceInternal );
    virtual void CreateShader(const ShaderCreationAttribs &ShaderCreationAttribs, IShader **ppShader)override;

	void CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture, bool bIsDeviceInternal);
    virtual void CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture)override;
    
	void CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler, bool bIsDeviceInternal);
    virtual void CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler)override;

	void CreateDepthStencilState( const DepthStencilStateDesc &DSSDesc, IDepthStencilState **ppDepthStencilState, bool bIsDeviceInternal );
    virtual void CreateDepthStencilState( const DepthStencilStateDesc &DSSDesc, IDepthStencilState **ppDepthStencilState )override;

	void CreateRasterizerState( const RasterizerStateDesc &RSDesc, IRasterizerState **ppRasterizerState, bool bIsDeviceInternal);
    virtual void CreateRasterizerState( const RasterizerStateDesc &RSDesc, IRasterizerState **ppRasterizerState )override;

	void CreateBlendState( const BlendStateDesc &BSDesc, IBlendState **ppBlendState, bool bIsDeviceInternal );
    virtual void CreateBlendState( const BlendStateDesc &BSDesc, IBlendState **ppBlendState )override;

    const GPUInfo& GetGPUInfo(){ return m_GPUInfo; }

protected:
    friend class DeviceContextGLImpl;
    friend class TextureBaseGL;
    friend class VertexDescGLImpl;
    friend class ShaderGLImpl;
    friend class BufferGLImpl;
    friend class TextureViewGLImpl;
    friend class SwapChainGLImpl;

    // Must be the first member because its constructor initializes OpenGL
    GLContext m_GLContext; 

    std::unordered_set<String> m_ExtensionStrings;

    VAOCache m_VAOCache;
    FBOCache m_FBOCache;
    ProgramPipelineCache m_PipelineCache;

    GPUInfo m_GPUInfo;

    // Any draw command fails if no VAO is bound. We will use this empty
    // VAO for draw commands with null input layout, such as these that
    // only use VertexID as input.
    GLObjectWrappers::GLVertexArrayObj m_EmptyVAO;

    TexRegionRender m_TexRegionRender;

private:
    virtual void TestTextureFormat( TEXTURE_FORMAT TexFormat );
    bool CheckExtension(const Char *ExtensionString);
    void FlagSupportedTexFormats();
    void QueryDeviceCaps();
};

}
