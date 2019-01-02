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

#pragma once

#include "RenderDeviceBase.h"
#include "GLContext.h"
#include "VAOCache.h"
#include "BaseInterfacesGL.h"
#include "FBOCache.h"
#include "TexRegionRender.h"
#include "EngineGLAttribs.h"

enum class GPU_VENDOR
{
    UNKNOWN, 
    INTEL,
    ATI,
    NVIDIA,
    QUALCOMM
};

struct GPUInfo
{
    GPU_VENDOR Vendor = GPU_VENDOR::UNKNOWN;
};

namespace Diligent
{

/// Implementation of the render device interface in OpenGL
// RenderDeviceGLESImpl is inherited from RenderDeviceGLImpl
class RenderDeviceGLImpl : public RenderDeviceBase<IGLDeviceBaseInterface>
{
public:
    using TRenderDeviceBase = RenderDeviceBase<IGLDeviceBaseInterface>;

    RenderDeviceGLImpl( IReferenceCounters *pRefCounters, IMemoryAllocator &RawMemAllocator, const EngineGLAttribs &InitAttribs );
    ~RenderDeviceGLImpl();
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;
    
	void CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBufferLayout, bool bIsDeviceInternal);
    virtual void CreateBuffer(const BufferDesc& BuffDesc, const BufferData &BuffData, IBuffer **ppBufferLayout)override final;

	void CreateShader(const ShaderCreationAttribs &ShaderCreationAttribs, IShader **ppShader, bool bIsDeviceInternal );
    virtual void CreateShader(const ShaderCreationAttribs &ShaderCreationAttribs, IShader **ppShader)override final;

	void CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture, bool bIsDeviceInternal);
    virtual void CreateTexture(const TextureDesc& TexDesc, const TextureData &Data, ITexture **ppTexture)override final;
    
	void CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler, bool bIsDeviceInternal);
    virtual void CreateSampler(const SamplerDesc& SamplerDesc, ISampler **ppSampler)override final;

    void CreatePipelineState( const PipelineStateDesc &PipelineDesc, IPipelineState **ppPipelineState, bool bIsDeviceInternal);
    virtual void CreatePipelineState( const PipelineStateDesc &PipelineDesc, IPipelineState **ppPipelineState )override final;
    
    virtual void CreateFence(const FenceDesc& Desc, IFence** ppFence)override final;

    virtual void CreateTextureFromGLHandle(Uint32 GLHandle, const TextureDesc &TexDesc, RESOURCE_STATE InitialState, ITexture **ppTexture)override final;

    virtual void CreateBufferFromGLHandle(Uint32 GLHandle, const BufferDesc &BuffDesc, RESOURCE_STATE InitialState, IBuffer **ppBuffer)override final;

    virtual void ReleaseStaleResources(bool ForceRelease = false)override final {}

    const GPUInfo& GetGPUInfo(){ return m_GPUInfo; }

    FBOCache& GetFBOCache(GLContext::NativeGLContextType Context);
    void OnReleaseTexture(ITexture *pTexture);

    VAOCache& GetVAOCache(GLContext::NativeGLContextType Context);
    void OnDestroyPSO(IPipelineState *pPSO);
    void OnDestroyBuffer(IBuffer *pBuffer);

    size_t GetCommandQueueCount()const { return 1; }
    Uint64 GetCommandQueueMask()const { return Uint64{1};}

protected:
    friend class DeviceContextGLImpl;
    friend class TextureBaseGL;
    friend class PipelineStateGLImpl;
    friend class ShaderGLImpl;
    friend class BufferGLImpl;
    friend class TextureViewGLImpl;
    friend class SwapChainGLImpl;
    friend class GLContextState;

    // Must be the first member because its constructor initializes OpenGL
    GLContext m_GLContext; 

    std::unordered_set<String> m_ExtensionStrings;

    ThreadingTools::LockFlag m_VAOCacheLockFlag;
    std::unordered_map<GLContext::NativeGLContextType, VAOCache> m_VAOCache;

    ThreadingTools::LockFlag m_FBOCacheLockFlag;
    std::unordered_map<GLContext::NativeGLContextType, FBOCache> m_FBOCache;

    GPUInfo m_GPUInfo;

    TexRegionRender m_TexRegionRender;
    
private:
    virtual void TestTextureFormat( TEXTURE_FORMAT TexFormat )override final;
    bool CheckExtension(const Char *ExtensionString);
    void FlagSupportedTexFormats();
    void QueryDeviceCaps();
};

}
