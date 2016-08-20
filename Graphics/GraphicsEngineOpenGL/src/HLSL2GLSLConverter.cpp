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

// Converter limitation:
// * Using Texture* keywords in macros is not supported. The following lines will not work:
//   -  #define TEXTURE2D Texture2D
//      TEXTURE2D MacroTex2D;
//
//
// List of supported HLSL Objects and methods:
//
// * Texture1D:
//  -  void GetDimensions (out {int, uint, float} Width);
//  -  void GetDimensions (in uint MipLevel, out {int, uint, float} Width, out {int, uint, float} NumberOfLevels);
//  -  ret Sample( sampler_state S, float Location [, int Offset] );
//  -  ret SampleBias( sampler_state S, float Location, float Bias [, int Offset] );
//  -  ret SampleLevel( sampler_state S, float Location, float LOD [, int Offset] )
//  -  ret SampleGrad( sampler_state S, float Location, float DDX, float DDY [, int Offset] );
//  - float SampleCmp( SamplerComparisonState S, float Location, float CompareValue, [int Offset] );
//  - float SampleCmpLevelZero( SamplerComparisonState S, float Location, float CompareValue, [int Offset] );
//  -  ret Load( int Location, [int Offset ] );
//
//
// * Texture1DArray:
//  - void GetDimensions( out {int, uint, float} Width, out {int, uint, float} Elements );
//  - void GetDimensions( in uint MipLevel, out {int, uint, float} Width, out {int, uint, float} Elements, out {int, uint, float} NumberOfLevels );
//  -  ret Sample( sampler_state S, float2 Location [, int Offset] );
//  -  ret SampleBias( sampler_state S, float2 Location, float Bias [, int Offset] );
//  -  ret SampleLevel( sampler_state S, float2 Location, float LOD [, int Offset] )
//  -  ret SampleGrad( sampler_state S, float2 Location, float DDX, float DDY [, int Offset] );
//  - float SampleCmp( SamplerComparisonState S, float2 Location, float CompareValue, [int Offset] );
//  - float SampleCmpLevelZero( SamplerComparisonState S, float2 Location, float CompareValue, [int Offset] );
//  -  ret Load( int2 Location, [int Offset ] );
// Remarks:
//  - Array index goes in Location.y

//
// * Texture2D:
//  - void GetDimensions( out {int, uint, float} Width, out {int, uint, float} Height );
//  - void GetDimensions( in uint MipLevel, out {int, uint, float} Width, out {int, uint, float} Height, out {int, uint, float} NumberOfLevels );
//  -  ret Sample( sampler_state S, float2 Location [, int2 Offset] );
//  -  ret SampleBias( sampler_state S, float2 Location, float Bias [, int2 Offset] );
//  -  ret SampleLevel( sampler_state S, float2 Location, float LOD [, int2 Offset] )
//  -  ret SampleGrad( sampler_state S, float2 Location, float2 DDX, float2 DDY [, int2 Offset] );
//  - float SampleCmp( SamplerComparisonState S, float2 Location, float CompareValue, [int2 Offset] );
//  - float SampleCmpLevelZero( SamplerComparisonState S, float2 Location, float CompareValue [, int2 Offset] );
//  -  ret Load( int2 Location, [int2 Offset ] );
//  -  ret Gather( sampler_state S, float2 Location [, int2 Offset] );
//  -  float4 GatherCmp( SamplerComparisonState S, float2 Location, float CompareValue [, int2 Offset] );
//
// * Texture2DArray:
//  - void GetDimensions( out {int, uint, float} Width, out {int, uint, float} Height, out {int, uint, float} Elements );
//  - void GetDimensions( in uint MipLevel, out {int, uint, float} Width, out {int, uint, float} Height, out {int, uint, float} Elements, out {int, uint, float} NumberOfLevels );
//  -  ret Sample( sampler_state S, float3 Location [, int2 Offset] );
//  -  ret SampleBias( sampler_state S, float3 Location, float Bias [, int2 Offset] );
//  -  ret SampleLevel( sampler_state S, float3 Location, float LOD [, int2 Offset] )
//  -  ret SampleGrad( sampler_state S, float3 Location, float2 DDX, float2 DDY [, int2 Offset] );
//  - float SampleCmp( SamplerComparisonState S, float2 Location, float CompareValue [, int2 Offset] );
//  -  ret Load( int Location3 [, int2 Offset ] );
//  -  ret Gather( sampler_state S, float3 Location [, int2 Offset] );
//  -  float4 GatherCmp( SamplerComparisonState S, float3 Location, float CompareValue [, int2 Offset] );
//  Remarks:
//  - Array index goes in Location.z
//  - SampleCmpLevelZero() is not supported as there is no corresponding OpenGL instruction. The
//    instruction will always return 0.
//
// * Texture3D:
//  - void GetDimensions( out {int, uint, float} Width, out {int, uint, float} Height, out {int, uint, float} Depth );
//  - void GetDimensions( in uint MipLevel, out {int, uint, float} Width, out {int, uint, float} Height, out {int, uint, float} Depth, out {int, uint, float} NumberOfLevels );
//  -  ret Sample( sampler_state S, float3 Location [, int3 Offset] );
//  -  ret SampleBias( sampler_state S, float3 Location, float Bias [, int3 Offset] );
//  -  ret SampleLevel( sampler_state S, float3 Location, float LOD [, int3 Offset] )
//  -  ret SampleGrad( sampler_state S, float3 Location, float3 DDX, float3 DDY [, int3 Offset] );
//  -  ret Load( int3 Location [, int3 Offset ] );
//
// * TextureCube:
//  - void GetDimensions( out {int, uint, float} Width, out {int, uint, float} Height );
//  - void GetDimensions( in uint MipLevel, out {int, uint, float} Width, out {int, uint, float} Height, out {int, uint, float} NumberOfLevels );
//  -  ret Sample( sampler_state S, float3 Location );
//  -  ret SampleBias( sampler_state S, float3 Location, float Bias );
//  -  ret SampleLevel( sampler_state S, float3 Location, float LOD ) - NO offset version
//  -  ret SampleGrad( sampler_state S, float3 Location, float3 DDX, float3 DDY );
//  - float SampleCmp( SamplerComparisonState S, float3 Location, float CompareValue );
//  -  ret Gather( sampler_state S, float3 Location );
//  -  float4 GatherCmp( SamplerComparisonState S, float3 Location, float CompareValue );
//  Remarks:
//  - SampleCmpLevelZero() is not supported as there is no corresponding OpenGL instruction. The
//    instruction will always return 0.

// * TextureCubeArray:
//  - void GetDimensions( out {int, uint, float} Width, out {int, uint, float} Height, out {int, uint, float} Elements );
//  - void GetDimensions( in uint MipLevel, out {int, uint, float} Width, out {int, uint, float} Height, out {int, uint, float} Elements, out {int, uint, float} NumberOfLevels );
//  -  ret Sample( sampler_state S, float4 Location );
//  -  ret SampleBias( sampler_state S, float4 Location, float Bias );
//  -  ret SampleLevel( sampler_state S, float4 Location, float LOD ) - NO offset version
//  -  ret SampleGrad( sampler_state S, float4 Location, float3 DDX, float3 DDY );
//  - float SampleCmp( SamplerComparisonState S, float4 Location, float CompareValue );
//  -  ret Gather( sampler_state S, float4 Location );
//  -  float4 GatherCmp( SamplerComparisonState S, float4 Location, float CompareValue );
//  Remarks:
//  - SampleCmpLevelZero() is not supported as there is no corresponding OpenGL instruction. The
//    instruction will always return 0.
//  - Array index goes in Location.w
//
// * Texture2DMS:
//  - void GetDimensions(out {int, uint, float} Width, out {int, uint, float} Height, out {int, uint, float} NumberOfSamples);
//  -  ret Load( int2 Location, int Sample, [int2 Offset ] );
//
// * Texture2DMSArray:
//  - void GetDimensions( out {int, uint, float} Width, out {int, uint, float} Height, out {int, uint, float} Elements, out {int, uint, float} NumberOfSamples );
//  -  ret Load( int3 Location, int Sample, [int2 Offset ] );
//
//
// * RWTexture1D:
//  - void GetDimensions(out {int, uint, float} Width);
//
// * RWTexture1DArray:
//  - void GetDimensions(out {int, uint, float} Width, out {int, uint, float} Elements);
//
// * RWTexture2D:
//  - void GetDimensions(out {int, uint, float} Width, out {int, uint, float} Height);
//
// * RWTexture2DArray:
//  - void GetDimensions(out {int, uint, float} Width, out {int, uint, float} Height, out {int, uint, float} Elements);
//
// * RWTexture3D:
//  - void GetDimensions(out {int, uint, float} Width, out {int, uint, float} Height, out {int, uint, float} Depth);
//

// \remarks
//   All GetDimensions() functions return valid value in NumberOfLevels only on Desktop GL 4.3+
//   For multisampled textures, GetDimensions() always returns 0 in NumberOfSamples.


// Support for HLSL intrinsics:

// [V] abs( {int, int2, int3, int4, float, float2, float3, float4} )
// [V] acos( {float, float2, float3, float4} )
//   (-) acos( {matrix types} )
// [V] all( {bool2, bool3, bool4})
//   (-) all( {bool, int, int2, int3, int4, float, float2, float3, float4, matrix types} )
// [V] any( {bool2, bool3, bool4})
//   (-) any( {bool, int, int2, int3, int4, float, float2, float3, float4, matrix types} )
// [V] asdouble( {uint} )
// [V] asfloat( {int, int2, int3, int4, uint, uint2, uint3, uint4, float, float2, float3, float4} )
//   (-) asfloat( {matrix types} )
// [V] asint( {int, int2, int3, int4, uint, uint2, uint3, uint4, float, float2, float3, float4} )
//   (-) asint( {matrix types} )
// [V] asuint( {int, int2, int3, int4, uint, uint2, uint3, uint4, float, float2, float3, float4} )
//   (-) asuint( {matrix types} )
// [V] asin( {float, float2, float3, float4} )
//   (-) asin( {matrix types} )
// [V] atan( {float, float2, float3, float4} )
//   (-) atan( {matrix types} )
// [V] atan2( {float, float2, float3, float4} )
//   (-) atan2( {matrix types} )
// [V] ceil( {float, float2, float3, float4} )
//   (-) ceil( {matrix types} )
// [V] clamp( {int, int2, int3, int4, uint, uint2, uint3, uint4, float, float2, float3, float4} )
//   (-) clamp( {matrix types} )
// [V] cos( {float, float2, float3, float4} )
//   (-) cos( {matrix types} )
// [V] cosh( {float, float2, float3, float4} )
//   (-) cosh( {matrix types} )
// [V] countbits( {int, int2, int3, int4, uint, uint2, uint3, uint4} )
// [V] cross(float3)
// [V] ddx
// [V] ddx_coarse - defined as ddx
// [V] ddx_fine - defined as ddx
// [V] ddy
// [V] ddy_coarse - defined as ddy
// [V] ddy_fine - defined as ddy
// [V] degrees( {float, float2, float3, float4} )
//   (-) degrees( {matrix types} )
// [V] determinant
// [V] distance( {float, float2, float3, float4} )
// [V] dot( {float, float2, float3, float4} )
//   (-) dot( {int, int2, int3, int4} )
// [V] dst - defined as distance
// [V] exp( {float, float2, float3, float4} )
//   (-) exp( {matrix types} )
// [V] exp2( {float, float2, float3, float4} )
//   (-) exp2( {matrix types} )
// [V] f16tof32( {int, int2, int3, int4, uint, uint2, uint3, uint4} )
// [V] f32tof16( {float, float2, float3, float4} ) -> {uint, uint2, uint3, uint4}
// [V] faceforward( {float, float2, float3, float4} )
// [V] firstbithigh( {int, int2, int3, int4, uint, uint2, uint3, uint4} )
// [V] firstbitlow( {int, int2, int3, int4, uint, uint2, uint3, uint4} )
// [V] floor( {float, float2, float3, float4} )
//   (-) floor( {matrix types} )
// [V] fma( {double, double2, double3, double4} )
// [V] fmod( {float, float2, float3, float4} )
//   (-) fmod( {matrix types} )
// [V] frac( {float, float2, float3, float4} )
//   (-) frac( {matrix types} )
// [V] frexp( {float, float2, float3, float4}, {int, int2, int3, int4} )
// [V] fwidth( {float, float2, float3, float4} )
//   (-) fwidth( {matrix types} )
// [V] isfinite( {float, float2, float3, float4} ) - implemented as (!isinf(x) && !isnan(x))
// [V] isinf( {float, float2, float3, float4} )
//   (-) isinf( {matrix types} )
// [V] isnan( {float, float2, float3, float4} )
//   (-) isnan( {matrix types} )
// [V] ldexp( {float, float2, float3, float4}, {int, int2, int3, int4} )
// [V] length( {float, float2, float3, float4} )
// [V] lerp( {float, float2, float3, float4} )
//   (-) lerp( {matrix types} )
// [V] log( {float, float2, float3, float4} )
//   (-) log( {matrix types} )
// [V] log2( {float, float2, float3, float4} )
//   (-) log2( {matrix types} )
// [V] log10( {float, float2, float3, float4} )
//   (-) log10( {matrix types} )
// [V] mad( {float, float2, float3, float4} )
//   (-) mad( {matrix types} )
// [V] max( {int, int2, int3, int4, uint, uint2, uint3, uint4, float, float2, float3, float4} )
//   (-) max( {matrix types} )
// [V] min( {int, int2, int3, int4, uint, uint2, uint3, uint4, float, float2, float3, float4} )
//   (-) min( {matrix types} )
// [V] modf( {float, float2, float3, float4} )
//   (-) modf( {int, int2, int3, int4, matrix types} )
// [V] mul - defined as a*b
// [V] noise( {float, float2, float3, float4} )
// [V] normalize( {float, float2, float3, float4} )
// [V] pow( {float, float2, float3, float4} )
//   (-) pow( {matrix types} )
// [V] radians( {float, float2, float3, float4} )
//   (-) radians( {matrix types} )
// [V] rcp( {float, float2, float3, float4} ) - defined as 1.0/(x)
// [V] reflect( {float, float2, float3, float4} )
// [V] refract( {float, float2, float3, float4} )
// [V] reversebits( {int, int2, int3, int4, uint, uint2, uint3, uint4} )
// [V] round( {float, float2, float3, float4} )
//   (-) round( {matrix types} )
// [V] rsqrt( {float, float2, float3, float4} )
//   (-) rsqrt( {matrix types} )
// [V] saturate( {float, float2, float3, float4} )
// [V] sign( {float, float2, float3, float4, int, int2, int3, int4} )
//   (-) sign( {matrix types} )
// [V] sin( {float, float2, float3, float4} )
//   (-) sin( {matrix types} )
// [V] sinh( {float, float2, float3, float4} )
//   (-) sinh( {matrix types} )
// [V] sincos( {float, float2, float3, float4} )
// [V] smoothstep( {float, float2, float3, float4} )
//   (-) smoothstep( {matrix types} )
// [V] sqrt( {float, float2, float3, float4} )
//   (-) sqrt( {matrix types} )
// [V] step( {float, float2, float3, float4} )
//   (-) step( {matrix types} )
// [V] tan( {float, float2, float3, float4} )
//   (-) tan( {matrix types} )
// [V] tanh( {float, float2, float3, float4} )
//   (-) tanh( {matrix types} )
// [V] transpose
// [V] trunc( {float, float2, float3, float4} )
//   (-) trunc( {matrix types} )

// [V] AllMemoryBarrier - calls all memory barrier functions in gl
// [V] AllMemoryBarrierWithGroupSync
// [V] DeviceMemoryBarrier - calls image, atomic counter & buffer memory barriers
// [V] DeviceMemoryBarrierWithGroupSync
// [V] GroupMemoryBarrier - calls group memory & shared memory barriers
// [V] GroupMemoryBarrierWithGroupSync

// [V] InterlockedAdd( {int, uint} )
// [V] InterlockedAnd( {int, uint} )
// [V] InterlockedCompareExchange( {int, uint} )
// [V] InterlockedCompareStore( {int, uint} )
// [V] InterlockedExchange( {int, uint} )
// [V] InterlockedMax( {int, uint} )
// [V] InterlockedMin( {int, uint} )
// [V] InterlockedOr( {int, uint} )
// [V] InterlockedXor( {int, uint} )

// [ ] Process2DQuadTessFactorsAvg
// [ ] Process2DQuadTessFactorsMax
// [ ] Process2DQuadTessFactorsMin
// [ ] ProcessIsolineTessFactors
// [ ] ProcessQuadTessFactorsAvg
// [ ] ProcessQuadTessFactorsMax
// [ ] ProcessQuadTessFactorsMin
// [ ] ProcessTriTessFactorsAvg
// [ ] ProcessTriTessFactorsMax
// [ ] ProcessTriTessFactorsMin

