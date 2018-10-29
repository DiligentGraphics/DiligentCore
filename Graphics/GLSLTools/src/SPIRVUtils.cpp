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

#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <array>

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

#include "SPIRVUtils.h"
#include "DebugUtilities.h"
#include "DataBlobImpl.h"
#include "RefCntAutoPtr.h"

static const char g_HLSLDefinitions[] = 
{
    #include "../../GraphicsEngineD3DBase/include/HLSLDefinitions_inc.fxh"
};

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
    Resources.maxMeshOutputVerticesNV = 256;
    Resources.maxMeshOutputPrimitivesNV = 512;
    Resources.maxMeshWorkGroupSizeX_NV = 32;
    Resources.maxMeshWorkGroupSizeY_NV = 1;
    Resources.maxMeshWorkGroupSizeZ_NV = 1;
    Resources.maxTaskWorkGroupSizeX_NV = 32;
    Resources.maxTaskWorkGroupSizeY_NV = 1;
    Resources.maxTaskWorkGroupSizeZ_NV = 1;
    Resources.maxMeshViewCountNV = 4;

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

class IoMapResolver : public glslang::TIoMapResolver
{
public:
    // Should return true if the resulting/current binding would be okay.
    // Basic idea is to do aliasing binding checks with this.
    virtual bool validateBinding(EShLanguage stage, const char* name, const glslang::TType& type, bool is_live)
    {
        return true;
    }

    // Should return a value >= 0 if the current binding should be overridden.
    // Return -1 if the current binding (including no binding) should be kept.
    virtual int resolveBinding(EShLanguage stage, const char* name, const glslang::TType& type, bool is_live)
    {
        // We do not care about actual binding value here.
        // We only need decoration to be present in SPIRV
        return 0;
    }

    // Should return a value >= 0 if the current set should be overridden.
    // Return -1 if the current set (including no set) should be kept.
    virtual int resolveSet(EShLanguage stage, const char* name, const glslang::TType& type, bool is_live)
    {
        // We do not care about actual descriptor set value here.
        // We only need decoration to be present in SPIRV
        return 0;
    }

    // Should return a value >= 0 if the current location should be overridden.
    // Return -1 if the current location (including no location) should be kept.
    virtual int resolveUniformLocation(EShLanguage stage, const char* name, const glslang::TType& type, bool is_live)
    {
        return -1;
    }

    // Should return true if the resulting/current setup would be okay.
    // Basic idea is to do aliasing checks and reject invalid semantic names.
    virtual bool validateInOut(EShLanguage stage, const char* name, const glslang::TType& type, bool is_live)
    {
        return true;
    }

    // Should return a value >= 0 if the current location should be overridden.
    // Return -1 if the current location (including no location) should be kept.
    virtual int resolveInOutLocation(EShLanguage stage, const char* name, const glslang::TType& type, bool is_live)
    {
        return -1;
    }

    // Should return a value >= 0 if the current component index should be overridden.
    // Return -1 if the current component index (including no index) should be kept.
    virtual int resolveInOutComponent(EShLanguage stage, const char* name, const glslang::TType& type, bool is_live)
    {
        return -1;
    }

    // Should return a value >= 0 if the current color index should be overridden.
    // Return -1 if the current color index (including no index) should be kept.
    virtual int resolveInOutIndex(EShLanguage stage, const char* name, const glslang::TType& type, bool is_live)
    {
        return -1;
    }

    // Notification of a uniform variable
    virtual void notifyBinding(EShLanguage stage, const char* name, const glslang::TType& type, bool is_live)
    {
    }

    // Notification of a in or out variable
    virtual void notifyInOut(EShLanguage stage, const char* name, const glslang::TType& type, bool is_live)
    {
    }

    // Called by mapIO when it has finished the notify pass
    virtual void endNotifications(EShLanguage stage)
    {
    }

    // Called by mapIO when it starts its notify pass for the given stage
    virtual void beginNotifications(EShLanguage stage)
    {
    }

    // Called by mipIO when it starts its resolve pass for the given stage
    virtual void beginResolve(EShLanguage stage)
    {
    }

    // Called by mapIO when it has finished the resolve pass
    virtual void endResolve(EShLanguage stage)
    {
    }
};

