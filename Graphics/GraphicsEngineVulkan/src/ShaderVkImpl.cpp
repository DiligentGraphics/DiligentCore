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

#include <array>
#include <cctype>
#include "pch.h"

#include "ShaderVkImpl.h"
#include "RenderDeviceVkImpl.h"
#include "DataBlobImpl.h"
#include "GLSLSourceBuilder.h"

#if !NO_GLSLANG
#include "SPIRVUtils.h"
#endif

namespace Diligent
{

ShaderVkImpl::ShaderVkImpl(IReferenceCounters*          pRefCounters,
                           RenderDeviceVkImpl*          pRenderDeviceVk,
                           const ShaderCreationAttribs& CreationAttribs) : 
    TShaderBase       (pRefCounters, pRenderDeviceVk, CreationAttribs.Desc),
    m_StaticResLayout (*this, pRenderDeviceVk->GetLogicalDevice()),
    m_StaticResCache  (ShaderResourceCacheVk::DbgCacheContentType::StaticShaderResources),
    m_StaticVarsMgr   (*this)
{
    if (CreationAttribs.Source != nullptr || CreationAttribs.FilePath != nullptr)
    {
#if NO_GLSLANG
        LOG_ERROR_AND_THROW("Diligent engine was not linked with glslang and can only consume compiled SPIRV bytecode.");
#else
        DEV_CHECK_ERR(CreationAttribs.ByteCode == nullptr, "'ByteCode' must be null when shader is created from source code or a file");
        DEV_CHECK_ERR(CreationAttribs.ByteCodeSize == 0, "'ByteCodeSize' must be 0 when shader is created from source code or a file");

        if (CreationAttribs.SourceLanguage == SHADER_SOURCE_LANGUAGE_HLSL)
        {
            m_SPIRV = HLSLtoSPIRV(CreationAttribs, CreationAttribs.ppCompilerOutput);
        }
        else
        {
            auto GLSLSource = BuildGLSLSourceString(CreationAttribs, pRenderDeviceVk->GetDeviceCaps(), TargetGLSLCompiler::glslang, "#define TARGET_API_VULKAN 1\n");
            m_SPIRV = GLSLtoSPIRV(m_Desc.ShaderType, GLSLSource.c_str(), static_cast<int>(GLSLSource.length()), CreationAttribs.ppCompilerOutput);
        }
    
        if (m_SPIRV.empty())
        {
            LOG_ERROR_AND_THROW("Failed to compile shader");
        }
#endif
    }
    else if (CreationAttribs.ByteCode != nullptr)
    {
        DEV_CHECK_ERR(CreationAttribs.ByteCodeSize != 0, "ByteCodeSize must not be 0");
        DEV_CHECK_ERR(CreationAttribs.ByteCodeSize % 4 == 0, "Byte code size (", CreationAttribs.ByteCodeSize, ") is not multiple of 4");
        m_SPIRV.resize(CreationAttribs.ByteCodeSize/4);
        memcpy(m_SPIRV.data(), CreationAttribs.ByteCode, CreationAttribs.ByteCodeSize);
    }
    else 
    {
        LOG_ERROR_AND_THROW("Shader source must be provided through one of the 'Source', 'FilePath' or 'ByteCode' members");
    }

    // We cannot create shader module here because resource bindings are assigned when
    // pipeline state is created
    
    // Load shader resources
    auto& Allocator = GetRawAllocator();
    auto* pRawMem = ALLOCATE(Allocator, "Allocator for ShaderResources", sizeof(SPIRVShaderResources));
    bool IsHLSLVertexShader = CreationAttribs.SourceLanguage == SHADER_SOURCE_LANGUAGE_HLSL && m_Desc.ShaderType == SHADER_TYPE_VERTEX;
    auto* pResources = new (pRawMem) SPIRVShaderResources(Allocator, pRenderDeviceVk, m_SPIRV, m_Desc, CreationAttribs.UseCombinedTextureSamplers ? CreationAttribs.CombinedSamplerSuffix : nullptr, IsHLSLVertexShader, m_EntryPoint);
    m_pShaderResources.reset(pResources, STDDeleterRawMem<SPIRVShaderResources>(Allocator));
    
    if (IsHLSLVertexShader)
    {
        MapHLSLVertexShaderInputs();
    }

    m_StaticResLayout.InitializeStaticResourceLayout(m_pShaderResources, GetRawAllocator(), m_StaticResCache);
    // m_StaticResLayout only contains static resources, so reference all of them
    m_StaticVarsMgr.Initialize(m_StaticResLayout, GetRawAllocator(), nullptr, 0, m_StaticResCache);
}

void ShaderVkImpl::MapHLSLVertexShaderInputs()
{
    for (Uint32 i = 0; i < m_pShaderResources->GetNumShaderStageInputs(); ++i)
    {
        const auto& Input = m_pShaderResources->GetShaderStageInputAttribs(i);
        const char* s = Input.Semantic;
        static const char* Prefix = "attrib";
        const char* p = Prefix;
        while(*s != 0 && *p != 0 && *p == std::tolower(static_cast<unsigned char>(*s)) )
        {
            ++p;
            ++s;
        }

        if (*p != 0)
        {
            LOG_ERROR_MESSAGE("Unable to map semantic '", Input.Semantic, "' to input location: semantics must have 'ATTRIBx' format.");
            continue;
        }

        char* EndPtr = nullptr;
        auto Location = strtol(s, &EndPtr, 10);
        if (*EndPtr != 0)
        {
            LOG_ERROR_MESSAGE("Unable to map semantic '", Input.Semantic, "' to input location: semantics must have 'ATTRIBx' format.");
            continue;
        }
        m_SPIRV[Input.LocationDecorationOffset] = Location;
    }
}

ShaderVkImpl::~ShaderVkImpl()
{
    m_StaticVarsMgr.Destroy(GetRawAllocator());
}

#ifdef DEVELOPMENT
bool ShaderVkImpl::DvpVerifyStaticResourceBindings()const
{
    return m_StaticResLayout.dvpVerifyBindings(m_StaticResCache);
}
#endif

}