// [ ] CheckAccessFullyMapped

// [ ] GetRenderTargetSampleCount
// [ ] GetRenderTargetSamplePosition

// [ ] EvaluateAttributeAtCentroid
// [ ] EvaluateAttributeAtSample
// [ ] EvaluateAttributeSnapped

// [ ] abort
// [ ] errorf
// [ ] printf
// [ ] clip
// [ ] msad4
// [ ] lit

// [ ] D3DCOLORtoUBYTE4

// Legacy not supported functions:
// [ ] tex1D
// [ ] tex1D
// [ ] tex1Dbias
// [ ] tex1Dgrad
// [ ] tex1Dlod
// [ ] tex1Dproj
// [ ] tex2D
// [ ] tex2D
// [ ] tex2Dbias
// [ ] tex2Dgrad
// [ ] tex2Dlod
// [ ] tex2Dproj
// [ ] tex3D
// [ ] tex3D
// [ ] tex3Dbias
// [ ] tex3Dgrad
// [ ] tex3Dlod
// [ ] tex3Dproj
// [ ] texCUBE
// [ ] texCUBE
// [ ] texCUBEbias
// [ ] texCUBEgrad
// [ ] texCUBElod
// [ ] texCUBEproj


#include "pch.h"
#include "HLSL2GLSLConverter.h"
#include "DataBlobImpl.h"
#include <unordered_set>
#include <string>

