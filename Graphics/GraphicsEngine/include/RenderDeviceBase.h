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
/// Implementation of the Diligent::RenderDeviceBase template class and related structures

#include "RenderDevice.h"
#include "DeviceObjectBase.h"
#include "Defines.h"
#include "ResourceMappingImpl.h"
#include "StateObjectsRegistry.h"
#include "HashUtils.h"
#include "ObjectBase.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "GraphicsUtilities.h"

using Diligent::RefCntAutoPtr;
using Diligent::RefCntWeakPtr;

namespace std
{
    /// Hash function specialization for Diligent::SamplerDesc structure.
    template<>
    struct hash<Diligent::SamplerDesc>
    {
        size_t operator()( const Diligent::SamplerDesc &SamDesc ) const
        {
                                           // Sampler name is ignored in comparison operator
                                           // and should not be hashed
            return Diligent::ComputeHash( // SamDesc.Name,
                                           static_cast<int>(SamDesc.MinFilter),
                                           static_cast<int>(SamDesc.MagFilter),
                                           static_cast<int>(SamDesc.MipFilter),
                                           static_cast<int>(SamDesc.AddressU),
                                           static_cast<int>(SamDesc.AddressV),
                                           static_cast<int>(SamDesc.AddressW),
                                           SamDesc.MipLODBias,
                                           SamDesc.MaxAnisotropy,
                                           static_cast<int>(SamDesc.ComparisonFunc),
                                           SamDesc.BorderColor[0], 
                                           SamDesc.BorderColor[1], 
                                           SamDesc.BorderColor[2], 
                                           SamDesc.BorderColor[3],
                                           SamDesc.MinLOD, SamDesc.MaxLOD );
        }
    };

    /// Hash function specialization for Diligent::StencilOpDesc structure.
    template<>
    struct hash<Diligent::StencilOpDesc>
    {
        size_t operator()( const Diligent::StencilOpDesc &StOpDesc ) const
        {
            return Diligent::ComputeHash( static_cast<int>( StOpDesc.StencilFailOp ),
                                           static_cast<int>( StOpDesc.StencilDepthFailOp ),
                                           static_cast<int>( StOpDesc.StencilPassOp ),
                                           static_cast<int>( StOpDesc.StencilFunc ) );
        }
    };

    /// Hash function specialization for Diligent::DepthStencilStateDesc structure.
    template<>
    struct hash<Diligent::DepthStencilStateDesc>
    {
        size_t operator()( const Diligent::DepthStencilStateDesc &DSSDesc ) const
        {
                                           // Depth-stencil state name is ignored in comparison operator
                                           // and should not be hashed
            return Diligent::ComputeHash( // DSSDesc.Name,
                                           DSSDesc.DepthEnable,
                                           DSSDesc.DepthWriteEnable,
                                           static_cast<int>(DSSDesc.DepthFunc),
                                           DSSDesc.StencilEnable,
                                           DSSDesc.StencilReadMask,
                                           DSSDesc.StencilWriteMask,
                                           DSSDesc.FrontFace,
                                           DSSDesc.BackFace );
        }
    };

    /// Hash function specialization for Diligent::RasterizerStateDesc structure.
    template<>
    struct hash<Diligent::RasterizerStateDesc>
    {
        size_t operator()( const Diligent::RasterizerStateDesc &RSDesc ) const
        {
                                           // Rasterizer state name is ignored in comparison operator
                                           // and should not be hashed
            return Diligent::ComputeHash( // RSDesc.Name,
                                           static_cast<int>( RSDesc.FillMode ),
                                           static_cast<int>( RSDesc.CullMode ),
                                           RSDesc.FrontCounterClockwise,
                                           RSDesc.DepthBias,
                                           RSDesc.DepthBiasClamp,
                                           RSDesc.SlopeScaledDepthBias,
                                           RSDesc.DepthClipEnable,
                                           RSDesc.ScissorEnable,
                                           RSDesc.AntialiasedLineEnable );
        }
    };

