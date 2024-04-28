/*
 *  Copyright 2019-2024 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#include "../../GraphicsEngineOpenGL/include/pch.h"

#include "ArchiverImpl.hpp"
#include "Archiver_Inc.hpp"

#include "RenderDeviceGLImpl.hpp"
#include "PipelineResourceSignatureGLImpl.hpp"
#include "PipelineStateGLImpl.hpp"
#include "ShaderGLImpl.hpp"
#include "DeviceObjectArchiveGL.hpp"
#include "SerializedPipelineStateImpl.hpp"
#include "ShaderToolsCommon.hpp"
#include "ParsingTools.hpp"

#if !DILIGENT_NO_GLSLANG
#    include "GLSLUtils.hpp"
#    include "GLSLangUtils.hpp"
#    include "spirv_glsl.hpp"
#endif

namespace Diligent
{

template <>
struct SerializedResourceSignatureImpl::SignatureTraits<PipelineResourceSignatureGLImpl>
{
    static constexpr DeviceType Type = DeviceType::OpenGL;

    template <SerializerMode Mode>
    using PRSSerializerType = PRSSerializerGL<Mode>;
};

namespace
{

#if !DILIGENT_NO_GLSLANG
static bool GetUseGLAngleMultiDrawWorkaround(const ShaderCreateInfo& ShaderCI)
{
    if (ShaderCI.SourceLanguage == SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM ||
        ShaderCI.Desc.ShaderType != SHADER_TYPE_VERTEX)
        return false;

    const auto Extensions = GetGLSLExtensions(ShaderCI.GLSLExtensions);
    for (const auto& Ext : Extensions)
    {
        if (Ext.first == "GL_ANGLE_multi_draw")
        {
            return Ext.second == "enable" || Ext.second == "require";
        }
    }

    return false;
}

static void PatchSourceForWebGL(std::string& Source, SHADER_TYPE ShaderType)
{
    // Remove location qualifiers
    {
        // WebGL only supports location qualifiers for VS inputs and FS outputs.
        const std::string InOutQualifier = ShaderType == SHADER_TYPE_VERTEX ? " out " : " in ";

        auto layout_pos = Source.find("layout");
        while (layout_pos != std::string::npos)
        {
            // layout(location = 3) flat out int _VSOut_PrimitiveID;
            // ^
            // layout_pos

            const auto declaration_end_pos = Source.find_first_of(";{", layout_pos + 6);
            if (declaration_end_pos == std::string::npos)
                break;
            // layout(location = 3) flat out int _VSOut_PrimitiveID;
            //                                                     ^
            //                                                  declaration_end_pos

            // layout(std140) uniform cbPrimitiveAttribs {
            //                                           ^
            //                                      declaration_end_pos

            const std::string Declaration = Source.substr(layout_pos, declaration_end_pos - layout_pos);
            // layout(location = 3) flat out int _VSOut_PrimitiveID

            if (Declaration.find(InOutQualifier) != std::string::npos)
            {
                const auto closing_paren_pos = Source.find(')', layout_pos);
                if (closing_paren_pos == std::string::npos)
                    break;

                // layout(location = 3) flat out int _VSOut_PrimitiveID;
                //                    ^
                //              closing_paren_pos

                for (size_t i = layout_pos; i <= closing_paren_pos; ++i)
                    Source[i] = ' ';
                //                      flat out int _VSOut_PrimitiveID;
            }

            layout_pos = Source.find("layout", layout_pos + 6);
        }
    }

    if (ShaderType == SHADER_TYPE_VERTEX)
    {
        // Replace gl_DrawIDARB with gl_DrawID
        size_t pos = Source.find("gl_DrawIDARB");
        while (pos != std::string::npos)
        {
            Source.replace(pos, 12, "gl_DrawID");
            pos = Source.find("gl_DrawIDARB", pos + 9);
        }
    }
}
#endif

struct CompiledShaderGL final : SerializedShaderImpl::CompiledShader
{
    String                 UnrolledSource;
    RefCntAutoPtr<IShader> pShaderGL;
    bool                   IsOptimized = false;

    CompiledShaderGL(IReferenceCounters*                          pRefCounters,
                     const ShaderCreateInfo&                      ShaderCI,
                     const ShaderGLImpl::CreateInfo&              GLShaderCI,
                     IRenderDevice*                               pRenderDeviceGL,
                     RENDER_DEVICE_TYPE                           DeviceType,
                     const SerializationDeviceImpl::GLProperties& GLProps)
    {
        if (GLProps.OptimizeShaders)
        {
            UnrolledSource = TransformSource(ShaderCI, GLShaderCI, DeviceType, GLProps);
            IsOptimized    = !UnrolledSource.empty();
        }
        if (UnrolledSource.empty())
        {
            UnrolledSource = UnrollSource(ShaderCI);
        }
        VERIFY_EXPR(!UnrolledSource.empty());

        // Use serialization CI to be consistent with what will be saved in the archive.
        const auto SerializationCI = GetSerializationCI(ShaderCI);
        if (pRenderDeviceGL)
        {
            // GL shader must be created through the render device as GL functions
            // are not loaded by the archiver.
            pRenderDeviceGL->CreateShader(SerializationCI, &pShaderGL);
            if (!pShaderGL)
                LOG_ERROR_AND_THROW("Failed to create GL shader '", (ShaderCI.Desc.Name ? ShaderCI.Desc.Name : ""), "'.");
        }
        else
        {
            pShaderGL = NEW_RC_OBJ(GetRawAllocator(), "Shader instance", ShaderGLImpl)(nullptr, SerializationCI, GLShaderCI, true /*bIsDeviceInternal*/);
        }
    }

    ShaderCreateInfo GetSerializationCI(ShaderCreateInfo ShaderCI) const
    {
        ShaderCI.FilePath       = nullptr;
        ShaderCI.ByteCode       = nullptr;
        ShaderCI.Source         = UnrolledSource.c_str();
        ShaderCI.SourceLength   = UnrolledSource.length();
        ShaderCI.ShaderCompiler = SHADER_COMPILER_DEFAULT;
        ShaderCI.Macros         = {}; // Macros are inlined into unrolled source

        if (IsOptimized)
        {
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL;
            ShaderCI.EntryPoint     = "main";
        }
        return ShaderCI;
    }

    virtual SerializedData Serialize(ShaderCreateInfo ShaderCI) const override final
    {
        const auto SerializationCI = GetSerializationCI(ShaderCI);
        return SerializedShaderImpl::SerializeCreateInfo(SerializationCI);
    }

    virtual IShader* GetDeviceShader() override final
    {
        return pShaderGL;
    }