namespace Diligent
{

inline bool IsNewLine(Char Symbol)
{
    return Symbol == '\r' || Symbol == '\n';
}

inline bool IsDelimiter(Char Symbol)
{
    static const Char* Delimeters = " \t\r\n";
    return strchr( Delimeters, Symbol ) != nullptr;
}

inline bool IsStatementSeparator(Char Symbol)
{
    static const Char* StatementSeparator = ";}";
    return strchr( StatementSeparator, Symbol ) != nullptr;
}


// IteratorType may be String::iterator or String::const_iterator.
// While iterator is convertible to const_iterator, 
// iterator& cannot be converted to const_iterator& (Microsoft compiler allows
// such conversion, while gcc does not)
template<typename InteratorType>
static bool SkipComment( const String &Input, InteratorType& Pos )
{
    // // Comment     /* Comment
    // ^              ^
    if( Pos == Input.end() || *Pos != '/' )
        return false;

    auto NextPos = Pos+1;
    // // Comment     /* Comment
    //  ^              ^
    if( NextPos == Input.end() )
        return false;
    
    if( *NextPos == '/' )
    {
        // Skip // comment
        Pos = NextPos + 1;
        // // Comment
        //   ^
        for( ; Pos != Input.end() && !IsNewLine(*Pos); ++Pos );
        return true;
    }
    else if( *NextPos == '*' )
    {
        // Skip /* comment */
        Pos = NextPos + 1;
        // /* Comment
        //   ^
        while( Pos != Input.end() )
        {
            if( *Pos == '*' )
            {
                // /* Comment */
                //            ^
                ++Pos;
                // /* Comment */
                //             ^
                if( Pos == Input.end() )
                    break;
                if( *Pos == '/' )
                {
                    ++Pos;
                    // /* Comment */
                    //              ^
                    break;
                }
            }
            else
            {
                // Must handle /* **/ properly
                ++Pos; 
            }
        }
        return true;
    }
        
    return false;
}

inline bool SkipDelimeters(const String &Input, String::const_iterator &SrcChar)
{
    for( ; SrcChar != Input.end() && IsDelimiter(*SrcChar); ++SrcChar );
    return SrcChar == Input.end();
}

// IteratorType may be String::iterator or String::const_iterator.
// While iterator is convertible to const_iterator, 
// iterator& cannot be converted to const_iterator& (Microsoft compiler allows
// such conversion, while gcc does not)
template<typename IteratorType>
inline bool SkipDelimetersAndComments(const String &Input, IteratorType &SrcChar)
{
    bool DelimiterFound = false;
    bool CommentFound = false;
    do
    {
        DelimiterFound = false;
        for( ; SrcChar != Input.end() && IsDelimiter(*SrcChar); ++SrcChar )
            DelimiterFound = true;

        CommentFound = SkipComment(Input, SrcChar);
    } while( SrcChar != Input.end() && (DelimiterFound || CommentFound) );
    
    return SrcChar == Input.end();
}

inline bool SkipIdentifier(const String &Input, String::const_iterator &SrcChar )
{
    if( SrcChar == Input.end() )
        return true;

    if( isalpha( *SrcChar ) || *SrcChar == '_' )
    {
        ++SrcChar;
        if( SrcChar == Input.end() )
            return true;
    }
    else
        return false;

    for( ; SrcChar != Input.end() && (isalnum( *SrcChar ) || *SrcChar == '_'); ++SrcChar );
    
    return SrcChar == Input.end();
}


HLSL2GLSLConverter::HLSL2GLSLConverter( IShaderSourceInputStreamFactory *pSourceStreamFactory ) : 
    m_pSourceStreamFactory(pSourceStreamFactory)
{
    // Populate HLSL keywords hash map
#define DEFINE_KEYWORD(kw)m_HLSLKeywords.insert( std::make_pair( #kw, TokenInfo( TokenType::kw, #kw ) ) )
    DEFINE_KEYWORD( cbuffer );
    DEFINE_KEYWORD( Texture1D );
    DEFINE_KEYWORD( Texture1DArray );
    DEFINE_KEYWORD( Texture2D );
    DEFINE_KEYWORD( Texture2DArray );
    DEFINE_KEYWORD( Texture3D );
    DEFINE_KEYWORD( TextureCube );
    DEFINE_KEYWORD( TextureCubeArray );
    DEFINE_KEYWORD( Texture2DMS );
    DEFINE_KEYWORD( Texture2DMSArray );
    DEFINE_KEYWORD( SamplerState );
    DEFINE_KEYWORD( SamplerComparisonState );
    DEFINE_KEYWORD( RWTexture1D );
    DEFINE_KEYWORD( RWTexture1DArray );
    DEFINE_KEYWORD( RWTexture2D );
    DEFINE_KEYWORD( RWTexture2DArray );
    DEFINE_KEYWORD( RWTexture3D );
#undef DEFINE_KEYWORD
    
#define DEFINE_BUILTIN_TYPE(Type)m_HLSLKeywords.insert( std::make_pair( #Type, TokenInfo( TokenType::BuiltInType, #Type ) ) )
    DEFINE_BUILTIN_TYPE( void );

    DEFINE_BUILTIN_TYPE( float4 );
    DEFINE_BUILTIN_TYPE( float3 );
    DEFINE_BUILTIN_TYPE( float2 );
    DEFINE_BUILTIN_TYPE( float );

    DEFINE_BUILTIN_TYPE( int4 );
    DEFINE_BUILTIN_TYPE( int3 );
    DEFINE_BUILTIN_TYPE( int2 );
    DEFINE_BUILTIN_TYPE( int );

    DEFINE_BUILTIN_TYPE( uint4 );
    DEFINE_BUILTIN_TYPE( uint3 );
    DEFINE_BUILTIN_TYPE( uint2 );
    DEFINE_BUILTIN_TYPE( uint  );

    DEFINE_BUILTIN_TYPE( bool4 );
    DEFINE_BUILTIN_TYPE( bool3 );
    DEFINE_BUILTIN_TYPE( bool2 );
    DEFINE_BUILTIN_TYPE( bool );

    DEFINE_BUILTIN_TYPE( float2x2 );
    DEFINE_BUILTIN_TYPE( float2x3 );
    DEFINE_BUILTIN_TYPE( float2x4 );

    DEFINE_BUILTIN_TYPE( float3x2 );
    DEFINE_BUILTIN_TYPE( float3x3 );
    DEFINE_BUILTIN_TYPE( float3x4 );

    DEFINE_BUILTIN_TYPE( float4x2 );
    DEFINE_BUILTIN_TYPE( float4x3 );
    DEFINE_BUILTIN_TYPE( float4x4 );
    DEFINE_BUILTIN_TYPE( matrix );
#undef DEFINE_BUILTIN_TYPE

    m_HLSLKeywords.insert( std::make_pair( "struct", TokenInfo( TokenType::_struct, "struct" ) ) );

#define DEFINE_FLOW_CONTROL_STATEMENT(Statement) m_HLSLKeywords.insert( std::make_pair( #Statement, TokenInfo( TokenType::FlowControl, #Statement ) ) )
    DEFINE_FLOW_CONTROL_STATEMENT( break );
    DEFINE_FLOW_CONTROL_STATEMENT( continue );
    DEFINE_FLOW_CONTROL_STATEMENT( discard );
    DEFINE_FLOW_CONTROL_STATEMENT( do );
    DEFINE_FLOW_CONTROL_STATEMENT( for );
    DEFINE_FLOW_CONTROL_STATEMENT( if );
    DEFINE_FLOW_CONTROL_STATEMENT( else );
    DEFINE_FLOW_CONTROL_STATEMENT( switch );
    DEFINE_FLOW_CONTROL_STATEMENT( while );
    DEFINE_FLOW_CONTROL_STATEMENT( return );
#undef DEFINE_FLOW_CONTROL_STATEMENT


    // Prepare texture function stubs
    //                          sampler  usampler  isampler sampler*Shadow
    const String Prefixes[] = {     "",      "u",      "i",            "" };
    const String Suffixes[] = {     "",       "",       "",      "Shadow" };
    for( int i = 0; i < _countof( Prefixes ); ++i )
    {
        const auto &Pref = Prefixes[i];
        const auto &Suff = Suffixes[i];
        // GetDimensions() does not return anything, so swizzle should be empty
#define DEFINE_GET_DIM_STUB(Name, Obj, NumArgs) m_GLSLStubs.emplace( make_pair( FunctionStubHashKey( Pref + Obj + Suff, "GetDimensions", NumArgs ), GLSLStubInfo(Name, "") ) )

        DEFINE_GET_DIM_STUB( "GetTex1DDimensions_1", "sampler1D", 1 ); // GetDimensions( Width )
        DEFINE_GET_DIM_STUB( "GetTex1DDimensions_3", "sampler1D", 3 ); // GetDimensions( Mip, Width, NumberOfMips )

        DEFINE_GET_DIM_STUB( "GetTex1DArrDimensions_2", "sampler1DArray",  2 ); // GetDimensions( Width, ArrElems )
        DEFINE_GET_DIM_STUB( "GetTex1DArrDimensions_4", "sampler1DArray",  4 ); // GetDimensions( Mip, Width, ArrElems, NumberOfMips )

        DEFINE_GET_DIM_STUB( "GetTex2DDimensions_2", "sampler2D",  2 ); // GetDimensions( Width, Height )
        DEFINE_GET_DIM_STUB( "GetTex2DDimensions_4", "sampler2D",  4 ); // GetDimensions( Mip, Width, Height, NumberOfMips );

        DEFINE_GET_DIM_STUB( "GetTex2DArrDimensions_3", "sampler2DArray",  3 ); // GetDimensions( Width, Height, ArrElems )
        DEFINE_GET_DIM_STUB( "GetTex2DArrDimensions_5", "sampler2DArray",  5 ); // GetDimensions( Mip, Width, Height, ArrElems, NumberOfMips )

        DEFINE_GET_DIM_STUB( "GetTex2DDimensions_2", "samplerCube",  2 ); // GetDimensions( Width, Height )
        DEFINE_GET_DIM_STUB( "GetTex2DDimensions_4", "samplerCube",  4 ); // GetDimensions( Mip, Width, Height, NumberOfMips )

        DEFINE_GET_DIM_STUB( "GetTex2DArrDimensions_3", "samplerCubeArray",  3 ); // GetDimensions( Width, Height, ArrElems )
        DEFINE_GET_DIM_STUB( "GetTex2DArrDimensions_5", "samplerCubeArray",  5 ); // GetDimensions( Mip, Width, Height, ArrElems, NumberOfMips )

        if( Suff == "" )
        {
            // No shadow samplers for Tex3D, Tex2DMS and Tex2DMSArr
            DEFINE_GET_DIM_STUB( "GetTex3DDimensions_3", "sampler3D",  3 ); // GetDimensions( Width, Height, Depth )
            DEFINE_GET_DIM_STUB( "GetTex3DDimensions_5", "sampler3D",  5 ); // GetDimensions( Mip, Width, Height, Depth, NumberOfMips )

            DEFINE_GET_DIM_STUB( "GetTex2DMSDimensions_3",    "sampler2DMS",       3 ); // GetDimensions( Width, Height, NumSamples )
            DEFINE_GET_DIM_STUB( "GetTex2DMSArrDimensions_4", "sampler2DMSArray",  4 ); // GetDimensions( Width, Height, ArrElems, NumSamples )

            // Images
            DEFINE_GET_DIM_STUB( "GetRWTex1DDimensions_1",    "image1D",       1 ); // GetDimensions( Width )
            DEFINE_GET_DIM_STUB( "GetRWTex1DArrDimensions_2", "image1DArray",  2 ); // GetDimensions( Width, ArrElems )
            DEFINE_GET_DIM_STUB( "GetRWTex2DDimensions_2",    "image2D",       2 ); // GetDimensions( Width, Height )
            DEFINE_GET_DIM_STUB( "GetRWTex2DArrDimensions_3", "image2DArray",  3 ); // GetDimensions( Width, Height, ArrElems )
            DEFINE_GET_DIM_STUB( "GetRWTex3DDimensions_3",    "image3D",       3 ); // GetDimensions( Width, Height, Depth )

            m_ImageTypes.insert( HashMapStringKey(Pref+"image1D") );
            m_ImageTypes.insert( HashMapStringKey(Pref+"image1DArray") );
            m_ImageTypes.insert( HashMapStringKey(Pref+"image2D") );
            m_ImageTypes.insert( HashMapStringKey(Pref+"image2DArray") );
            m_ImageTypes.insert( HashMapStringKey(Pref+"image3D") );
        }
#undef DEFINE_GET_DIM_STUB
    }

    String Dimensions[] = { "1D", "1DArray", "2D", "2DArray", "3D", "Cube", "CubeArray" };
    for( int d = 0; d< _countof( Dimensions ); ++d)
    {
        String Dim = Dimensions[d];
        for( int i = 0; i < 3; ++i )
        {
            auto GLSLSampler = Prefixes[i] + "sampler" + Dim;

            // Use default swizzle to return the same number of components as specified in the texture declaration
            // Converter will insert _SWIZZLEn, where n is the number of components, after the function stub.
            // Example:
            // Texture2D<float3> Tex2D;
            // ...
            // Tex2D.Sample(Tex2D_sampler, f2UV) -> Sample_2(Tex2D, Tex2D_sampler, f2UV)_SWIZZLE3
            const Char *Swizzle = "_SWIZZLE";

#define     DEFINE_STUB(Name, Obj, Func, NumArgs) m_GLSLStubs.emplace( make_pair( FunctionStubHashKey( Obj, Func, NumArgs ), GLSLStubInfo(Name, Swizzle) ) )

            DEFINE_STUB( "Sample_2",      GLSLSampler, "Sample",      2 ); // Sample     ( Sampler, Location )
            DEFINE_STUB( "SampleBias_3",  GLSLSampler, "SampleBias",  3 ); // SampleBias ( Sampler, Location, Bias )
            DEFINE_STUB( "SampleLevel_3", GLSLSampler, "SampleLevel", 3 ); // SampleLevel( Sampler, Location, LOD )
            DEFINE_STUB( "SampleGrad_4",  GLSLSampler, "SampleGrad",  4 ); // SampleGrad ( Sampler, Location, DDX, DDY )
            if( Dim != "Cube" && Dim != "CubeArray" )
            {
                // No offset versions for cube & cube array
                DEFINE_STUB( "Sample_3",      GLSLSampler, "Sample",      3 ); // Sample     ( Sampler, Location, Offset )
                DEFINE_STUB( "SampleBias_4",  GLSLSampler, "SampleBias",  4 ); // SampleBias ( Sampler, Location, Bias, Offset )
                DEFINE_STUB( "SampleLevel_4", GLSLSampler, "SampleLevel", 4 ); // SampleLevel( Sampler, Location, LOD, Offset )
                DEFINE_STUB( "SampleGrad_5",  GLSLSampler, "SampleGrad",  5 ); // SampleGrad ( Sampler, Location, DDX, DDY, Offset )
            }
            if( Dim != "1D" && Dim != "1DArray" && Dim != "3D" )
            {
                // Gather always returns float4 independent of the number of components, so no swizzling
                Swizzle = ""; 
                DEFINE_STUB( "Gather_2", GLSLSampler, "Gather", 2 ); // Gather( SamplerState, Location )
                DEFINE_STUB( "Gather_3", GLSLSampler, "Gather", 3 ); // Gather( SamplerState, Location, Offset )
            }
        }
    }

    // Gather always returns float4 independent of the number of components, so no swizzling
    const Char *Swizzle = "";
    DEFINE_STUB( "GatherCmp_3", "sampler2DShadow",   "GatherCmp", 3 );      // GatherCmp( SmplerCmp, Location, CompareValue )
    DEFINE_STUB( "GatherCmp_4", "sampler2DShadow",   "GatherCmp", 4 );      // GatherCmp( SmplerCmp, Location, CompareValue, Offset )
    DEFINE_STUB( "GatherCmp_3", "sampler2DArrayShadow",  "GatherCmp", 3 );  // GatherCmp( SmplerCmp, Location, CompareValue )
    DEFINE_STUB( "GatherCmp_4", "sampler2DArrayShadow",  "GatherCmp", 4 );  // GatherCmp( SmplerCmp, Location, CompareValue, Offset )
    DEFINE_STUB( "GatherCmp_3", "samplerCubeShadow",      "GatherCmp", 3 ); // GatherCmp( SmplerCmp, Location, CompareValue )
    DEFINE_STUB( "GatherCmp_3", "samplerCubeArrayShadow", "GatherCmp", 3 ); // GatherCmp( SmplerCmp, Location, CompareValue )

    // All load operations should return the same number of components as specified
    // in texture declaraion, so use swizzling. Example:
    // Texture3D<int2> Tex3D;
    // ...
    // Tex3D.Load(i4Location) -> LoadTex3D_1(Tex3D, i4Location)_SWIZZLE2
    Swizzle = "_SWIZZLE";
    for( int i = 0; i < 3; ++i )
    {
        auto Pref = Prefixes[i];
        DEFINE_STUB( "LoadTex1D_1",      Pref + "sampler1D",        "Load", 1 ); // Load( Location )
        DEFINE_STUB( "LoadTex1DArr_1",   Pref + "sampler1DArray",   "Load", 1 ); // Load( Location )
        DEFINE_STUB( "LoadTex2D_1",      Pref + "sampler2D",        "Load", 1 ); // Load( Location )
        DEFINE_STUB( "LoadTex2DArr_1",   Pref + "sampler2DArray",   "Load", 1 ); // Load( Location )
        DEFINE_STUB( "LoadTex3D_1",      Pref + "sampler3D",        "Load", 1 ); // Load( Location )
        DEFINE_STUB( "LoadTex2DMS_2",    Pref + "sampler2DMS",      "Load", 2 ); // Load( Location, Sample )
        DEFINE_STUB( "LoadTex2DMSArr_2", Pref + "sampler2DMSArray", "Load", 2 ); // Load( Location, Sample )

        DEFINE_STUB( "LoadTex1D_2",      Pref + "sampler1D",        "Load", 2 ); // Load( Location, Offset )
        DEFINE_STUB( "LoadTex1DArr_2",   Pref + "sampler1DArray",   "Load", 2 ); // Load( Location, Offset )
        DEFINE_STUB( "LoadTex2D_2",      Pref + "sampler2D",        "Load", 2 ); // Load( Location, Offset )
        DEFINE_STUB( "LoadTex2DArr_2",   Pref + "sampler2DArray",   "Load", 2 ); // Load( Location, Offset )
        DEFINE_STUB( "LoadTex3D_2",      Pref + "sampler3D",        "Load", 2 ); // Load( Location, Offset )
        DEFINE_STUB( "LoadTex2DMS_3",    Pref + "sampler2DMS",      "Load", 3 ); // Load( Location, Sample, Offset )
        DEFINE_STUB( "LoadTex2DMSArr_3", Pref + "sampler2DMSArray", "Load", 3 ); // Load( Location, Sample, Offset )

        DEFINE_STUB( "LoadRWTex1D_1",      Pref + "image1D",        "Load", 1 ); // Load( Location )
        DEFINE_STUB( "LoadRWTex1DArr_1",   Pref + "image1DArray",   "Load", 1 ); // Load( Location )
        DEFINE_STUB( "LoadRWTex2D_1",      Pref + "image2D",        "Load", 1 ); // Load( Location )
        DEFINE_STUB( "LoadRWTex2DArr_1",   Pref + "image2DArray",   "Load", 1 ); // Load( Location )
        DEFINE_STUB( "LoadRWTex3D_1",      Pref + "image3D",        "Load", 1 ); // Load( Location )
    }

    // SampleCmp() returns float independent of the number of components, so
    // use no swizzling
    Swizzle = "";

    DEFINE_STUB( "SampleCmpTex1D_3",    "sampler1DShadow",       "SampleCmp", 3 );   // SampleCmp( SamplerCmp, Location, CompareValue )
    DEFINE_STUB( "SampleCmpTex1DArr_3", "sampler1DArrayShadow",  "SampleCmp", 3 );   // SampleCmp( SamplerCmp, Location, CompareValue )
    DEFINE_STUB( "SampleCmpTex2D_3",    "sampler2DShadow",       "SampleCmp", 3 );   // SampleCmp( SamplerCmp, Location, CompareValue )
    DEFINE_STUB( "SampleCmpTex2DArr_3", "sampler2DArrayShadow",  "SampleCmp", 3 );   // SampleCmp( SamplerCmp, Location, CompareValue )
    DEFINE_STUB( "SampleCmpTexCube_3",    "samplerCubeShadow",     "SampleCmp", 3 ); // SampleCmp( SamplerCmp, Location, CompareValue )
    DEFINE_STUB( "SampleCmpTexCubeArr_3", "samplerCubeArrayShadow","SampleCmp", 3 ); // SampleCmp( SamplerCmp, Location, CompareValue )

    DEFINE_STUB( "SampleCmpTex1D_4",    "sampler1DShadow",       "SampleCmp", 4 ); // SampleCmp( SamplerCmp, Location, CompareValue, Offset )
    DEFINE_STUB( "SampleCmpTex1DArr_4", "sampler1DArrayShadow",  "SampleCmp", 4 ); // SampleCmp( SamplerCmp, Location, CompareValue, Offset )
    DEFINE_STUB( "SampleCmpTex2D_4",    "sampler2DShadow",       "SampleCmp", 4 ); // SampleCmp( SamplerCmp, Location, CompareValue, Offset )
    DEFINE_STUB( "SampleCmpTex2DArr_4", "sampler2DArrayShadow",  "SampleCmp", 4 ); // SampleCmp( SamplerCmp, Location, CompareValue, Offset )


    DEFINE_STUB( "SampleCmpLevel0Tex1D_3",    "sampler1DShadow",       "SampleCmpLevelZero", 3 );    // SampleCmpLevelZero( SamplerCmp, Location, CompareValue )
    DEFINE_STUB( "SampleCmpLevel0Tex1DArr_3", "sampler1DArrayShadow",  "SampleCmpLevelZero", 3 );    // SampleCmpLevelZero( SamplerCmp, Location, CompareValue )
    DEFINE_STUB( "SampleCmpLevel0Tex2D_3",    "sampler2DShadow",       "SampleCmpLevelZero", 3 );    // SampleCmpLevelZero( SamplerCmp, Location, CompareValue )
    DEFINE_STUB( "SampleCmpLevel0Tex2DArr_3", "sampler2DArrayShadow",  "SampleCmpLevelZero", 3 );    // SampleCmpLevelZero( SamplerCmp, Location, CompareValue )
    DEFINE_STUB( "SampleCmpLevel0TexCube_3",    "samplerCubeShadow",     "SampleCmpLevelZero", 3 );  // SampleCmpLevelZero( SamplerCmp, Location, CompareValue )
    DEFINE_STUB( "SampleCmpLevel0TexCubeArr_3", "samplerCubeArrayShadow","SampleCmpLevelZero", 3 );  // SampleCmpLevelZero( SamplerCmp, Location, CompareValue )
                           
    DEFINE_STUB( "SampleCmpLevel0Tex1D_4",    "sampler1DShadow",       "SampleCmpLevelZero", 4 ); // SampleCmpLevelZero( SamplerCmp, Location, CompareValue, Offset )
    DEFINE_STUB( "SampleCmpLevel0Tex1DArr_4", "sampler1DArrayShadow",  "SampleCmpLevelZero", 4 ); // SampleCmpLevelZero( SamplerCmp, Location, CompareValue, Offset )
    DEFINE_STUB( "SampleCmpLevel0Tex2D_4",    "sampler2DShadow",       "SampleCmpLevelZero", 4 ); // SampleCmpLevelZero( SamplerCmp, Location, CompareValue, Offset )
    DEFINE_STUB( "SampleCmpLevel0Tex2DArr_4", "sampler2DArrayShadow",  "SampleCmpLevelZero", 4 ); // SampleCmpLevelZero( SamplerCmp, Location, CompareValue, Offset )


    // InterlockedOp( dest, val )
    // InterlockedOp( dest, val, original_val )
#define DEFINE_ATOMIC_OP_STUBS(Op)\
    DEFINE_STUB( "Interlocked" Op "SharedVar_2", "shared_var", "Interlocked" Op, 2 ); \
    DEFINE_STUB( "Interlocked" Op "SharedVar_3", "shared_var", "Interlocked" Op, 3 ); \
    DEFINE_STUB( "Interlocked" Op "Image_2", "image", "Interlocked" Op, 2 ); \
    DEFINE_STUB( "Interlocked" Op "Image_3", "image", "Interlocked" Op, 3 ); \
    m_AtomicOperations.insert( HashMapStringKey("Interlocked" Op) );


    DEFINE_ATOMIC_OP_STUBS( "Add" );
    DEFINE_ATOMIC_OP_STUBS( "And" );
    DEFINE_ATOMIC_OP_STUBS( "Exchange" );
    DEFINE_ATOMIC_OP_STUBS( "Max" );
    DEFINE_ATOMIC_OP_STUBS( "Min" );
    DEFINE_ATOMIC_OP_STUBS( "Or" );
    DEFINE_ATOMIC_OP_STUBS( "Xor" );

    // InterlockedCompareExchange( dest, compare_value, value, original_value )
    DEFINE_STUB( "InterlockedCompareExchangeSharedVar_4", "shared_var", "InterlockedCompareExchange", 4 );
    DEFINE_STUB( "InterlockedCompareExchangeImage_4",          "image", "InterlockedCompareExchange", 4 );
    m_AtomicOperations.insert( HashMapStringKey("InterlockedCompareExchange") );

    // InterlockedCompareStore( dest, compare_value, value )
    DEFINE_STUB( "InterlockedCompareStoreSharedVar_3", "shared_var", "InterlockedCompareStore", 3 );
    DEFINE_STUB( "InterlockedCompareStoreImage_3",          "image", "InterlockedCompareStore", 3 );
    m_AtomicOperations.insert( HashMapStringKey("InterlockedCompareStore") );

#undef DEFINE_STUB
}

String CompressNewLines( const String& Str )
{
    String Out;
    auto Char = Str.begin();
    while( Char != Str.end() )
    {
        if( *Char == '\r' )
        {
            ++Char;
            // Replace \r\n with \n
            if( Char != Str.end() && *Char == '\n' )
            {
                Out.push_back( '\n' );
                ++Char;
            }
            else
                Out.push_back( '\r' );
        }
        else
        {
            Out.push_back( *(Char++) );
        }
    }
    return Out;
}

static Int32 CountNewLines(const String& Str)
{
    Int32 NumNewLines = 0;
    auto Char = Str.begin();
    while( Char != Str.end() )
    {
        if( *Char == '\r' )
        {
            ++NumNewLines;
            ++Char;
            // \r\n should be counted as one newline
            if( Char != Str.end() && *Char == '\n' )
                ++Char;
        }
        else
        {
            if( *Char == '\n' )
                ++NumNewLines;
            ++Char;
        }
    }
    return NumNewLines;
}


// IteratorType may be String::iterator or String::const_iterator.
// While iterator is convertible to const_iterator, 
// iterator& cannot be converted to const_iterator& (Microsoft compiler allows
// such conversion, while gcc does not)
template<typename IteratorType>
String HLSL2GLSLConverter::PrintTokenContext( IteratorType &TargetToken, Int32 NumAdjacentLines )
{
    if( TargetToken == m_Tokens.end() )
        --TargetToken;

    //\n  ++ x ;
    //\n  ++ y ;
    //\n  if ( x != 0 )
    //         ^
    //\n      x += y ;
    //\n
    //\n  if ( y != 0 )
    //\n      x += 2 ;

    const int NumSepChars = 20;
    String Ctx(">");
    for( int i = 0; i < NumSepChars; ++i )Ctx.append( "  >" );
    Ctx.push_back( '\n' );

    // Find first token in the current line
    auto CurrLineStartToken = TargetToken;
    Int32 NumLinesAbove = 0;
    while( CurrLineStartToken != m_Tokens.begin() )
    {
        NumLinesAbove += CountNewLines(CurrLineStartToken->Delimiter);
        if( NumLinesAbove > 0 )
            break;
        --CurrLineStartToken;
    }
    //\n  if( x != 0 )
    //    ^

    // Find first token in the line NumAdjacentLines above
    auto TopLineStart = CurrLineStartToken;
    while( TopLineStart != m_Tokens.begin() && NumLinesAbove <= NumAdjacentLines )
    {
        --TopLineStart;
        NumLinesAbove += CountNewLines(TopLineStart->Delimiter);
    }
    //\n  ++ x ;
    //    ^
    //\n  ++ y ;
    //\n  if ( x != 0 )

    // Write everything from the top line up to the current line start
    auto Token = TopLineStart;
    for( ; Token != CurrLineStartToken; ++Token )
    {
        Ctx.append( CompressNewLines(Token->Delimiter) );
        Ctx.append(Token->Literal);
    }

    //\n  if ( x != 0 )
    //    ^

    Int32 NumLinesBelow = 0;
    String Spaces; // Accumulate whitespaces preceding current token
    bool AccumWhiteSpaces = true;
    while( Token != m_Tokens.end() && NumLinesBelow == 0  )
    {
        if( AccumWhiteSpaces )
        {
            for( const auto &Char : Token->Delimiter )
            {
                if( IsNewLine( Char ) )
                    Spaces.clear();
                else if( Char == '\t' )
                    Spaces.push_back( Char );
                else
                    Spaces.push_back( ' ' );
            }
        }

        // Acumulate spaces until we encounter current token
        if( Token == TargetToken )
            AccumWhiteSpaces = false;

        if( AccumWhiteSpaces )
            Spaces.append( Token->Literal.length(), ' ' );

        Ctx.append( CompressNewLines(Token->Delimiter) );
        Ctx.append(Token->Literal);
        ++Token;
        
        if( Token == m_Tokens.end() )
            break;

        NumLinesBelow += CountNewLines(Token->Delimiter);
    }

    // Write ^ on the line below
    Ctx.push_back( '\n' );
    Ctx.append( Spaces );
    Ctx.push_back( '^' );

    // Write NumAdjacentLines lines below current line
    while( Token != m_Tokens.end() && NumLinesBelow <= NumAdjacentLines )
    {
        Ctx.append( CompressNewLines(Token->Delimiter) );
        Ctx.append(Token->Literal);
        ++Token;

        if( Token == m_Tokens.end() )
            break;

        NumLinesBelow += CountNewLines(Token->Delimiter);
    }

    Ctx.append("\n<");
    for( int i = 0; i < NumSepChars; ++i )Ctx.append( "  <" );
    Ctx.push_back( '\n' );

    return Ctx;
}


#define VERIFY_PARSER_STATE( Token, Condition, ... )\
    if( !(Condition) )                                                  \
    {                                                                   \
        Diligent::MsgStream ss;                                        \
        Diligent::FormatMsg( ss, __VA_ARGS__ );                        \
        LOG_ERROR_AND_THROW( ss.str(), "\n", PrintTokenContext(Token, 4) );\
    }

template<typename IterType>
bool SkipPrefix(const Char* RefStr, IterType &begin, IterType end)
{
    auto pos = begin;
    while( *RefStr && pos != end )
    {
        if( *(RefStr++) != *(pos++) )
            return false;
    }
    if( *RefStr == 0 )
    {
        begin = pos;
        return true;
    }

    return false;
}

// The method scans the source code and replaces 
// all #include directives with the contents of the 
// file. It maintains a set of already parsed includes
// to avoid double inclusion
void HLSL2GLSLConverter::InsertIncludes( String &GLSLSource )
{
    // Put all the includes into the set to avoid multiple inclusion
    std::unordered_set<String> ProcessedIncludes;

    do
    {
        // Find the first #include statement
        auto Pos = GLSLSource.begin();
        auto IncludeStartPos = GLSLSource.end();
        while( Pos != GLSLSource.end() )
        {
            // #   include "TestFile.fxh"
            if( SkipDelimetersAndComments( GLSLSource, Pos ) )
                break;
            if( *Pos == '#' )
            {
                IncludeStartPos = Pos;
                // #   include "TestFile.fxh"
                // ^
                ++Pos;
                // #   include "TestFile.fxh"
                //  ^
                if( SkipDelimetersAndComments( GLSLSource, Pos ) )
                {
                    // End of the file reached - break
                    break;
                }
                // #   include "TestFile.fxh"
                //     ^
                if( SkipPrefix( "include", Pos, GLSLSource.end() ) )
                {
                    // #   include "TestFile.fxh"
                    //            ^
                    break;
                }
                else
                {
                    // This is not an #include directive:
                    // #define MACRO
                    // Continue search through the file
                }
            }
            else
                ++Pos;
        }

        // No more #include found
        if( Pos == GLSLSource.end() )
            break;

        // Find open quotes
        if( SkipDelimetersAndComments( GLSLSource, Pos ) )
            LOG_ERROR_AND_THROW( "Unexpected EOF after #include directive" );
        // #   include "TestFile.fxh"
        //             ^
        if( *Pos != '\"' && *Pos != '<' )
            LOG_ERROR_AND_THROW( "Missing open quotes or \'<\' after #include directive" );
        ++Pos;
        // #   include "TestFile.fxh"
        //              ^
        auto IncludeNameStartPos = Pos;
        // Find closing quotes
        while( Pos != GLSLSource.end() && *Pos != '\"' && *Pos != '>' )++Pos;
        // #   include "TestFile.fxh"
        //                          ^
        if( Pos == GLSLSource.end() )
            LOG_ERROR_AND_THROW( "Missing closing quotes or \'>\' after #include directive" );

        // Get the name of the include file
        auto IncludeName = String( IncludeNameStartPos, Pos );
        ++Pos;
        // #   include "TestFile.fxh"
        // ^                         ^
        // IncludeStartPos           Pos
        GLSLSource.erase( IncludeStartPos, Pos );

        // Convert the name to lower case
        String IncludeFileLowercase = IncludeName;
        std::transform( IncludeFileLowercase.begin(), IncludeFileLowercase.end(), IncludeFileLowercase.begin(), ::tolower );
        // Insert the lower-case name into the set
        auto It = ProcessedIncludes.insert( IncludeFileLowercase );
        // If the name was actually inserted, which means the include encountered for the first time,
        // replace the text with the file content
        if( It.second )
        {
            RefCntAutoPtr<IFileStream> pIncludeDataStream;
            m_pSourceStreamFactory->CreateInputStream( IncludeName.c_str(), &pIncludeDataStream );
            if( !pIncludeDataStream )
                LOG_ERROR_AND_THROW( "Failed to open include file ", IncludeName )
            RefCntAutoPtr<Diligent::IDataBlob> pIncludeData( new Diligent::DataBlobImpl );
            pIncludeDataStream->Read( pIncludeData );

            // Get include text
            auto IncludeText = reinterpret_cast<const Char*> (pIncludeData->GetDataPtr());
            size_t NumSymbols = pIncludeData->GetSize();

            // Insert the text into source
            GLSLSource.insert( IncludeStartPos-GLSLSource.begin(), IncludeText, NumSymbols );
        }
    } while( true );
}


void ReadNumericConstant(const String &Source, String::const_iterator &Pos, String &Output)
{
#define COPY_SYMBOL(){ Output.push_back( *(Pos++) ); if( Pos == Source.end() )return; }

    while( Pos != Source.end() && *Pos >= '0' && *Pos <= '9' )
        COPY_SYMBOL()

    if( *Pos == '.' )
    {
        COPY_SYMBOL()
        // Copy all numbers
        while( Pos != Source.end() && *Pos >= '0' && *Pos <= '9' )
            COPY_SYMBOL()
    }
    
    // Scientific notation
    // e+1242, E-234
    if( *Pos == 'e' || *Pos == 'E' )
    {
        COPY_SYMBOL()

        if( *Pos == '+' || *Pos == '-' )
            COPY_SYMBOL()

        // Skip all numbers
        while( Pos != Source.end() && *Pos >= '0' && *Pos <= '9' )
            COPY_SYMBOL()
    }

    if( *Pos == 'f' || *Pos == 'F' )
        COPY_SYMBOL()
#undef COPY_SYMBOL
}


// The function convertes source code into a token list
void HLSL2GLSLConverter::Tokenize( const String &Source )
{
#define CHECK_END(...) \
    if( SrcPos == Source.end() )        \
    {                                   \
        LOG_ERROR_MESSAGE(__VA_ARGS__)  \
        break;                          \
    }

    int OpenBracketCount = 0;
    int OpenBraceCount = 0;
    int OpenStapleCount = 0;
    
    // Push empty node in the beginning of the list to facilitate
    // backwards searching
    m_Tokens.push_back( TokenInfo() );

    // https://msdn.microsoft.com/en-us/library/windows/desktop/bb509638(v=vs.85).aspx

    // Notes:
    // * Operators +, - are not detected
    //   * This might be a + b, -a or -10
    // * Operator ?: is not detected
    auto SrcPos = Source.begin();
    while( SrcPos != Source.end() )
    {
        TokenInfo NewToken;
        auto DelimStart = SrcPos;
        SkipDelimetersAndComments( Source, SrcPos );
        if( DelimStart != SrcPos )
        {
            auto DelimSize = SrcPos - DelimStart;
            NewToken.Delimiter.reserve(DelimSize);
            NewToken.Delimiter.append(DelimStart, SrcPos);
        }
        if( SrcPos == Source.end() )
            break;

        switch( *SrcPos )
        {
            case '#':
            {
                NewToken.Type = TokenType::PreprocessorDirective;
                auto DirectiveStart = SrcPos;
                ++SrcPos;
                SkipDelimetersAndComments( Source, SrcPos );
                CHECK_END( "Missing preprocessor directive" );
                SkipIdentifier( Source, SrcPos );
                auto DirectiveSize = SrcPos - DirectiveStart;
                NewToken.Literal.reserve(DirectiveSize);
                NewToken.Literal.append(DirectiveStart, SrcPos);
            }
            break;

            case ';':
                NewToken.Type = TokenType::Semicolon;
                NewToken.Literal.push_back( *(SrcPos++) );
            break;

            case '=':
                if( m_Tokens.size() > 0 && NewToken.Delimiter == "" )
                { 
                    auto &LastToken = m_Tokens.back();
                    // +=, -=, *=, /=, %=, <<=, >>=, &=, |=, ^=
                    if( LastToken.Literal == "+" || 
                        LastToken.Literal == "-" || 
                        LastToken.Literal == "*" ||
                        LastToken.Literal == "/" ||
                        LastToken.Literal == "%" || 
                        LastToken.Literal == "<<" || 
                        LastToken.Literal == ">>" || 
                        LastToken.Literal == "&" ||
                        LastToken.Literal == "|" ||
                        LastToken.Literal == "^")
                    {
                        LastToken.Type = TokenType::Assignment;
                        LastToken.Literal.push_back( *(SrcPos++) );
                        continue;
                    }
                    else if( LastToken.Literal == "<" || 
                             LastToken.Literal == ">" || 
                             LastToken.Literal == "=" ||
                             LastToken.Literal == "!" )
                    {
                        LastToken.Type = TokenType::ComparisonOp;
                        LastToken.Literal.push_back( *(SrcPos++) );
                        continue;
                    }
                }
                
                NewToken.Type = TokenType::Assignment;
                NewToken.Literal.push_back( *(SrcPos++) );
            break;

            case '|':
            case '&':
                if( m_Tokens.size() > 0 && NewToken.Delimiter == "" && 
                    m_Tokens.back().Literal.length() == 1 && m_Tokens.back().Literal[0] == *SrcPos )
                {
                    m_Tokens.back().Type = TokenType::BooleanOp;
                    m_Tokens.back().Literal.push_back( *(SrcPos++) );
                    continue;
                }
                else
                {
                    NewToken.Type = TokenType::BitwiseOp;
                    NewToken.Literal.push_back( *(SrcPos++) );
                }
            break;

            case '<':
            case '>':
                if( m_Tokens.size() > 0 && NewToken.Delimiter == "" && 
                    m_Tokens.back().Literal.length() == 1 && m_Tokens.back().Literal[0] == *SrcPos )
                {
                    m_Tokens.back().Type = TokenType::BitwiseOp;
                    m_Tokens.back().Literal.push_back( *(SrcPos++) );
                    continue;
                }
                else
                {
                    // Note: we do not distinguish between comparison operators
                    // and template arguments like in Texture2D<float> at this
                    // point. This will be clarified when textures are processed.
                    NewToken.Type = TokenType::ComparisonOp;
                    NewToken.Literal.push_back( *(SrcPos++) );
                }
            break;

            case '+':
            case '-':
                if( m_Tokens.size() > 0 && NewToken.Delimiter == "" && 
                    m_Tokens.back().Literal.length() == 1 && m_Tokens.back().Literal[0] == *SrcPos )
                {
                    m_Tokens.back().Type = TokenType::IncDecOp;
                    m_Tokens.back().Literal.push_back( *(SrcPos++) );
                    continue;
                }
                else
                {
                    // We do not currently distinguish between math operator a + b,
                    // unary operator -a and numerical constant -1:
                    NewToken.Literal.push_back( *(SrcPos++) );
                }
            break;
            
            case '~':
            case '^':
                NewToken.Type = TokenType::BitwiseOp;
                NewToken.Literal.push_back( *(SrcPos++) );
            break;

            case '*':
            case '/':
            case '%':
                NewToken.Type = TokenType::MathOp;
                NewToken.Literal.push_back( *(SrcPos++) );
            break;

            case '!':
                NewToken.Type = TokenType::BooleanOp;
                NewToken.Literal.push_back( *(SrcPos++) );
            break;

            case ',':
                NewToken.Type = TokenType::Comma;
                NewToken.Literal.push_back( *(SrcPos++) );
            break;

#define BRACKET_CASE(Symbol, TokenType, Action) \
            case Symbol:                                    \
                NewToken.Type = TokenType;                  \
                NewToken.Literal.push_back( *(SrcPos++) );  \
                Action;                                     \
            break;
            BRACKET_CASE( '(', TokenType::OpenBracket,    ++OpenBracketCount );
            BRACKET_CASE( ')', TokenType::ClosingBracket, --OpenBracketCount );
            BRACKET_CASE( '{', TokenType::OpenBrace,      ++OpenBraceCount   );
            BRACKET_CASE( '}', TokenType::ClosingBrace,   --OpenBraceCount   );
            BRACKET_CASE( '[', TokenType::OpenStaple,     ++OpenStapleCount  );
            BRACKET_CASE( ']', TokenType::ClosingStaple,  --OpenStapleCount  );
#undef BRACKET_CASE


            default:
            {
                auto IdentifierStartPos = SrcPos;
                SkipIdentifier( Source, SrcPos );
                if( IdentifierStartPos != SrcPos )
                {
                    auto IDSize = SrcPos - IdentifierStartPos;
                    NewToken.Literal.reserve( IDSize );
                    NewToken.Literal.append( IdentifierStartPos, SrcPos );
                    auto KeywordIt = m_HLSLKeywords.find(NewToken.Literal.c_str());
                    if( KeywordIt != m_HLSLKeywords.end() )
                    {
                        NewToken.Type = KeywordIt->second.Type;
                        VERIFY( NewToken.Literal == KeywordIt->second.Literal, "Inconsistent literal" );
                    }
                    else
                    {
                        NewToken.Type = TokenType::Identifier;
                    }
                }

                if( NewToken.Type == TokenType::Undefined )
                {
                    bool bIsNumericalCostant = *SrcPos >= '0' && *SrcPos <= '9';
                    if( !bIsNumericalCostant && *SrcPos == '.' )
                    {
                        auto NextPos = SrcPos+1;
                        bIsNumericalCostant = NextPos != Source.end() && *NextPos >= '0' && *NextPos <= '9';
                    }
                    if( bIsNumericalCostant )
                    {
                        ReadNumericConstant(Source, SrcPos, NewToken.Literal);
                        NewToken.Type = TokenType::NumericConstant;
                    }
                }

                if( NewToken.Type == TokenType::Undefined )
                {
                    NewToken.Literal.push_back( *(SrcPos++) );
                }
                // Operators
                // https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx
            }
                
        }

        m_Tokens.push_back( NewToken );
    }
#undef CHECK_END
}

void HLSL2GLSLConverter::FindClosingBracket( TokenListType::iterator &Token, 
                                              const TokenListType::iterator &ScopeEnd,
                                              TokenType OpenBracketType, 
                                              TokenType ClosingBracketType )
{
    VERIFY_EXPR( Token->Type == OpenBracketType );
    ++Token; // Skip open bracket
    int BracketCount = 1;
    // Find matching closing bracket
    while( Token != ScopeEnd )
    {
        if( Token->Type == OpenBracketType )
            ++BracketCount;
        else if( Token->Type == ClosingBracketType )
        {
            --BracketCount;
            if( BracketCount == 0 )
                break;
        }
        ++Token;
    }
    VERIFY_PARSER_STATE( Token, BracketCount == 0, "No matching closing bracket found in the scope" );
}

// The function replaces cbuffer with uniform and adds semicolon if it is missing after the closing brace:
// cbuffer
// {
//    ...
// }; <- Semicolon must be here
//
void HLSL2GLSLConverter::ProcessConstantBuffer( TokenListType::iterator &Token )
{
    VERIFY_EXPR( Token->Type == TokenType::cbuffer );

    // Replace "cbuffer" with "uniform"
    Token->Literal = "uniform";
    ++Token;
    // cbuffer CBufferName
    //         ^

    VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "Unexpected EOF after \"cbuffer\" keyword" );
    VERIFY_PARSER_STATE( Token, Token->Type == TokenType::Identifier, "Identifier expected after \"cbuffer\" keyword" );
    const auto& CBufferName = Token->Literal;

