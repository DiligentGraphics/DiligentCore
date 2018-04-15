/*     Copyright 2015-2018 Egor Yusov
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

#if PLATFORM_ANDROID
    // Android specific include files.
#   include <unordered_map>

    // Header files.
#   include <android_native_app_glue.h>
#   include "shaderc/shaderc.hpp"
    // Static variable that keeps ANativeWindow and asset manager instances.
    //static android_app *Android_application = nullptr;
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
#	include <MoltenGLSLToSPIRVConverter/GLSLToSPIRVConverter.h>
#else
#	include "SPIRV/GlslangToSpv.h"
#endif

#include "GLSL2SPIRV.h"
#include "DebugUtilities.h"

namespace Diligent
{

void InitializeGlslang()
{
#if !PLATFORM_ANDROID
    glslang::InitializeProcess();
#endif
}

void FinalizeGlslang()
{
#if !PLATFORM_ANDROID
    glslang::FinalizeProcess();
#endif
}

EShLanguage ShaderTypeToShLanguage(SHADER_TYPE ShaderType)
{
    switch(ShaderType)
    {
        case SHADER_TYPE_VERTEX:    return EShLangVertex;
        case SHADER_TYPE_HULL:      return EShLangTessControl;
        case SHADER_TYPE_DOMAIN:    return EShLangTessEvaluation;
        case SHADER_TYPE_GEOMETRY:  return EShLangGeometry;
        case SHADER_TYPE_PIXEL:     return EShLangFragment;
        case SHADER_TYPE_COMPUTE:   return EShLangCompute;

        default:
            UNEXPECTED("Unexpected shader type");
            return EShLangCount;
    }
}

TBuiltInResource InitResources()
{
    TBuiltInResource Resources;

    Resources.maxLights = 32;
    Resources.maxClipPlanes = 6;
    Resources.maxTextureUnits = 32;
    Resources.maxTextureCoords = 32;
    Resources.maxVertexAttribs = 64;
    Resources.maxVertexUniformComponents = 4096;
    Resources.maxVaryingFloats = 64;
    Resources.maxVertexTextureImageUnits = 32;
    Resources.maxCombinedTextureImageUnits = 80;
    Resources.maxTextureImageUnits = 32;
    Resources.maxFragmentUniformComponents = 4096;
    Resources.maxDrawBuffers = 32;
    Resources.maxVertexUniformVectors = 128;
    Resources.maxVaryingVectors = 8;
    Resources.maxFragmentUniformVectors = 16;
    Resources.maxVertexOutputVectors = 16;
    Resources.maxFragmentInputVectors = 15;
    Resources.minProgramTexelOffset = -8;
    Resources.maxProgramTexelOffset = 7;
    Resources.maxClipDistances = 8;
    Resources.maxComputeWorkGroupCountX = 65535;
    Resources.maxComputeWorkGroupCountY = 65535;
    Resources.maxComputeWorkGroupCountZ = 65535;
    Resources.maxComputeWorkGroupSizeX = 1024;
    Resources.maxComputeWorkGroupSizeY = 1024;
    Resources.maxComputeWorkGroupSizeZ = 64;
    Resources.maxComputeUniformComponents = 1024;
    Resources.maxComputeTextureImageUnits = 16;
    Resources.maxComputeImageUniforms = 8;
    Resources.maxComputeAtomicCounters = 8;
    Resources.maxComputeAtomicCounterBuffers = 1;
    Resources.maxVaryingComponents = 60;
    Resources.maxVertexOutputComponents = 64;
    Resources.maxGeometryInputComponents = 64;
    Resources.maxGeometryOutputComponents = 128;
    Resources.maxFragmentInputComponents = 128;
    Resources.maxImageUnits = 8;
    Resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
    Resources.maxCombinedShaderOutputResources = 8;
    Resources.maxImageSamples = 0;
    Resources.maxVertexImageUniforms = 0;
    Resources.maxTessControlImageUniforms = 0;
    Resources.maxTessEvaluationImageUniforms = 0;
    Resources.maxGeometryImageUniforms = 0;
    Resources.maxFragmentImageUniforms = 8;
    Resources.maxCombinedImageUniforms = 8;
    Resources.maxGeometryTextureImageUnits = 16;
    Resources.maxGeometryOutputVertices = 256;
    Resources.maxGeometryTotalOutputComponents = 1024;
    Resources.maxGeometryUniformComponents = 1024;
    Resources.maxGeometryVaryingComponents = 64;
    Resources.maxTessControlInputComponents = 128;
    Resources.maxTessControlOutputComponents = 128;
    Resources.maxTessControlTextureImageUnits = 16;
    Resources.maxTessControlUniformComponents = 1024;
    Resources.maxTessControlTotalOutputComponents = 4096;
    Resources.maxTessEvaluationInputComponents = 128;
    Resources.maxTessEvaluationOutputComponents = 128;
    Resources.maxTessEvaluationTextureImageUnits = 16;
    Resources.maxTessEvaluationUniformComponents = 1024;
    Resources.maxTessPatchComponents = 120;
    Resources.maxPatchVertices = 32;
    Resources.maxTessGenLevel = 64;
    Resources.maxViewports = 16;
    Resources.maxVertexAtomicCounters = 0;
    Resources.maxTessControlAtomicCounters = 0;
    Resources.maxTessEvaluationAtomicCounters = 0;
    Resources.maxGeometryAtomicCounters = 0;
    Resources.maxFragmentAtomicCounters = 8;
    Resources.maxCombinedAtomicCounters = 8;
    Resources.maxAtomicCounterBindings = 1;
    Resources.maxVertexAtomicCounterBuffers = 0;
    Resources.maxTessControlAtomicCounterBuffers = 0;
    Resources.maxTessEvaluationAtomicCounterBuffers = 0;
    Resources.maxGeometryAtomicCounterBuffers = 0;
    Resources.maxFragmentAtomicCounterBuffers = 1;
    Resources.maxCombinedAtomicCounterBuffers = 1;
    Resources.maxAtomicCounterBufferSize = 16384;
    Resources.maxTransformFeedbackBuffers = 4;
    Resources.maxTransformFeedbackInterleavedComponents = 64;
    Resources.maxCullDistances = 8;
    Resources.maxCombinedClipAndCullDistances = 8;
    Resources.maxSamples = 4;
    Resources.limits.nonInductiveForLoops = 1;
    Resources.limits.whileLoops = 1;
    Resources.limits.doWhileLoops = 1;
    Resources.limits.generalUniformIndexing = 1;
    Resources.limits.generalAttributeMatrixVectorIndexing = 1;
    Resources.limits.generalVaryingIndexing = 1;
    Resources.limits.generalSamplerIndexing = 1;
    Resources.limits.generalVariableIndexing = 1;
    Resources.limits.generalConstantMatrixVectorIndexing = 1;

    return Resources;
}

std::vector<unsigned int> GLSLtoSPIRV(const SHADER_TYPE ShaderType, const char *ShaderSource) 
{
    std::vector<unsigned int> spirv;

#if PLATFORM_ANDROID

    // On Android, use shaderc instead.
    shaderc::Compiler compiler;
    shaderc::SpvCompilationResult module =
        compiler.CompileGlslToSpv(pshader, strlen(pshader), MapShadercType(shader_type), "shader");
    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        LOGE("Error: Id=%d, Msg=%s", module.GetCompilationStatus(), module.GetErrorMessage().c_str());
        return false;
    }
    spirv.assign(module.cbegin(), module.cend());

#else

    EShLanguage ShLang = ShaderTypeToShLanguage(ShaderType);
    glslang::TShader Shader(ShLang);
    TBuiltInResource Resources = InitResources();

    // Enable SPIR-V and Vulkan rules when parsing GLSL
    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

    const char *ShaderStrings[] = { ShaderSource };
    Shader.setStrings(ShaderStrings, 1);
    
    if (!Shader.parse(&Resources, 100, false, messages)) 
    {
        LOG_ERROR_MESSAGE("Failed to parse shader source: \n", Shader.getInfoLog(), '\n', Shader.getInfoDebugLog());
        return std::move(spirv);
    }

    glslang::TProgram Program;
    Program.addShader(&Shader);

    //
    // Program-level processing...
    //
    if (!Program.link(messages))
    {
        LOG_ERROR_MESSAGE("Failed to link program: \n", Shader.getInfoLog(), '\n', Shader.getInfoDebugLog());
        return std::move(spirv);
    }

    glslang::GlslangToSpv(*Program.getIntermediate(ShLang), spirv);
#endif

    return std::move(spirv);
}

}