private:
    static String UnrollSource(const ShaderCreateInfo& CI)
    {
        String Source;
        if (CI.Macros)
        {
            if (CI.SourceLanguage != SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM)
                AppendShaderMacros(Source, CI.Macros);
            else
                DEV_ERROR("Shader macros are ignored when compiling GLSL verbatim in OpenGL backend");
        }
        Source.append(UnrollShaderIncludes(CI));
        return Source;
    }

    static String TransformSource(const ShaderCreateInfo&                      ShaderCI,
                                  const ShaderGLImpl::CreateInfo&              GLShaderCI,
                                  RENDER_DEVICE_TYPE                           DeviceType,
                                  const SerializationDeviceImpl::GLProperties& GLProps)
    {
        std::string OptimizedGLSL;

#if !DILIGENT_NO_GLSLANG

        RENDER_DEVICE_TYPE            CompileDeviceType = DeviceType;
        RenderDeviceShaderVersionInfo MaxShaderVersion  = GLShaderCI.DeviceInfo.MaxShaderVersion;

        const bool UseGLAngleMultiDrawWorkaround = GetUseGLAngleMultiDrawWorkaround(ShaderCI);
        if (UseGLAngleMultiDrawWorkaround)
        {
            // Since GLSLang does not support GL_ANGLE_multi_draw extension, we need to compile the shader
            // for desktop GL.
            CompileDeviceType = RENDER_DEVICE_TYPE_GL;

            // Use GLSL4.6 as it uses the gl_DrawID built-in variable, same as the ANGLE extension.
            MaxShaderVersion.GLSL = {4, 6};
        }

        const std::string GLSLSourceString = BuildGLSLSourceString(
            {
                ShaderCI,
                GLShaderCI.AdapterInfo,
                GLShaderCI.DeviceInfo.Features,
                CompileDeviceType,
                MaxShaderVersion,
                TargetGLSLCompiler::glslang,
                GLProps.ZeroToOneClipZ, // Note that this is not the same as GLShaderCI.DeviceInfo.NDC.MinZ == 0
            });

        const SHADER_SOURCE_LANGUAGE SourceLang = ParseShaderSourceLanguageDefinition(GLSLSourceString);
        if (ShaderCI.SourceLanguage == SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM && SourceLang != SHADER_SOURCE_LANGUAGE_DEFAULT)
        {
            // This combination of ShaderCI.SourceLanguage and SourceLang indicates that the shader source
            // was retrieved from the existing shader object via IShader::GetBytecode (by e.g. Render State Cache,
            // see RenderStateCacheImpl::SerializeShader).
            // In this case, we don't need to do anything with the source.
            return OptimizedGLSL;
        }

        GLSLangUtils::GLSLtoSPIRVAttribs Attribs;
        Attribs.ShaderType = ShaderCI.Desc.ShaderType;
        VERIFY_EXPR(DeviceType == RENDER_DEVICE_TYPE_GL || DeviceType == RENDER_DEVICE_TYPE_GLES);
        Attribs.Version = DeviceType == RENDER_DEVICE_TYPE_GL ? GLSLangUtils::SpirvVersion::GL : GLSLangUtils::SpirvVersion::GLES;

        Attribs.ppCompilerOutput = GLShaderCI.ppCompilerOutput;
        Attribs.ShaderSource     = GLSLSourceString.c_str();
        Attribs.SourceCodeLen    = static_cast<int>(GLSLSourceString.length());

        std::vector<unsigned int> SPIRV = GLSLangUtils::GLSLtoSPIRV(Attribs);
        if (SPIRV.empty())
            LOG_ERROR_AND_THROW("Failed to compile shader '", ShaderCI.Desc.Name, "'");

        ShaderVersion GLSLVersion;
        Bool          IsES = false;
        GetGLSLVersion(ShaderCI, TargetGLSLCompiler::driver, DeviceType, GLShaderCI.DeviceInfo.MaxShaderVersion, GLSLVersion, IsES);

        diligent_spirv_cross::CompilerGLSL::Options Options;
        Options.es      = IsES;
        Options.version = GLSLVersion.Major * 100 + GLSLVersion.Minor * 10;

        if (UseGLAngleMultiDrawWorkaround)
        {
            // gl_DrawID is not supported in GLES, so compile the shader for desktop GL.
            // This is OK as we strip the version directive and extensions and only leave the GLSL code.
            Options.es = false;

            // Use GLSL4.1 as WebGL does not support binding qualifiers.
            Options.version                  = 410;
            Options.enable_420pack_extension = false;
        }

        Options.separate_shader_objects = GLShaderCI.DeviceInfo.Features.SeparablePrograms;
        // On some targets (WebGPU), uninitialized variables are banned.
        Options.force_zero_initialized_variables = true;
        // For opcodes where we have to perform explicit additional nan checks, very ugly code is generated.
        Options.relax_nan_checks = true;

        Options.fragment.default_float_precision = diligent_spirv_cross::CompilerGLSL::Options::Precision::DontCare;
        Options.fragment.default_int_precision   = diligent_spirv_cross::CompilerGLSL::Options::Precision::DontCare;

#    if PLATFORM_APPLE
        // Apple does not support GL_ARB_shading_language_420pack extension
        Options.enable_420pack_extension = false;
#    endif

        diligent_spirv_cross::CompilerGLSL Compiler{std::move(SPIRV)};
        Compiler.set_common_options(Options);

        OptimizedGLSL = Compiler.compile();
        if (OptimizedGLSL.empty())
            LOG_ERROR_AND_THROW("Failed to generate GLSL for shader '", ShaderCI.Desc.Name, "'");

        // Remove #version directive
        //   The version is added by BuildGLSLSourceString() in ShaderGLImpl.
        // Remove #extension directives
        //   The extensions are added by BuildGLSLSourceString() in ShaderGLImpl.
        // Also remove #error directives like the following:
        //   #ifndef GL_ARB_shader_draw_parameters
        //   #error GL_ARB_shader_draw_parameters is not supported.
        //   #endif
        Parsing::StripPreprocessorDirectives(OptimizedGLSL, {{"version"}, {"extension"}, {"error"}});

        if (UseGLAngleMultiDrawWorkaround)
        {
            PatchSourceForWebGL(OptimizedGLSL, ShaderCI.Desc.ShaderType);
        }

        AppendShaderSourceLanguageDefinition(OptimizedGLSL, (SourceLang != SHADER_SOURCE_LANGUAGE_DEFAULT) ? SourceLang : ShaderCI.SourceLanguage);
#endif

        return OptimizedGLSL;
    }
};

