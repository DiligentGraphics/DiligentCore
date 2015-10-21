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

#include <D3Dcompiler.h>

#include "ShaderD3D11Impl.h"
#include "RenderDeviceD3D11Impl.h"
#include "StringTools.h"
#include "ResourceMapping.h"
#include "BufferD3D11Impl.h"
#include "GraphicsUtilities.h"
#include "TextureViewD3D11Impl.h"
#include "SamplerD3D11Impl.h"
#include "BufferViewD3D11Impl.h"
#include "D3D11DebugUtilities.h"
#include "DataBlobImpl.h"

using namespace Diligent;

namespace Diligent
{
const String ShaderD3D11Impl::m_SamplerSuffix = "_sampler";
const UINT InvalidBindPoint = std::numeric_limits<UINT>::max();

static const Char* g_HLSLDefinitions = 
{
    #include "HLSLDefinitions_inc.fxh"
};


class D3DIncludeImpl : public ID3DInclude
{
public:
    D3DIncludeImpl(IShaderSourceInputStreamFactory *pStreamFactory) : 
        m_pStreamFactory(pStreamFactory)
    {

    }

    STDMETHOD( Open )(THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
    {
        RefCntAutoPtr<IFileStream> pSourceStream;
        m_pStreamFactory->CreateInputStream( pFileName, &pSourceStream );
        if( pSourceStream == nullptr )
        {
            LOG_ERROR( "Failed to open shader include file ", pFileName, ". Check that the file exists" );
            return E_FAIL;
        }

        RefCntAutoPtr<Diligent::IDataBlob> pFileData( new Diligent::DataBlobImpl );
        pSourceStream->Read( pFileData );
        *ppData = pFileData->GetDataPtr();
        *pBytes = static_cast<UINT>( pFileData->GetSize() );

        m_DataBlobs.insert( std::make_pair(*ppData, pFileData) );

        return S_OK;
    }

    STDMETHOD( Close )(THIS_ LPCVOID pData)
    {
        m_DataBlobs.erase( pData );
        return S_OK;
    }

private:
    IShaderSourceInputStreamFactory *m_pStreamFactory;
    std::unordered_map< LPCVOID, RefCntAutoPtr<Diligent::IDataBlob> > m_DataBlobs;
};

static
HRESULT CompileShader( const char* Source,
                       LPCSTR strFunctionName,
                       const D3D_SHADER_MACRO* pDefines, 
                       IShaderSourceInputStreamFactory *pIncludeStreamFactory,
                       LPCSTR profile, 
                       ID3DBlob **ppBlobOut )
{
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#else
    // Warning: do not use this flag as it causes shader compiler to fail the compilation and 
    // report strange errors:
    // dwShaderFlags |= D3D10_SHADER_OPTIMIZATION_LEVEL3;
#endif
	HRESULT hr;
	do
	{
        CComPtr<ID3DBlob> errors;
        auto SourceLen = strlen(Source);
         
        D3DIncludeImpl IncludeImpl(pIncludeStreamFactory);
        hr = D3DCompile( Source, SourceLen, NULL, pDefines, &IncludeImpl, strFunctionName, profile, dwShaderFlags, 0, ppBlobOut, &errors );
       
		if( FAILED(hr) || errors )
		{
            std::wstringstream errorss;
            ComErrorDesc ErrDesc(hr);
            if( FAILED(hr) )
                Diligent::FormatMsg( errorss, "Failed to compile shader\n" );
            else
                Diligent::FormatMsg( errorss, "Shader compiler output:\n" );
            Diligent::FormatMsg( errorss, ErrDesc.Get(), "\n" );
            if( errors )
                Diligent::FormatMsg( errorss, (char*)errors->GetBufferPointer() );
            auto ErrorDesc = errorss.str();
            OutputDebugString( ErrorDesc.c_str() );
			if( FAILED(hr) 
#ifdef PLATFORM_WINDOWS
                && IDRETRY != MessageBox( NULL, ErrorDesc.c_str() , L"FX Error", MB_ICONERROR | (Source == nullptr ? MB_ABORTRETRYIGNORE : 0) ) 
#endif
                )
			{
				break;
			}
		}
	} while( FAILED(hr) );
	return hr;
}

const char* DXShaderProfileToString(SHADER_PROFILE DXProfile)
{
    return "5_0";
    //switch(DXProfile)
    //{
    //    case DX_SHADER_PROFILE_4_0: return "4_0";
    //    case DX_SHADER_PROFILE_5_0: return "5_0";
    //    default: UNEXPECTED("Unknown DirectX shader profile" ); return "";
    //}
}

void ShaderD3D11Impl::LoadShaderResources()
{
    CComPtr<ID3D11ShaderReflection> pShaderReflection;
    CHECK_D3D_RESULT_THROW( D3DReflect( m_pShaderByteCode->GetBufferPointer(), m_pShaderByteCode->GetBufferSize(), __uuidof(pShaderReflection), reinterpret_cast<void**>(static_cast<ID3D11ShaderReflection**>(&pShaderReflection)) ),
                            "Failed to get the shader reflection" );

    struct ResourceBindInfo
    {
        String Name;
        UINT BindPoint;
    };

    std::vector<ResourceBindInfo> Textures;
    std::unordered_map<String, ResourceBindInfo> Samplers;

    D3D11_SHADER_DESC shaderDesc;
    memset( &shaderDesc, 0, sizeof(shaderDesc) );
    pShaderReflection->GetDesc( &shaderDesc );
    for( UINT Res = 0; Res < shaderDesc.BoundResources; ++Res )
    {
        D3D11_SHADER_INPUT_BIND_DESC BindingDesc;
        memset( &BindingDesc, 0, sizeof( BindingDesc ) );
        pShaderReflection->GetResourceBindingDesc( Res, &BindingDesc );

        switch( BindingDesc.Type )
        {
            case D3D_SIT_CBUFFER:
            {
                ConstBuffBindInfo NewCBInfo(this, BindingDesc.Name, BindingDesc.BindPoint);
#ifdef _DEBUG
                std::for_each( m_ConstanBuffers.begin(), m_ConstanBuffers.end(), 
                               [&]( const ConstBuffBindInfo &CBInfo )
                               {
                                   VERIFY( CBInfo.Name != BindingDesc.Name, "Constant buffer with the same name already exists" );
                               }
                );
#endif
                m_ConstanBuffers.emplace_back( NewCBInfo );
                break;
            }
            
            case D3D_SIT_TBUFFER:
            {
                UNSUPPORTED( "TBuffers are not supported" );
                break;
            }

            case D3D_SIT_TEXTURE:
            {
                
                if( BindingDesc.Dimension == D3D_SRV_DIMENSION_BUFFER )
                {
                    m_BuffSRVs.emplace_back( BuffSRVBindInfo(this, BindingDesc.Name, BindingDesc.BindPoint) );
                }
                else
                {
                    ResourceBindInfo NewTextureInfo = { BindingDesc.Name, BindingDesc.BindPoint };
                    Textures.emplace_back( NewTextureInfo );
                }
                break;
            }

            case D3D_SIT_SAMPLER:
            {
                ResourceBindInfo NewSamplerInfo = { BindingDesc.Name, BindingDesc.BindPoint };
                Samplers.emplace( std::make_pair( BindingDesc.Name, NewSamplerInfo ) );
                break;
            }

            case D3D_SIT_UAV_RWTYPED:
            {
                if( BindingDesc.Dimension == D3D_SRV_DIMENSION_BUFFER )
                {
                    m_BuffUAVs.push_back( BuffUAVBindInfo( this, BindingDesc.Name, BindingDesc.BindPoint, BindingDesc.Type ) );
                }
                else
                {
                    m_TexUAVs.push_back( TexUAVBindInfo( this, BindingDesc.Name, BindingDesc.BindPoint, BindingDesc.Type ) );
                }
                break;
            }

            case D3D_SIT_STRUCTURED:
            {
                UNSUPPORTED( "Structured buffers are not supported" );
                break;
            }

            case D3D_SIT_UAV_RWSTRUCTURED:
            {
                m_BuffUAVs.push_back( BuffUAVBindInfo( this, BindingDesc.Name, BindingDesc.BindPoint, BindingDesc.Type ) );
                break;
            }

            case D3D_SIT_BYTEADDRESS:
            {
                UNSUPPORTED( "Byte address buffers are not supported" );
                break;
            }

            case D3D_SIT_UAV_RWBYTEADDRESS:
            {
                m_BuffUAVs.push_back( BuffUAVBindInfo(this, BindingDesc.Name, BindingDesc.BindPoint, BindingDesc.Type) );
                break;
            }

            case D3D_SIT_UAV_APPEND_STRUCTURED:
            {
                UNSUPPORTED( "Append structured buffers are not supported" );
                break;
            }

            case D3D_SIT_UAV_CONSUME_STRUCTURED:
            {
                UNSUPPORTED( "Consume structured buffers are not supported" );
                break;
            }

            case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
            {
                UNSUPPORTED( "RW structured buffers with counter are not supported" );
                break;
            }
        }
    }

    // Merge texture and sampler list
    for( auto it = Textures.begin(); it != Textures.end(); ++it )
    {
        auto SamplerName = it->Name + m_SamplerSuffix;
        auto SamplerIt = Samplers.find(SamplerName);
        UINT SamplerBindPoint = InvalidBindPoint;
        if( SamplerIt != Samplers.end() )
        {
            VERIFY( SamplerIt->second.Name == SamplerName, "Unexpected sampler name" );
            SamplerBindPoint = SamplerIt->second.BindPoint;
        }
        else
        {
            SamplerName = "";
        }
        TexAndSamplerBindInfo TexAndSamplerInfo(this, it->Name, it->BindPoint, SamplerName, SamplerBindPoint);
        m_TexAndSamplers.emplace_back( TexAndSamplerInfo );
    }

    // After all resources are loaded, we can populate shader variable hash map.
    // The map contains raw pointers, but none of the arrays will ever change.
#define STORE_SHADER_VARIABLES(ResArr)\
    {                                                                       \
        for( auto it = ResArr.begin(); it != ResArr.end(); ++it )           \
            /* HashMapStringKey will make a copy of the string*/            \
            m_VariableHash.insert( std::make_pair( Diligent::HashMapStringKey(it->GetName()), &*it ) ); \
    }

    STORE_SHADER_VARIABLES(m_ConstanBuffers)
    STORE_SHADER_VARIABLES(m_TexAndSamplers)
    STORE_SHADER_VARIABLES(m_TexUAVs)
    STORE_SHADER_VARIABLES(m_BuffUAVs)
    STORE_SHADER_VARIABLES(m_BuffSRVs)

#undef STORE_SHADER_VARIABLES
}

ShaderD3D11Impl::ShaderD3D11Impl(RenderDeviceD3D11Impl *pRenderDeviceD3D11, const ShaderCreationAttribs &ShaderCreationAttribs) : 
    TShaderBase(pRenderDeviceD3D11, ShaderCreationAttribs.Desc)
{
    std::string strShaderProfile;
    switch(ShaderCreationAttribs.Desc.ShaderType)
    {
        case SHADER_TYPE_VERTEX:  strShaderProfile="vs"; break;
        case SHADER_TYPE_PIXEL:   strShaderProfile="ps"; break;
        case SHADER_TYPE_GEOMETRY:strShaderProfile="gs"; break;
        case SHADER_TYPE_HULL:    strShaderProfile="hs"; break;
        case SHADER_TYPE_DOMAIN:  strShaderProfile="ds"; break;
        case SHADER_TYPE_COMPUTE: strShaderProfile="cs"; break;

        default: UNEXPECTED( "Unknown shader type" );
    }
    strShaderProfile += "_";
    auto *pProfileSuffix = DXShaderProfileToString(ShaderCreationAttribs.Desc.TargetProfile);
    strShaderProfile += pProfileSuffix;

    String ShaderSource(g_HLSLDefinitions);
    if( ShaderCreationAttribs.Source )
        ShaderSource.append( ShaderCreationAttribs.Source );
    else
    {
        VERIFY(ShaderCreationAttribs.pShaderSourceStreamFactory, "Input stream factory is null");
        RefCntAutoPtr<IFileStream> pSourceStream;
        ShaderCreationAttribs.pShaderSourceStreamFactory->CreateInputStream( ShaderCreationAttribs.FilePath, &pSourceStream );
        RefCntAutoPtr<Diligent::IDataBlob> pFileData( new Diligent::DataBlobImpl );
        pSourceStream->Read( pFileData );
        // Null terminator is not read from the stream!
        auto* FileDataPtr = reinterpret_cast<Char*>( pFileData->GetDataPtr() );
        auto Size = pFileData->GetSize();
        ShaderSource.append( FileDataPtr, FileDataPtr + Size/sizeof(*FileDataPtr) );
    }

    const D3D_SHADER_MACRO *pDefines = nullptr;
    std::vector<D3D_SHADER_MACRO> D3DMacros;
    if( ShaderCreationAttribs.Macros )
    {
        for( auto* pCurrMacro = ShaderCreationAttribs.Macros; pCurrMacro->Name && pCurrMacro->Definition; ++pCurrMacro )
        {
            D3DMacros.push_back( {pCurrMacro->Name, pCurrMacro->Definition} );
        }
        D3DMacros.push_back( {nullptr, nullptr} );
        pDefines = D3DMacros.data();
    }

    CHECK_D3D_RESULT_THROW( CompileShader( ShaderSource.c_str(), ShaderCreationAttribs.EntryPoint, pDefines, ShaderCreationAttribs.pShaderSourceStreamFactory, strShaderProfile.c_str(), &m_pShaderByteCode ),
                            "Failed to compile the shader");

    auto *pDeviceD3D11 = pRenderDeviceD3D11->GetD3D11Device();
    switch(ShaderCreationAttribs.Desc.ShaderType)
    {

#define CREATE_SHADER(SHADER_NAME, ShaderName)\
        case SHADER_TYPE_##SHADER_NAME:         \
        {                                       \
            ID3D11##ShaderName##Shader *pShader;    \
            HRESULT hr = pDeviceD3D11->Create##ShaderName##Shader( m_pShaderByteCode->GetBufferPointer(), m_pShaderByteCode->GetBufferSize(), NULL, &pShader ); \
            CHECK_D3D_RESULT_THROW( hr, "Failed to create D3D11 shader" );      \
            if( SUCCEEDED(hr) )                     \
            {                                       \
                pShader->QueryInterface( __uuidof(ID3D11DeviceChild), reinterpret_cast<void**>( static_cast<ID3D11DeviceChild**>(&m_pShader) ) ); \
                pShader->Release();                 \
            }                                       \
            break;                                  \
        }

        CREATE_SHADER(VERTEX,   Vertex)
        CREATE_SHADER(PIXEL,    Pixel)
        CREATE_SHADER(GEOMETRY, Geometry)
        CREATE_SHADER(DOMAIN,   Domain)
        CREATE_SHADER(HULL,     Hull)
        CREATE_SHADER(COMPUTE,  Compute)

        default: UNEXPECTED( "Unknown shader type" );
    }
    
    if(!m_pShader)
        LOG_ERROR_AND_THROW("Failed to create the shader from the byte code");

    LoadShaderResources();

    // Byte code is only required for the vertex shader to create input layout
    if( ShaderCreationAttribs.Desc.ShaderType != SHADER_TYPE_VERTEX )
        m_pShaderByteCode.Release();
}

ShaderD3D11Impl::~ShaderD3D11Impl()
{
}

#define LOG_RESORUCE_BINDING_ERROR(ResType, pResource, VarName, ShaderName, ...)\
{                                                                                                   \
    const auto &ResName = pResource->GetDesc().Name;                                                \
    LOG_ERROR_MESSAGE( "Failed to bind ", ResType, " \"", ResName, "\" to variable \"", VarName,    \
                        "\" in shader \"", ShaderName, "\". ", __VA_ARGS__ );                \
}

void ShaderD3D11Impl::BindConstantBuffers( IResourceMapping* pResourceMapping, Uint32 Flags )
{
    if( !pResourceMapping )
    {
        LOG_ERROR_MESSAGE( "Failed to bind constant buffers in shader \"", m_Desc.Name ? m_Desc.Name : "", "\": resource mapping is null" );
        return;
    }
    // Bind constant buffers
    if( Flags & BIND_SHADER_RESOURCES_RESET_BINDINGS )
    {
        m_BoundCBs.clear();
    }
    for( auto it = m_ConstanBuffers.begin(); it != m_ConstanBuffers.end(); ++it )
    {
        if( (Flags & BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED) && it->IsBound() )
            continue;
        
        RefCntAutoPtr<IDeviceObject> pBuffer;
        if( pResourceMapping )
            pResourceMapping->GetResource( it->Name.c_str(), &pBuffer );
        if( pBuffer )
        {
            it->Set( pBuffer );
        }
        else
        {
            if( (Flags & BIND_SHADER_RESOURCES_ALL_RESOLVED) && !it->IsBound() )
                LOG_ERROR_MESSAGE("Cannot bind resource to shader variable \"", it->Name, "\": resource not found in the resource mapping")
            continue;
        }
    }
}


void ShaderD3D11Impl::ConstBuffBindInfo::Set( IDeviceObject *pBuffer )
{
    auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>( m_pShader );
    auto &BoundCBs = pShaderD3D11->m_BoundCBs;
    if( BoundCBs.size() <= BindPoint )
        BoundCBs.resize( BindPoint + 1 );

    if( pBuffer )
    {
        RefCntAutoPtr<IBufferD3D11> pBuffD3D11;
        pBuffer->QueryInterface( IID_BufferD3D11, reinterpret_cast<IObject**>(static_cast<IBufferD3D11**>(&pBuffD3D11)) );
        if( pBuffD3D11 )
        {
            if( pBuffD3D11->GetDesc().BindFlags & BIND_UNIFORM_BUFFER )
            {
                BoundCBs[BindPoint].pd3d11Buff = pBuffD3D11->GetD3D11Buffer();
                VERIFY(BoundCBs[BindPoint].pd3d11Buff, "No relevant D3D11 buffer")
                BoundCBs[BindPoint].pBuff = pBuffD3D11;
            }
            else
            {
                LOG_RESORUCE_BINDING_ERROR("buffer", pBuffer, Name, pShaderD3D11->m_Desc.Name, "Buffer was not created with BIND_UNIFORM_BUFFER flag.")
            }
        }
        else
        {
            LOG_RESORUCE_BINDING_ERROR("buffer", pBuffer, Name, pShaderD3D11->m_Desc.Name, "Incorrect resource type: buffer is expected.")
        }
    }
    else
    {
        BoundCBs[BindPoint] = BoundCB();
    }
}

bool ShaderD3D11Impl::ConstBuffBindInfo::IsBound()
{
    auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>( m_pShader );
    auto &BoundCBs = pShaderD3D11->m_BoundCBs;
    if( BindPoint < BoundCBs.size() && BoundCBs[BindPoint].pd3d11Buff != nullptr )
    {
        VERIFY(BoundCBs[BindPoint].pBuff != nullptr, "No relevant buffer resource")
        return true;
    }

    return false;
}

template<typename TResourceViewType>
struct ResourceViewTraits{};

template<>
struct ResourceViewTraits<ITextureViewD3D11>
{
    static const Char *Name;
};
const Char *ResourceViewTraits<ITextureViewD3D11>::Name = "texture view";

template<>
struct ResourceViewTraits<IBufferViewD3D11>
{
    static const Char *Name;
};
const Char *ResourceViewTraits<IBufferViewD3D11>::Name = "buffer view";

// Helper template class that facilitates binding SRVs and UAVs
class BindViewHelper
{
public:
    BindViewHelper(IResourceMapping *pRM, Uint32 Fl) :
        pResourceMapping(pRM),
        Flags(Fl)
    {}