    ++Token;
    // cbuffer CBufferName 
    //                    ^
    while( Token != m_Tokens.end() && Token->Type != TokenType::OpenBrace )
        ++Token;
    // cbuffer CBufferName
    // {                   
    // ^
    VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "Missing open brace in the definition of cbuffer ", CBufferName );

    // Find closing brace
    FindClosingBracket( Token, m_Tokens.end(), TokenType::OpenBrace, TokenType::ClosingBrace );

    VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "No matching closing brace found in the definition of cbuffer ", CBufferName );
    ++Token; // Skip closing brace
    // cbuffer CBufferName
    // {                   
    //    ...
    // }
    // int a
    // ^
 
    if( Token == m_Tokens.end() || Token->Type != TokenType::Semicolon )
    {
        m_Tokens.insert( Token, TokenInfo( TokenType::Semicolon, ";" ) );
        // cbuffer CBufferName
        // {                   
        //    ...
        // };
        // int a;
        // ^
    }
}


// The function finds all sampler states in the current scope ONLY, and puts them into the 
// hash table. The hash table indicates if the sampler is comparison or not. It is required to 
// match HLSL texture declaration to sampler* or sampler*Shadow.
//
// GLSL only allows samplers as uniform variables and function agruments. It does not allow
// local variables of sampler type. So the two possible scopes the function can process are
// global scope and the function argument list.
//
// Only samplers in the current scope are processed, all samplers in nested scopes are ignored
//
// After the function returns, Token points to the end of the scope (m_Tokens.end() for global scope,
// or closing bracket for the function argument list)
//
// Example 1:
//
//   Token
//   |
//    SamplerState g_Sampler;
//    SamplerComparsionState g_CmpSampler;
//    void Function(in SamplerState in_Sampler)
//    {
//    }
//
// SamplersHash = { {g_Sampler, "false"}, { {g_CmpSampler, "true"} }
//
// Example 2:
//
//    SamplerState g_Sampler;
//    SamplerComparsionState g_CmpSampler;
//                 Token
//                 |
//    void Function(in SamplerState in_Sampler)
//    {
//    }
//
// SamplersHash = { {in_Sampler, "false"} }
//
void HLSL2GLSLConverter::ParseSamplers( TokenListType::iterator &Token, SamplerHashType &SamplersHash )
{
    VERIFY_EXPR( Token->Type == TokenType::OpenBracket || Token->Type == TokenType::OpenBrace || Token == m_Tokens.begin() );
    Uint32 ScopeDepth = 1;
    bool IsFunctionArgumentList = Token->Type == TokenType::OpenBracket;

    // Skip scope start symbol, which is either open bracket or m_Tokens.begin()
    ++Token; 
    while( Token != m_Tokens.end() && ScopeDepth > 0 )
    {
        if( Token->Type == TokenType::OpenBracket ||
            Token->Type == TokenType::OpenBrace )
        {
            // Increase scope depth
            ++ScopeDepth;
            ++Token;
        }
        else if(Token->Type == TokenType::ClosingBracket ||
                Token->Type == TokenType::ClosingBrace )
        {
            // Decrease scope depth
            --ScopeDepth;
            if( ScopeDepth == 0 )
                break;
            ++Token;
        }
        else if( ( Token->Type == TokenType::SamplerState || 
                   Token->Type == TokenType::SamplerComparisonState ) &&
                   // ONLY parse sampler states in the current scope, skip
                   // all nested scopes
                   ScopeDepth == 1 )
        {
            const auto &SamplerType = Token->Literal;
            bool bIsComparison = Token->Type == TokenType::SamplerComparisonState;
            // SamplerState LinearClamp;
            // ^
            ++Token;

            // There may be a number of samplers declared after single
            // Sampler[Comparison]State keyword:
            // SamplerState Tex2D1_sampler, Tex2D2_sampler;
            do
            {
                // SamplerState LinearClamp;
                //              ^
                VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "Unexpected EOF in ", SamplerType, " declaration" );
                VERIFY_PARSER_STATE( Token, Token->Type == TokenType::Identifier, "Missing identifier in ", SamplerType, " declaration" );
                const auto &SamplerName = Token->Literal;

                // Add sampler state into the hash map
                SamplersHash.insert( std::make_pair( SamplerName, bIsComparison ) );

                ++Token;
                // SamplerState LinearClamp ;
                //                          ^

                // We cannot just remove sampler declarations, because samplers can
                // be passed to functions as arguments.
                // SamplerState and SamplerComparisonState are #defined as int, so all
                // sampler variables will just be unused global variables or function parameters. 
                // Hopefully GLSL compiler will be able to optimize them out.

                if( IsFunctionArgumentList )
                {
                    // In function argument list, every arument
                    // has its own type declaration
                    break;
                }

                // Go to the next sampler declaraion or statement end
                while( Token != m_Tokens.end() && Token->Type != TokenType::Comma && Token->Type != TokenType::Semicolon )
                    ++Token;
                VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "Unexpected EOF while parsing ", SamplerType, " declaration" );

                if( Token->Type == TokenType::Comma )
                {
                    // SamplerState Tex2D1_sampler, Tex2D2_sampler ;
                    //                            ^
                    ++Token;
                    // SamplerState Tex2D1_sampler, Tex2D2_sampler ;
                    //                              ^
                }
                else
                {
                    // SamplerState Tex2D1_sampler, Tex2D2_sampler ;
                    //                                             ^
                    break;
                }
            }while( Token != m_Tokens.end() );
        }
        else
            ++Token;
    }
    VERIFY_PARSER_STATE( Token, ScopeDepth == 1 && Token == m_Tokens.end() || ScopeDepth == 0, "Error parsing scope" );
}

