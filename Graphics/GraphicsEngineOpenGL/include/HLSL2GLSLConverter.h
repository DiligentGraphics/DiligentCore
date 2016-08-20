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

#pragma once

#include "Shader.h"
#include "HashUtils.h"
#include <list>
#include <unordered_set>

namespace Diligent
{
    struct FunctionStubHashKey
    {
        FunctionStubHashKey(const String& _Obj,  const String& _Func, Uint32 _NumArgs) : 
            Object(_Obj),
            Function(_Func),
            NumArguments(_NumArgs)
        {
        }

        FunctionStubHashKey(const Char* _Obj, const Char* _Func, Uint32 _NumArgs) : 
            Object(_Obj),
            Function(_Func),
            NumArguments(_NumArgs)
        {
        }

        FunctionStubHashKey( FunctionStubHashKey && Key ) : 
            Object(std::move(Key.Object)),
            Function(std::move(Key.Function)),
            NumArguments(Key.NumArguments)
        {
        }

        bool operator==(const FunctionStubHashKey& rhs)const
        {
            return Object == rhs.Object &&
                   Function == rhs.Function &&
                   NumArguments == rhs.NumArguments;
        }

        HashMapStringKey Object;
        HashMapStringKey Function;
        Uint32 NumArguments;
    };
}

namespace std
{
    template<>struct hash < Diligent::FunctionStubHashKey >
    {
        size_t operator()( const Diligent::FunctionStubHashKey &Key ) const
        {
            return ComputeHash(Key.Object, Key.Function, Key.NumArguments);
        }
    };
}

namespace Diligent
{
    class HLSL2GLSLConverter
    {
    public:
        HLSL2GLSLConverter(IShaderSourceInputStreamFactory *pSourceStreamFactory);
        String Convert(const Char* HLSLSource, size_t NumSymbols, const Char* EntryPoint, SHADER_TYPE ShaderType);

    private:
        void InsertIncludes(String &GLSLSource);
        
        void ProcessShaderDeclaration(const Char* EntryPoint, SHADER_TYPE ShaderType);
        
        IShaderSourceInputStreamFactory* m_pSourceStreamFactory;

        struct HLSLObjectInfo
        {
            String GLSLType; // sampler2D, sampler2DShadow, image2D, etc.
            Uint32 NumComponents; // 0,1,2,3 or 4
                                  // Texture2D<float4>  -> 4
                                  // Texture2D<uint>    -> 1
                                  // Texture2D          -> 0
            HLSLObjectInfo( const String& Type, Uint32 NComp ) :
                GLSLType( Type ),
                NumComponents( NComp )
            {}
        };
        typedef std::unordered_map<HashMapStringKey, HLSLObjectInfo> ObjectsTypeHashType;
        
        // Stack of parsed objects, for every scope level.
        // There are currently only two levels: 
        // level 0 - global scope, contains all global objects
        //           (textures, buffers)
        // level 1 - function body, contains all objects
        //           defined as function arguments
        std::vector< ObjectsTypeHashType > m_Objects;

        struct GLSLStubInfo
        {
            String Name;
            String Swizzle;
            GLSLStubInfo( const String& _Name, const char* _Swizzle ) :
                Name( _Name ),
                Swizzle( _Swizzle )
            {}
        };
        // Hash map that maps GLSL object, method and number of arguments
        // passed to the original function, to the GLSL stub function
        // Example: {"sampler2D", "Sample", 2} -> {"Sample_2", "_SWIZZLE"}
        std::unordered_map<FunctionStubHashKey, GLSLStubInfo> m_GLSLStubs;


        void Tokenize(const String &Source);

        String BuildGLSLSource();

        enum class TokenType
        {
            Undefined,
            PreprocessorDirective,
            Operator,
            OpenBrace,
            ClosingBrace,
            OpenBracket,
            ClosingBracket,
            OpenStaple,
            ClosingStaple,
            OpenAngleBracket,
            ClosingAngleBracket,
            Identifier,
            NumericConstant,
            Semicolon,
            Comma,
            cbuffer,
            Texture1D,
            Texture1DArray,
            Texture2D,
            Texture2DArray,
            Texture3D,
            TextureCube,
            TextureCubeArray,
            Texture2DMS,
            Texture2DMSArray,
            RWTexture1D,
            RWTexture1DArray,
            RWTexture2D,
            RWTexture2DArray,
            RWTexture3D,
            SamplerState,
            SamplerComparisonState,
            BuiltInType,
            TextBlock,
            _struct,
            Assignment,
            ComparisonOp,
            BooleanOp,
            BitwiseOp,
            IncDecOp,
            MathOp,
            FlowControl
        };

        struct TokenInfo
        {
            TokenType Type;
            String Literal;
            String Delimiter;
            TokenInfo(TokenType _Type = TokenType :: Undefined,
                       const Char* _Literal = "",
                       const Char* _Delimiter = "") : 
                Type( _Type ),
                Literal( _Literal ),
                Delimiter(_Delimiter)
            {}
        };

        typedef std::list<TokenInfo> TokenListType;
        typedef std::unordered_map<String, bool> SamplerHashType;

