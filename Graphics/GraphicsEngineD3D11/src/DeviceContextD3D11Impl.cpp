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
#include "DeviceContextD3D11Impl.h"
#include "BufferD3D11Impl.h"
#include "VertexDescD3D11Impl.h"
#include "ShaderD3D11Impl.h"
#include "Texture1D_D3D11.h"
#include "Texture2D_D3D11.h"
#include "Texture3D_D3D11.h"
#include "SamplerD3D11Impl.h"
#include "D3D11TypeConversions.h"
#include "TextureViewD3D11Impl.h"
#include "DSStateD3D11Impl.h"
#include "RasterizerStateD3D11Impl.h"
#include "BlendStateD3D11Impl.h"
#include "DSStateD3D11Impl.h"
#include "SwapChainD3D11Impl.h"
#include "D3D11DebugUtilities.h"

using namespace Diligent;

namespace Diligent
{

    DeviceContextD3D11Impl::DeviceContextD3D11Impl( IRenderDevice *pDevice, ID3D11DeviceContext *pd3d11DeviceContext ) :
        TDeviceContextBase(pDevice),
        m_pd3d11DeviceContext( pd3d11DeviceContext ),
        m_BoundD3D11IndexFmt(DXGI_FORMAT_UNKNOWN),
        m_BoundD3D11IndexDataStartOffset(0)
    {
        for( int ShaderType = 0; ShaderType < NumShaderTypes; ++ShaderType )
        {
            m_BoundCBs     [ShaderType].reserve( D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT ); // 14
            m_BoundD3D11CBs[ShaderType].reserve( D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT ); // 14

            m_BoundSamplers     [ShaderType].reserve( D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT );// 16
            m_BoundD3D11Samplers[ShaderType].reserve( D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT );// 16

            m_BoundSRVs     [ShaderType].reserve( D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT );// 128
            m_BoundD3D11SRVs[ShaderType].reserve( D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT );// 128

            m_BoundUAVs     [ShaderType].reserve( D3D11_PS_CS_UAV_REGISTER_COUNT );// 8
            m_BoundD3D11UAVs[ShaderType].reserve( D3D11_PS_CS_UAV_REGISTER_COUNT );// 8
        }
    }

    IMPLEMENT_QUERY_INTERFACE( DeviceContextD3D11Impl, IID_DeviceContextD3D11, TDeviceContextBase )

    void DeviceContextD3D11Impl::SetShaders( IShader **ppShaders, Uint32 NumShadersToSet )
    {
        TDeviceContextBase::SetShaders( ppShaders, NumShadersToSet );
    }

    void DeviceContextD3D11Impl::BindShaderResources( IResourceMapping *pResourceMapping, Uint32 Flags )
    {
        TDeviceContextBase::BindShaderResources( pResourceMapping, Flags );
        for( auto it = m_pBoundShaders.begin(); it != m_pBoundShaders.end(); ++it )
        {
            (*it)->BindResources( pResourceMapping, Flags );
        }
    }


    SHADER_TYPE GetShaderTypeFromIndex( Int32 Index )
    {
        return static_cast<SHADER_TYPE>(1 << Index);
    }

    Int32 GetShaderTypeIndex( SHADER_TYPE Type )
    {
        auto ShaderIndex = 0;
        switch( Type )
        {
            case SHADER_TYPE_UNKNOWN: ShaderIndex = -1; break;
            case SHADER_TYPE_VERTEX:  ShaderIndex =  0; break;
            case SHADER_TYPE_PIXEL:   ShaderIndex =  1; break;
            case SHADER_TYPE_GEOMETRY:ShaderIndex =  2; break;
            case SHADER_TYPE_HULL:    ShaderIndex =  3; break;
            case SHADER_TYPE_DOMAIN:  ShaderIndex =  4; break;
            case SHADER_TYPE_COMPUTE: ShaderIndex =  5; break;
            default: UNEXPECTED( "Unexpected shader type (", Type, ")" ); ShaderIndex = -1;
        }
        VERIFY( Type == GetShaderTypeFromIndex(ShaderIndex), "Incorrect shader type index" );
        return ShaderIndex;
    }

    const ID3D11Buffer*              GetD3D11Resource( const ShaderD3D11Impl::BoundCB      &CB )     { return CB.pd3d11Buff; }
    const ID3D11SamplerState*        GetD3D11Resource( const ShaderD3D11Impl::BoundSampler &Sampler ){ return Sampler.pd3d11Sampler; }
    const ID3D11ShaderResourceView*  GetD3D11Resource( const ShaderD3D11Impl::BoundSRV     &SRV )    { return SRV.pd3d11View; }
    const ID3D11UnorderedAccessView* GetD3D11Resource( const ShaderD3D11Impl::BoundUAV     &UAV )    { return UAV.pd3d11View; }

    /// This helper template function facilitates binding different D3D11 resources to the device context

    /// \tparam TD3D11ResourceType - Type of D3D11 resource being bound (ID3D11ShaderResourceView, 
    ///                              ID3D11UnorderedAccessView, ID3D11Buffer or ID3D11SamplerState).
    /// \tparam TBoundResourceType - Type of the struct describing resources associated with the 
    ///                              bound entity (ShaderD3D11Impl::BoundCB, ShaderD3D11Impl::BoundSRV, etc.)
    /// \tparam TGetResourcesMethod - Type of the method that is used to get the bound resources array from 
    ///                               the shader (such as &ShaderD3D11Impl::GetBoundCBs).
    /// \tparam TUnbindProc - Type of the procedure that unbinds the resource from the context
    ///                       (for instance, if resource is set as SRV, it must be unbound from UAV slots)
    /// \tparam TD3D11ResourceSetMethod - Type of the method that bounds the new resources to the D3D11 
    ///                                   device context
    /// \param pShader - Pointer to the shader whose resources are to be bound
    /// \param BoundResourcesArr - Pointer to the array of structures describing currently bound
    ///                            shader resources, for each shader stage
    /// \param BoundD3D11ResourcesArr - Pointer to the array of currently bound D3D11
    ///                                 shader resources, for each shader stage
    /// \param GetResources - Pointer to the method to get array of bound shader resources from the shader
    /// \param UnbindProc - Function to perform required unbind steps
    /// \param D3D11ResourceSetMethod - Function to set the new D3D11 resources to the D3D11 device context
    template<typename TD3D11ResourceType, 
             typename TBoundResourceType,
             typename TGetResourcesMethod, 
             typename TUnbindProc,
             typename TD3D11ResourceSetMethod>
    void UpdateBoundResources(ShaderD3D11Impl *pShader,
                              std::vector<TBoundResourceType> BoundResourcesArr[],
                              std::vector<TD3D11ResourceType*> BoundD3D11ResourcesArr[], 
                              TGetResourcesMethod GetResources, 
                              TUnbindProc UnbindProc,
                              TD3D11ResourceSetMethod D3D11ResourceSetMethod)
    {
        const auto &NewResources = (pShader->*GetResources)();
        auto NumNewResources = NewResources.size();
        if( NumNewResources == 0 )
            return;
        
        auto ShaderType = pShader->GetDesc().ShaderType;
        auto ShaderInd = GetShaderTypeIndex(ShaderType);
        // Get currently bound resources for the shader type
        auto &BoundResources      = BoundResourcesArr     [ShaderInd];
        auto &BoundD3D11Resources = BoundD3D11ResourcesArr[ShaderInd];

        VERIFY(BoundD3D11Resources.size() == BoundResources.size(), "Inconsistent array sizes")
        if( NumNewResources > BoundD3D11Resources.size() )
            BoundD3D11Resources.resize(NumNewResources);
        if( NumNewResources > BoundResources.size() )
            BoundResources.resize(NumNewResources);

        // Compute the minimum and the maximum slot numbers of updated D3D11 resources
        Int32 MinSlot = std::numeric_limits<Int32>::max();
        Int32 MaxSlot = std::numeric_limits<Int32>::min();
        for( Int32 iSlot = 0; iSlot < static_cast<Int32>(NumNewResources); ++iSlot )
        {
            const auto& pNewRes      = NewResources     [iSlot];
            const auto& pNewD3D11Res = GetD3D11Resource(pNewRes);
            if( pNewD3D11Res != nullptr && pNewD3D11Res != BoundD3D11Resources[iSlot] )
            {
                // Perform the required steps to unbind the resource
                UnbindProc( pNewRes );

                MinSlot = std::min( MinSlot, iSlot );
                MaxSlot = std::max( MaxSlot, iSlot );
                BoundD3D11Resources[iSlot] = const_cast<TD3D11ResourceType*>(pNewD3D11Res);
                BoundResources[iSlot] = pNewRes;
            }
        }

        if( MaxSlot >= MinSlot )
        {
            D3D11ResourceSetMethod(ShaderInd, MinSlot, MaxSlot - MinSlot + 1, &BoundD3D11Resources[MinSlot]);
        }

        // Verify that the resource array and the D3D11 resource array are consistent
        dbgVerifyResourceArrays( BoundResources, BoundD3D11Resources, pShader );
    }