void ParseImageFormat(const String &Comment, String& ImageFormat)
{
    //    /* format = r32f */ 
    // ^
    auto Pos = Comment.begin();
    if( SkipDelimeters( Comment, Pos ) )
        return;
    //    /* format = r32f */ 
    //    ^
    if( *Pos != '/' )
        return;
    ++Pos;
    //    /* format = r32f */ 
    //     ^
    //    // format = r32f
    //     ^
    if( Pos == Comment.end() || (*Pos != '/' && *Pos != '*') )
        return;
    ++Pos;
    //    /* format = r32f */ 
    //      ^
    if( SkipDelimeters( Comment, Pos ) )
        return;
    //    /* format = r32f */ 
    //       ^
    if( !SkipPrefix( "format", Pos, Comment.end() ) )
        return;
    //    /* format = r32f */ 
    //             ^
    if( SkipDelimeters( Comment, Pos ) )
        return;
    //    /* format = r32f */ 
    //              ^
    if( *Pos != '=' )
        return;
    ++Pos;
    //    /* format = r32f */ 
    //               ^
    if( SkipDelimeters( Comment, Pos ) )
        return;
    //    /* format = r32f */ 
    //                ^

    auto ImgFmtStartPos = Pos;
    SkipIdentifier( Comment, Pos );

    ImageFormat = String( ImgFmtStartPos, Pos );
}

// The function processes texture declaration that is indicated by Token, converts it to
// corresponding GLSL sampler type and adds the new sampler into Objects hash map.
//
// Samplers is the stack of sampler states found in all nested scopes.
// GLSL only supports samplers as global uniform variables or function arguments. 
// Consequently, there are two possible levels in Samplers stack:
// level 0 - global sampler states (always present)
// level 1 - samplers declared as function arguments (only present when parsing function body)
//
// The function uses the following rules to convert HLSL texture declaration into GLSL sampler:
// - HLSL texture dimension defines GLSL sampler dimension:
//      - Texture2D   -> sampler2D
//      - TextureCube -> samplerCube
// - HLSL texture component type defines GLSL sampler type. If no type is specified, float4 is assumed:
//      - Texture2D<float>     -> sampler2D
//      - Texture3D<uint4>     -> usampler3D
//      - Texture2DArray<int2> -> isampler2DArray
//      - Texture2D            -> sampler2D
// - To distinguish if sampler should be shadow or not, the function tries to find <Texture Name>_sampler
//   in the provided sampler state stack. If the sampler type is comparison, the texture is converted
//   to shadow sampler. If sampler state is either not comparison or not found, regular sampler is used
//   Examples:
//      - Texture2D g_ShadowMap;                        -> sampler2DShadow 
//        SamplerComparisonState g_ShadowMap_sampler;
//      - Texture2D g_Tex2D;                            -> sampler2D g_Tex2D;
//        SamplerState g_Tex2D_sampler;
//        Texture3D g_Tex3D;                            -> sampler3D g_Tex3D;
//
void HLSL2GLSLConverter::ProcessTextureDeclaration( TokenListType::iterator &Token, 
                                                     const std::vector<SamplerHashType> &Samplers, 
                                                     ObjectsTypeHashType &Objects )
{
    auto TexDeclToken = Token;
    auto TextureDim = TexDeclToken->Type;
    // Texture2D < float > ... ;
    // ^
    bool IsRWTexture = 
        TextureDim == TokenType::RWTexture1D      ||
        TextureDim == TokenType::RWTexture1DArray ||
        TextureDim == TokenType::RWTexture2D      ||
        TextureDim == TokenType::RWTexture2DArray ||
        TextureDim == TokenType::RWTexture3D;

    ++Token;
    // Texture2D < float > ... ;
    //           ^
#define CHECK_EOF() VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "Unexpected EOF in ", TexDeclToken->Literal, " declaration" )
    CHECK_EOF();

    auto TypeDefinitionStart = Token;
    String GLSLSampler;
    String LayoutQualifier;
    Uint32 NumComponents = 0;
    if( Token->Literal == "<" )
    {
        // Fix token type
        VERIFY_EXPR( Token->Type == TokenType::ComparisonOp );
        Token->Type = TokenType::OpenAngleBracket;

        ++Token;
        CHECK_EOF();
        // Texture2D < float > ... ;
        //             ^
        auto TexFmtToken = Token;
        VERIFY_PARSER_STATE( Token, Token->Type == TokenType::BuiltInType, "Texture format type must be built-in type" );
        if( Token->Literal == "float"  || Token->Literal == "float2" || 
            Token->Literal == "float3" || Token->Literal == "float4" )
        { 
            if( Token->Literal == "float" )
                NumComponents = 1;
            else
                NumComponents = Token->Literal.back() - '0';
        }
        else if( Token->Literal == "int"  || Token->Literal == "int2" || 
                 Token->Literal == "int3" || Token->Literal == "int4" )
        { 
            GLSLSampler.push_back( 'i' );
            if( Token->Literal == "int" )
                NumComponents = 1;
            else
                NumComponents = Token->Literal.back() - '0';
        }
        else if( Token->Literal == "uint"  || Token->Literal == "uint2" || 
                 Token->Literal == "uint3" || Token->Literal == "uint4" )
        { 
            GLSLSampler.push_back( 'u' );
            if( Token->Literal == "uint" )
                NumComponents = 1;
            else
                NumComponents = Token->Literal.back() - '0';
        }
        else
        {
            VERIFY_PARSER_STATE( Token, false, Token->Literal, " is not valid texture component type\n"
                                 "Only the following texture element types are supported: float[2,3,4], int[2,3,4], uint[2,3,4]");
        }
        VERIFY_PARSER_STATE( Token, NumComponents >= 1 && NumComponents <= 4, "Between 1 and 4 components expected, ", NumComponents ," deduced");

        ++Token;
        CHECK_EOF();
        // Texture2D < float > ... ;
        //                   ^
        if( (TextureDim == TokenType::Texture2DMS ||
             TextureDim == TokenType::Texture2DMSArray ) &&
             Token->Literal == "," )
        {
            // Texture2DMS < float, 4 > ... ;
            //                    ^
            ++Token;
            CHECK_EOF();
            // Texture2DMS < float, 4 > ... ;
            //                      ^
            VERIFY_PARSER_STATE( Token, Token->Type == TokenType::NumericConstant, "Number of samples is expected in ", TexDeclToken->Literal, " declaration" );

            // We do not really need the number of samples, so just skip it
            ++Token;
            CHECK_EOF();
            // Texture2DMS < float, 4 > ... ;
            //                        ^
        }
        VERIFY_PARSER_STATE( Token, Token->Literal == ">", "Missing \">\" in ", TexDeclToken->Literal, " declaration" );
        // Fix token type
        VERIFY_EXPR( Token->Type == TokenType::ComparisonOp );
        Token->Type = TokenType::ClosingAngleBracket;

        if( IsRWTexture )
        {
            String ImgFormat;
            // RWTexture2D<float /* format = r32f */ >
            //                                       ^
            ParseImageFormat( Token->Delimiter, ImgFormat );
            if( ImgFormat.length() == 0 )
            {
                // RWTexture2D</* format = r32f */ float >
                //                                 ^
                //                            TexFmtToken
                ParseImageFormat( TexFmtToken->Delimiter, ImgFormat );
            }

            if( ImgFormat.length() != 0 )
            {
                LayoutQualifier = String("layout(") + ImgFormat + ") ";
            }
        }

        ++Token;
        // Texture2D < float > TexName ;
        //                     ^
        CHECK_EOF();
    }

    if( IsRWTexture )
        GLSLSampler.append( "image" );
    else
        GLSLSampler.append( "sampler" );

    switch( TextureDim )
    {
        case TokenType::RWTexture1D:
        case TokenType::Texture1D:          GLSLSampler += "1D";        break;

        case TokenType::RWTexture1DArray:
        case TokenType::Texture1DArray:     GLSLSampler += "1DArray";   break;

        case TokenType::RWTexture2D:
        case TokenType::Texture2D:          GLSLSampler += "2D";        break;

        case TokenType::RWTexture2DArray:
        case TokenType::Texture2DArray:     GLSLSampler += "2DArray";   break;

        case TokenType::RWTexture3D:
        case TokenType::Texture3D:          GLSLSampler += "3D";        break;

        case TokenType::TextureCube:        GLSLSampler += "Cube";      break;
        case TokenType::TextureCubeArray:   GLSLSampler += "CubeArray"; break;
        case TokenType::Texture2DMS:        GLSLSampler += "2DMS";      break;
        case TokenType::Texture2DMSArray:   GLSLSampler += "2DMSArray"; break;
        default: UNEXPECTED("Unexpected texture type");
    }

    //   TypeDefinitionStart
    //           |
    // Texture2D < float > TexName ;
    //                     ^
    m_Tokens.erase( TypeDefinitionStart, Token );
    // Texture2D TexName ;
    //           ^

    bool IsGlobalScope = Samplers.size() == 1;
    
    // There may be more than one texture variable declared in the same
    // statement:
    // Texture2D<float> g_Tex2D1, g_Tex2D1;
    do
    {
        // Texture2D TexName ;
        //           ^
        VERIFY_PARSER_STATE( Token, Token->Type == TokenType::Identifier, "Identifier expected in ", TexDeclToken->Literal, " declaration" );

        // Make sure there is a delimiter between sampler keyword and the
        // identifier. In cases like this
        // Texture2D<float>Name;
        // There will be no whitespace
        if( Token->Delimiter == "" )
            Token->Delimiter = " ";

        // Texture2D TexName ;
        //           ^
        const auto &TextureName = Token->Literal;

        auto CompleteGLSLSampler = GLSLSampler;
        if( !IsRWTexture )
        {
            // Try to find matching sampler
            auto SamplerName = TextureName + "_sampler";
            // Search all scopes starting with the innermost
            for( auto ScopeIt = Samplers.rbegin(); ScopeIt != Samplers.rend(); ++ScopeIt )
            {
                auto SamplerIt = ScopeIt->find( SamplerName );
                if( SamplerIt != ScopeIt->end() )
                {
                    if( SamplerIt->second )
                        CompleteGLSLSampler.append( "Shadow" );
                    break;
                }
            }
        }

        // TexDeclToken
        // |
        // Texture2D TexName ;
        //           ^
        TexDeclToken->Literal = "";
        TexDeclToken->Literal.append( LayoutQualifier );
        if( IsGlobalScope )
        {
            // Samplers and images in global scope must be declared uniform.
            // Function arguments must not be declared uniform
            TexDeclToken->Literal.append( "uniform " );
        }
        TexDeclToken->Literal.append( CompleteGLSLSampler );
        Objects.insert( std::make_pair( HashMapStringKey(TextureName), HLSLObjectInfo(CompleteGLSLSampler, NumComponents) ) );

        // In global sceop, multiple variables can be declared in the same statement
        if( IsGlobalScope )
        {
            // Texture2D TexName, TexName2 ;
            //           ^

            // Go to the next texture in the declaration or to the statement end
            while( Token != m_Tokens.end() && Token->Type != TokenType::Comma && Token->Type != TokenType::Semicolon )
                ++Token;
            if( Token->Type == TokenType::Comma )
            {
                // Texture2D TexName, TexName2 ;
                //                  ^
                Token->Type = TokenType::Semicolon;
                Token->Literal = ";";
                // Texture2D TexName; TexName2 ;
                //                  ^
                
                ++Token;
                // Texture2D TexName; TexName2 ;
                //                    ^

                // Insert empty token that will contain next sampler/image declaration
                TexDeclToken = m_Tokens.insert( Token, TokenInfo(TextureDim, "", "\n") );
                // Texture2D TexName;
                // <Texture Declaration TBD> TexName2 ;
                // ^                         ^
                // TexDeclToken              Token 
            }
            else
            {
                // Texture2D TexName, TexName2 ;
                //                             ^
                ++Token;
                break;
            }
        }

    } while( IsGlobalScope && Token != m_Tokens.end() );

#undef SKIP_DELIMITER
#undef CHECK_EOF
}


// Finds an HLSL object with the given name in object stack
const HLSL2GLSLConverter::HLSLObjectInfo *HLSL2GLSLConverter::FindHLSLObject( const String &Name )
{
    for( auto ScopeIt = m_Objects.rbegin(); ScopeIt != m_Objects.rend(); ++ScopeIt )
    {
        auto It = ScopeIt->find( Name.c_str() );
        if( It != ScopeIt->end() )
            return &It->second;
    }
    return nullptr;
}

Uint32 HLSL2GLSLConverter::CountFunctionArguments( TokenListType::iterator &Token, const TokenListType::iterator &ScopeEnd )
{
    Uint32 NumArguments = 0;
    int NumOpenBrackets = 1;
    ++Token;
    while( Token != ScopeEnd && NumOpenBrackets != 0 )
    {
        // Do not count arguments of nested functions:
        // TestText.Sample( TestText_sampler, float2(0.0, 1.0)  );
        //                                           ^
        //                                        NumOpenBrackets == 2
        if( NumOpenBrackets == 1 && (Token->Literal == "," || Token->Type == TokenType::ClosingBracket) )
            ++NumArguments;

        if( Token->Type == TokenType::OpenBracket )
            ++NumOpenBrackets;
        else if( Token->Type == TokenType::ClosingBracket )
            --NumOpenBrackets;

        ++Token;
    }
    return NumArguments;
}

