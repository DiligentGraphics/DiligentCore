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

/// \file
/// Declaration of Diligent::DeviceContextD3D11Impl class

#include "DeviceContextD3D11.h"
#include "DeviceContextBase.h"
#include "ShaderD3D11Impl.h"

#ifdef _DEBUG
#   define VERIFY_CONTEXT_BINDINGS
#endif

namespace Diligent
{

/// Implementation of the Diligent::IDeviceContextD3D11 interface
class DeviceContextD3D11Impl : public DeviceContextBase<IDeviceContextD3D11>
{
public:
    typedef DeviceContextBase<IDeviceContextD3D11> TDeviceContextBase;

    DeviceContextD3D11Impl(IRenderDevice *pDevice, ID3D11DeviceContext *pd3d11DeviceContext);
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;

    virtual void SetShaders( IShader **ppShaders, Uint32 NumShadersToSet )override;

    virtual void BindShaderResources( IResourceMapping *pResourceMapping, Uint32 Flags )override;

    virtual void SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags )override;
    virtual void ClearState()override;

    virtual void SetVertexDescription( IVertexDescription *pVertexDesc )override;

    virtual void SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset )override;

    virtual void SetDepthStencilState( IDepthStencilState *pDepthStencilState, Uint32 StencilRef = 0 )override;

    virtual void SetRasterizerState( IRasterizerState *pRS )override;

    virtual void SetBlendState( IBlendState *pBS, const float* pBlendFactors, Uint32 SampleMask )override;

    virtual void SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 RTWidth, Uint32 RTHeight )override;

    virtual void SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 RTWidth, Uint32 RTHeight )override;

    virtual void SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil )override;

    virtual void Draw( DrawAttribs &DrawAttribs )override;

    virtual void DispatchCompute( const DispatchComputeAttribs &DispatchAttrs )override;

    virtual void ClearDepthStencil( ITextureView *pView, Uint32 ClearFlags, float fDepth, Uint8 Stencil)override;

    virtual void ClearRenderTarget( ITextureView *pView, const float *RGBA )override;

    virtual void Flush()override;
    
    ID3D11DeviceContext* GetD3D11DeviceContext(){ return m_pd3d11DeviceContext; }
    
    void RebindRenderTargets();

    /// Clears the state caches. This function is called once per frame
    /// (before present) to release all outstanding objects
    /// that are only kept alive by references in the cache
    void ClearShaderStateCache();

    /// Number of different shader types (Vertex, Pixel, Geometry, Domain, Hull, Compute)
    static const int NumShaderTypes = 6;

