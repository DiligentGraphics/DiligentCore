/*     Copyright 2015-2017 Egor Yusov
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

#include <unordered_map>
#include <D3Dcompiler.h>

#include "D3DErrors.h"
#include "DataBlobImpl.h"
#include "RefCntAutoPtr.h"
#include <atlcomcli.h>
#include "ShaderD3DBase.h"

namespace Diligent
{

const Char* g_HLSLDefinitions = 
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

        RefCntAutoPtr<Diligent::IDataBlob> pFileData( MakeNewRCObj<Diligent::DataBlobImpl>()(0) );
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
            OutputDebugStringW( ErrorDesc.c_str() );
			if( FAILED(hr) 
#ifdef PLATFORM_WIN32
                && IDRETRY != MessageBoxW( NULL, ErrorDesc.c_str() , L"FX Error", MB_ICONERROR | (Source == nullptr ? MB_ABORTRETRYIGNORE : 0) ) 
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
    switch(DXProfile)
    {
        case SHADER_PROFILE_DX_4_0: return "4_0";
        case SHADER_PROFILE_DX_5_0: return "5_0";
        case SHADER_PROFILE_DX_5_1: return "5_1";
        //default: UNEXPECTED("Unknown DirectX shader profile" ); return "";
        default: return "5_0";
    }
}

ShaderD3DBase::ShaderD3DBase(const ShaderCreationAttribs &CreationAttribs)
{
    std::string strShaderProfile;
    switch(CreationAttribs.Desc.ShaderType)
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
    auto *pProfileSuffix = DXShaderProfileToString(CreationAttribs.Desc.TargetProfile);
    strShaderProfile += pProfileSuffix;

    String ShaderSource(g_HLSLDefinitions);
    if( CreationAttribs.Source )
        ShaderSource.append( CreationAttribs.Source );
    else
    {
        VERIFY(CreationAttribs.pShaderSourceStreamFactory, "Input stream factory is null");
        VERIFY(CreationAttribs.FilePath, "File path is null. Either shader source or source file path must be specified.");
        RefCntAutoPtr<IFileStream> pSourceStream;
        CreationAttribs.pShaderSourceStreamFactory->CreateInputStream( CreationAttribs.FilePath, &pSourceStream );
        RefCntAutoPtr<Diligent::IDataBlob> pFileData( MakeNewRCObj<Diligent::DataBlobImpl>()(0) );
        if (pSourceStream == nullptr)
            LOG_ERROR_AND_THROW("Failed to open shader source file")
        pSourceStream->Read( pFileData );
        // Null terminator is not read from the stream!
        auto* FileDataPtr = reinterpret_cast<Char*>( pFileData->GetDataPtr() );
        auto Size = pFileData->GetSize();
        ShaderSource.append( FileDataPtr, FileDataPtr + Size/sizeof(*FileDataPtr) );
    }

    const D3D_SHADER_MACRO *pDefines = nullptr;
    std::vector<D3D_SHADER_MACRO> D3DMacros;
    if( CreationAttribs.Macros )
    {
        for( auto* pCurrMacro = CreationAttribs.Macros; pCurrMacro->Name && pCurrMacro->Definition; ++pCurrMacro )
        {
            D3DMacros.push_back( {pCurrMacro->Name, pCurrMacro->Definition} );
        }
        D3DMacros.push_back( {nullptr, nullptr} );
        pDefines = D3DMacros.data();
    }

    CHECK_D3D_RESULT_THROW( CompileShader( ShaderSource.c_str(), CreationAttribs.EntryPoint, pDefines, CreationAttribs.pShaderSourceStreamFactory, strShaderProfile.c_str(), &m_pShaderByteCode ),
                            "Failed to compile the shader");
}

}