        const HLSLObjectInfo *FindHLSLObject(const String &Name );

        void ProcessObjectMethods(const TokenListType::iterator &ScopeStart, const TokenListType::iterator &ScopeEnd);

        void ProcessRWTextures(const TokenListType::iterator &ScopeStart, const TokenListType::iterator &ScopeEnd);

        void ProcessAtomics(const TokenListType::iterator &ScopeStart, 
                            const TokenListType::iterator &ScopeEnd);

        void ProcessScope(const TokenListType::iterator &ScopeStart, 
                          const TokenListType::iterator &ScopeEnd);

        void ProcessConstantBuffer(TokenListType::iterator &Token);
        void ParseSamplers(TokenListType::iterator &ScopeStart, SamplerHashType &SamplersHash);
        void ProcessTextureDeclaration(TokenListType::iterator &Token, const std::vector<SamplerHashType> &SamplersHash, ObjectsTypeHashType &Objects);
        bool ProcessObjectMethod(TokenListType::iterator &Token, const TokenListType::iterator &ScopeStart, const TokenListType::iterator &ScopeEnd);
        Uint32 CountFunctionArguments(TokenListType::iterator &Token, const TokenListType::iterator &ScopeEnd);
        bool ProcessRWTextureStore(TokenListType::iterator &Token, const TokenListType::iterator &ScopeEnd);
        void RemoveFlowControlAttribute(TokenListType::iterator &Token);
        void RemoveSemantics();
        void RemoveSpecialShaderAttributes();
        void RemoveSemanticsFromBlock(TokenListType::iterator &Token, TokenType OpenBracketType, TokenType ClosingBracketType, bool IsStruct);
        
        // IteratorType may be String::iterator or String::const_iterator.
        // While iterator is convertible to const_iterator, 
        // iterator& cannot be converted to const_iterator& (Microsoft compiler allows
        // such conversion, while gcc does not)
        template<typename IteratorType>
        String PrintTokenContext(IteratorType &TargetToken, Int32 NumAdjacentLines);

        struct ShaderParameterInfo
        {
            enum class StorageQualifier
            {
                Unknown,
                In,
                Out
            }storageQualifier;
            String Type;
            String Name;
            String Semantic;

            ShaderParameterInfo() :
                storageQualifier(StorageQualifier::Unknown)
            {}
        };
        void ParseShaderParameters( TokenListType::iterator &Token, std::vector<ShaderParameterInfo>& Params );
        void ProcessFragmentShaderArguments( std::vector<ShaderParameterInfo>& Params,
                                             String &GlobalVariables,
                                             String &Epilogue,
                                             String &Prologue );
        void ProcessVertexShaderArguments( std::vector<ShaderParameterInfo>& Params,
                                           String &GlobalVariables,
                                           String &Epilogue,
                                           String &Prologue );
        void ProcessComputeShaderArguments( TokenListType::iterator &TypeToken,
                                            std::vector<ShaderParameterInfo>& Params,
                                            String &GlobalVariables,
                                            String &Prologue );

        void FindClosingBracket( TokenListType::iterator &Token, const TokenListType::iterator &ScopeEnd, TokenType OpenBracketType, TokenType ClosingBracketType );

        void ProcessReturnStatements( TokenListType::iterator &Token, const String &Epilogue, const char *EntryPoint );

        // Tokenized source code
        TokenListType m_Tokens;
        
        // HLSL keyword->token info hash map
        // Example: "Texture2D" -> TokenInfo(TokenType::Texture2D, "Texture2D")
        std::unordered_map<HashMapStringKey, TokenInfo> m_HLSLKeywords;

        // Set of all GLSL image types (image1D, uimage1D, iimage1D, image2D, ... )
        std::unordered_set<HashMapStringKey> m_ImageTypes;

        // Set of all HLSL atomic operations (InterlockedAdd, InterlockedOr, ...)
        std::unordered_set<HashMapStringKey> m_AtomicOperations;
    };
}

//  Intro
// DirectX and OpenGL use different shading languages. While mostly being very similar,
// the language syntax differ substantially in some places. Having two versions of each 
// shader is clearly not an option for real projects. Maintaining intermediate representation
// that translates to both languages is one solution, but it might complicate shader development
// and debugging.
// 
// HLSL converter allows HLSL shader files to be converted into GLSL source.
// The entire shader development can thus be performed using HLSL tools. Since no intermediate
// representation is used, shader files can be directly compiled by HLSL compiler.
// All tools available for HLSL shader devlopment, analysis and optimization can be 
// used. The source can then be transaprently converted to GLSL.
//
//
//  Using HLSL Converter
// * The following rules are used to convert HLSL texture declaration into GLSL sampler:
//   - HLSL texture dimension defines GLSL sampler dimension:
//        - Texture2D   -> sampler2D
//        - TextureCube -> samplerCube
//   - HLSL texture component type defines GLSL sampler type. If no type is specified, float4 is assumed:
//        - Texture2D<float>     -> sampler2D
//        - Texture3D<uint4>     -> usampler3D
//        - Texture2DArray<int2> -> isampler2DArray
//        - Texture2D            -> sampler2D
//    - To distinguish if sampler should be shadow or not, the converter tries to find <Texture Name>_sampler
//      among samplers (global variables and function arguments). If the sampler type is comparison, 
//      the texture is converted to shadow sampler. If sampler state is either not comparison or not found, 
//      regular sampler is used.
//      Examples:
//        - Texture2D g_ShadowMap;                        -> sampler2DShadow g_ShadowMap;
//          SamplerComparisonState g_ShadowMap_sampler;
//        - Texture2D g_Tex2D;                            -> sampler2D g_Tex2D;
//          SamplerState g_Tex2D_sampler;
//          Texture3D g_Tex3D;                            -> sampler3D g_Tex3D;
// 
// * GLSL requires format to be specified for all images allowing writes. HLSL converter allows GLSL image 
//   format specification inside the special comment block:
//   Example:
//     RWTexture2D<float /* format=r32f */ > Tex2D;