private:
    
    /// Goes through the list of bound shaders and binds all required
    /// d3d11 resource to the d3d11 device context, for each shader.
    void SetD3DShadersAndResources();
    
    /// Binds d3d11 index buffer to the d3d11 device context.
    void SetD3DIndexBuffer(VALUE_TYPE IndexType);

    /// Binds d3d11 vertex buffers to the d3d11 device context.
    void SetD3DVertexBuffers();

    /// Binds d3d11 constant buffers to the d3d11 device context
    void SetD3DConstantBuffers ( class ShaderD3D11Impl *pShader );
    /// Binds d3d11 shader resource views to the d3d11 device context
    void SetD3DSRVs            ( class ShaderD3D11Impl *pShader );
    /// Binds d3d11 unordered access views to the d3d11 device context
    void SetD3DUAVs            ( class ShaderD3D11Impl *pShader );
    /// Binds d3d11 samplers to the d3d11 device context
    void SetD3DSamplers        ( class ShaderD3D11Impl *pShader );

    /// Helper template function used to facilitate resource unbinding
    template<typename TBoundResourceType,
             typename TD3D11ResourceType,
             typename TSetD3D11View>
    void UnbindResourceView(std::vector<TBoundResourceType> BoundResourcesArr[],
                            std::vector<TD3D11ResourceType> BoundD3D11ResourcesArr[], 
                            IDeviceObject *pResToUnbind,
                            TSetD3D11View SetD3D11ViewMethods[]);

    /// Unbinds a texture from the shader resource view slots.
    /// \note The function only unbinds the texture from d3d11 device
    ///       context. All shader bindings are retained.
    void UnbindTextureFromInput(ITexture *pTexture);

    /// Unbinds a buffer from the input (shader resource views slots, index 
    /// and vertex buffer slots).
    /// \note The function only unbinds the buffer from d3d11 device
    ///       context. All shader bindings are retained.
    void UnbindBufferFromInput(IBuffer *pBuffer);

    /// Unbinds a resource from the UAV slots.
    /// \note The function only unbinds the texture from the device
    ///       context. All shader bindings are retained.
    void UnbindResourceFromUAV(IDeviceObject *pResource);

    /// Unbinds a texture from render target slots.
    void UnbindTextureFromRenderTarget(IDeviceObject *pResource);

    /// Unbinds a texture from depth-stencil.
    void UnbindTextureFromDepthStencil(IDeviceObject *pResource);

    /// Helper template function that facilitates shader binding
    /// \tparam TD3D11ShaderType - Type of D3D11 shader being set (ID3D11PixelShader, ID3D11VertexShader, etc.)
    /// \tparam TBindShaderMethdType - Type of the d3d11 device context mehtod that sets the shader 
    ///                                (ID3D11DeviceContext::VSSetShader, ID3D11DeviceContext::PSSetShader, etc.)
    /// \param BoundShaderFlags - Flags indicating which shader stages are currently active
    /// \param NewShaders - Array of pointers to the new shaders to be bound
    /// \param ShaderType - Type of the shader being bound
    /// \param BindShaderMethod - Pointer to the d3d11 device context mehtod that sets the shader 
    ///                           (ID3D11DeviceContext::VSSetShader, ID3D11DeviceContext::PSSetShader, etc.)
    template<typename TD3D11ShaderType, typename TBindShaderMethdType>
    void SetD3D11ShaderHelper(Uint32 BoundShaderFlags,
                              ShaderD3D11Impl* NewShaders[],
                              SHADER_TYPE ShaderType,
                              TBindShaderMethdType BindShaderMethod);

    Diligent::CComPtr<ID3D11DeviceContext> m_pd3d11DeviceContext; ///< D3D11 device context
    DrawAttribs m_LastDrawAttribs;

    /// An array of D3D11 constant buffers currently bound to the D3D11 device context,
    /// for each shader type.
    std::vector<ID3D11Buffer*>              m_BoundD3D11CBs     [NumShaderTypes];
    /// An array of D3D11 shader resource views currently bound to the D3D11 device context,
    /// for each shader type.
    std::vector<ID3D11ShaderResourceView*>  m_BoundD3D11SRVs    [NumShaderTypes];
    /// An array of D3D11 samplers currently bound to the D3D11 device context,
    /// for each shader type.
    std::vector<ID3D11SamplerState*>        m_BoundD3D11Samplers[NumShaderTypes];
    /// An array of D3D11 UAVs currently bound to the D3D11 device context,
    /// for each shader type.
    std::vector<ID3D11UnorderedAccessView*> m_BoundD3D11UAVs    [NumShaderTypes];

    /// An array of strong references to resources associated with the bound 
    /// constant buffers, for each shader type.
    std::vector<ShaderD3D11Impl::BoundCB> m_BoundCBs[NumShaderTypes];
    /// An array of strong references to resources associated with the 
    /// bound SRV, for each shader type.
    std::vector<ShaderD3D11Impl::BoundSRV> m_BoundSRVs[NumShaderTypes];
    /// An array of strong references to resources associated with the 
    /// bound samplers, for each shader type.
    std::vector<ShaderD3D11Impl::BoundSampler> m_BoundSamplers[NumShaderTypes];
    /// An array of strong references to resources associated with the 
    /// bound UAVs, for each shader type.
    std::vector<ShaderD3D11Impl::BoundUAV> m_BoundUAVs[NumShaderTypes];

    /// An array of D3D11 vertex buffers currently bound to the D3D device context
    std::vector< Diligent::CComPtr<ID3D11Buffer> >  m_BoundD3D11VertexBuffers;
    /// An array of strides of currently bound vertex buffers
    std::vector< UINT > m_BoundD3D11VBStrides;
    /// An array of offsets of currently bound vertex buffers
    std::vector< UINT > m_BoundD3D11VBOffsets;

    /// Strong reference to the D3D11 buffer currently bound as index buffer 
    /// to the D3D device context
    Diligent::CComPtr<ID3D11Buffer>  m_BoundD3D11IndexBuffer;
    /// Format of currently bound D3D11 index buffer
    DXGI_FORMAT m_BoundD3D11IndexFmt;
    /// Offset of currently bound D3D11 index buffer
    Uint32 m_BoundD3D11IndexDataStartOffset;

    /// Strong references to the currently bound D3D11 shaders
    Diligent::CComPtr<ID3D11DeviceChild> m_BoundD3DShaders[NumShaderTypes];

#ifdef VERIFY_CONTEXT_BINDINGS
    /// Helper template function used to facilitate context verification
    template<UINT MaxResources, typename TD3D11ResourceType, typename TGetD3D11ResourcesType>
    void dbgVerifyContextResources(std::vector<TD3D11ResourceType> BoundD3D11ResourcesArr[],
                                   TGetD3D11ResourcesType GetD3D11ResMethods[],
                                   const Char *ResourceName,
                                   SHADER_TYPE ShaderType);

    /// Debug function that verifies that SRVs cached in m_BoundD3D11SRVs 
    /// array comply with resources actually bound to the D3D11 device context
    void dbgVerifyContextSRVs(SHADER_TYPE ShaderType = SHADER_TYPE_UNKNOWN);

    /// Debug function that verifies that UAVs cached in m_BoundD3D11UAVs 
    /// array comply with resources actually bound to the D3D11 device context
    void dbgVerifyContextUAVs(SHADER_TYPE ShaderType = SHADER_TYPE_UNKNOWN);

    /// Debug function that verifies that samplers cached in m_BoundD3D11Samplers
    /// array comply with resources actually bound to the D3D11 device context
    void dbgVerifyContextSamplers(SHADER_TYPE ShaderType = SHADER_TYPE_UNKNOWN);

    /// Debug function that verifies that constant buffers cached in m_BoundD3D11CBs 
    /// array comply with buffers actually bound to the D3D11 device context
    void dbgVerifyContextCBs(SHADER_TYPE ShaderType = SHADER_TYPE_UNKNOWN);

    /// Debug function that verifies that the index buffer cached in 
    /// m_BoundD3D11IndexBuffer is the buffer actually bound to the D3D11 
    /// device context
    void dbgVerifyIndexBuffer();

    /// Debug function that verifies that vertex buffers cached in 
    /// m_BoundD3D11VertexBuffers are the buffers actually bound to the D3D11 
    /// device context
    void dbgVerifyVertexBuffers();

    /// Debug function that verifies that shaders cached in 
    /// m_BoundD3DShaders are the shaders actually bound to the D3D11 
    /// device context
    void dbgVerifyShaders();

#else

    #define dbgVerifyContextSRVs(...)
    #define dbgVerifyContextUAVs(...)
    #define dbgVerifyContextSamplers(...)
    #define dbgVerifyContextCBs(...)
    #define dbgVerifyIndexBuffer(...)
    #define dbgVerifyVertexBuffers(...)
    #define dbgVerifyShaders(...)

#endif
};

}