// The function processes HLSL object method in current scope and replaces it
// with the corresponding GLSL function stub
// Example:
// Texture2D<float2> Tex2D;
// ...
// Tex2D.Sample(Tex2D_sampler, f2UV) -> Sample_2(Tex2D, Tex2D_sampler, f2UV)_SWIZZLE2
bool HLSL2GLSLConverter::ProcessObjectMethod(TokenListType::iterator &Token, 
                                              const TokenListType::iterator &ScopeStart, 
                                              const TokenListType::iterator &ScopeEnd)
{
    // TestText.Sample( ...
    //         ^
    //      DotToken
    auto DotToken = Token;
    VERIFY_EXPR(DotToken != ScopeEnd && Token->Literal == ".");
    auto MethodToken = DotToken;
    ++MethodToken;
    VERIFY_EXPR( MethodToken != ScopeEnd && MethodToken->Type == TokenType::Identifier);
    // TestText.Sample( ...
    //          ^
    //     MethodToken
    auto IdentifierToken = DotToken;
    // m_Tokens contains dummy node at the beginning, so we can
    // check for ScopeStart to break the loop
    while( IdentifierToken != ScopeStart && IdentifierToken->Type !=  TokenType::Identifier)
        --IdentifierToken;
    if( IdentifierToken == ScopeStart )
        return false;
    // TestTextArr[2].Sample( ...
    // ^
    // IdentifierToken

    // Try to find identifier
    const auto *pObjectInfo = FindHLSLObject(IdentifierToken->Literal);
    if( pObjectInfo == nullptr )
    {
        return false;
    }
    const auto &ObjectType = pObjectInfo->GLSLType;

    auto ArgsListStartToken = MethodToken;
    ++ArgsListStartToken;

    // TestText.Sample( ...
    //                ^
    //     ArgsListStartToken

    if( ArgsListStartToken == ScopeEnd || ArgsListStartToken->Type != TokenType::OpenBracket )
        return false;
    auto ArgsListEndToken = ArgsListStartToken;
    Uint32 NumArguments = CountFunctionArguments( ArgsListEndToken, ScopeEnd );

    if( ArgsListEndToken == ScopeEnd )
        return false;
    // TestText.Sample( TestText_sampler, float2(0.0, 1.0)  );
    //                                                       ^
    //                                               ArgsListEndToken
    auto Stub = m_GLSLStubs.find( FunctionStubHashKey(ObjectType, MethodToken->Literal.c_str(), NumArguments) );
    if( Stub == m_GLSLStubs.end() )
    {
        LOG_ERROR_MESSAGE( "Unable to find function stub for ", IdentifierToken->Literal, ".", MethodToken->Literal, "(", NumArguments, " args). GLSL object type: ", ObjectType  );
        return false;
    }

    //            DotToken
    //               V
    // TestTextArr[2].Sample( TestTextArr_sampler, ... 
    // ^                    ^
    // IdentifierToken      ArgsListStartToken 
   
    *ArgsListStartToken = TokenInfo(TokenType::Comma, ",");
    // TestTextArr[2].Sample, TestTextArr_sampler, ... 
    //               ^      ^
    //           DotToken  ArgsListStartToken

    m_Tokens.erase(DotToken, ArgsListStartToken);
    // TestTextArr[2], TestTextArr_sampler, ... 
    // ^    
    // IdentifierToken

    m_Tokens.insert( IdentifierToken, TokenInfo( TokenType::Identifier, Stub->second.Name.c_str(), IdentifierToken->Delimiter.c_str()) );
    IdentifierToken->Delimiter = " ";
    // FunctionStub TestTextArr[2], TestTextArr_sampler, ... 
    //              ^    
    //              IdentifierToken


    m_Tokens.insert( IdentifierToken, TokenInfo( TokenType::OpenBracket, "(") );
    // FunctionStub( TestTextArr[2], TestTextArr_sampler, ... 
    //               ^    
    //               IdentifierToken

    Token = ArgsListStartToken;
    // FunctionStub( TestTextArr[2], TestTextArr_sampler, ... 
    //                             ^    
    //                           Token

    // Nested function calls will be automatically processed:
    // FunctionStub( TestTextArr[2], TestTextArr_sampler, TestTex.Sample(...
    //                             ^    
    //                           Token

    
    // Add swizzling if there is any
    if( Stub->second.Swizzle.length() > 0 )
    {
        // FunctionStub( TestTextArr[2], TestTextArr_sampler, ...    );
        //                                                            ^    
        //                                                     ArgsListEndToken

        auto SwizzleToken = m_Tokens.insert( ArgsListEndToken, TokenInfo(TokenType::TextBlock, Stub->second.Swizzle.c_str(), "") );
        SwizzleToken->Literal.push_back( '0' + pObjectInfo->NumComponents );
        // FunctionStub( TestTextArr[2], TestTextArr_sampler, ...    )_SWIZZLE4;
        //                                                                     ^    
        //                                                            ArgsListEndToken
    }
    return true;
}

void HLSL2GLSLConverter::RemoveFlowControlAttribute( TokenListType::iterator &Token )
{
    VERIFY_EXPR( Token->Type == TokenType::FlowControl );
    // [ branch ] if ( ...
    //            ^
    auto PrevToken = Token;
    --PrevToken;
    // [ branch ] if ( ...
    //          ^
    // Note that dummy empty token is inserted into the beginning of the list
    if( PrevToken == m_Tokens.begin() || PrevToken->Type != TokenType::ClosingStaple )
        return;
    
    --PrevToken;
    // [ branch ] if ( ...
    //   ^
    if( PrevToken == m_Tokens.begin() || PrevToken->Type != TokenType::Identifier )
        return;
        
    --PrevToken;
    // [ branch ] if ( ...
    // ^
    if( PrevToken == m_Tokens.begin() || PrevToken->Type != TokenType::OpenStaple )
        return;
        
    //  [ branch ] if ( ...
    //  ^          ^
    // PrevToken   Token
    Token->Delimiter = PrevToken->Delimiter;
    m_Tokens.erase( PrevToken, Token );
}

// The function finds all HLSL object methods in the current scope and calls ProcessObjectMethod()
// that replaces them with the corresponding GLSL function stub.
void HLSL2GLSLConverter::ProcessObjectMethods(const TokenListType::iterator &ScopeStart, 
                                               const TokenListType::iterator &ScopeEnd)
{
    auto Token = ScopeStart;
    while( Token != ScopeEnd )
    {
        // Search for .identifier pattern

        if( Token->Literal == "." )
        {
            auto DotToken = Token;
            ++Token;
            if( Token == ScopeEnd )
                break;
            if( Token->Type == TokenType::Identifier )
            {
                if( ProcessObjectMethod( DotToken, ScopeStart, ScopeEnd ) )
                    Token = DotToken;
            }
            else
            {
                ++Token;
                continue;
            }
        }
        else
            ++Token;
    }
}

// The function processes HLSL RW texture operator [] and replaces it with
// corresponding imageStore GLSL function.
// Example:
// RWTex[Location] = f3Value -> imageStore( RWTex,Location, _ExpandVector(f3Value))
// _ExpandVector() function expands any input vector to 4-component vector
bool HLSL2GLSLConverter::ProcessRWTextureStore( TokenListType::iterator &Token, 
                                                 const TokenListType::iterator &ScopeEnd )
{
    // RWTex[Location.x] = float4(0.0, 0.0, 0.0, 1.0);
    // ^
    auto AssignmentToken = Token;
    while( AssignmentToken != ScopeEnd && 
          !(AssignmentToken->Type == TokenType::Assignment || AssignmentToken->Type == TokenType::Semicolon) )
        ++AssignmentToken;

    // The function is called for ALL RW texture objects found, so this may not be
    // the store operation, but something else (for instance:
    // InterlockedExchange(Tex2D_I1[GTid.xy], 1, iOldVal) )
    if( AssignmentToken == ScopeEnd || AssignmentToken->Type != TokenType::Assignment )
        return false;
    // RWTex[Location.x] = float4(0.0, 0.0, 0.0, 1.0);
    //                   ^
    //            AssignmentToken
    auto ClosingStaplePos = AssignmentToken;
    while( ClosingStaplePos != Token && ClosingStaplePos->Type != TokenType::ClosingStaple )
        --ClosingStaplePos;
    if( ClosingStaplePos == Token )
        return false;
    // RWTex[Location.x] = float4(0.0, 0.0, 0.0, 1.0);
    //                 ^
    //          ClosingStaplePos

    auto OpenStaplePos = ClosingStaplePos;
    while( OpenStaplePos != Token && OpenStaplePos->Type != TokenType::OpenStaple )
        --OpenStaplePos;
    if( OpenStaplePos == Token )
        return false;
    // RWTex[Location.x] = float4(0.0, 0.0, 0.0, 1.0);
    //      ^
    //  OpenStaplePos

    auto SemicolonToken = AssignmentToken;
    while( SemicolonToken != ScopeEnd && SemicolonToken->Type != TokenType::Semicolon )
        ++SemicolonToken;
    if( SemicolonToken == ScopeEnd )
        return false;
    // RWTex[Location.x] = float4(0.0, 0.0, 0.0, 1.0);
    // ^                                             ^
    // Token                                    SemicolonToken

    m_Tokens.insert( Token, TokenInfo(TokenType::Identifier, "imageStore", Token->Delimiter.c_str()) );
    m_Tokens.insert( Token, TokenInfo(TokenType::OpenBracket, "(", "" ) );
    Token->Delimiter = " ";
    // imageStore( RWTex[Location.x] = float4(0.0, 0.0, 0.0, 1.0);

    OpenStaplePos->Delimiter = "";
    OpenStaplePos->Type = TokenType::Comma;
    OpenStaplePos->Literal = ",";
    // imageStore( RWTex,Location.x] = float4(0.0, 0.0, 0.0, 1.0);
    //                             ^
    //                         ClosingStaplePos

    auto LocationToken = OpenStaplePos;
    ++LocationToken;
    m_Tokens.insert( LocationToken, TokenInfo( TokenType::Identifier, "_ToIvec", " " ) );
    m_Tokens.insert( LocationToken, TokenInfo( TokenType::OpenBracket, "(", "" ) );
    // imageStore( RWTex, _ToIvec(Location.x] = float4(0.0, 0.0, 0.0, 1.0);
    //                                      ^
    //                               ClosingStaplePos

    m_Tokens.insert( ClosingStaplePos, TokenInfo( TokenType::ClosingBracket, ")", "" ) );
    // imageStore( RWTex, _ToIvec(Location.x)] = float4(0.0, 0.0, 0.0, 1.0);
    //                                       ^
    //                                ClosingStaplePos

    ClosingStaplePos->Delimiter = "";
    ClosingStaplePos->Type = TokenType::Comma;
    ClosingStaplePos->Literal = ",";
    // imageStore( RWTex, _ToIvec(Location.x), = float4(0.0, 0.0, 0.0, 1.0);
    //                                         ^
    //                                   AssignmentToken

    AssignmentToken->Delimiter = "";
    AssignmentToken->Type = TokenType::OpenBracket;
    AssignmentToken->Literal = "(";
    // imageStore( RWTex, _ToIvec(Location.x),( float4(0.0, 0.0, 0.0, 1.0);
    //                                        ^

    m_Tokens.insert( AssignmentToken, TokenInfo(TokenType::Identifier, "_ExpandVector", " " ) );
    // imageStore( RWTex, _ToIvec(Location.x), _ExpandVector( float4(0.0, 0.0, 0.0, 1.0);
    //                                                      ^

    // Insert closing bracket for _ExpandVector
    m_Tokens.insert( SemicolonToken, TokenInfo(TokenType::ClosingBracket, ")", "" ) );
    // imageStore( RWTex,  _ToIvec(Location.x), _ExpandVector( float4(0.0, 0.0, 0.0, 1.0));

    // Insert closing bracket for imageStore
    m_Tokens.insert( SemicolonToken, TokenInfo(TokenType::ClosingBracket, ")", "" ) );
    // imageStore( RWTex,  _ToIvec(Location.x), _ExpandVector( float4(0.0, 0.0, 0.0, 1.0)));

    return false;
}

// Function finds all RW textures in current scope and calls ProcessRWTextureStore()
// that detects if this is store operation and converts it to imageStore()
void HLSL2GLSLConverter::ProcessRWTextures(const TokenListType::iterator &ScopeStart, 
                                            const TokenListType::iterator &ScopeEnd)
{
    auto Token = ScopeStart;
    while( Token != ScopeEnd )
    {
        if( Token->Type == TokenType::Identifier )
        {
            // Try to find the object in all scopes
            const auto *pObjectInfo = FindHLSLObject(Token->Literal);
            if( pObjectInfo == nullptr )
            {
                ++Token;
                continue;
            }

            // Check if the object is image type
            auto ImgTypeIt = m_ImageTypes.find( pObjectInfo->GLSLType.c_str() );
            if( ImgTypeIt == m_ImageTypes.end() )
            {
                ++Token;
                continue;
            }

            // Handle store. If this is not store operation, 
            // ProcessRWTextureStore() returns false.
            auto TmpToken = Token;
            if( ProcessRWTextureStore( TmpToken, ScopeEnd ) )
                Token = TmpToken;
            else
                ++Token;
        }
        else
            ++Token;
    }
}

// The function processes all atomic operations in current scope and replaces them with
// corresponding GLSL function
void HLSL2GLSLConverter::ProcessAtomics(const TokenListType::iterator &ScopeStart, 
                                         const TokenListType::iterator &ScopeEnd)
{
    auto Token = ScopeStart;
    while( Token != ScopeEnd )
    {
        if( Token->Type == TokenType::Identifier )
        {
            auto AtomicIt = m_AtomicOperations.find(Token->Literal.c_str());
            if( AtomicIt == m_AtomicOperations.end() )
            {
                ++Token;
                continue;
            }

            auto OperationToken = Token;
            // InterlockedAdd(g_i4SharedArray[GTid.x].x, 1, iOldVal);
            // ^
            ++Token;
            // InterlockedAdd(g_i4SharedArray[GTid.x].x, 1, iOldVal);
            //               ^
            VERIFY_PARSER_STATE( Token, Token != ScopeEnd, "Unexpected EOF" );
            VERIFY_PARSER_STATE( Token, Token->Type == TokenType::OpenBracket, "Open bracket is expected" );
            
            ++Token;
            // InterlockedAdd(g_i4SharedArray[GTid.x].x, 1, iOldVal);
            //                ^
            VERIFY_PARSER_STATE( Token, Token != ScopeEnd, "Unexpected EOF" );
            VERIFY_PARSER_STATE( Token, Token->Type == TokenType::Identifier, "Identifier is expected" );
            
            auto ArgsListEndToken = Token;
            auto NumArguments = CountFunctionArguments( ArgsListEndToken, ScopeEnd );
            // InterlockedAdd(Tex2D[GTid.xy], 1, iOldVal);
            //                                           ^
            //                                       ArgsListEndToken
            VERIFY_PARSER_STATE( ArgsListEndToken, ArgsListEndToken != ScopeEnd, "Unexpected EOF" );

            const auto *pObjectInfo = FindHLSLObject(Token->Literal);
            if( pObjectInfo != nullptr )
            {
                // InterlockedAdd(Tex2D[GTid.xy], 1, iOldVal);
                //                ^
                auto Stub = m_GLSLStubs.find( FunctionStubHashKey("image", OperationToken->Literal.c_str(), NumArguments) );
                VERIFY_PARSER_STATE(OperationToken, Stub != m_GLSLStubs.end(), "Unable to find function stub for funciton ", OperationToken->Literal, " with ", NumArguments, " arguments"  );

                // Find first comma
                int NumOpenBrackets = 1;
                while( Token != ScopeEnd && NumOpenBrackets != 0 )
                {
                    // Do not count arguments of nested functions:
                    if( NumOpenBrackets == 1 && (Token->Type == TokenType::Comma || Token->Type == TokenType::ClosingBracket) )
                        break;

                    if( Token->Type == TokenType::OpenBracket )
                        ++NumOpenBrackets;
                    else if( Token->Type == TokenType::ClosingBracket )
                        --NumOpenBrackets;

                    ++Token;
                }
                // InterlockedAdd(Tex2D[GTid.xy], 1, iOldVal);
                //                              ^
                VERIFY_PARSER_STATE( Token, Token != ScopeEnd, "Unexpected EOF" );
                VERIFY_PARSER_STATE( Token, Token->Type == TokenType::Comma, "Comma is expected" );

                --Token;
                // InterlockedAdd(Tex2D[GTid.xy], 1, iOldVal);
                //                             ^
                VERIFY_PARSER_STATE( Token, Token->Type == TokenType::ClosingStaple, "Expected \']\'" );
                auto ClosingBracketToken = Token;
                --Token;
                m_Tokens.erase( ClosingBracketToken );
                // InterlockedAdd(Tex2D[GTid.xy, 1, iOldVal);
                //                           ^
                while( Token != ScopeStart && Token->Type != TokenType::OpenStaple )
                    --Token;
                // InterlockedAdd(Tex2D[GTid.xy, 1, iOldVal);
                //                     ^

                VERIFY_PARSER_STATE( Token, Token != ScopeStart, "Expected \'[\'" );
                Token->Type = TokenType::Comma;
                Token->Literal = ",";
                // InterlockedAdd(Tex2D,GTid.xy, 1, iOldVal);
                //                     ^

                OperationToken->Literal = Stub->second.Name;
                // InterlockedAddImage_3(Tex2D,GTid.xy, 1, iOldVal);
            }
            else
            {
                // InterlockedAdd(g_i4SharedArray[GTid.x].x, 1, iOldVal);
                //                ^
                auto Stub = m_GLSLStubs.find( FunctionStubHashKey("shared_var", OperationToken->Literal.c_str(), NumArguments) );
                VERIFY_PARSER_STATE(OperationToken, Stub != m_GLSLStubs.end(), "Unable to find function stub for funciton ", OperationToken->Literal, " with ", NumArguments, " arguments"  );
                OperationToken->Literal = Stub->second.Name;
                // InterlockedAddSharedVar_3(g_i4SharedArray[GTid.x].x, 1, iOldVal);
            }
            Token = ArgsListEndToken;
        }
        else
            ++Token;
    }
}