struct ShaderStageInfoGL
{
    ShaderStageInfoGL() {}

    ShaderStageInfoGL(const SerializedShaderImpl* _pShader) :
        Type{_pShader->GetDesc().ShaderType},
        pShader{_pShader}
    {}

    // Needed only for ray tracing
    void Append(const SerializedShaderImpl*) {}

    Uint32 Count() const { return 1; }

    SHADER_TYPE                 Type    = SHADER_TYPE_UNKNOWN;
    const SerializedShaderImpl* pShader = nullptr;
};

#ifdef DILIGENT_DEBUG
inline SHADER_TYPE GetShaderStageType(const ShaderStageInfoGL& Stage)
{
    return Stage.Type;
}
#endif

} // namespace

template <typename CreateInfoType>
void SerializedPipelineStateImpl::PrepareDefaultSignatureGL(const CreateInfoType& CreateInfo) noexcept(false)
{
    // Add empty device signature - there must be some device-specific data for OpenGL in the archive
    // or there will be an error when unpacking the signature.
    std::vector<ShaderGLImpl*> DummyShadersGL;
    CreateDefaultResourceSignature<PipelineStateGLImpl, PipelineResourceSignatureGLImpl>(DeviceType::OpenGL, CreateInfo.PSODesc, SHADER_TYPE_UNKNOWN, DummyShadersGL);
}