    /// Template function that facilitates binding D3D11 SRVs, CBs and samplers to the device context

    /// \tparam TD3D11ResourceType - Type of D3D11 resource being bound (ID3D11ShaderResourceView, 
    ///                              ID3D11Buffer or ID3D11SamplerState).
    /// \tparam TBoundResourceType - Type of the struct describing resources associated with the 
    ///                              bound entity (ShaderD3D11Impl::BoundCB, ShaderD3D11Impl::BoundSRV, etc.)
    /// \tparam TGetResourcesMethod - Type of the method that is used to get the bound resources array from 
    ///                               the shader (such as &ShaderD3D11Impl::GetBoundCBs).
    /// \tparam TUnbindProc - Type of the procedure that unbinds the resource from the context
    ///                       (for instance, if resource is set as SRV, it must be unbound from UAV slots)
    /// \tparam TSetD3D11Resource - Type of the D3D11 device context method used to set the 
    ///                             resource (such as &ID3D11DeviceContext::SetShaderResources)
    /// \param pShader - Pointer to the shader whose resources are to be bound
    /// \param BoundResourcesArr - Pointer to the array of structures describing currently bound
    ///                            shader resources, for each shader stage
    /// \param BoundD3D11ResourcesArr - Pointer to the array of currently bound D3D11
    ///                                 shader resources, for each shader stage
    /// \param GetResources - Pointer to the method to get array of bound shader resources from the shader
    /// \param UnbindProc - Function to perform required unbind steps
    /// \param SetD3D11ResourcesArr - Pointer to the array of device context methods used
    ///                               to set the resource, for every shader stage
	/// \param pd3d11DeviceCtx     - Device context
    template<typename TD3D11ResourceType, 
             typename TBoundResourceType,
             typename TGetResourcesMethod, 
             typename TUnbindProc,
             typename TSetD3D11Resource>
    void UpdateBoundResources(ShaderD3D11Impl *pShader,
                              std::vector<TBoundResourceType> BoundResourcesArr[],
                              std::vector<TD3D11ResourceType*> BoundD3D11ResourcesArr[], 
                              TGetResourcesMethod GetResources, 
                              TUnbindProc UnbindProc,
                              TSetD3D11Resource SetD3D11ResourcesArr[],
                              ID3D11DeviceContext *pd3d11DeviceCtx)
    {
        UpdateBoundResources(pShader, BoundResourcesArr, BoundD3D11ResourcesArr, 
                             GetResources, UnbindProc,
                             [&](Int32 ShaderInd, UINT MinSlot, UINT NumSlots, TD3D11ResourceType** ppD3D11Resources)
                             {
                                auto SetD3D11Resources = SetD3D11ResourcesArr[ShaderInd];
                                VERIFY(SetD3D11Resources, "Set D3D11 resource function pointer is null.");
                                if( SetD3D11Resources )
                                {
                                    (pd3d11DeviceCtx->*SetD3D11Resources)(MinSlot, NumSlots, ppD3D11Resources);
                                }
                             }
        );
    }

/// Helper macro used to create an array of device context methods to
/// set particular resource for every shader stage
#define DEFINE_D3D11CTX_FUNC_POINTERS(ArrayName, FuncName) \
    typedef decltype (&ID3D11DeviceContext::VS##FuncName) T##FuncName##Type;  \
    static const T##FuncName##Type ArrayName[] =    \
    {                                           \
        &ID3D11DeviceContext::VS##FuncName,  \
        &ID3D11DeviceContext::PS##FuncName,  \
        &ID3D11DeviceContext::GS##FuncName,  \
        &ID3D11DeviceContext::HS##FuncName,  \
        &ID3D11DeviceContext::DS##FuncName,  \
        &ID3D11DeviceContext::CS##FuncName   \
    };

    DEFINE_D3D11CTX_FUNC_POINTERS(SetCBMethods,      SetConstantBuffers)
    DEFINE_D3D11CTX_FUNC_POINTERS(SetSRVMethods,     SetShaderResources)
    DEFINE_D3D11CTX_FUNC_POINTERS(SetSamplerMethods, SetSamplers)

    typedef decltype (&ID3D11DeviceContext::CSSetUnorderedAccessViews) TSetUnorderedAccessViewsType;
    static const TSetUnorderedAccessViewsType SetUAVMethods[] =
    {
        nullptr, 
        nullptr, 
        nullptr, 
        nullptr, 
        nullptr, 
        &ID3D11DeviceContext::CSSetUnorderedAccessViews
    };

    void DeviceContextD3D11Impl::SetD3DConstantBuffers( ShaderD3D11Impl *pShader )
    {
        UpdateBoundResources(pShader, m_BoundCBs, m_BoundD3D11CBs, 
                             &ShaderD3D11Impl::GetBoundCBs,
                             [&](const ShaderD3D11Impl::BoundCB &BoundCB)
                             {
                                auto *pResource = const_cast<IDeviceObject *>(BoundCB.pBuff.RawPtr());
                                RefCntAutoPtr<IBuffer> pBuff;
                                pResource->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>( static_cast<IBuffer**>(&pBuff) ) );
                                if( pBuff )
                                {
                                    if( pBuff->GetDesc().BindFlags & BIND_UNORDERED_ACCESS )
                                        UnbindResourceFromUAV( pBuff );
                                }
                                else
                                {
                                    UNEXPECTED( "Resource \"", pResource->GetDesc().Name, "\" is expected to be a buffer" )
                                }
                             },
                             SetCBMethods,
                             m_pd3d11DeviceContext );
        dbgVerifyContextCBs(pShader->GetDesc().ShaderType);
    }
    