// The function parses shader arguments and puts them into Params array
void HLSL2GLSLConverter::ParseShaderParameters( TokenListType::iterator &Token, std::vector<ShaderParameterInfo>& Params )
{
    // void TestPS  ( in VSOutput In,
    //              ^
    VERIFY_EXPR(Token->Type == TokenType::OpenBracket);
    ++Token;
    // void TestPS  ( in VSOutput In,
    //                ^
    while(Token != m_Tokens.end())
    {
        ShaderParameterInfo ParamInfo;
        if( Token->Literal == "in" )
        {
            //void TestPS  ( in VSOutput In,
            //               ^
            ParamInfo.storageQualifier = ShaderParameterInfo::StorageQualifier::In;
            ++Token;
            //void TestPS  ( in VSOutput In,
            //                  ^
        }
        else if( Token->Literal == "out" )
        {
            //          out float4 Color : SV_Target,
            //          ^
            ParamInfo.storageQualifier = ShaderParameterInfo::StorageQualifier::Out;
            ++Token;
            //          out float4 Color : SV_Target,
            //              ^
        }
        else
            ParamInfo.storageQualifier = ShaderParameterInfo::StorageQualifier::In;
        VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "Unexpected EOF while parsing argument list" );
        VERIFY_PARSER_STATE( Token, Token->Type == TokenType::BuiltInType || Token->Type == TokenType::Identifier, 
                             "Missing argument type" );

        ParamInfo.Type = Token->Literal;

        ++Token;
        //          out float4 Color : SV_Target,
        //                     ^
        VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "Unexpected EOF while parsing argument list" );
        VERIFY_PARSER_STATE( Token, Token->Type == TokenType::Identifier, "Missing argument name after ", ParamInfo.Type );
        ParamInfo.Name = Token->Literal;

        ++Token;
        //          out float4 Color : SV_Target,
        //                           ^
        VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "Unexpected end of file after argument \"", ParamInfo.Name, '\"' );
        if( Token->Literal == ":" )
        {
            ++Token;
            //          out float4 Color : SV_Target,
            //                             ^
            VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "Unexpected end of file while looking for semantic for argument \"", ParamInfo.Name, '\"' );
            VERIFY_PARSER_STATE( Token, Token->Type == TokenType::Identifier, "Missing semantic for argument \"", ParamInfo.Name, '\"' );
            ParamInfo.Semantic = Token->Literal;
            // Transform to lower case -  semantics are case-insensitive
            std::transform( ParamInfo.Semantic.begin(), ParamInfo.Semantic.end(), ParamInfo.Semantic.begin(), ::tolower );
            
            ++Token;
            //          out float4 Color : SV_Target,
            //                                      ^
        }

        VERIFY_PARSER_STATE( Token, Token->Literal == "," || Token->Type == TokenType::ClosingBracket, "\',\' or \')\' is expected after argument \"", ParamInfo.Name, '\"' );
        Params.push_back( ParamInfo );
        if( Token->Type == TokenType::ClosingBracket )
            break;
        ++Token;
    }
}

void DeclareVariable(const String& Type, const String& Name, const Char* InitValue, bool bForceType, std::stringstream &OutSS)
{
    OutSS << Type << ' ' << Name;
    if( InitValue )
    {
        OutSS << " = ";
        if( bForceType )
        {
            OutSS << Type << '(';
        }
        OutSS << InitValue;
        if( bForceType )
            OutSS <<  ')';
    }
    OutSS << ";\n" ;
}

void DeclareInterfaceBlock( const Char* Qualifier, Uint32 InterfaceBlockNum, const String& ParamType, const String& ParamName, std::stringstream &OutSS )
{
    OutSS << Qualifier <<" _IntererfaceBlock" << InterfaceBlockNum << "\n"
          "{\n"
          "    " << ParamType << ' ' << ParamName << ";\n"
          "};\n";
}

void HLSL2GLSLConverter::ProcessFragmentShaderArguments(std::vector<ShaderParameterInfo>& Params,
                                                         String &GlobalVariables,
                                                         String &Epilogue,
                                                         String &Prologue)
{
    stringstream GlobalVarsSS, PrologueSS, EpilogueSS;
    int InterfaceBlockNum = 0;
    for( const auto &Param : Params )
    {
        if( Param.storageQualifier == ShaderParameterInfo::StorageQualifier::In )
        {
            if( Param.Semantic == "" )
            {
                DeclareInterfaceBlock( "in", InterfaceBlockNum, Param.Type, Param.Name, GlobalVarsSS );
                ++InterfaceBlockNum;
            }
            else if( Param.Semantic == "sv_position" )
            {
                DeclareVariable( Param.Type.c_str(), Param.Name.c_str(), "gl_FragCoord", false, PrologueSS );
            }
            else
            {
                LOG_ERROR_AND_THROW( "Semantic \"", Param.Semantic, "\" is not supported in a pixel shader." );
            }
        }
        else if( Param.storageQualifier == ShaderParameterInfo::StorageQualifier::Out )
        {
            const auto& Semantic = Param.Semantic;
            auto RTIndexPos = Semantic.begin();
            int RTIndex = -1;
            if( SkipPrefix( "sv_target", RTIndexPos, Semantic.end() ) )
            {
                if( RTIndexPos != Semantic.end() )
                {
                    if( *RTIndexPos >= '0' && *RTIndexPos <= '9' )
                    {
                        RTIndex = *RTIndexPos - '0';
                        if( (RTIndexPos + 1) != Semantic.end() )
                            RTIndex = -1;
                    }
                }
                else
                    RTIndex = 0;
            }

            if( RTIndex >= 0 && RTIndex < MaxRenderTargets )
            {
                String GlobalVarName = "_out_";
                GlobalVarName.append( Param.Name );

                GlobalVarsSS << "layout(location = " << RTIndex << ") out " 
                             << Param.Type << ' ' << GlobalVarName << ";\n";

                DeclareVariable( Param.Type, Param.Name, nullptr, false, PrologueSS );

                EpilogueSS << GlobalVarName << " = " << Param.Name << ";\n";
            }
            else
            {
                LOG_ERROR_AND_THROW( "Unexpected output semantic \"", Semantic, "\". The only allowed output semantic for fragment shader is SV_Target*" );
            }
        }
    }
    GlobalVariables = GlobalVarsSS.str();
    Prologue = PrologueSS.str();
    Epilogue = EpilogueSS.str();
}


void HLSL2GLSLConverter::ProcessVertexShaderArguments( std::vector<ShaderParameterInfo>& Params,
                                                        String &GlobalVariables,
                                                        String &Epilogue,
                                                        String &Prologue )
{
    stringstream GlobalVarsSS, PrologueSS, EpilogueSS;
    GlobalVarsSS <<
        "\n"
        "#ifndef GL_ES\n"
        "out gl_PerVertex\n"
        "{\n"
        "    vec4 gl_Position;\n"
        "};\n"
        "#endif\n";
    int InterfaceBlockNum = 0;
    for( const auto &Param : Params )
    {
        const auto& Semantic = Param.Semantic;
        if( Param.storageQualifier == ShaderParameterInfo::StorageQualifier::In )
        {
            auto SemanticEndPos = Semantic.begin();
            if( SkipPrefix( "attrib", SemanticEndPos, Semantic.end() ) )
            {
                char* EndPtr = nullptr;
                auto InputLocation = strtol(&*SemanticEndPos, &EndPtr, 10);
                if( EndPtr == nullptr || *EndPtr != 0 )
                {
                    LOG_ERROR_AND_THROW( "Unexpected input semantic \"", Semantic, "\". The only allowed semantic for the vertex shader input attributes is ATTRIB*" );
                }

                String GlobalVarName = "_in_";
                GlobalVarName.append( Param.Name );
                GlobalVarsSS << "layout(location = " << InputLocation <<  ") in " 
                             << Param.Type << ' ' << GlobalVarName << ";\n";

                DeclareVariable( Param.Type.c_str(), Param.Name.c_str(), GlobalVarName.c_str(), false, PrologueSS );
            }
            else if( Semantic == "sv_vertexid" )
            {
                DeclareVariable( Param.Type.c_str(), Param.Name.c_str(), "gl_VertexID", true, PrologueSS );
            }
            else if( Semantic == "sv_instanceid" )
            {
                DeclareVariable( Param.Type.c_str(), Param.Name.c_str(), "gl_InstanceID", true, PrologueSS );
            }
            else
            {
                LOG_ERROR_AND_THROW( "Unexpected input semantic \"", Semantic, "\". The only allowed semantics for the vertex shader inputs are \"ATTRIB*\", \"SV_VertexID\", and \"SV_InstanceID\"." );
            }
        }
        else if( Param.storageQualifier == ShaderParameterInfo::StorageQualifier::Out )
        {
            if( Semantic == "" )
            {
                // Should be struct
                DeclareInterfaceBlock( "out", InterfaceBlockNum, Param.Type, Param.Name, GlobalVarsSS );
                ++InterfaceBlockNum;
            }
            else if( Semantic == "sv_position" )
            {
                DeclareVariable( Param.Type.c_str(), Param.Name.c_str(), nullptr, false, PrologueSS );
                EpilogueSS << "gl_Position = " << Param.Name << ";\n" ;
            }
            else
            {
                LOG_ERROR_AND_THROW( "Unexpected output semantic \"", Semantic, "\". The only allowed semantic for the vertex shader output is \"SV_Position\"." );
            }
        }
    }
    GlobalVariables = GlobalVarsSS.str();
    Prologue = PrologueSS.str();
    Epilogue = EpilogueSS.str();
}


void HLSL2GLSLConverter::ProcessComputeShaderArguments( TokenListType::iterator &TypeToken,
                                                         std::vector<ShaderParameterInfo>& Params,
                                                         String &GlobalVariables,
                                                         String &Prologue )
{
    stringstream GlobalVarsSS, PrologueSS;

    auto Token = TypeToken;
    //[numthreads(16,16,1)]
    //void TestCS(uint3 DTid : SV_DispatchThreadID)
    //^
    --Token;
    //[numthreads(16,16,1)]
    //                    ^
    //void TestCS(uint3 DTid : SV_DispatchThreadID)
    VERIFY_PARSER_STATE( Token, Token != m_Tokens.begin() && Token->Type == TokenType::ClosingStaple, "Missing numthreads declaration");

    while( Token != m_Tokens.begin() && Token->Type != TokenType::OpenStaple )
        --Token;
    //[numthreads(16,16,1)]
    //^                    
    VERIFY_PARSER_STATE( Token, Token != m_Tokens.begin(), "Missing numthreads() declaration");
    auto OpenStapleToken = Token;

    ++Token;
    //[numthreads(16,16,1)]
    // ^                    
    VERIFY_PARSER_STATE( Token, Token != m_Tokens.end() && Token->Type == TokenType::Identifier && Token->Literal == "numthreads",
                         "Missing numthreads() declaration" );
    
    ++Token;
    //[numthreads(16,16,1)]
    //           ^                    
    VERIFY_PARSER_STATE( Token, Token != m_Tokens.end() && Token->Type == TokenType::OpenBracket,
                         "Missing \'(\' after numthreads" );

    String CSGroupSize[3] = {};
    static const Char *DirNames[] = { "X", "Y", "Z" };
    for( int i = 0; i < 3; ++i )
    {
        ++Token;
        //[numthreads(16,16,1)]
        //            ^                    
        VERIFY_PARSER_STATE( Token, Token != m_Tokens.end() && (Token->Type == TokenType::NumericConstant || Token->Type == TokenType::Identifier),
                             "Missing group size for ", DirNames[i], " direction" );
        CSGroupSize[i] = Token->Literal.c_str();
        ++Token;
        //[numthreads(16,16,1)]
        //              ^    ^                 
        const Char* ExpectedLiteral = (i < 2) ? "," : ")";
        VERIFY_PARSER_STATE( Token, Token != m_Tokens.end() && Token->Literal == ExpectedLiteral,
                                "Missing \'", ExpectedLiteral, "\' after ", DirNames[i], " direction" );
    }

    //OpenStapleToken
    //V
    //[numthreads(16,16,1)]
    //void TestCS(uint3 DTid : SV_DispatchThreadID)
    //^
    //TypeToken
    TypeToken->Delimiter = OpenStapleToken->Delimiter;
    m_Tokens.erase( OpenStapleToken, TypeToken );
    // 
    // void TestCS(uint3 DTid : SV_DispatchThreadID)

    GlobalVarsSS << "layout ( local_size_x = " << CSGroupSize[0] 
                 << ", local_size_y = " << CSGroupSize[1] << ", local_size_z = " << CSGroupSize[2] << " ) in;\n";

    for( const auto &Param : Params )
    {
        const auto& Semantic = Param.Semantic;
        if( Param.storageQualifier == ShaderParameterInfo::StorageQualifier::In )
        {
            auto SemanticEndPos = Semantic.begin();
            if( Semantic == "sv_dispatchthreadid" )
            {
                DeclareVariable( Param.Type.c_str(), Param.Name.c_str(), "gl_GlobalInvocationID", true, PrologueSS );
            }
            else if( Semantic == "sv_groupid" )
            {
                DeclareVariable( Param.Type.c_str(), Param.Name.c_str(), "gl_WorkGroupID", true, PrologueSS );
            }
            else if( Semantic == "sv_groupthreadid" )
            {
                DeclareVariable( Param.Type.c_str(), Param.Name.c_str(), "gl_LocalInvocationID", true, PrologueSS );
            }
            else if( Semantic == "sv_groupindex" )
            {
                DeclareVariable( Param.Type.c_str(), Param.Name.c_str(), "gl_LocalInvocationIndex", true, PrologueSS );
            }
            else
            {
                LOG_ERROR_AND_THROW( "Unexpected input semantic \"", Semantic, "\". The only allowed semantics for the vertex shader inputs are \"ATTRIB*\", \"SV_VertexID\", and \"SV_InstanceID\"." );
            }
        }
        else if( Param.storageQualifier == ShaderParameterInfo::StorageQualifier::Out )
        {
            LOG_ERROR_AND_THROW( "Output variables are not allowed in compute shaders" );
        }
    }

    GlobalVariables = GlobalVarsSS.str();
    Prologue = PrologueSS.str();
}

void HLSL2GLSLConverter::ProcessReturnStatements( TokenListType::iterator &Token, const String &Epilogue, const char *EntryPoint )
{
    VERIFY_EXPR( Token->Type == TokenType::OpenBrace );

    ++Token; // Skip open brace
    int BraceCount = 1;
    // Find matching closing brace
    while( Token != m_Tokens.end() )
    {
        if( Token->Type == TokenType::OpenBrace )
            ++BraceCount;
        else if( Token->Type == TokenType::ClosingBrace )
        {
            --BraceCount;
            if( BraceCount == 0 )
                break;
        }
        else if( Token->Type == TokenType::FlowControl )
        {
            if( Token->Literal == "return" )
            {
                //if( x < 0.5 ) return;
                //              ^
                m_Tokens.insert( Token, TokenInfo(TokenType::OpenBrace, "{", "\r\n"));
                Token->Delimiter = "";
                //if( x < 0.5 )
                //{return;
                // ^

                m_Tokens.insert(Token, TokenInfo(TokenType::TextBlock, Epilogue.c_str(), "\r\n"));
                //if( x < 0.5 )
                //{
                //gl_Position = f4PosWS;
                //return;
                //^

                while( Token->Type != TokenType::Semicolon )
                    ++Token;
                //if( x < 0.5 )
                //{
                //gl_Position = f4PosWS;
                //return;
                //      ^

                VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "Unexpected end of file while looking for the \';\'" );
                ++Token;
                //if( x < 0.5 )
                //{
                //gl_Position = f4PosWS;
                //return;
                //float a;
                //^

                m_Tokens.insert( Token, TokenInfo(TokenType::ClosingBrace, "}", "\r\n") );
                //if( x < 0.5 )
                //{
                //gl_Position = f4PosWS;
                //return;
                //}
                //int a;
                //^

                continue;
            }
        }
        ++Token;
    }
    VERIFY_PARSER_STATE( Token, BraceCount == 0, "No matching closing bracket found" );

    // void main ()
    // {
    //      ...
    // }
    // ^
    VERIFY_PARSER_STATE(Token, Token != m_Tokens.end(), "Unexpected end of file while looking for the end of body of shader entry point \"", EntryPoint, "\"." );
    VERIFY_EXPR( Token->Type == TokenType::ClosingBrace );
    // Insert epilogue before the closing brace
    m_Tokens.insert(Token, TokenInfo(TokenType::TextBlock, Epilogue.c_str(), Token->Delimiter.c_str()));
    Token->Delimiter = "\n";
}