template <typename CreateInfoType>
void SerializedPipelineStateImpl::PatchShadersGL(const CreateInfoType& CreateInfo) noexcept(false)
{
    std::vector<ShaderStageInfoGL> ShaderStages;
    SHADER_TYPE                    ActiveShaderStages = SHADER_TYPE_UNKNOWN;
    PipelineStateGLImpl::ExtractShaders<SerializedShaderImpl>(CreateInfo, ShaderStages, ActiveShaderStages);

    VERIFY_EXPR(m_Data.Shaders[static_cast<size_t>(DeviceType::OpenGL)].empty());
    for (size_t i = 0; i < ShaderStages.size(); ++i)
    {
        const auto& Stage             = ShaderStages[i];
        const auto& CI                = Stage.pShader->GetCreateInfo();
        const auto* pCompiledShaderGL = Stage.pShader->GetShader<CompiledShaderGL>(DeviceObjectArchive::DeviceType::OpenGL);
        const auto  SerCI             = pCompiledShaderGL->GetSerializationCI(CI);

        SerializeShaderCreateInfo(DeviceType::OpenGL, SerCI);
    }
    VERIFY_EXPR(m_Data.Shaders[static_cast<size_t>(DeviceType::OpenGL)].size() == ShaderStages.size());
}

INSTANTIATE_PATCH_SHADER_METHODS(PatchShadersGL)
INSTANTIATE_DEVICE_SIGNATURE_METHODS(PipelineResourceSignatureGLImpl)