    /// Hash function specialization for Diligent::BlendStateDesc structure.
    template<>
    struct hash<Diligent::BlendStateDesc>
    {
        size_t operator()( const Diligent::BlendStateDesc &BSDesc ) const
        {
            std::size_t Seed = 0;
            for( int i = 0; i < Diligent::BlendStateDesc::MaxRenderTargets; ++i )
            {
                const auto& rt = BSDesc.RenderTargets[i];
                Diligent::HashCombine( Seed, 
                             rt.BlendEnable,
                             static_cast<int>( rt.SrcBlend ),
                             static_cast<int>( rt.DestBlend ),
                             static_cast<int>( rt.BlendOp ),
                             static_cast<int>( rt.SrcBlendAlpha ),
                             static_cast<int>( rt.DestBlendAlpha ),
                             static_cast<int>( rt.BlendOpAlpha ),
                             rt.RenderTargetWriteMask );
            }
            Diligent::HashCombine( Seed,
                                    // Blend state name is ignored in comparison operator
                                    // and should not be hashed
                                    // BSDesc.Name,
                                    BSDesc.AlphaToCoverageEnable,
                                    BSDesc.IndependentBlendEnable );
            return Seed;
        }
    };


    /// Hash function specialization for Diligent::BlendStateDesc structure.
    template<>
    struct hash<Diligent::TextureViewDesc>
    {
        size_t operator()( const Diligent::TextureViewDesc &TexViewDesc ) const
        {
            std::size_t Seed = 0;
            Diligent::HashCombine( Seed,
                                    static_cast<Diligent::Int32>(TexViewDesc.ViewType),
                                    static_cast<Diligent::Int32>(TexViewDesc.TextureType),
                                    static_cast<Diligent::Int32>(TexViewDesc.Format),
                                    TexViewDesc.MostDetailedMip,
                                    TexViewDesc.NumMipLevels,
                                    TexViewDesc.FirstArraySlice,
                                    TexViewDesc.NumArraySlices,
                                    TexViewDesc.AccessFlags );
            return Seed;
        }
    };
}

namespace Diligent
{

/// Base implementation of a render device

/// \warning
/// Render device must *NOT* hold strong references to any
/// object it creates to avoid circular dependencies.
/// Device context, swap chain and all object the device creates
/// keep strong reference to the device.
/// Device only holds weak reference to immediate context.
template<typename BaseInterface = IRenderDevice>
class RenderDeviceBase : public ObjectBase<BaseInterface>
{
public:
    RenderDeviceBase() :
        m_TextureFormatsInfo( TEX_FORMAT_NUM_FORMATS ),
        m_TexFmtInfoInitFlags( TEX_FORMAT_NUM_FORMATS ),
        m_SamplersRegistry("sampler"),
        m_DSSRegistry("ds state"),
        m_RSRegistry("rasterizer state"),
        m_BSRegistry("blend state")
    {
        // Initialize texture format info
        for( Uint32 Fmt = TEX_FORMAT_UNKNOWN; Fmt < TEX_FORMAT_NUM_FORMATS; ++Fmt )
            static_cast<TextureFormatAttribs&>(m_TextureFormatsInfo[Fmt]) = GetTextureFormatAttribs( static_cast<TEXTURE_FORMAT>(Fmt) );

        // https://msdn.microsoft.com/en-us/library/windows/desktop/ff471325(v=vs.85).aspx
        std::vector<TEXTURE_FORMAT> FilterableFormats =
        {
            TEX_FORMAT_RGBA32_FLOAT,    // OpenGL ES3.1 does not require this format to be filterable
            TEX_FORMAT_RGBA16_FLOAT,
            TEX_FORMAT_RGBA16_UNORM,
            TEX_FORMAT_RGBA16_SNORM,
            TEX_FORMAT_RG32_FLOAT,      // OpenGL ES3.1 does not require this format to be filterable
            TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS,
            //TEX_FORMAT_R10G10B10A2_UNORM,
            TEX_FORMAT_R11G11B10_FLOAT,
            TEX_FORMAT_RGBA8_UNORM,
            TEX_FORMAT_RGBA8_UNORM_SRGB,
            TEX_FORMAT_RGBA8_SNORM,
            TEX_FORMAT_RG16_FLOAT,
            TEX_FORMAT_RG16_UNORM,
            TEX_FORMAT_RG16_SNORM,
            TEX_FORMAT_R32_FLOAT,       // OpenGL ES3.1 does not require this format to be filterable
            TEX_FORMAT_R24_UNORM_X8_TYPELESS,
            TEX_FORMAT_RG8_UNORM,
            TEX_FORMAT_RG8_SNORM,
            TEX_FORMAT_R16_FLOAT,
            TEX_FORMAT_R16_UNORM,
            TEX_FORMAT_R16_SNORM,
            TEX_FORMAT_R8_UNORM,
            TEX_FORMAT_R8_SNORM,
            TEX_FORMAT_A8_UNORM,
            TEX_FORMAT_RGB9E5_SHAREDEXP,
            TEX_FORMAT_RG8_B8G8_UNORM,
            TEX_FORMAT_G8R8_G8B8_UNORM,
            TEX_FORMAT_BC1_UNORM,
            TEX_FORMAT_BC1_UNORM_SRGB,
            TEX_FORMAT_BC2_UNORM,
            TEX_FORMAT_BC2_UNORM_SRGB,
            TEX_FORMAT_BC3_UNORM,
            TEX_FORMAT_BC3_UNORM_SRGB,
            TEX_FORMAT_BC4_UNORM,
            TEX_FORMAT_BC4_SNORM,
            TEX_FORMAT_BC5_UNORM,
            TEX_FORMAT_BC5_SNORM,
            TEX_FORMAT_B5G6R5_UNORM
        };
        for( auto fmt = FilterableFormats.begin(); fmt != FilterableFormats.end(); ++fmt )
            m_TextureFormatsInfo[ *fmt ].Filterable = true;
    }