    template<typename IterType>
    void Bind( IterType &pShaderVar)
    {
        if( (Flags & BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED) && pShaderVar->IsBound() )
            return;

        const auto& VarName = pShaderVar->GetName();
        RefCntAutoPtr<IDeviceObject> pView;
        if( pResourceMapping )
            pResourceMapping->GetResource( VarName.c_str(), &pView );
        if( pView )
        {
            pShaderVar->Set(pView);
        }
        else
        {
            if( (Flags & BIND_SHADER_RESOURCES_ALL_RESOLVED) && !pShaderVar->IsBound() )
                LOG_ERROR_MESSAGE( "Cannot bind resource to shader variable \"", VarName, "\": resource view not found in the resource mapping" )
            return;
        }
    }

private:
    IResourceMapping* const pResourceMapping;
    const Uint32 Flags;
};

// Helper template function to facilitate setting shader variables
// of different types
template<typename TResourceViewType, ///< Type of the view (ITextureViewD3D11 or IBufferViewD3D11)
         typename TD3D11ViewType,    ///< Type of the D3D11 view (ID3D11ShaderResourceView or ID3D11UnorderedAccessView)
         typename TBoundViewType,    ///< Type of the bound view data array element (BoundSRV or BoundUAV)
         typename TGetResourceMethodType,   ///< Type of the method to get resource from the view (ITextureViewD3D11::GetTexture or IBufferViewD3D11::GetBuffer)
         typename TBindSamplerProcType, ///< Type of the procedure to set samplers
         typename TViewTypeEnum>    ///< Type of the expected view enum
void SetShaderVariableHelper(IDeviceObject *pView,
                             std::vector<TBoundViewType> &ViewArray,
                             UINT BindPoint, 
                             const String& VarName, 
                             TGetResourceMethodType GetResourceMethod, 
                             const Diligent::INTERFACE_ID ViewID,
                             typename TViewTypeEnum ExpectedViewType,
                             const String& ShaderName,
                             TBindSamplerProcType BindSamplerProc)
{
    if( ViewArray.size() <= BindPoint )
        ViewArray.resize( BindPoint + 1 );
    if( pView )
    {
        RefCntAutoPtr<TResourceViewType> pViewD3D11;
        pView->QueryInterface( ViewID, reinterpret_cast<IObject**>(static_cast<TResourceViewType**>(&pViewD3D11)) );
        if( pViewD3D11 )
        {
            auto ViewType = pViewD3D11->GetDesc().ViewType;
            if( ViewType == ExpectedViewType )
            {
                auto &BoundView = ViewArray[BindPoint];
                BoundView.pd3d11View = static_cast<TD3D11ViewType*>( pViewD3D11->GetD3D11View() );
                VERIFY(BoundView.pd3d11View, "No relevant D3D11 view")
                BoundView.pView = pViewD3D11;
                BoundView.pResource = (pViewD3D11->*GetResourceMethod)();
                VERIFY(BoundView.pResource, "No relevant resource")
                BindSamplerProc(pViewD3D11);
            }
            else
            {
                const auto *ExpectedViewTypeName = GetViewTypeLiteralName( ExpectedViewType );
                const auto *ActualViewTypeName = GetViewTypeLiteralName( ViewType );
                LOG_RESORUCE_BINDING_ERROR(ResourceViewTraits<TResourceViewType>::Name, pViewD3D11, VarName, ShaderName, 
                                            "Incorrect view type: ", ExpectedViewTypeName, " is expected, ", ActualViewTypeName, " provided." );
            }
        }
        else
        {
            LOG_RESORUCE_BINDING_ERROR("resource", pView, VarName, ShaderName, "Incorect resource type: ", ResourceViewTraits<TResourceViewType>::Name, " is expected.")
        }   
    } 
    else
    {
        ViewArray[BindPoint] = TBoundViewType();
        BindSamplerProc(nullptr);
    }
}

void ShaderD3D11Impl::TexAndSamplerBindInfo::Set( IDeviceObject *pView )
{
    auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>( m_pShader );
    SetShaderVariableHelper<ITextureViewD3D11, ID3D11ShaderResourceView>(pView, pShaderD3D11->m_BoundSRVs, BindPoint, Name,
                            &ITextureViewD3D11::GetTexture, IID_TextureViewD3D11, TEXTURE_VIEW_SHADER_RESOURCE,
                            pShaderD3D11->m_Desc.Name,
                            [&]( ITextureViewD3D11 *pTexViewD3D11 )
                            {
                                if( SamplerBindPoint != InvalidBindPoint )
                                {
                                    auto &BoundSamplers = pShaderD3D11->m_BoundSamplers;
                                    if( BoundSamplers.size() <= SamplerBindPoint )
                                        BoundSamplers.resize( SamplerBindPoint + 1 );
                                    auto &BndSam = BoundSamplers[SamplerBindPoint];
                                    if( pTexViewD3D11 )
                                    {
                                        auto pSampler = pTexViewD3D11->GetSampler();
                                        if( pSampler )
                                        {
                                            BndSam.pd3d11Sampler = ValidatedCast<SamplerD3D11Impl>(pSampler)->m_pd3dSampler;
                                            VERIFY(BndSam.pd3d11Sampler, "No relevant D3D11 sampler")
                                            BndSam.pSampler = pSampler;
                                        }
                                        else
                                        {
                                            LOG_ERROR_MESSAGE( "Failed to bind sampler to variable \"", SamplerName, ". Sampler is not set in the texture view \"", pTexViewD3D11->GetDesc().Name, "\"" );
                                        }
                                    }
                                    else
                                    {
                                        BndSam = BoundSampler();
                                    }
                                }          
                            } 
                            );

}

void ShaderD3D11Impl::BuffSRVBindInfo::Set( IDeviceObject *pView )
{
    auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>( m_pShader );
    SetShaderVariableHelper<IBufferViewD3D11, ID3D11ShaderResourceView>(pView, pShaderD3D11->m_BoundSRVs, BindPoint, Name,
                            &IBufferViewD3D11::GetBuffer, IID_BufferViewD3D11, BUFFER_VIEW_SHADER_RESOURCE,
                            pShaderD3D11->m_Desc.Name,
                            []( IBufferViewD3D11 * ){} 
                            );
}

void ShaderD3D11Impl::TexUAVBindInfo::Set( IDeviceObject *pView )
{
    auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>( m_pShader );
    SetShaderVariableHelper<ITextureViewD3D11, ID3D11UnorderedAccessView>(
                            pView, pShaderD3D11->m_BoundUAVs, BindPoint, Name,
                            &ITextureViewD3D11::GetTexture, IID_TextureViewD3D11, TEXTURE_VIEW_UNORDERED_ACCESS,
                            pShaderD3D11->m_Desc.Name,
                            []( ITextureViewD3D11 * ){} 
                            );
}

void ShaderD3D11Impl::BuffUAVBindInfo::Set( IDeviceObject *pView )
{
    auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>( m_pShader );
    SetShaderVariableHelper<IBufferViewD3D11, ID3D11UnorderedAccessView>(pView, pShaderD3D11->m_BoundUAVs, BindPoint, Name,
                            &IBufferViewD3D11::GetBuffer, IID_BufferViewD3D11, BUFFER_VIEW_UNORDERED_ACCESS,
                            pShaderD3D11->m_Desc.Name,
                            []( IBufferViewD3D11 * ){} 
                            );
}

template<typename TBoundViewType>
bool CheckBoundResource(const std::vector<TBoundViewType> &ViewArray, UINT BindPoint)
{
    if( BindPoint < ViewArray.size() && ViewArray[BindPoint].pd3d11View != nullptr )
    {
        VERIFY(ViewArray[BindPoint].pResource != nullptr, "No relevant resource")
        VERIFY(ViewArray[BindPoint].pView != nullptr, "No relevant resource view")
        return true;
    }
    return false;
}

bool ShaderD3D11Impl::TexAndSamplerBindInfo::IsBound()
{
    auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>( m_pShader );
    return CheckBoundResource( pShaderD3D11->m_BoundSRVs, BindPoint );
}

bool ShaderD3D11Impl::BuffSRVBindInfo::IsBound()
{
    auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>( m_pShader );
    return CheckBoundResource( pShaderD3D11->m_BoundSRVs, BindPoint );
}

bool ShaderD3D11Impl::UAVBindInfoBase::IsBound()
{
    auto *pShaderD3D11 = ValidatedCast<ShaderD3D11Impl>( m_pShader );
    return CheckBoundResource( pShaderD3D11->m_BoundUAVs, BindPoint );
}

void ShaderD3D11Impl::BindSRVsAndSamplers(IResourceMapping* pResourceMapping, Uint32 Flags )
{
    if( !pResourceMapping )
    {
        LOG_ERROR_MESSAGE( "Failed to bind SRVs and samplers in shader \"", m_Desc.Name ? m_Desc.Name : "", "\": resource mapping is null" );
        return;
    }

    if( Flags & BIND_SHADER_RESOURCES_RESET_BINDINGS )
    {
        m_BoundSRVs.clear();
        m_BoundSamplers.clear();
    }
    
    BindViewHelper BindSRVHelper(pResourceMapping, Flags);

    for( auto it = m_TexAndSamplers.begin(); it != m_TexAndSamplers.end(); ++it )
    {
        BindSRVHelper.Bind(it);
    }

    for( auto it = m_BuffSRVs.begin(); it != m_BuffSRVs.end(); ++it )
    {
        BindSRVHelper.Bind(it);
    }
}

void ShaderD3D11Impl::BindUAVs( IResourceMapping* pResourceMapping, Uint32 Flags )
{
    if( !pResourceMapping )
    {
        LOG_ERROR_MESSAGE( "Failed to bind UAVs in shader \"", m_Desc.Name ? m_Desc.Name : "", "\": resource mapping is null" );
        return;
    }

    // Bind constant buffers
    if( Flags & BIND_SHADER_RESOURCES_RESET_BINDINGS )
    {
        m_BoundUAVs.clear();
    }

    BindViewHelper BindUAVHelper(pResourceMapping, Flags);

    for( auto it = m_TexUAVs.begin(); it != m_TexUAVs.end(); ++it )
    {
        BindUAVHelper.Bind(it);
    }

    for( auto it = m_BuffUAVs.begin(); it != m_BuffUAVs.end(); ++it )
    {
        BindUAVHelper.Bind(it);
    }
}

void ShaderD3D11Impl::BindResources( IResourceMapping* pResourceMapping, Uint32 Flags  )
{
    if( !pResourceMapping )
    {
        LOG_ERROR_MESSAGE( "Failed to bind resources in shader \"", m_Desc.Name ? m_Desc.Name : "", "\": resource mapping is null" );
        return;
    }

    BindConstantBuffers( pResourceMapping, Flags );
    BindSRVsAndSamplers( pResourceMapping, Flags );
    BindUAVs(pResourceMapping, Flags );
}

IShaderVariable* ShaderD3D11Impl::GetShaderVariable( const Char* Name )
{
    // Name will be implicitly converted to HashMapStringKey without making a copy
    auto it = m_VariableHash.find( Name );
    if( it == m_VariableHash.end() )
    {
        LOG_ERROR_MESSAGE( "Shader variable \"", Name, "\" is not found in shader \"", m_Desc.Name ? m_Desc.Name : "", "\". Attempts to set the variable will be silently ignored." );
        return &m_DummyShaderVar;
    }
    return it->second;
}



IMPLEMENT_QUERY_INTERFACE( ShaderD3D11Impl, IID_ShaderD3D11, TShaderBase )

#ifdef VERIFY_SHADER_BINDINGS
void ShaderD3D11Impl::dbgVerifyBindings()
{
    const auto *ShaderName = m_Desc.Name ? m_Desc.Name : "";
#define LOG_MISSING_BINDING(VarType, Name)\
    LOG_ERROR_MESSAGE( "No resource is bound to ", VarType, " variable \"", Name, "\" in shader \"", ShaderName, "\"" );

    for( auto cb = m_ConstanBuffers.begin(); cb != m_ConstanBuffers.end(); ++cb )
    {
        if( m_BoundCBs.size() <= cb->BindPoint || m_BoundCBs[cb->BindPoint].pd3d11Buff == nullptr )
            LOG_MISSING_BINDING("constant buffer", cb->Name)
    }

    for( auto tex = m_TexAndSamplers.begin(); tex != m_TexAndSamplers.end(); ++tex )
    {
        if( m_BoundSRVs.size() <= tex->BindPoint || m_BoundSRVs[tex->BindPoint].pd3d11View == nullptr )
            LOG_MISSING_BINDING("texture", tex->Name)

        if( tex->SamplerBindPoint != InvalidBindPoint && 
            (m_BoundSamplers.size() <= tex->SamplerBindPoint || m_BoundSamplers[tex->SamplerBindPoint].pd3d11Sampler == nullptr) )
            LOG_MISSING_BINDING("sampler", tex->SamplerName)
    }

    for( auto buf = m_BuffSRVs.begin(); buf != m_BuffSRVs.end(); ++buf )
    {
        if( m_BoundSRVs.size() <= buf->BindPoint || m_BoundSRVs[buf->BindPoint].pd3d11View == nullptr )
            LOG_MISSING_BINDING("buffer", buf->Name)
    }

    for( auto uav = m_TexUAVs.begin(); uav != m_TexUAVs.end(); ++uav )
    {
        if( m_BoundUAVs.size() <= uav->BindPoint || m_BoundUAVs[uav->BindPoint].pd3d11View == nullptr )
            LOG_MISSING_BINDING("texture UAV", uav->Name)
    }

    for( auto uav = m_BuffUAVs.begin(); uav != m_BuffUAVs.end(); ++uav )
    {
        if( m_BoundUAVs.size() <= uav->BindPoint || m_BoundUAVs[uav->BindPoint].pd3d11View == nullptr )
            LOG_MISSING_BINDING("buffer UAV", uav->Name)
    }
}
#endif

}