    void DeviceContextD3D11Impl::SetD3DSRVs( ShaderD3D11Impl *pShader )
    {
        UpdateBoundResources(pShader, m_BoundSRVs, m_BoundD3D11SRVs, 
                             &ShaderD3D11Impl::GetBoundSRVs,
                             [&](const ShaderD3D11Impl::BoundSRV &BoundSRV)
                             {
                                auto *pResource = const_cast<IDeviceObject *>(BoundSRV.pResource.RawPtr());
                                RefCntAutoPtr<ITexture> pTexture;
                                pResource->QueryInterface( IID_Texture, reinterpret_cast<IObject**>( static_cast<ITexture**>(&pTexture) ) );
                                if( pTexture )
                                {
                                    auto BindFlags = pTexture->GetDesc().BindFlags;
                                    if( BindFlags & BIND_UNORDERED_ACCESS )
                                        UnbindResourceFromUAV(pTexture);
                                    if( BindFlags & BIND_RENDER_TARGET )
                                        UnbindTextureFromRenderTarget(pTexture);
                                    if( BindFlags & BIND_DEPTH_STENCIL )
                                        UnbindTextureFromDepthStencil(pTexture);
                                }
                                else
                                {
                                    RefCntAutoPtr<IBuffer> pBuffer;
                                    pResource->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(static_cast<IBuffer**>(&pBuffer)) );
                                    if( pBuffer )
                                    {
                                        if( pBuffer->GetDesc().BindFlags & BIND_UNORDERED_ACCESS )
                                            UnbindResourceFromUAV( pBuffer );
                                    }
                                    else
                                    {
                                        UNEXPECTED( "Resource \"", pResource->GetDesc().Name, "\" is expected to be a texture or a buffer." )
                                    }
                                }
                             },
                             SetSRVMethods,
                             m_pd3d11DeviceContext );
        dbgVerifyContextSRVs(pShader->GetDesc().ShaderType);
    }

    void DeviceContextD3D11Impl::SetD3DSamplers( ShaderD3D11Impl *pShader )
    {
        UpdateBoundResources(pShader, m_BoundSamplers, m_BoundD3D11Samplers, 
                             &ShaderD3D11Impl::GetBoundSamplers,
                             [](const ShaderD3D11Impl::BoundSampler& ){}, // Do nothing
                             SetSamplerMethods,
                             m_pd3d11DeviceContext );
        dbgVerifyContextSamplers(pShader->GetDesc().ShaderType);
    }

    void DeviceContextD3D11Impl::SetD3DUAVs( ShaderD3D11Impl *pShader )
    {
        UpdateBoundResources(pShader, m_BoundUAVs, m_BoundD3D11UAVs, 
                            &ShaderD3D11Impl::GetBoundUAVs,
                            [&](const ShaderD3D11Impl::BoundUAV& BoundUAV)
                            {
                                auto *pResource = const_cast<IDeviceObject *>(BoundUAV.pResource.RawPtr());
                                RefCntAutoPtr<ITexture> pTexture;
                                pResource->QueryInterface( IID_Texture, reinterpret_cast<IObject**>( static_cast<ITexture**>(&pTexture) ) );
                                if( pTexture )
                                {
                                    // It is unlikely the texture is not used for input, so do not
                                    // check the bind flags
                                    UnbindTextureFromInput( pTexture );
                                }
                                else
                                {
                                    RefCntAutoPtr<IBuffer> pBuffer;
                                    pResource->QueryInterface( IID_Buffer, reinterpret_cast<IObject**>(static_cast<IBuffer**>(&pBuffer)) );
                                    if( pBuffer )
                                    {
                                        // It is unlikely the buffer is not used for input, so do not
                                        // check the bind flags
                                        UnbindBufferFromInput( pBuffer );
                                    }
                                    else
                                    {
                                        UNEXPECTED( "Resource \"", pResource->GetDesc().Name, "\" is expected to be a texture or a buffer." )
                                    }
                                }
                            },
                            [&](Int32 ShaderInd, UINT MinUAVSlot, UINT NumUAVSlots, ID3D11UnorderedAccessView** ppUAVs)
                            {
                                switch( ShaderInd )
                                {
                                    case 0: // SHADER_TYPE_PIXEL:
                                        UNSUPPORTED( "UAVs are not currently implemented in pixel shader" );
                                        /// \TODO: need to keep track of bound RTVs and provide the render targets here.
                                        ///        Also, we need to unbind UAVs appropriatelly.
                                        ///        Maybe deffer actual render target binding to the Draw call()?
                                        m_pd3d11DeviceContext->OMSetRenderTargetsAndUnorderedAccessViews( 0, nullptr, nullptr, MinUAVSlot, NumUAVSlots, ppUAVs, nullptr );
                                    break;

                                    case 1: //SHADER_TYPE_VERTEX:
                                    case 2: //SHADER_TYPE_GEOMETRY:
                                    case 3: //SHADER_TYPE_DOMAIN:
                                    case 4: //SHADER_TYPE_HULL:
                                        UNSUPPORTED( "UAVs are not currently supported in this type of shader" );
                                    break;

                                    case 5: //SHADER_TYPE_COMPUTE:
                                        m_pd3d11DeviceContext->CSSetUnorderedAccessViews( MinUAVSlot, NumUAVSlots, ppUAVs, nullptr );
                                    break;

                                    default: UNEXPECTED( "Unknown shader type" );
                                }                             
                            }
                        );
        dbgVerifyContextUAVs(pShader->GetDesc().ShaderType);
    }

    template<typename TD3D11ShaderType, typename TBindShaderMethdType>
    void DeviceContextD3D11Impl :: SetD3D11ShaderHelper(Uint32 BoundShaderFlags,
                                                         ShaderD3D11Impl* NewShaders[], 
                                                         SHADER_TYPE ShaderType,
                                                         TBindShaderMethdType BindShaderMethod)
    {
        auto ShaderTypeInd = GetShaderTypeIndex( ShaderType );
        auto &BoundD3D11Shader = m_BoundD3DShaders[ShaderTypeInd];
        auto *pShaderD3D11 = NewShaders[ShaderTypeInd];
        if( BoundShaderFlags & ShaderType )
        {
            VERIFY( pShaderD3D11 != nullptr, "Shader is null" );
            VERIFY( pShaderD3D11->GetDesc().ShaderType == ShaderType, "Unexpected shader type" );
            if( BoundD3D11Shader != pShaderD3D11->m_pShader )
            {
                BoundD3D11Shader = pShaderD3D11->m_pShader;
                RefCntAutoPtr<TD3D11ShaderType> pd3d11Shader;
				pShaderD3D11->m_pShader->QueryInterface(__uuidof(TD3D11ShaderType), reinterpret_cast<void**>( static_cast<TD3D11ShaderType**>(&pd3d11Shader) ) );
                (m_pd3d11DeviceContext->*BindShaderMethod)(pd3d11Shader, nullptr, 0);
            }
        }
        else
        {
            if( BoundD3D11Shader != nullptr )
            {
                BoundD3D11Shader = nullptr;
                (m_pd3d11DeviceContext->*BindShaderMethod)(nullptr, nullptr, 0);
            }
        }
    }

    void DeviceContextD3D11Impl :: SetD3DShadersAndResources()
    {
        Uint32 BoundShaderFlags = 0;
        // Set shaders
        ShaderD3D11Impl* NewShaders[NumShaderTypes] = {};
        for( auto it = m_pBoundShaders.begin(); it != m_pBoundShaders.end(); ++it )
        {
            auto *pCurrShader = it->RawPtr();
            if( pCurrShader )
            {
                auto ShaderType = pCurrShader->GetDesc().ShaderType;
                auto *pShaderD3D11 = static_cast<ShaderD3D11Impl*>(pCurrShader);
                auto ShaderTypeInd = GetShaderTypeIndex(ShaderType);
                NewShaders[ShaderTypeInd] = pShaderD3D11;

                if( BoundShaderFlags & ShaderType )
                    LOG_ERROR_MESSAGE( "More than one shader of type ", GetShaderTypeLiteralName(ShaderType), " is bound to the context" )
                BoundShaderFlags |= ShaderType;

#ifdef VERIFY_SHADER_BINDINGS
                pShaderD3D11->dbgVerifyBindings();
#endif

                // Set D3D11 shader resources
                SetD3DConstantBuffers ( pShaderD3D11 );
                SetD3DSRVs            ( pShaderD3D11 );
                SetD3DSamplers        ( pShaderD3D11 );
                SetD3DUAVs            ( pShaderD3D11 );
            }
        }
        
        // There is no need to unbind the compute shader from the context
        //if( BoundShaderFlags & SHADER_TYPE_COMPUTE )
        //{
        //    LOG_ERROR_MESSAGE( "Bound compute shader will be ignored by the draw command" )
        //    BoundShaderFlags &= ~SHADER_TYPE_COMPUTE;
        //}

#define SET_SHADER(NAME, Name, N) SetD3D11ShaderHelper<ID3D11##Name##Shader>(BoundShaderFlags, NewShaders, SHADER_TYPE_##NAME, &ID3D11DeviceContext::N##SSetShader )

        // These shaders which are not set will be unbound from the D3D11 device context
        SET_SHADER( VERTEX,   Vertex,   V );
        SET_SHADER( PIXEL,    Pixel,    P );
        SET_SHADER( GEOMETRY, Geometry, G );
        SET_SHADER( DOMAIN,   Domain,   D );
        SET_SHADER( HULL,     Hull,     H );
        // There is no need to unbind the compute shader from the context
        //SET_SHADER( COMPUTE,  Compute,  C );
    }

    void DeviceContextD3D11Impl::SetD3DIndexBuffer(VALUE_TYPE IndexType)
    {
        if( !m_pIndexBuffer )
        {
            LOG_ERROR_MESSAGE( "Index buffer is not set up for indexed draw command" );
            return;
        }
        BufferD3D11Impl *pBuffD3D11 = static_cast<BufferD3D11Impl *>(m_pIndexBuffer.RawPtr());
        DXGI_FORMAT D3D11IndexFmt = DXGI_FORMAT_UNKNOWN;
        if( IndexType == VT_UINT32 )
            D3D11IndexFmt = DXGI_FORMAT_R32_UINT;
        else if( IndexType == VT_UINT16 )
            D3D11IndexFmt = DXGI_FORMAT_R16_UINT;
        else
        {
            LOG_ERROR_MESSAGE( "Unsupported index format. Only R16_UINT and R32_UINT are allowed." );
            return;
        }
        if( m_pIndexBuffer->GetDesc().BindFlags & BIND_UNORDERED_ACCESS )
            UnbindResourceFromUAV(m_pIndexBuffer);

        if( m_BoundD3D11IndexBuffer != pBuffD3D11->m_pd3d11Buffer ||
            m_BoundD3D11IndexFmt != D3D11IndexFmt ||
            m_BoundD3D11IndexDataStartOffset != m_IndexDataStartOffset )
        {
            m_BoundD3D11IndexBuffer = pBuffD3D11->m_pd3d11Buffer;
            m_BoundD3D11IndexFmt = D3D11IndexFmt;
            m_BoundD3D11IndexDataStartOffset = m_IndexDataStartOffset;
            m_pd3d11DeviceContext->IASetIndexBuffer( pBuffD3D11->m_pd3d11Buffer, D3D11IndexFmt, m_IndexDataStartOffset );
        }
    }

    void DeviceContextD3D11Impl::SetD3DVertexBuffers()
    {
        auto &VertexDescSP = m_pVertexDesc;
        if( !VertexDescSP )
        {
            // There might be no vertex description if, for instance, full screen quad
            // is rendered
            m_pd3d11DeviceContext->IASetInputLayout( nullptr );
            return;
        }
        auto* pVertexDescD3D11 = static_cast<VertexDescD3D11Impl *>(VertexDescSP.RawPtr());
        m_pd3d11DeviceContext->IASetInputLayout( pVertexDescD3D11->m_pd3d11InputLayout );

        ID3D11Buffer *ppD3D11Buffers[MaxBufferSlots] = { nullptr };
        UINT Strides[MaxBufferSlots] = { 0 };
        UINT Offsets[MaxBufferSlots] = { 0 };
        UINT NumBoundBuffers = static_cast<UINT>(m_VertexStreams.size());
        VERIFY( NumBoundBuffers <= MaxBufferSlots, "Too many buffers are being set" );
        NumBoundBuffers = std::min( NumBoundBuffers, static_cast<UINT>(MaxBufferSlots) );
        const auto *TightStrides = VertexDescSP->GetTightStrides();
        for( UINT Buff = 0; Buff < NumBoundBuffers; ++Buff )
        {
            auto &CurrStream = m_VertexStreams[Buff];
            VERIFY( CurrStream.pBuffer, "Attempting to bind a null buffer for rendering" );
            ppD3D11Buffers[Buff] = static_cast<BufferD3D11Impl*>(CurrStream.pBuffer.RawPtr())->m_pd3d11Buffer;
            Strides[Buff] = CurrStream.Stride ? CurrStream.Stride : TightStrides[Buff];
            Offsets[Buff] = CurrStream.Offset;
            if(CurrStream.pBuffer->GetDesc().BindFlags & BIND_UNORDERED_ACCESS)
                UnbindResourceFromUAV(CurrStream.pBuffer);
        }

        UINT NumBuffersToSet = NumBoundBuffers;
        bool BindVBs = false;
        if( NumBoundBuffers != m_BoundD3D11VertexBuffers.size() )
        {
            // If number of currently bound d3d11 buffers is larger than NumBuffersToSet, the
            // unused buffers will be unbound
            NumBuffersToSet = std::max(NumBuffersToSet, static_cast<UINT>(m_BoundD3D11VertexBuffers.size()) );
            m_BoundD3D11VertexBuffers.resize(NumBoundBuffers);
            m_BoundD3D11VBStrides.resize(NumBoundBuffers);
            m_BoundD3D11VBOffsets.resize( NumBoundBuffers );
            BindVBs = true;
        }

        for( UINT Slot = 0; Slot < NumBoundBuffers; ++Slot )
        {
            if( m_BoundD3D11VertexBuffers[Slot] != ppD3D11Buffers[Slot] ||
                m_BoundD3D11VBStrides[Slot] != Strides[Slot] ||
                m_BoundD3D11VBOffsets[Slot] != Offsets[Slot] )
            {
                BindVBs = true;

                m_BoundD3D11VertexBuffers[Slot] = ppD3D11Buffers[Slot];
                m_BoundD3D11VBStrides[Slot] = Strides[Slot];
                m_BoundD3D11VBOffsets[Slot] = Offsets[Slot];
            }
        }

        if( BindVBs )
        {
            m_pd3d11DeviceContext->IASetVertexBuffers( 0, NumBuffersToSet, ppD3D11Buffers, Strides, Offsets );
        }
    }

    void DeviceContextD3D11Impl::Draw( DrawAttribs &DrawAttribs )
    {
        m_LastDrawAttribs = DrawAttribs;

        SetD3DShadersAndResources();
        
        SetD3DVertexBuffers();

        if( DrawAttribs.IsIndexed )
        {
            SetD3DIndexBuffer(DrawAttribs.IndexType);
        }
        
        // Verify bindings after all resources are set
        dbgVerifyContextSRVs();
        dbgVerifyContextUAVs();
        dbgVerifyContextSamplers();
        dbgVerifyContextCBs();
        dbgVerifyVertexBuffers();
        dbgVerifyIndexBuffer();
        dbgVerifyShaders();

        auto D3D11Topology = TopologyToDX11Topology( DrawAttribs.Topology );
        m_pd3d11DeviceContext->IASetPrimitiveTopology( D3D11Topology );

        if( DrawAttribs.IsIndirect )
        {
            VERIFY( DrawAttribs.pIndirectDrawAttribs, "Indirect draw command attributes buffer is not set" );
            auto *pBufferD3D11 = static_cast<BufferD3D11Impl*>(DrawAttribs.pIndirectDrawAttribs);
            ID3D11Buffer *pd3d11ArgsBuff = pBufferD3D11 ? pBufferD3D11->m_pd3d11Buffer.RawPtr() : nullptr;
            if( DrawAttribs.IsIndexed )
                m_pd3d11DeviceContext->DrawIndexedInstancedIndirect( pd3d11ArgsBuff, DrawAttribs.IndirectDrawArgsOffset );
            else
                m_pd3d11DeviceContext->DrawInstancedIndirect( pd3d11ArgsBuff, DrawAttribs.IndirectDrawArgsOffset );
        }
        else
        {
            if( DrawAttribs.NumInstances > 1 )
            {
                if( DrawAttribs.IsIndexed )
                    m_pd3d11DeviceContext->DrawIndexedInstanced( DrawAttribs.NumIndices, DrawAttribs.NumInstances, DrawAttribs.FirstIndexLocation, DrawAttribs.BaseVertex, DrawAttribs.FirstInstanceLocation );
                else
                    m_pd3d11DeviceContext->DrawInstanced( DrawAttribs.NumVertices, DrawAttribs.NumInstances, DrawAttribs.StartVertexLocation, DrawAttribs.FirstInstanceLocation );
            }
            else
            {
                if( DrawAttribs.IsIndexed )
                    m_pd3d11DeviceContext->DrawIndexed( DrawAttribs.NumIndices, DrawAttribs.FirstIndexLocation, DrawAttribs.BaseVertex );
                else
                    m_pd3d11DeviceContext->Draw( DrawAttribs.NumVertices, DrawAttribs.StartVertexLocation );
            }
        }
    }

    void DeviceContextD3D11Impl::DispatchCompute( const DispatchComputeAttribs &DispatchAttrs )
    {
        bool bCSBound = false;
        ShaderD3D11Impl* NewShaders[NumShaderTypes] = {};
        for( auto it = m_pBoundShaders.begin(); it != m_pBoundShaders.end() && !bCSBound; ++it )
        {
            auto *pCurrShader = it->RawPtr();
            if( pCurrShader && pCurrShader->GetDesc().ShaderType == SHADER_TYPE_COMPUTE )
            {
                bCSBound = true;
                auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>(pCurrShader);
                NewShaders[GetShaderTypeIndex(SHADER_TYPE_COMPUTE)] = pShaderD3D11;
                SetD3D11ShaderHelper<ID3D11ComputeShader>( SHADER_TYPE_COMPUTE, NewShaders, SHADER_TYPE_COMPUTE, &ID3D11DeviceContext::CSSetShader );

#ifdef VERIFY_SHADER_BINDINGS
                pShaderD3D11->dbgVerifyBindings();
#endif

                // Set requried shader resources
                SetD3DConstantBuffers ( pShaderD3D11 );
                SetD3DSRVs            ( pShaderD3D11 );
                SetD3DSamplers        ( pShaderD3D11 );
                SetD3DUAVs            ( pShaderD3D11 );

                break;
            }
        }

        if( !bCSBound )
        {
            LOG_ERROR_MESSAGE( "Compute shader is not bound to the pipeline" );
            return;
        }

        // Verify bindings
        dbgVerifyContextSRVs();
        dbgVerifyContextUAVs();
        dbgVerifyContextSamplers();
        dbgVerifyContextCBs();
        dbgVerifyShaders();

        if( DispatchAttrs.pIndirectDispatchAttribs )
        {
            CHECK_DYNAMIC_TYPE( BufferD3D11Impl, DispatchAttrs.pIndirectDispatchAttribs );
            auto *pd3d11Buff = static_cast<BufferD3D11Impl*>(DispatchAttrs.pIndirectDispatchAttribs)->GetD3D11Buffer();
            m_pd3d11DeviceContext->DispatchIndirect( pd3d11Buff, DispatchAttrs.DispatchArgsByteOffset );
        }
        else
            m_pd3d11DeviceContext->Dispatch( DispatchAttrs.ThreadGroupCountX, DispatchAttrs.ThreadGroupCountY, DispatchAttrs.ThreadGroupCountZ );
    }

    void DeviceContextD3D11Impl::ClearDepthStencil( ITextureView *pView, Uint32 ClearFlags, float fDepth, Uint8 Stencil )
    {
        ID3D11DepthStencilView *pd3d11DSV = nullptr;
        if( pView != nullptr )
        {
            const auto& ViewDesc = pView->GetDesc();
            VERIFY( ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL, "Incorrect view type: depth stencil is expected" );
            CHECK_DYNAMIC_TYPE( TextureViewD3D11Impl, pView );
            auto *pViewD3D11 = static_cast<TextureViewD3D11Impl*>(pView);
            pd3d11DSV = static_cast<ID3D11DepthStencilView *>(pViewD3D11->GetD3D11View());
        }
        else
        {
            pd3d11DSV = ValidatedCast<SwapChainD3D11Impl>(m_pSwapChain.RawPtr())->GetDSV();
        }
        UINT32 d3d11ClearFlags = 0;
        if( ClearFlags & CLEAR_DEPTH_FLAG )   d3d11ClearFlags |= D3D11_CLEAR_DEPTH;
        if( ClearFlags & CLEAR_STENCIL_FLAG ) d3d11ClearFlags |= D3D11_CLEAR_STENCIL;
        // The full extent of the resource view is always cleared. 
        // Viewport and scissor settings are not applied.
        m_pd3d11DeviceContext->ClearDepthStencilView( pd3d11DSV, d3d11ClearFlags, fDepth, Stencil );
    }

    void DeviceContextD3D11Impl::ClearRenderTarget( ITextureView *pView, const float *RGBA )
    {
        ID3D11RenderTargetView *pd3d11RTV = nullptr;
        if( pView != nullptr )
        {
            const auto& ViewDesc = pView->GetDesc();
            VERIFY( ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET, "Incorrect view type: render target is expected" );
            CHECK_DYNAMIC_TYPE( TextureViewD3D11Impl, pView );
            auto *pViewD3D11 = static_cast<TextureViewD3D11Impl*>(pView);
            pd3d11RTV = static_cast<ID3D11RenderTargetView*>(pViewD3D11->GetD3D11View());
        }
        else
        {
            pd3d11RTV = ValidatedCast<SwapChainD3D11Impl>(m_pSwapChain.RawPtr())->GetRTV();
        }

        static const float Zero[4] = { 0.f, 0.f, 0.f, 0.f };
        if( RGBA == nullptr )
            RGBA = Zero;

        // The full extent of the resource view is always cleared. 
        // Viewport and scissor settings are not applied.
        m_pd3d11DeviceContext->ClearRenderTargetView( pd3d11RTV, RGBA );
    }

    void DeviceContextD3D11Impl::Flush()
    {
        m_pd3d11DeviceContext->Flush();
    }

    void DeviceContextD3D11Impl::SetVertexBuffers( Uint32 StartSlot, Uint32 NumBuffersSet, IBuffer **ppBuffers, Uint32 *pStrides, Uint32 *pOffsets, Uint32 Flags )
    {
        TDeviceContextBase::SetVertexBuffers( StartSlot, NumBuffersSet, ppBuffers, pStrides, pOffsets, Flags );
    }

    void DeviceContextD3D11Impl::ClearState()
    {
        TDeviceContextBase::ClearState();

    }

    void DeviceContextD3D11Impl::SetVertexDescription( IVertexDescription *pVertexDesc )
    {
        TDeviceContextBase::SetVertexDescription( pVertexDesc );
    }

    void DeviceContextD3D11Impl::SetIndexBuffer( IBuffer *pIndexBuffer, Uint32 ByteOffset )
    {
        TDeviceContextBase::SetIndexBuffer( pIndexBuffer, ByteOffset );
    }

    void DeviceContextD3D11Impl::SetDepthStencilState( IDepthStencilState *pDepthStencilState, Uint32 StencilRef )
    {
        if( TDeviceContextBase::SetDepthStencilState( pDepthStencilState, StencilRef ) )
        {
            CHECK_DYNAMIC_TYPE( DSStateD3D11Impl, pDepthStencilState );
            auto *pDSSD3D11 = static_cast<DSStateD3D11Impl*>(pDepthStencilState);
            auto *pd3d11DSS = pDSSD3D11->GetD3D11DepthStencilState();
            m_pd3d11DeviceContext->OMSetDepthStencilState( pd3d11DSS, StencilRef );
        }
    }

    void DeviceContextD3D11Impl::SetRasterizerState( IRasterizerState *pRasterizerState )
    {
        if( TDeviceContextBase::SetRasterizerState( pRasterizerState ) )
        {
            CHECK_DYNAMIC_TYPE( RasterizerStateD3D11Impl, pRasterizerState );
            auto *pRSD3D11 = static_cast<RasterizerStateD3D11Impl*>(pRasterizerState);
            auto *pd3d11RS = pRSD3D11->GetD3D11RasterizerState();
            m_pd3d11DeviceContext->RSSetState( pd3d11RS );
        }
    }

    void DeviceContextD3D11Impl::SetBlendState( IBlendState *pBS, const float* pBlendFactors, Uint32 SampleMask )
    {
        if( TDeviceContextBase::SetBlendState( pBS, pBlendFactors, SampleMask ) )
        {
            CHECK_DYNAMIC_TYPE( BlendStateD3D11Impl, pBS );
            auto *pBSD3D11 = static_cast<BlendStateD3D11Impl*>(pBS);
            auto *pd3d11BS = pBSD3D11->GetD3D11BlendState();
            m_pd3d11DeviceContext->OMSetBlendState( pd3d11BS, m_BlendFactors, SampleMask );
        }
    }

    void DeviceContextD3D11Impl::SetViewports( Uint32 NumViewports, const Viewport *pViewports, Uint32 RTWidth, Uint32 RTHeight  )
    {
        const Uint32 MaxViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        VERIFY( NumViewports < MaxViewports, "Too many viewports are being set" );
        NumViewports = std::min( NumViewports, MaxViewports );

        TDeviceContextBase::SetViewports( NumViewports, pViewports, RTWidth, RTHeight );
        
        D3D11_VIEWPORT d3d11Viewports[MaxViewports];
        VERIFY( NumViewports == m_Viewports.size(), "Unexpected number of viewports" );
        for( Uint32 vp = 0; vp < NumViewports; ++vp )
        {
            d3d11Viewports[vp].TopLeftX = m_Viewports[vp].TopLeftX;
            d3d11Viewports[vp].TopLeftY = m_Viewports[vp].TopLeftY;
            d3d11Viewports[vp].Width    = m_Viewports[vp].Width;
            d3d11Viewports[vp].Height   = m_Viewports[vp].Height;
            d3d11Viewports[vp].MinDepth = m_Viewports[vp].MinDepth;
            d3d11Viewports[vp].MaxDepth = m_Viewports[vp].MaxDepth;
        }
        // All viewports must be set atomically as one operation. 
        // Any viewports not defined by the call are disabled.
        m_pd3d11DeviceContext->RSSetViewports( NumViewports, d3d11Viewports );
    }

    void DeviceContextD3D11Impl::SetScissorRects( Uint32 NumRects, const Rect *pRects, Uint32 RTWidth, Uint32 RTHeight  )
    {
        const Uint32 MaxScissorRects = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        VERIFY( NumRects < MaxScissorRects, "Too many scissor rects are being set" );
        NumRects = std::min( NumRects, MaxScissorRects );

        TDeviceContextBase::SetScissorRects(NumRects, pRects, RTWidth, RTHeight);

        D3D11_RECT d3d11ScissorRects[MaxScissorRects];
        VERIFY( NumRects == m_ScissorRects.size(), "Unexpected number of scissor rects" );
        for( Uint32 sr = 0; sr < NumRects; ++sr )
        {
            d3d11ScissorRects[sr].left   = m_ScissorRects[sr].left;
            d3d11ScissorRects[sr].top    = m_ScissorRects[sr].top;
            d3d11ScissorRects[sr].right  = m_ScissorRects[sr].right;
            d3d11ScissorRects[sr].bottom = m_ScissorRects[sr].bottom;
        }

        // All scissor rects must be set atomically as one operation. 
        // Any scissor rects not defined by the call are disabled.
        m_pd3d11DeviceContext->RSSetScissorRects( NumRects, d3d11ScissorRects );
    }

    void DeviceContextD3D11Impl::RebindRenderTargets()
    {
        const Uint32 MaxD3D11RTs = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
        Uint32 NumRenderTargets = static_cast<Uint32>( m_pBoundRenderTargets.size() );
        VERIFY( NumRenderTargets <= MaxD3D11RTs, "D3D11 only allows 8 simultaneous render targets" );
        NumRenderTargets = std::min( MaxD3D11RTs, NumRenderTargets );

        ID3D11RenderTargetView *pd3d11RTs[MaxD3D11RTs] = {};
        ID3D11DepthStencilView *pd3d11DSV = nullptr;

        auto *pDepthStencil = m_pBoundDepthStencil.RawPtr();
        if( NumRenderTargets == 0 && pDepthStencil == nullptr )
        {
            NumRenderTargets = 1;
            auto *pSwapChainD3D11 = ValidatedCast<SwapChainD3D11Impl>( m_pSwapChain.RawPtr() );
            pd3d11RTs[0] = pSwapChainD3D11->GetRTV();
            pd3d11DSV = pSwapChainD3D11->GetDSV();
        }
        else
        {
            for( Uint32 rt = 0; rt < NumRenderTargets; ++rt )
            {
                auto *pView = m_pBoundRenderTargets[rt].RawPtr();
                if( pView )
                {
                    auto *pViewD3D11 = static_cast<TextureViewD3D11Impl*>(pView);
                    pd3d11RTs[rt] = static_cast<ID3D11RenderTargetView*>(pViewD3D11->GetD3D11View());
                }
                else
                    pd3d11RTs[rt] = nullptr;
            }

            if( pDepthStencil != nullptr )
            {
                auto *pViewD3D11 = static_cast<TextureViewD3D11Impl*>(pDepthStencil);
                pd3d11DSV = static_cast<ID3D11DepthStencilView*>(pViewD3D11->GetD3D11View());
            }
        }

        m_pd3d11DeviceContext->OMSetRenderTargets(NumRenderTargets, pd3d11RTs, pd3d11DSV);
    }

    void UnbindView( ID3D11DeviceContext *pContext, TSetShaderResourcesType SetSRVMethod, UINT Slot )
    {
        ID3D11ShaderResourceView *ppNullView[] = { nullptr };
        (pContext->*SetSRVMethod)(Slot, 1, ppNullView);
    }

    void UnbindView( ID3D11DeviceContext *pContext, TSetUnorderedAccessViewsType SetUAVMethod, UINT Slot )
    {
        ID3D11UnorderedAccessView *ppNullView[] = { nullptr };
        (pContext->*SetUAVMethod)(Slot, 1, ppNullView, nullptr);
    }

    /// \tparam TBoundResourceType - Type of the struct describing resources associated with the 
    ///                              bound entity (ShaderD3D11Impl::BoundCB, ShaderD3D11Impl::BoundSRV, etc.)
    /// \tparam TD3D11ResourceType - Type of the D3D11 resource (ID3D11ShaderResourceView or ID3D11UnorderedAccessView)
    /// \tparam TSetD3D11View - Type of the D3D11 device context method used to set the D3D11 view
    /// \param BoundResourcesArr - Pointer to the array of strong references to currently bound
    ///                            shader resources, for each shader stage
    /// \param BoundD3D11ResourcesArr - Pointer to the array of currently bound D3D11
    ///                                 shader resources, for each shader stage
    /// \param pResToUnbind - Resource to unbind
    /// \param SetD3D11ViewMethods - Array of pointers to device context methods used to set the view,
    ///                              for every shader stage
    template<typename TBoundResourceType,
             typename TD3D11ResourceType,
             typename TSetD3D11View>
    void DeviceContextD3D11Impl::UnbindResourceView(std::vector<TBoundResourceType> BoundResourcesArr[],
                                                     std::vector<TD3D11ResourceType> BoundD3D11ResourcesArr[], 
                                                     IDeviceObject *pResToUnbind,
                                                     TSetD3D11View SetD3D11ViewMethods[])
    {
        for( Int32 ShaderTypeInd = 0; ShaderTypeInd < NumShaderTypes; ++ShaderTypeInd )
        {
            auto &BoundResources = BoundResourcesArr[ShaderTypeInd];
            auto &BoundD3D11Views = BoundD3D11ResourcesArr[ShaderTypeInd];
            VERIFY( BoundResources.size() == BoundD3D11Views.size(), "Inconsistent shader resource view array sizes" );
            for( Uint32 Slot = 0; Slot < BoundResources.size(); ++Slot )
            {
                if( BoundResources[Slot].pResource == pResToUnbind )
                {
                    BoundResources[Slot] = TBoundResourceType();
                    BoundD3D11Views[Slot] = nullptr;

                    auto SetViewMethod = SetD3D11ViewMethods[ShaderTypeInd];
                    UnbindView( m_pd3d11DeviceContext, SetViewMethod, Slot );
                }
            }

            // Pop null resources from the end of arrays
            while( BoundD3D11Views.size() > 0 && BoundD3D11Views.back() == nullptr )
            {
                BoundD3D11Views.pop_back();
                VERIFY( BoundResources.back().pResource  == nullptr, "Unexpected non-null resource detected" )
                VERIFY( BoundResources.back().pView      == nullptr, "Unexpected non-null resource view detected" )
                VERIFY( BoundResources.back().pd3d11View == nullptr, "Unexpected non-null D3D11 resource view detected" )
                BoundResources.pop_back();
            }
            VERIFY(BoundResources.size() == BoundD3D11Views.size(), "Inconsistent array size") 
        }
    }

    void DeviceContextD3D11Impl::UnbindTextureFromInput( ITexture *pTexture )
    {
        VERIFY( pTexture, "Null texture provided" )
        if( !pTexture )return;

        UnbindResourceView( m_BoundSRVs, m_BoundD3D11SRVs, pTexture, SetSRVMethods );
    }

    void DeviceContextD3D11Impl::UnbindBufferFromInput( IBuffer *pBuffer )
    {
        VERIFY( pBuffer, "Null buffer provided" )
        if( !pBuffer )return;

        UnbindResourceView( m_BoundSRVs, m_BoundD3D11SRVs, pBuffer, SetSRVMethods );
        
        auto BuffBindFlags = pBuffer->GetDesc().BindFlags;

        if( (BuffBindFlags & BIND_INDEX_BUFFER) && m_BoundD3D11IndexBuffer )
        {
            auto pd3d11IndBuffer = ValidatedCast<BufferD3D11Impl>( pBuffer )->GetD3D11Buffer();
            if( pd3d11IndBuffer == m_BoundD3D11IndexBuffer )
            {
                // Only unbind D3D11 buffer from the context!
                // m_pIndexBuffer.Release();
                m_BoundD3D11IndexBuffer.Release();
                m_BoundD3D11IndexFmt = DXGI_FORMAT_UNKNOWN;
                m_BoundD3D11IndexDataStartOffset = 0;
                m_pd3d11DeviceContext->IASetIndexBuffer( nullptr, m_BoundD3D11IndexFmt, m_BoundD3D11IndexDataStartOffset );
            }
            dbgVerifyIndexBuffer();
        }

        if( BuffBindFlags & BIND_VERTEX_BUFFER )
        {
            auto pd3d11VB = ValidatedCast<BufferD3D11Impl>( pBuffer )->GetD3D11Buffer();
            for( Uint32 Slot = 0; Slot < m_BoundD3D11VertexBuffers.size(); ++Slot )
            {
                auto &BoundD3D11VB = m_BoundD3D11VertexBuffers[Slot];
                if(BoundD3D11VB == pd3d11VB)
                {
                    // Unbind only D3D11 buffer
                    //*VertStream = VertexStreamInfo();
                    ID3D11Buffer *ppNullBuffer[] = { nullptr };
                    const UINT Zero[] = { 0 };
                    m_BoundD3D11VertexBuffers[Slot].Release();
                    m_BoundD3D11VBStrides[Slot] = 0;
                    m_BoundD3D11VBOffsets[Slot] = 0;
                    m_pd3d11DeviceContext->IASetVertexBuffers( Slot, _countof( ppNullBuffer ), ppNullBuffer, Zero, Zero );
                }
            }
            dbgVerifyVertexBuffers();
        }

        if( BuffBindFlags & BIND_UNIFORM_BUFFER )
        {
            for( Int32 ShaderTypeInd = 0; ShaderTypeInd < NumShaderTypes; ++ShaderTypeInd )
            {
                auto &BoundCBs = m_BoundCBs[ShaderTypeInd];
                auto &BoundD3D11CBs = m_BoundD3D11CBs[ShaderTypeInd];
                VERIFY( BoundCBs.size() == BoundD3D11CBs.size(), "Inconsistent constant buffer array sizes" );
                auto NumSlots = std::min( BoundCBs.size(), BoundD3D11CBs.size() );
                for( Uint32 Slot = 0; Slot < NumSlots; ++Slot )
                {
                    if( BoundCBs[Slot].pBuff == pBuffer )
                    {
                        BoundCBs[Slot] = ShaderD3D11Impl::BoundCB();
                        BoundD3D11CBs[Slot] = nullptr;
                        auto SetCBMethod = SetCBMethods[ShaderTypeInd];
                        ID3D11Buffer *ppNullBuffer[] = { nullptr };
                        (m_pd3d11DeviceContext->*SetCBMethod)(Slot, 1, ppNullBuffer);
                    }
                }
            }

            dbgVerifyContextCBs();
        }
    }

    void DeviceContextD3D11Impl::UnbindResourceFromUAV( IDeviceObject *pResource )
    {
        VERIFY( pResource, "Null resource provided" )
        if( !pResource )return;

        UnbindResourceView( m_BoundUAVs, m_BoundD3D11UAVs, pResource, SetUAVMethods );
    }

    void DeviceContextD3D11Impl::UnbindTextureFromRenderTarget( IDeviceObject *pResource )
    {
        VERIFY( pResource, "Null resource provided" )
        if( !pResource )return;
        VERIFY(ValidatedCast<ITexture>( pResource ), "Resource is not a texture")

        bool bRebindRenderTargets = false;
        for( auto rt = m_pBoundRenderTargets.begin(); rt != m_pBoundRenderTargets.end(); ++rt )
        {
            if( auto pTexView = *rt )
            {
                if( pTexView->GetTexture() == pResource )
                {
                    (*rt).Release();
                    bRebindRenderTargets = true;
                }
            }
        }

        if( bRebindRenderTargets )
            RebindRenderTargets();
    }

    void DeviceContextD3D11Impl::UnbindTextureFromDepthStencil(IDeviceObject *pResource)
    {
        VERIFY( pResource, "Null resource provided" )
        if( !pResource )return;
        VERIFY(ValidatedCast<ITexture>( pResource ), "Resource is not a texture")

        if( m_pBoundDepthStencil && m_pBoundDepthStencil->GetTexture() == pResource )
        {
            m_pBoundDepthStencil.Release();
            RebindRenderTargets();
        }
    }

    void DeviceContextD3D11Impl::SetRenderTargets( Uint32 NumRenderTargets, ITextureView *ppRenderTargets[], ITextureView *pDepthStencil )
    {
        if( TDeviceContextBase::SetRenderTargets( NumRenderTargets, ppRenderTargets, pDepthStencil ) )
        {
            for( Uint32 RT = 0; RT < NumRenderTargets; ++RT )
                if( ppRenderTargets[RT] )
                {
                    UnbindTextureFromInput( ppRenderTargets[RT]->GetTexture() );
                }
            if( pDepthStencil )
                UnbindTextureFromInput( pDepthStencil->GetTexture() );

            RebindRenderTargets();

            // Set the viewport to match the render target size
            SetViewports(1, nullptr, 0, 0);
        }
    }
    
    template<typename TD3D11ResourceType, typename TSetD3D11ResMethodType>
    void SetD3D11ResourcesHelper(ID3D11DeviceContext *pDeviceCtx, 
                                 TSetD3D11ResMethodType SetD3D11ResMethod, 
                                 UINT StartSlot, UINT NumSlots, 
                                 TD3D11ResourceType **ppResources)
    {
        (pDeviceCtx->*SetD3D11ResMethod)(StartSlot, NumSlots, ppResources);
    }

    template<>
    void SetD3D11ResourcesHelper(ID3D11DeviceContext *pDeviceCtx, 
                                 TSetUnorderedAccessViewsType SetD3D11UAVMethod, 
                                 UINT StartSlot, UINT NumSlots, 
                                 ID3D11UnorderedAccessView **ppUAVs)
    {
        (pDeviceCtx->*SetD3D11UAVMethod)(StartSlot, NumSlots, ppUAVs, nullptr);
    }

    template<typename TD3D11ResourceType, typename TBoundResourceType, typename TSetD3D11ResMethodType>
    void ClearStateCacheInternal(std::vector<TD3D11ResourceType*> &BoundD3D11Res,
                                 std::vector<TBoundResourceType> &BoundRes,
                                 TSetD3D11ResMethodType SetD3D11ResMethod,
                                 ID3D11DeviceContext *pDeviceCtx)
    {
        if( BoundD3D11Res.size() )
        {
            memset( BoundD3D11Res.data(), 0, BoundD3D11Res.size() * sizeof( BoundD3D11Res[0] ) );
            SetD3D11ResourcesHelper( pDeviceCtx, SetD3D11ResMethod, 0, static_cast<UINT>(BoundD3D11Res.size()), BoundD3D11Res.data() );
            BoundD3D11Res.clear();
            BoundRes.clear();
        }
    }

    void DeviceContextD3D11Impl::ClearShaderStateCache()
    {
        for( int ShaderType = 0; ShaderType < NumShaderTypes; ++ShaderType )
        {
            ClearStateCacheInternal( m_BoundD3D11CBs     [ShaderType], m_BoundCBs     [ShaderType], SetCBMethods[ShaderType],      m_pd3d11DeviceContext );
            ClearStateCacheInternal( m_BoundD3D11SRVs    [ShaderType], m_BoundSRVs    [ShaderType], SetSRVMethods[ShaderType],     m_pd3d11DeviceContext );
            ClearStateCacheInternal( m_BoundD3D11Samplers[ShaderType], m_BoundSamplers[ShaderType], SetSamplerMethods[ShaderType], m_pd3d11DeviceContext );
            ClearStateCacheInternal( m_BoundD3D11UAVs    [ShaderType], m_BoundUAVs    [ShaderType], SetUAVMethods[ShaderType],     m_pd3d11DeviceContext );
        }

        dbgVerifyContextSRVs();
        dbgVerifyContextUAVs();
        dbgVerifyContextSamplers();
        dbgVerifyContextCBs();
        
        // We do not unbind vertex buffers and index buffer as this can explicitly 
        // be done by the user
    }