    ~RenderDeviceBase()
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_RenderDevice, ObjectBase<BaseInterface> )

    virtual void CreateResourceMapping( const ResourceMappingDesc &MappingDesc, IResourceMapping **ppMapping );
   
    virtual const DeviceCaps& GetDeviceCaps()const
    {
        return m_DeviceCaps;
    }

    virtual const TextureFormatInfo &GetTextureFormatInfo(TEXTURE_FORMAT TexFormat)
    {
        VERIFY( TexFormat >= TEX_FORMAT_UNKNOWN && TexFormat < TEX_FORMAT_NUM_FORMATS, "Texture format out of range" );
        const auto& TexFmtInfo = m_TextureFormatsInfo[TexFormat];
        VERIFY( TexFmtInfo.Format == TexFormat, "Sanity check failed" );
        return TexFmtInfo;
    }

    virtual const TextureFormatInfoExt &GetTextureFormatInfoExt( TEXTURE_FORMAT TexFormat )
    {
        VERIFY( TexFormat >= TEX_FORMAT_UNKNOWN && TexFormat < TEX_FORMAT_NUM_FORMATS, "Texture format out of range" );
        const auto& TexFmtInfo = m_TextureFormatsInfo[TexFormat];
        VERIFY( TexFmtInfo.Format == TexFormat, "Sanity check failed" );
        if( !m_TexFmtInfoInitFlags[TexFormat] )
        {
            if( TexFmtInfo.Supported )
                TestTextureFormat(TexFormat);
            m_TexFmtInfoInitFlags[TexFormat] = true;
        }
        return TexFmtInfo;
    }

    void OnCreateDeviceObject( IDeviceObject *pNewObject )
    {
    }

    StateObjectsRegistry<SamplerDesc>           &GetSamplerRegistry(){ return m_SamplersRegistry; }
    StateObjectsRegistry<DepthStencilStateDesc> &GetDSStateRegistry(){ return m_DSSRegistry;  }
    StateObjectsRegistry<RasterizerStateDesc>   &GetRSRegistry()     { return m_RSRegistry;  }
    StateObjectsRegistry<BlendStateDesc>        &GetBSRegistry()     { return m_BSRegistry;  }
    
    /// Set weak reference to the immediate context
    void SetImmediateContext(IDeviceContext *pImmediateContext)
    {
        VERIFY( m_wpImmediateContext.Lock() == nullptr, "Immediate context has already been set" );
        m_wpImmediateContext = pImmediateContext;
    }

    RefCntAutoPtr<IDeviceContext> GetImmediateContext(){ return m_wpImmediateContext.Lock(); }

protected:
    
    virtual void TestTextureFormat(TEXTURE_FORMAT TexFormat) = 0;

