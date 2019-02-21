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

/// \file
/// Definition of the Diligent::IHLSL2GLSLConverter interface

#include "../../GraphicsEngine/interface/Shader.h"
#include "../../../Primitives/interface/DataBlob.h"

namespace Diligent
{

// {1FDE020A-9C73-4A76-8AEF-C2C6C2CF0EA5}
static constexpr INTERFACE_ID IID_HLSL2GLSLConversionStream =
{ 0x1fde020a, 0x9c73, 0x4a76, { 0x8a, 0xef, 0xc2, 0xc6, 0xc2, 0xcf, 0xe, 0xa5 } };

class IHLSL2GLSLConversionStream : public IObject
{
public:
    virtual void Convert(const Char* EntryPoint,
                         SHADER_TYPE ShaderType,
                         bool        IncludeDefintions,
                         const char* SamplerSuffix,
                         bool        UseInOutLocationQualifiers,
                         IDataBlob** ppGLSLSource) = 0;
};


// {44A21160-77E0-4DDC-A57E-B8B8B65B5342}
static constexpr INTERFACE_ID IID_HLSL2GLSLConverter =
{ 0x44a21160, 0x77e0, 0x4ddc, { 0xa5, 0x7e, 0xb8, 0xb8, 0xb6, 0x5b, 0x53, 0x42 } };

/// Interface to the buffer object implemented in OpenGL
class IHLSL2GLSLConverter : public IObject
{
public:
    virtual void CreateStream(const Char*                       InputFileName, 
                              IShaderSourceInputStreamFactory*  pSourceStreamFactory, 
                              const Char*                       HLSLSource, 
                              size_t                            NumSymbols, 
                              IHLSL2GLSLConversionStream**      ppStream)const = 0;
};

}