static void LogCompilerError(const char* DebugOutputMessage,
                             const char* InfoLog,
                             const char* InfoDebugLog,
                             const char* ShaderSource,
                             size_t      SourceCodeLen,
                             IDataBlob** ppCompilerOutput)
{
    std::string ErrorLog(InfoLog);
    if (*InfoDebugLog != '\0')
    {
        ErrorLog.push_back('\n');
        ErrorLog.append(InfoDebugLog);
    }
    LOG_ERROR_MESSAGE(DebugOutputMessage, ErrorLog);

    if (ppCompilerOutput != nullptr)
    {
        auto* pOutputDataBlob = MakeNewRCObj<DataBlobImpl>()(SourceCodeLen + 1 + ErrorLog.length() + 1);
        char* DataPtr = reinterpret_cast<char*>(pOutputDataBlob->GetDataPtr());
        memcpy(DataPtr, ErrorLog.data(), ErrorLog.length() + 1);
        memcpy(DataPtr + ErrorLog.length() + 1, ShaderSource, SourceCodeLen + 1);
        pOutputDataBlob->QueryInterface(IID_DataBlob, reinterpret_cast<IObject**>(ppCompilerOutput));
    }
}

static std::vector<unsigned int> CompileShaderInternal(glslang::TShader&           Shader,
                                                       EShMessages                 messages,
                                                       glslang::TShader::Includer* pIncluder,
                                                       const char*                 ShaderSource,
                                                       size_t                      SourceCodeLen,
                                                       IDataBlob**                 ppCompilerOutput)
{
    Shader.setAutoMapBindings(true);
    TBuiltInResource Resources = InitResources();
    auto ParseResult = pIncluder != nullptr ?
        Shader.parse(&Resources, 100, false, messages, *pIncluder) : 
        Shader.parse(&Resources, 100, false, messages);
    if (!ParseResult)
    {
        LogCompilerError("Failed to parse shader source: \n", Shader.getInfoLog(), Shader.getInfoDebugLog(), ShaderSource, SourceCodeLen, ppCompilerOutput);
        return {};
    }

    glslang::TProgram Program;
    Program.addShader(&Shader);
    if (!Program.link(messages))
    {
        LogCompilerError("Failed to link program: \n", Program.getInfoLog(), Program.getInfoDebugLog(), ShaderSource, SourceCodeLen, ppCompilerOutput);
        return {};
    }

    IoMapResolver Resovler;
    // This step is essential to set bindings and descriptor sets
    Program.mapIO(&Resovler);

    std::vector<unsigned int> spirv;
    glslang::GlslangToSpv(*Program.getIntermediate(Shader.getStage()), spirv);

    return std::move(spirv);
}


class IncluderImpl : public glslang::TShader::Includer
{
public:
    IncluderImpl(IShaderSourceInputStreamFactory* pInputStreamFactory) :
        m_pInputStreamFactory(pInputStreamFactory)
    {}

    // For the "system" or <>-style includes; search the "system" paths.
    virtual IncludeResult* includeSystem(const char* headerName,
                                         const char* /*includerName*/,
                                         size_t /*inclusionDepth*/)
    {
        DEV_CHECK_ERR(m_pInputStreamFactory != nullptr, "The shader source conains #include directives, but no input stream factory was provided");
        RefCntAutoPtr<IFileStream> pSourceStream;
        m_pInputStreamFactory->CreateInputStream( headerName, &pSourceStream );
        if( pSourceStream == nullptr )
        {
            LOG_ERROR( "Failed to open shader include file \"", headerName, "\". Check that the file exists" );
            return nullptr;
        }

        RefCntAutoPtr<IDataBlob> pFileData( MakeNewRCObj<DataBlobImpl>()(0) );
        pSourceStream->Read( pFileData );
        auto* pNewInclude =
            new IncludeResult
            {
                headerName,
                reinterpret_cast<const char*>(pFileData->GetDataPtr()),
                pFileData->GetSize(),
                nullptr
            };

        m_IncludeRes.emplace(pNewInclude);
        m_DataBlobs.emplace(pNewInclude, std::move(pFileData));
        return pNewInclude;
    }