INSTANTIATE_PREPARE_DEF_SIGNATURE_GL(GraphicsPipelineStateCreateInfo);
INSTANTIATE_PREPARE_DEF_SIGNATURE_GL(ComputePipelineStateCreateInfo);
INSTANTIATE_PREPARE_DEF_SIGNATURE_GL(TilePipelineStateCreateInfo);
INSTANTIATE_PREPARE_DEF_SIGNATURE_GL(RayTracingPipelineStateCreateInfo);


void SerializationDeviceImpl::GetPipelineResourceBindingsGL(const PipelineResourceBindingAttribs& Info,
                                                            std::vector<PipelineResourceBinding>& ResourceBindings)
{
    const auto            ShaderStages        = (Info.ShaderStages == SHADER_TYPE_UNKNOWN ? static_cast<SHADER_TYPE>(~0u) : Info.ShaderStages);
    constexpr SHADER_TYPE SupportedStagesMask = (SHADER_TYPE_ALL_GRAPHICS | SHADER_TYPE_COMPUTE);

    SignatureArray<PipelineResourceSignatureGLImpl> Signatures      = {};
    Uint32                                          SignaturesCount = 0;
    SortResourceSignatures(Info.ppResourceSignatures, Info.ResourceSignaturesCount, Signatures, SignaturesCount);

    PipelineResourceSignatureGLImpl::TBindings BaseBindings = {};
    for (Uint32 s = 0; s < SignaturesCount; ++s)
    {
        const auto& pSignature = Signatures[s];
        if (pSignature == nullptr)
            continue;

        for (Uint32 r = 0; r < pSignature->GetTotalResourceCount(); ++r)
        {
            const auto& ResDesc = pSignature->GetResourceDesc(r);
            const auto& ResAttr = pSignature->GetResourceAttribs(r);
            const auto  Range   = PipelineResourceToBindingRange(ResDesc);

            for (auto Stages = ShaderStages & SupportedStagesMask; Stages != 0;)
            {
                const auto ShaderStage = ExtractLSB(Stages);
                if ((ResDesc.ShaderStages & ShaderStage) == 0)
                    continue;

                ResourceBindings.push_back(ResDescToPipelineResBinding(ResDesc, ShaderStage, BaseBindings[Range] + ResAttr.CacheOffset, 0 /*space*/));
            }
        }
        pSignature->ShiftBindings(BaseBindings);
    }
}

void SerializedShaderImpl::CreateShaderGL(IReferenceCounters*     pRefCounters,
                                          const ShaderCreateInfo& ShaderCI,
                                          RENDER_DEVICE_TYPE      DeviceType,
                                          IDataBlob**             ppCompilerOutput) noexcept(false)
{
    const ShaderGLImpl::CreateInfo GLShaderCI{
        m_pDevice->GetDeviceInfo(),
        m_pDevice->GetAdapterInfo(),
        // Do not overwrite compiler output from other APIs.
        // TODO: collect all outputs.
        ppCompilerOutput == nullptr || *ppCompilerOutput == nullptr ? ppCompilerOutput : nullptr,
    };

    CreateShader<CompiledShaderGL>(DeviceType::OpenGL, pRefCounters, ShaderCI, GLShaderCI, m_pDevice->GetRenderDevice(DeviceType),
                                   DeviceType, m_pDevice->GetGLProperties());
}

} // namespace Diligent