    /// Helper template function to facilitate device object creation
    template<typename TObjectType, typename TObjectDescType, typename TObjectConstructor>
    void CreateDeviceObject( const Char *ObjectTypeName, const TObjectDescType &Desc, TObjectType **ppObject, TObjectConstructor ConstructObject );

    DeviceCaps m_DeviceCaps;

    // All state object registries hold raw pointers.
    // This is safe because every object unregisters itself
    // when it is deleted.
    StateObjectsRegistry<SamplerDesc> m_SamplersRegistry;     ///< Sampler state registry
    StateObjectsRegistry<DepthStencilStateDesc> m_DSSRegistry;///< Depth-stencil state registry
    StateObjectsRegistry<RasterizerStateDesc> m_RSRegistry;   ///< Rasterizer state registry
    StateObjectsRegistry<BlendStateDesc> m_BSRegistry;        ///< Blend state registry
    std::vector<TextureFormatInfoExt> m_TextureFormatsInfo;
    std::vector<bool> m_TexFmtInfoInitFlags;
    
    /// Weak reference to the immediate context. Immediate context holds strong reference
    /// to the device, so we must use weak reference to avoid circular dependencies.
    Diligent::RefCntWeakPtr<IDeviceContext> m_wpImmediateContext;
};


template<typename BaseInterface>
void RenderDeviceBase<BaseInterface>::CreateResourceMapping( const ResourceMappingDesc &MappingDesc, IResourceMapping **ppMapping )
{
    VERIFY( ppMapping != nullptr, "Null pointer provided" );
    if( ppMapping == nullptr )
        return;
    VERIFY( *ppMapping == nullptr, "Overwriting reference to existing object may cause memory leaks" );

    auto *pResourceMapping( new ResourceMappingImpl() );
    pResourceMapping->QueryInterface( IID_ResourceMapping, reinterpret_cast<IObject**>(ppMapping) );
    if( MappingDesc.pEntries )
    {
        for( auto *pEntry = MappingDesc.pEntries; pEntry->Name && pEntry->pObject; ++pEntry )
        {
            (*ppMapping)->AddResource( pEntry->Name, pEntry->pObject, true );
        }
    }
}


/// \tparam TObjectType - type of the object being created (IBuffer, ITexture, IBlendState, etc.)
/// \tparam TObjectDescType - type of the object description structure (BufferDesc, TextureDesc, BlendStateDesc, etc.)
/// \tparam TObjectConstructor - type of the function that constructs the object
/// \param ObjectTypeName - string name of the object type ("buffer", "texture", etc.)
/// \param Desc - object description
/// \param ppObject - memory address where the pointer to the created object will be stored
/// \param ConstructObject - function that constructs the object
template<typename BaseInterface>
template<typename TObjectType, typename TObjectDescType, typename TObjectConstructor>
void RenderDeviceBase<BaseInterface> :: CreateDeviceObject( const Char *ObjectTypeName, const TObjectDescType &Desc, TObjectType **ppObject, TObjectConstructor ConstructObject )
{
    VERIFY( ppObject != nullptr, "Null pointer provided" );
    if(!ppObject)
        return;

    VERIFY( *ppObject == nullptr, "Overwriting reference to existing object may cause memory leaks" );
    // Do not release *ppObject here!
    // Should this happen, RefCntAutoPtr<> will take care of this!
    //if( *ppObject )
    //{
    //    (*ppObject)->Release();
    //    *ppObject = nullptr;
    //}

    *ppObject = nullptr;

    try
    {
        ConstructObject();
    }
    catch( const std::runtime_error & )
    {
        VERIFY( *ppObject == nullptr, "Object was created despite error" );
        if( *ppObject )
        {
            (*ppObject)->Release();
            *ppObject = nullptr;
        }
        auto ObjectDescString = GetObjectDescString( Desc );
        if( ObjectDescString.length() )
        {
            LOG_ERROR( "Failed to create ", ObjectTypeName, " object \"", Desc.Name ? Desc.Name : "", "\"\n", ObjectDescString )
        }
        else
        {
            LOG_ERROR( "Failed to create ", ObjectTypeName, " object \"", Desc.Name ? Desc.Name : "", "\"" )
        }
    }
}

}