#ifdef VERIFY_CONTEXT_BINDINGS

   DEFINE_D3D11CTX_FUNC_POINTERS(GetCBMethods,      GetConstantBuffers)
   DEFINE_D3D11CTX_FUNC_POINTERS(GetSRVMethods,     GetShaderResources)
   DEFINE_D3D11CTX_FUNC_POINTERS(GetSamplerMethods, GetSamplers)

    typedef decltype (&ID3D11DeviceContext::CSGetUnorderedAccessViews) TGetUnorderedAccessViewsType;
    static const TGetUnorderedAccessViewsType GetUAVMethods[] =
    {
        nullptr, 
        nullptr, 
        nullptr, 
        nullptr, 
        nullptr, 
        &ID3D11DeviceContext::CSGetUnorderedAccessViews
    };

    /// \tparam MaxResources - Maximum number of resources that can be bound to D3D11 context
    /// \tparam TD3D11ResourceType - Type of D3D11 resource being checked (ID3D11ShaderResourceView, 
    ///                              ID3D11UnorderedAccessView, ID3D11Buffer or ID3D11SamplerState).
    /// \tparam TGetD3D11ResourcesType - Type of the device context method used to get the bound 
    ///                                  resources
    /// \param BoundD3D11ResourcesArr - Pointer to the array of currently bound D3D11
    ///                                 resources, for each shader stage
    /// \param GetD3D11ResMethods - Pointer to the array of device context methods to get the bound
    ///                             resources, for each shader stage
    /// \param ResourceName - Resource name
    /// \param ShaderType - Shader type for which to check the resources. If Diligent::SHADER_TYPE_UNKNOWN
    ///                     is provided, all shader stages will be checked
    template<UINT MaxResources, 
             typename TD3D11ResourceType, 
             typename TGetD3D11ResourcesType>
    void DeviceContextD3D11Impl::dbgVerifyContextResources(std::vector<TD3D11ResourceType> BoundD3D11ResourcesArr[],
                                                            TGetD3D11ResourcesType GetD3D11ResMethods[],
                                                            const Char *ResourceName,
                                                            SHADER_TYPE ShaderType)
    {
        int iStartInd = 0, iEndInd = NumShaderTypes;
        if( ShaderType != SHADER_TYPE_UNKNOWN )
        {
            iStartInd = GetShaderTypeIndex(ShaderType);
            iEndInd = iStartInd + 1;
        }
        for( int iShaderTypeInd = iStartInd; iShaderTypeInd < iEndInd; ++iShaderTypeInd )
        {
            const auto ShaderName = GetShaderTypeLiteralName( GetShaderTypeFromIndex(iShaderTypeInd) );
            TD3D11ResourceType pctxResources[MaxResources] = {};
            auto GetResMethod = GetD3D11ResMethods[iShaderTypeInd];
            if( GetResMethod )
            {
                (m_pd3d11DeviceContext->*GetResMethod)(0, _countof( pctxResources ), pctxResources);
            }
            const auto& BoundResources = BoundD3D11ResourcesArr[iShaderTypeInd];
            auto NumBoundResources = BoundResources.size();
            for( Uint32 Slot = 0; Slot < _countof( pctxResources ); ++Slot )
            {
                if( Slot < NumBoundResources )
                {
                    VERIFY( BoundResources[Slot] == pctxResources[Slot], ResourceName, " binding mismatch found for ", ShaderName, " shader type at slot ", Slot )
                }
                else
                {
                    VERIFY( pctxResources[Slot] == nullptr, ResourceName, " binding mismatch found for ", ShaderName, " shader type at slot ", Slot )
                }
                
                if( pctxResources[Slot] )
                    pctxResources[Slot]->Release();
            }
        }
    }

    void DeviceContextD3D11Impl::dbgVerifyContextSRVs(SHADER_TYPE ShaderType)
    {
        dbgVerifyContextResources<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>( m_BoundD3D11SRVs, GetSRVMethods, "SRV", ShaderType );
    }

    void DeviceContextD3D11Impl::dbgVerifyContextUAVs(SHADER_TYPE ShaderType)
    {
        dbgVerifyContextResources<D3D11_PS_CS_UAV_REGISTER_COUNT>( m_BoundD3D11UAVs, GetUAVMethods, "UAV", ShaderType );
    }

    void DeviceContextD3D11Impl::dbgVerifyContextSamplers(SHADER_TYPE ShaderType)
    {
        dbgVerifyContextResources<D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT>( m_BoundD3D11Samplers, GetSamplerMethods, "Sampler", ShaderType );
    }

    void DeviceContextD3D11Impl::dbgVerifyContextCBs(SHADER_TYPE ShaderType)
    {
        dbgVerifyContextResources<D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT>( m_BoundD3D11CBs, GetCBMethods, "Constant buffer", ShaderType );
    }

    void DeviceContextD3D11Impl::dbgVerifyIndexBuffer()
    {
        RefCntAutoPtr<ID3D11Buffer> pctxIndexBuffer;
        DXGI_FORMAT Fmt = DXGI_FORMAT_UNKNOWN;
        UINT Offset = 0;
        m_pd3d11DeviceContext->IAGetIndexBuffer( &pctxIndexBuffer, &Fmt, &Offset );
        if( m_BoundD3D11IndexBuffer && !pctxIndexBuffer )
            UNEXPECTED( "D3D11 index buffer is not bound to the context" );
        if( !m_BoundD3D11IndexBuffer && pctxIndexBuffer )
            UNEXPECTED( "Unexpected D3D11 index buffer is bound to the context" );

        if( m_BoundD3D11IndexBuffer && pctxIndexBuffer )
        {
            VERIFY(m_BoundD3D11IndexBuffer == pctxIndexBuffer, "Index buffer binding mismatch detected");
            VERIFY(m_BoundD3D11IndexFmt == Fmt, "Index buffer format mismatch detected");
            VERIFY(m_BoundD3D11IndexDataStartOffset == Offset, "Index buffer offset mismatch detected");
        }
    }

    void DeviceContextD3D11Impl::dbgVerifyVertexBuffers()
    {
        const Uint32 MaxVBs = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
        ID3D11Buffer *pVBs[MaxVBs];
        UINT Strides[MaxVBs];
        UINT Offsets[MaxVBs];
        m_pd3d11DeviceContext->IAGetVertexBuffers( 0, MaxVBs, pVBs, Strides, Offsets );
        auto NumBoundVBs = m_BoundD3D11VertexBuffers.size();
        VERIFY(m_BoundD3D11VBStrides.size() == NumBoundVBs, "Unexpected bound D3D11 VB stride array size");
        VERIFY(m_BoundD3D11VBOffsets.size() == NumBoundVBs, "Unexpected bound D3D11 VB offset array size");
        for( Uint32 Slot = 0; Slot < MaxVBs; ++Slot )
        {
            if( Slot < NumBoundVBs )
            {
                const auto &BoundD3D11VB = m_BoundD3D11VertexBuffers[Slot];
                auto BoundVBStride = m_BoundD3D11VBStrides[Slot];
                auto BoundVBOffset = m_BoundD3D11VBOffsets[Slot];
                if(BoundD3D11VB && !pVBs[Slot] )
                    VERIFY( pVBs[Slot] == nullptr, "Missing D3D11 buffer detected at slot ", Slot );
                if(!BoundD3D11VB && pVBs[Slot] )
                    VERIFY( pVBs[Slot] == nullptr, "Unexpected D3D11 buffer detected at slot ", Slot );
                if( BoundD3D11VB && pVBs[Slot] )
                {
                    VERIFY( BoundD3D11VB == pVBs[Slot], "Vertex buffer mismatch detected at slot ", Slot );
                    VERIFY( BoundVBOffset == Offsets[Slot], "Offset mismatch detected at slot ", Slot );
                    VERIFY( BoundVBStride == Strides[Slot], "Stride mismatch detected at slot ", Slot );
                }
            }
            else
            {
                VERIFY( pVBs[Slot] == nullptr, "Unexpected D3D11 buffer detected at slot ", Slot );
            }

            if( pVBs[Slot] )
                pVBs[Slot]->Release();
        }
    }

    template<typename TD3D11ShaderType, typename TGetShaderMethodType>
    void dbgVerifyShadersHelper(SHADER_TYPE ShaderType,
                                const CComPtr<ID3D11DeviceChild> BoundD3DShaders[], 
                                ID3D11DeviceContext *pCtx,
                                TGetShaderMethodType GetShaderMethod)
    {
        RefCntAutoPtr<TD3D11ShaderType> pctxShader;
        (pCtx->*GetShaderMethod)(&pctxShader, nullptr, nullptr);
        const auto &BoundShader = BoundD3DShaders[GetShaderTypeIndex( ShaderType )];
        VERIFY( BoundShader == pctxShader, GetShaderTypeLiteralName(ShaderType), " binding mismatch detected" );
    }
    void DeviceContextD3D11Impl::dbgVerifyShaders()
    {
#define VERIFY_SHADER(NAME, Name, N) dbgVerifyShadersHelper<ID3D11##Name##Shader>(SHADER_TYPE_##NAME, m_BoundD3DShaders, m_pd3d11DeviceContext, &ID3D11DeviceContext::N##SGetShader )
        // These shaders which are not set will be unbound from the D3D11 device context
        VERIFY_SHADER( VERTEX,   Vertex,   V );
        VERIFY_SHADER( PIXEL,    Pixel,    P );
        VERIFY_SHADER( GEOMETRY, Geometry, G );
        VERIFY_SHADER( DOMAIN,   Domain,   D );
        VERIFY_SHADER( HULL,     Hull,     H );
        VERIFY_SHADER( COMPUTE,  Compute,  C );
    }

#endif

}