void HLSL2GLSLConverter::ProcessShaderDeclaration( const Char* EntryPoint, SHADER_TYPE ShaderType )
{
    auto EntryPointToken = m_Tokens.begin();
    int NumOpenBraces = 0;
    while( EntryPointToken != m_Tokens.end() )
    {
        if( EntryPointToken->Type == TokenType::OpenBrace )
            ++NumOpenBraces;
        else if( EntryPointToken->Type == TokenType::ClosingBrace )
        {
            --NumOpenBraces;
            VERIFY_PARSER_STATE( EntryPointToken, NumOpenBraces >= 0, "Unexpected \'}\'");
        }

        // Search global scope only
        if( NumOpenBraces == 0 &&
            EntryPointToken->Type == TokenType::Identifier &&
            EntryPointToken->Literal == EntryPoint )
            break;
        ++EntryPointToken;
    }
    VERIFY_PARSER_STATE( EntryPointToken, EntryPointToken != m_Tokens.end(), "Unable to find shader entry point \"", EntryPoint,'\"' );
    // void TestPS  ( in VSOutput In,
    //      ^
    //  EntryPointToken
    auto TypeToken = EntryPointToken;
    --TypeToken;
    // void TestPS  ( in VSOutput In,
    // ^
    // TypeToken
    VERIFY_PARSER_STATE( TypeToken, TypeToken != m_Tokens.begin(), "Missing return type for shader entry point \"", EntryPoint,'\"' );
    VERIFY_PARSER_STATE( TypeToken, TypeToken->Literal == "void", "Unexpected return type \"", TypeToken->Literal, "\" for shader entry point \"", EntryPoint, "\"\n"
                                                       "Shader outputs should be declared as out parameters to the function.");

    auto ArgsListStartToken = EntryPointToken;
    ++ArgsListStartToken;
    // void TestPS  ( in VSOutput In,
    //              ^
    //       ArgsListStartToken
    VERIFY_PARSER_STATE( ArgsListStartToken, ArgsListStartToken->Type == TokenType::OpenBracket, "Missing argument list for shader entry point \"", EntryPoint,'\"' );

    std::vector<ShaderParameterInfo> ShaderParams;
    auto ArgsListEndToken = ArgsListStartToken;
    ParseShaderParameters( ArgsListEndToken, ShaderParams );

    ++ArgsListStartToken;
    //           ArgsListStartToken
    //               V
    //void TestPS  ( in VSOutput In,
    //               out float4 Color : SV_Target,
    //               out float3 Color2 : SV_Target2 )
    //                                              ^
    //                                        ArgsListEndToken

    m_Tokens.erase(ArgsListStartToken, ArgsListEndToken);
    //void TestPS  ()
    EntryPointToken->Literal = "main";
    //void main ()

    String GlobalVariables, Epilogue, Prologue;
    try
    {
        if( ShaderType == SHADER_TYPE_PIXEL )
        {
            ProcessFragmentShaderArguments( ShaderParams, GlobalVariables, Epilogue, Prologue );
        }
        else if( ShaderType == SHADER_TYPE_VERTEX )
        {
            ProcessVertexShaderArguments( ShaderParams, GlobalVariables, Epilogue, Prologue );
        }
        else if( ShaderType == SHADER_TYPE_COMPUTE )
        {
            ProcessComputeShaderArguments( TypeToken, ShaderParams, GlobalVariables, Prologue );
        }
    }
    catch( const std::runtime_error & )
    {
        LOG_ERROR_AND_THROW( "Failed to process shader parameters for shader \"", EntryPoint, "\"." );
    }

    
    // void main ()
    // ^
    // TypeToken

    // Insert global variables before the function
    m_Tokens.insert(TypeToken, TokenInfo(TokenType::TextBlock, GlobalVariables.c_str(), TypeToken->Delimiter.c_str()));
    TypeToken->Delimiter = "\n";
    auto BodyStartToken = ArgsListEndToken;
    while( BodyStartToken != m_Tokens.end() && BodyStartToken->Type != TokenType::OpenBrace )
        ++BodyStartToken;
    // void main ()
    // {
    // ^
    VERIFY_PARSER_STATE(BodyStartToken, BodyStartToken != m_Tokens.end(), "Unexpected end of file while looking for the body of shader entry point \"", EntryPoint, "\"." );
    auto FirstStatementToken = BodyStartToken;
    ++FirstStatementToken;
    // void main ()
    // {
    //      int a;
    //      ^
    VERIFY_PARSER_STATE(FirstStatementToken, FirstStatementToken != m_Tokens.end(), "Unexpected end of file while looking for the body of shader entry point \"", EntryPoint, "\"." );
    
    // Insert prologue before the first token
    m_Tokens.insert(FirstStatementToken, TokenInfo(TokenType::TextBlock, Prologue.c_str(), "\n"));

    if( Epilogue.length() > 0 )
    {
        auto BodyEndToken = BodyStartToken;
        ProcessReturnStatements( BodyEndToken, Epilogue, EntryPoint );
    }
}


void HLSL2GLSLConverter::RemoveSemanticsFromBlock(TokenListType::iterator &Token, TokenType OpenBracketType, TokenType ClosingBracketType, bool IsStruct)
{
    VERIFY_EXPR( Token->Type == OpenBracketType );

    int NumOpenBrackets = 0;
    while( Token != m_Tokens.end() )
    {
        if( Token->Type == OpenBracketType )
            ++NumOpenBrackets;
        else if( Token->Type == ClosingBracketType )
            --NumOpenBrackets;
        
        if( Token->Literal == ":" )
        {
            // float4 Pos : POSITION;
            //            ^
            auto ColonToken = Token;
            ++Token;
            // float4 Pos : POSITION;
            //              ^
            if( Token->Type == TokenType::Identifier )
            {
                auto SemanticToken = Token;
                ++Token;
                // float4 Pos : POSITION;
                //                      ^

                // float4 Pos : POSITION, Normal : NORMAL;
                //                      ^

                // float4 Pos : POSITION)
                //                      ^
                if( Token->Type == TokenType::Semicolon || Token->Literal == "," || Token->Type == TokenType::ClosingBracket )
                {
                    if( IsStruct )
                    {
                        auto TmpIt = SemanticToken->Literal.begin();
                        auto IsSVSemantic = SkipPrefix( "SV_", TmpIt, SemanticToken->Literal.end() );
                        VERIFY_PARSER_STATE(SemanticToken, !IsSVSemantic, "System-value semantics are not allowed in structures. Please make this explicit input/output to the shader")
                    }
                    m_Tokens.erase( ColonToken, Token );
                    // float4 Pos ;
                    //            ^
                }
            }
        }
        else
            ++Token;

        if( NumOpenBrackets == 0 )
            break;
    }
    VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "Unexpected EOF while parsing body of the structure" );
}

void HLSL2GLSLConverter::RemoveSemantics()
{
    auto Token = m_Tokens.begin();
    int NumOpenBraces = 0;
    while( Token != m_Tokens.end() )
    {
        if( Token->Type == TokenType::OpenBrace )
            ++NumOpenBraces;
        else if( Token->Type == TokenType::ClosingBrace )
            --NumOpenBraces;

        // Search global scope only
        if( NumOpenBraces == 0 )
        {
            if( Token->Type == TokenType::_struct )
            {
                //struct MyStruct
                //^ 
                while( Token != m_Tokens.end() && Token->Type != TokenType::OpenBrace )
                    ++Token;

                VERIFY_PARSER_STATE( Token, Token != m_Tokens.end(), "Unexpected EOF while searching for the structure body" );
                //struct MyStruct
                //{
                //^ 
                RemoveSemanticsFromBlock( Token, TokenType::OpenBrace, TokenType::ClosingBrace, true );

                // struct MyStruct
                // {
                //    ...
                // };
                //  ^
                continue;
            }
            else if( Token->Type == TokenType::Identifier )
            {
                // Searh for "Identifier(" pattern
                // In global scope this should be texture declaration
                // It can also be other things like macro. But this is not a problem.
                ++Token;
                if( Token == m_Tokens.end() )
                    break;
                if( Token->Type == TokenType::OpenBracket )
                {
                    RemoveSemanticsFromBlock( Token, TokenType::OpenBracket, TokenType::ClosingBracket, false );
                    // void TestVS( ... )
                    // {
                    // ^
                }

                continue;
            }
        }

        ++Token;
    }
}

// Remove special shader attributes such as [numthreads(16, 16, 1)]
void HLSL2GLSLConverter::RemoveSpecialShaderAttributes()
{
    auto Token = m_Tokens.begin();
    int NumOpenBraces = 0;
    while( Token != m_Tokens.end() )
    {
        if( Token->Type == TokenType::OpenBrace )
            ++NumOpenBraces;
        else if( Token->Type == TokenType::ClosingBrace )
            --NumOpenBraces;

        // Search global scope only
        if( NumOpenBraces == 0 )
        {
            if( Token->Type == TokenType::OpenStaple )
            {
                auto OpenStaple = Token;
                ++Token;
                if( Token == m_Tokens.end() )
                    break;
                if( Token->Literal == "numthreads" )
                {
                    ++Token;
                    if( Token->Type != TokenType::OpenBracket )
                        continue;
                    while( Token != m_Tokens.end() && Token->Type != TokenType::ClosingStaple )
                        ++Token;
                    if( Token != m_Tokens.end() )
                    {
                        ++Token;
                        if( Token != m_Tokens.end() )
                            Token->Delimiter = OpenStaple->Delimiter + Token->Delimiter;
                        m_Tokens.erase( OpenStaple, Token );
                        continue;
                    }
                }
            }
        }
        ++Token;
    }
}

String HLSL2GLSLConverter::BuildGLSLSource()
{
    String Output;
    for( const auto& Token : m_Tokens )
    {
        Output.append( Token.Delimiter );
        Output.append( Token.Literal );
    }
    return Output;
}

void HLSL2GLSLConverter::ProcessScope( const TokenListType::iterator &ScopeStart, 
                                        const TokenListType::iterator &ScopeEnd )
{
    VERIFY_EXPR( ScopeStart->Type == TokenType::OpenBrace && ScopeEnd->Type == TokenType::ClosingBrace );

    ProcessObjectMethods(ScopeStart, ScopeEnd);

    ProcessRWTextures(ScopeStart, ScopeEnd);

    ProcessAtomics(ScopeStart, ScopeEnd);
}

String HLSL2GLSLConverter::Convert( const Char* HLSLSource, size_t NumSymbols, const Char* EntryPoint, SHADER_TYPE ShaderType )
{
    String GLSLSource(HLSLSource, NumSymbols);

    InsertIncludes( GLSLSource );

    Tokenize(GLSLSource);

    std::unordered_map<String, bool> SamplersHash;
    auto Token = m_Tokens.begin();
    // Process constant buffers, fix floating point constants and 
    // remove flow control attributes
    while( Token != m_Tokens.end() )
    {
        switch( Token->Type )
        {
            case TokenType::cbuffer:
                ProcessConstantBuffer( Token );
            break;

            case TokenType::NumericConstant:
                // This all work is only required because some GLSL compilers are so stupid that
                // flood shader output with insane warnings like this:
                // WARNING: 0:259: Only GLSL version > 110 allows postfix "F" or "f" for float
                // even when compiling for GL 4.3 AND the code IS UNDER #if 0
                if( Token->Literal.back() == 'f' || Token->Literal.back() == 'F' )
                    Token->Literal.pop_back();
                ++Token;
            break;

            case TokenType::FlowControl:
                // Remove flow control attributes like [flatten], [branch], [loop], etc.
                RemoveFlowControlAttribute( Token );
                ++Token;
            break;

            default:
                ++Token;
        }
    }

    // Process textures. GLSL does not allow local variables
    // of sampler type, so the only two scopes where textures can 
    // be declared are global scope and a function argument list.
    {
        TokenListType::iterator FunctionStart = m_Tokens.end();
        std::vector< SamplerHashType > Samplers;
        
        // Find all samplers in the global scope
        Samplers.emplace_back( SamplerHashType() );
        m_Objects.emplace_back( ObjectsTypeHashType() );
        Token = m_Tokens.begin();
        ParseSamplers( Token, Samplers.back() );
        VERIFY_EXPR( Token == m_Tokens.end() );

        Int32 ScopeDepth = 0;

        Token = m_Tokens.begin();
        while( Token != m_Tokens.end() )
        {
            // Detect global function declaration by looking for the pattern
            // <return type> Identifier (
            // in global scope
            if( ScopeDepth == 0 && Token->Type == TokenType::Identifier )
            {
                // float4 Func ( in float2 f2UV, 
                //        ^
                //       Token
                auto ReturnTypeToken = Token;
                --ReturnTypeToken;
                ++Token;
                if( Token == m_Tokens.end() )
                    break;
                // ReturnTypeToken
                // |
                // float4 Func ( in float2 f2UV, 
                //             ^
                //             Token
                if( (ReturnTypeToken->Type == TokenType::BuiltInType ||
                     ReturnTypeToken->Type == TokenType::Identifier ) &&
                     Token->Type == TokenType::OpenBracket )
                {
                    // Parse samplers in the function argument list 
                    Samplers.emplace_back( SamplerHashType() );
                    // GLSL does not support sampler variables,
                    // so the only place where a new sampler
                    // declaration is allowed is function argument 
                    // list
                    auto ArgListEnd = Token;
                    ParseSamplers( ArgListEnd, Samplers.back() );
                    // float4 Func ( in float2 f2UV ) 
                    //                              ^
                    //                          ArgListEnd           
                    auto OpenBrace = ArgListEnd;
                    ++OpenBrace;
                    // float4 Func ( in float2 f2UV )
                    // {
                    // ^
                    if( OpenBrace->Type == TokenType::OpenBrace )
                    {
                        // We need to go through the function argument
                        // list as there may be texture declaraions
                        ++Token;
                        // float4 Func ( in float2 f2UV, 
                        //               ^
                        //             Token

                        // Put empty table on top of the object stack
                        m_Objects.emplace_back( ObjectsTypeHashType() );
                    }
                    else
                    {
                        // For some reason there is no open brace after
                        // what should be argument list - pop the samplers
                        Samplers.pop_back();
                    }
                }
            }

            if( Token->Type == TokenType::OpenBrace )
            {
                if( Samplers.size() == 2 && ScopeDepth == 0 )
                {
                    VERIFY_EXPR( FunctionStart == m_Tokens.end() );
                    // This is the first open brace after the 
                    // Samplers stack has grown to two -> this is
                    // the beginning of a function body
                    FunctionStart = Token;
                }
                ++ScopeDepth;
                ++Token;
            }
            else if( Token->Type == TokenType::ClosingBrace )
            {
                --ScopeDepth;
                if( Samplers.size() == 2 && ScopeDepth == 0 )
                {
                    // We are returning to the global scope now and 
                    // the samplers stack size is 2 -> this was a function
                    // body. We need to process it now.
                    ProcessScope( FunctionStart, Token );
                    // Pop function arguments from the sampler and object
                    // stacks
                    Samplers.pop_back();
                    m_Objects.pop_back();
                    FunctionStart = m_Tokens.end();
                }
                ++Token;
            }
            else if( Token->Type == TokenType::Texture1D      ||
                     Token->Type == TokenType::Texture1DArray ||
                     Token->Type == TokenType::Texture2D      ||
                     Token->Type == TokenType::Texture2DArray ||
                     Token->Type == TokenType::Texture3D      ||
                     Token->Type == TokenType::TextureCube    ||
                     Token->Type == TokenType::TextureCubeArray ||
                     Token->Type == TokenType::Texture2DMS      ||
                     Token->Type == TokenType::Texture2DMSArray ||
                     Token->Type == TokenType::RWTexture1D      ||
                     Token->Type == TokenType::RWTexture1DArray ||
                     Token->Type == TokenType::RWTexture2D      ||
                     Token->Type == TokenType::RWTexture2DArray ||
                     Token->Type == TokenType::RWTexture3D )
            {
                // Process texture declaration, and add it to the top of the
                // object stack
                ProcessTextureDeclaration( Token, Samplers, m_Objects.back() );
            }
            else
                ++Token;
        }
    }

    ProcessShaderDeclaration( EntryPoint, ShaderType );

    RemoveSemantics();

    RemoveSpecialShaderAttributes();

    GLSLSource = BuildGLSLSource();

    return GLSLSource;
}

}
