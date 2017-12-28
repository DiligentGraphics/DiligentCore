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

#pragma once

#include "Shader.h"
#include "StringTools.h"

/// \file
/// D3D shader resource loading

namespace Diligent
{
    template<typename D3D_SHADER_DESC, 
             typename D3D_SHADER_INPUT_BIND_DESC,
             typename TShaderReflection, 

             typename TOnResourcesCounted,
             typename TOnNewCB, 
             typename TOnNewTexUAV, 
             typename TOnNewBuffUAV, 
             typename TOnNewBuffSRV,
             typename TOnNewSampler,
             typename TOnNewTexSRV>
    void LoadD3DShaderResources(ID3DBlob *pShaderByteCode, 

                                TOnResourcesCounted OnResourcesCounted, 
                                TOnNewCB OnNewCB, 
                                TOnNewTexUAV OnNewTexUAV, 
                                TOnNewBuffUAV OnNewBuffUAV, 
                                TOnNewBuffSRV OnNewBuffSRV,
                                TOnNewSampler OnNewSampler,
                                TOnNewTexSRV OnNewTexSRV, 

                                const ShaderDesc &ShdrDesc,
                                const Char *SamplerSuffix)
    {
        CComPtr<TShaderReflection> pShaderReflection;
        CHECK_D3D_RESULT_THROW( D3DReflect( pShaderByteCode->GetBufferPointer(), pShaderByteCode->GetBufferSize(), __uuidof(pShaderReflection), reinterpret_cast<void**>(static_cast<TShaderReflection**>(&pShaderReflection)) ),
                                "Failed to get the shader reflection" );

        D3D_SHADER_DESC shaderDesc = {};
        pShaderReflection->GetDesc( &shaderDesc );

        std::vector<D3DShaderResourceAttribs, STDAllocatorRawMem<D3DShaderResourceAttribs> > Resources( STD_ALLOCATOR_RAW_MEM(D3DShaderResourceAttribs, GetRawAllocator(), "Allocator for vector<D3DShaderResourceAttribs>") );
        Resources.reserve(shaderDesc.BoundResources);

        Uint32 NumCBs = 0, NumTexSRVs = 0, NumTexUAVs = 0, NumBufSRVs = 0, NumBufUAVs = 0, NumSamplers = 0;
        // Number of resources to skip (used for array resources)
        UINT SkipCount = 1;
        for( UINT Res = 0; Res < shaderDesc.BoundResources; Res += SkipCount )
        {
            D3D_SHADER_INPUT_BIND_DESC BindingDesc = {};
            pShaderReflection->GetResourceBindingDesc( Res, &BindingDesc );

            String Name(BindingDesc.Name);
            
            SkipCount = 1;
            
            UINT BindCount = BindingDesc.BindCount;
            
            // Handle arrays
            // For shader models 5_0 and before, every resource array element is enumerated individually.
            // For instance, if the following texture array is defined in the shader:
            //
            //     Texture2D<float3> g_tex2DDiffuse[4];
            //
            // The reflection system will enumerate 4 resources with the following names:
            // "g_tex2DDiffuse[0]"
            // "g_tex2DDiffuse[1]"
            // "g_tex2DDiffuse[2]"
            // "g_tex2DDiffuse[3]"
            // 
            // Notice that if some array element is not used by the shader, it will not be enumerated

            auto OpenBracketPos = Name.find('[');
            if (String::npos != OpenBracketPos)
            {
                VERIFY(BindCount == 1, "When array elements are enumerated individually, BindCount is expected to always be 1");

                // Name == "g_tex2DDiffuse[0]"
                //                        ^
                //                   OpenBracketPos
                Name.erase(OpenBracketPos, Name.length() - OpenBracketPos);
                // Name == "g_tex2DDiffuse"
                VERIFY_EXPR(Name.length() == OpenBracketPos);
#ifdef _DEBUG
                for (const auto &ExistingRes : Resources)
                {
                    VERIFY(ExistingRes.Name != Name, "Resource with the same name has already been enumerated. All array elements are expected to be enumerated one after another");
                }
#endif
                for( UINT ArrElem = Res+1; ArrElem < shaderDesc.BoundResources; ++ArrElem)
                {
                    D3D_SHADER_INPUT_BIND_DESC ArrElemBindingDesc = {};
                    pShaderReflection->GetResourceBindingDesc( ArrElem, &ArrElemBindingDesc );
                    
                    // Make sure this case is handled correctly:
                    // "g_tex2DDiffuse[.]" != "g_tex2DDiffuse2[.]"
                    if ( strncmp(Name.c_str(), ArrElemBindingDesc.Name, OpenBracketPos) == 0 && ArrElemBindingDesc.Name[OpenBracketPos] == '[')
                    {
                        //g_tex2DDiffuse[2]
                        //               ^
                        UINT Ind = atoi(ArrElemBindingDesc.Name+OpenBracketPos+1);
                        BindCount = std::max(BindCount, Ind+1);
                        VERIFY(ArrElemBindingDesc.BindPoint == BindingDesc.BindPoint + Ind, 
                               "Array elements are expected to use contigous bind points.\n", 
                                BindingDesc.Name, " uses slot ", BindingDesc.BindPoint, ", so ", ArrElemBindingDesc.Name, " is expected to use slot ", BindingDesc.BindPoint + Ind, " while ", ArrElemBindingDesc.BindPoint, " is actually used" );
                        
                        // Note that skip count may not necessarily be the same as BindCount.
                        // If some array elements are not used by the shader, the reflection system skips them
                        ++SkipCount;
                    }
                    else
                    {
                        break;
                    }
                }
            }


            SHADER_VARIABLE_TYPE VarType = SHADER_VARIABLE_TYPE_NUM_TYPES;
            bool IsStaticSampler = false;
            if (BindingDesc.Type == D3D_SIT_SAMPLER)
            {
                for (Uint32 s = 0; s < ShdrDesc.NumStaticSamplers; ++s)
                {
                    if( StrCmpSuff(Name.c_str(), ShdrDesc.StaticSamplers[s].TextureName, SamplerSuffix) )
                    {
                        IsStaticSampler = true;
                        break;
                    }
                }
                // Use texture name to derive sampler type
                VarType = GetShaderVariableType(ShdrDesc.DefaultVariableType, ShdrDesc.VariableDesc, ShdrDesc.NumVariables,
                        [&](const char *TexName)
                        {
                            return StrCmpSuff(Name.c_str(), TexName, SamplerSuffix);
                        }
                    );
            }
            else
            {
                VarType = GetShaderVariableType(Name.c_str(), ShdrDesc.DefaultVariableType, ShdrDesc.VariableDesc, ShdrDesc.NumVariables);
            }


            switch( BindingDesc.Type )
            {
                case D3D_SIT_CBUFFER:                       ++NumCBs;                                                                        break;
                case D3D_SIT_TBUFFER:                       UNSUPPORTED( "TBuffers are not supported" );                                     break;
                case D3D_SIT_TEXTURE:                       ++(BindingDesc.Dimension == D3D_SRV_DIMENSION_BUFFER ? NumBufSRVs : NumTexSRVs); break;
                case D3D_SIT_SAMPLER:                       ++NumSamplers;                                                                   break;
                case D3D_SIT_UAV_RWTYPED:                   ++(BindingDesc.Dimension == D3D_SRV_DIMENSION_BUFFER ? NumBufUAVs : NumTexUAVs); break;
                case D3D_SIT_STRUCTURED:                    ++NumBufSRVs;                                                                    break;
                case D3D_SIT_UAV_RWSTRUCTURED:              ++NumBufUAVs;                                                                    break;
                case D3D_SIT_BYTEADDRESS:                   UNSUPPORTED( "Byte address buffers are not supported" );                         break;
                case D3D_SIT_UAV_RWBYTEADDRESS:             ++NumBufUAVs;                                                                    break;
                case D3D_SIT_UAV_APPEND_STRUCTURED:         UNSUPPORTED( "Append structured buffers are not supported" );                    break;
                case D3D_SIT_UAV_CONSUME_STRUCTURED:        UNSUPPORTED( "Consume structured buffers are not supported" );                   break;
                case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: UNSUPPORTED( "RW structured buffers with counter are not supported" );           break;
                default: UNEXPECTED("Unexpected resource type");
            }
            Resources.emplace_back(std::move(Name), BindingDesc.BindPoint, BindCount, BindingDesc.Type, VarType, BindingDesc.Dimension, D3DShaderResourceAttribs::InvalidSamplerId, IsStaticSampler);
        }


#ifdef _DEBUG
        if(ShdrDesc.NumVariables != 0 || ShdrDesc.NumStaticSamplers != 0 )
        {
            for (Uint32 v = 0; v < ShdrDesc.NumVariables; ++v)
            {
                bool VariableFound = false;
                const auto *VarName = ShdrDesc.VariableDesc[v].Name;

                for (const auto& Res : Resources)
                {
                    // Skip samplers as they are not handled as independent variables
                    if (Res.GetInputType() != D3D_SIT_SAMPLER && Res.Name.compare(VarName) == 0)
                    {
                        VariableFound = true;   
                        break;
                    }
                }
                if(!VariableFound)
                {
                    LOG_WARNING_MESSAGE("Variable \"", VarName, "\" not found in shader \"", ShdrDesc.Name, '\"');
                }
            }

            for (Uint32 s = 0; s < ShdrDesc.NumStaticSamplers; ++s)
            {
                bool TextureFound = false;
                const auto *TexName = ShdrDesc.StaticSamplers[s].TextureName;

                for (const auto& Res : Resources)
                {
                    if ( Res.GetInputType() == D3D_SIT_TEXTURE && Res.GetSRVDimension() != D3D_SRV_DIMENSION_BUFFER && Res.Name.compare(TexName) == 0)
                    {
                        TextureFound = true;
                        break;
                    }
                }
                if(!TextureFound)
                {
                    LOG_WARNING_MESSAGE("Static sampler specifies a texture \"", TexName, "\" that is not found in shader \"", ShdrDesc.Name, '\"');
                }
            }
        }
#endif

        OnResourcesCounted(NumCBs, NumTexSRVs, NumTexUAVs, NumBufSRVs, NumBufUAVs, NumSamplers);

        std::vector<D3DShaderResourceAttribs, STDAllocatorRawMem<D3DShaderResourceAttribs> > TextureSRVs( STD_ALLOCATOR_RAW_MEM(D3DShaderResourceAttribs, GetRawAllocator(), "Allocator for vector<D3DShaderResourceAttribs>") );
        TextureSRVs.reserve(NumTexSRVs);

        for(auto &Res : Resources)
        {
            switch( Res.GetInputType() )
            {
                case D3D_SIT_CBUFFER:
                {
                    OnNewCB( std::move(Res) );
                    break;
                }
            
                case D3D_SIT_TBUFFER:
                {
                    UNSUPPORTED( "TBuffers are not supported" );
                    break;
                }

                case D3D_SIT_TEXTURE:
                {
                    if( Res.GetSRVDimension() == D3D_SRV_DIMENSION_BUFFER )
                    {
                        OnNewBuffSRV( std::move(Res) );
                    }
                    else
                    {
                        TextureSRVs.emplace_back( std::move(Res) );
                    }
                    break;
                }

                case D3D_SIT_SAMPLER:
                {
                    OnNewSampler( std::move(Res) );
                    break;
                }

                case D3D_SIT_UAV_RWTYPED:
                {
                    if( Res.GetSRVDimension() == D3D_SRV_DIMENSION_BUFFER )
                    {
                        OnNewBuffUAV( std::move(Res) );
                    }
                    else
                    {
                        OnNewTexUAV( std::move(Res) );
                    }
                    break;
                }

                case D3D_SIT_STRUCTURED:
                {
                    OnNewBuffSRV( std::move(Res) );
                    break;
                }

                case D3D_SIT_UAV_RWSTRUCTURED:
                {
                    OnNewBuffUAV( std::move(Res) );
                    break;
                }

                case D3D_SIT_BYTEADDRESS:
                {
                    UNSUPPORTED( "Byte address buffers are not supported" );
                    break;
                }

                case D3D_SIT_UAV_RWBYTEADDRESS:
                {
                    OnNewBuffUAV( std::move(Res) );
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

        // Process texture SRVs. We need to do this after all samplers are initialized
        for( auto &Tex : TextureSRVs )
        {
            OnNewTexSRV( std::move(Tex) );
        }
    }
}