    // For the "local"-only aspect of a "" include. Should not search in the
    // "system" paths, because on returning a failure, the parser will
    // call includeSystem() to look in the "system" locations.
    virtual IncludeResult* includeLocal(const char* /*headerName*/,
                                        const char* /*includerName*/,
                                        size_t /*inclusionDepth*/)
    {
        return nullptr;
    }

    // Signals that the parser will no longer use the contents of the
    // specified IncludeResult.
    virtual void releaseInclude(IncludeResult* IncldRes)
    {
        m_DataBlobs.erase(IncldRes);
    }

private:
    IShaderSourceInputStreamFactory* const m_pInputStreamFactory;
    std::unordered_set<std::unique_ptr<IncludeResult>> m_IncludeRes;
    std::unordered_map<IncludeResult*, RefCntAutoPtr<IDataBlob>> m_DataBlobs;
};

std::vector<unsigned int> HLSLtoSPIRV(const ShaderCreationAttribs& Attribs, IDataBlob** ppCompilerOutput)
{
    EShLanguage ShLang = ShaderTypeToShLanguage(Attribs.Desc.ShaderType);
    glslang::TShader Shader(ShLang);
    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgReadHlsl | EShMsgHlslLegalization);

    VERIFY_EXPR(Attribs.SourceLanguage == SHADER_SOURCE_LANGUAGE_HLSL);
    
    Shader.setEnvInput(glslang::EShSourceHlsl, ShLang, glslang::EShClientVulkan, 100);
    Shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    Shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
    Shader.setHlslIoMapping(true);
    Shader.setEntryPoint(Attribs.EntryPoint);

    RefCntAutoPtr<IDataBlob> pFileData(MakeNewRCObj<DataBlobImpl>()(0));
    const char* SourceCode = 0;
    int SourceCodeLen = 0;
    if (Attribs.Source)
    {
        SourceCode = Attribs.Source;
        SourceCodeLen = static_cast<int>(strlen(Attribs.Source));
    }
    else
    {
        VERIFY(Attribs.pShaderSourceStreamFactory, "Input stream factory is null");
        RefCntAutoPtr<IFileStream> pSourceStream;
        Attribs.pShaderSourceStreamFactory->CreateInputStream(Attribs.FilePath, &pSourceStream);
        if (pSourceStream == nullptr)
            LOG_ERROR_AND_THROW("Failed to open shader source file");

        pSourceStream->Read(pFileData);
        SourceCode = reinterpret_cast<char*>(pFileData->GetDataPtr());
        SourceCodeLen = static_cast<int>(pFileData->GetSize());
    }

    std::string Defines;
    if (Attribs.Macros != nullptr)
    {
        Defines = g_HLSLDefinitions;
        Defines += '\n';
        auto* pMacro = Attribs.Macros;
        while (pMacro->Name != nullptr && pMacro->Definition != nullptr)
        {
            Defines += "#define ";
            Defines += pMacro->Name;
            Defines += ' ';
            Defines += pMacro->Definition;
            Defines += "\n";
            ++pMacro;
        }
        Shader.setPreamble(Defines.c_str());
    }
    else
    {
        Shader.setPreamble(g_HLSLDefinitions);
    }
    const char* ShaderStrings      [] = {SourceCode};
    const int   ShaderStringLenghts[] = {SourceCodeLen};
    const char* Names              [] = {Attribs.FilePath != nullptr ? Attribs.FilePath : ""};
    Shader.setStringsWithLengthsAndNames(ShaderStrings, ShaderStringLenghts, Names, 1);
    
    IncluderImpl Includer(Attribs.pShaderSourceStreamFactory);
    return CompileShaderInternal(Shader, messages, &Includer, SourceCode, SourceCodeLen, ppCompilerOutput);
}

std::vector<unsigned int> GLSLtoSPIRV(const SHADER_TYPE ShaderType, const char* ShaderSource, int SourceCodeLen, IDataBlob** ppCompilerOutput) 
{
    EShLanguage ShLang = ShaderTypeToShLanguage(ShaderType);
    glslang::TShader Shader(ShLang);
    
    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

    const char* ShaderStrings[] = {ShaderSource };
    int         Lenghts[]       = {SourceCodeLen};
    Shader.setStringsWithLengths(ShaderStrings, Lenghts, 1);
    
    return CompileShaderInternal(Shader, messages, nullptr, ShaderSource, SourceCodeLen, ppCompilerOutput);
}

}