//  Requirements:
//  * Shader entry points must be declared as void functions with all outputs listed
//    as out variables
//      ** Members of structures cannot have system-value semantic (such as SV_Position).
//         Such variables must be declared as direct shader input/output
//  * GLSL allows samplers to be declared as global variables or function arguments only.
//    It does not allow local variables of sampler type.
//
// Important notes/known issues:
//
// * GLSL compiler does not handle float3 structure members correctly. It is 
//   strongly suggested not to use this type in structure definitions
//
// * At least NVidia GLSL compiler does not apply layout(row_major) to
//   structure members. By default, all matrices in both HLSL and GLSL
//   are column major
//
// * GLSL compiler does not properly handle structs passed as function arguments!!!!
//   struct MyStruct
//   {
//        matrix Matr;
//   }
//   void Func(in MyStruct S)
//   {
//        ...
//        mul(f4PosWS, S.Matr); <--- This will not work!!!
//   }
//   DO NOT pass structs to functions, use only built-in types!!!
//
// * GLSL does not support most of the implicit type conversions. The following are some
//   examples of required modifications to HLSL code:
//   ** float4 vec = 0; ->  float4 vec = float4(0.0, 0.0, 0.0, 0.0);
//   ** float x = 0;    ->  float x = 0.0;
//   ** uint x = 0;     ->  uint x = 0u;
//   ** GLES is immensely strict about type conversions. For instance,
//      this code will produce compiler error: float4(0, 0, 0, 0)
//      It must be written as float4(0.0, 0.0, 0.0, 0.0)
// * GLSL does not support relational and boolean operations on vector types:
//   ** float2 p = float2(1.0,2.0), q = float2(3.0,4.0);
//      bool2 b = x < y;   -> Error
//      all(p<q)           -> Error
//   ** To facilitate relational and boolean operations on vector types, the following
//      functions are predefined:
//      - Less
//      - LessEqual
//      - Greater
//      - GreaterEqual
//      - Equal
//      - NotEqual
//      - Not
//      - And
//      - Or
//      - BoolToFloat
//   ** Examples:
//      bool2 b = x < y;   -> bool2 b = Less(x, y);
//      all(p>=q)          -> all( GreaterEqual(p,q) )
//
// * When accessing elements of an HLSL matrix, the first index is always row:
//     mat[row][column]
//   In GLSL, the first index is always column:
//     mat[column][row]
//   MATRIX_ELEMENT(mat, row, col) macros is provided to facilitate matrix element retrieval

// * The following functions do not have counterparts in GLSL and should be avoided:
//   ** Texture2DArray.SampleCmpLevelZero()
//   ** TextureCube.SampleCmpLevelZero()
//   ** TextureCubeArray.SampleCmpLevelZero()


// * Shader converter creates shader interface blocks to process non-system generated 
//   input/output parameters. For instance, to process Out parameter of the vertex 
//   shader below
//
//      struct VSOutput{ ... };
//      void VertexShader(out VSOutput Out){...}
//
//   the following interface block will be created:
//
//      out _IntererfaceBlock0
//      {
//          VSOutput Out;
//      };
//
//   OpenGL requires that interface block definitions in different shader stages
//   must match exaclty: they must define the exact same variables (type/array count 
//   and NAME), in the exact same order. Since variable names must match, this
//   effectively means that shader input/output parameter names must also match
//   exactly. This limitation seems to be relaxed in desktop GL and some GLES.
//   For instance, the following code works fine on Desktop GL and on Intel GLES, 
//   but fails on NVidia GLES:
//
//      struct VSOutput{ ... };
//      void VertexShader(out VSOutput Out){...}
//      void PixelShader(in VSOutput In){...}
//   
//  To make it run on NVidia GLES, shader intput/output parameter names must
//  be exactly the same:
//
//      struct VSOutput{ ... };
//      void VertexShader(out VSOutput VSOut){...}
//      void PixelShader(in VSOutput VSOut){...}
//
//  Moreover, even when fragment shader does not actually use the parameter,
//  it still must be declared
//
//  If the requirements above are not met, the shaders are still compiled successfuly,
//  but glDraw*() command fails with useless error 1282 (IVALID_OPERATION)