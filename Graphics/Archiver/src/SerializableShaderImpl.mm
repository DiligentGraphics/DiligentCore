/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include <stdio.h>
#include "SerializableShaderImpl.hpp"

#include "RenderDeviceMtlImpl.hpp"
#include "ShaderMtlImpl.hpp"
#include "PipelineStateMtlImpl.hpp"

#include "spirv_msl.hpp"

namespace Diligent
{
static_assert(std::is_same_v<MtlArchiverResourceCounters, MtlResourceCounters>,
              "MtlArchiverResourceCounters and MtlResourceCounters must be same types");
    
struct SerializableShaderImpl::CompiledShaderMtlImpl final : ICompiledShaderMtl
{
    String                                      MslSource;
    std::vector<uint32_t>                       SPIRV;
    std::unique_ptr<SPIRVShaderResources>       SPIRVResources;
    MtlFunctionArguments::BufferTypeInfoMapType BufferTypeInfoMap;
};

void SerializableShaderImpl::CompileShaderMtl(ShaderCreateInfo& ShaderCI, String& CompilationLog) noexcept(false)
{
    auto* pShaderMtl = new CompiledShaderMtlImpl{};
    m_pShaderMtl.reset(pShaderMtl);

    // Mem leak when used RefCntAutoPtr
    IDataBlob* pLog           = nullptr;
    ShaderCI.ppCompilerOutput = &pLog;

    // Convert HLSL/GLSL/SPIRV to MSL
    try
    {
        ShaderMtlImpl::ConvertToMSL(ShaderCI,
                                    m_pDevice->GetDeviceInfo(),
                                    m_pDevice->GetAdapterInfo(),
                                    pShaderMtl->MslSource,
                                    pShaderMtl->SPIRV,
                                    pShaderMtl->SPIRVResources,
                                    pShaderMtl->BufferTypeInfoMap); // may throw exception
    }
    catch (...)
    {
        if (pLog && pLog->GetConstDataPtr())
        {
            CompilationLog += "Failed to compile Metal shader:\n";
            CompilationLog += static_cast<const char*>(pLog->GetConstDataPtr());
        }
    }

    if (pLog)
        pLog->Release();
}

const SPIRVShaderResources* SerializableShaderImpl::GetMtlShaderSPIRVResources() const
{
    auto* pShaderMtl = static_cast<const CompiledShaderMtlImpl*>(m_pShaderMtl.get());
    return pShaderMtl && pShaderMtl->SPIRVResources ? pShaderMtl->SPIRVResources.get() : nullptr;
}


SerializedMemory SerializableShaderImpl::PatchShaderMtl(const RefCntAutoPtr<PipelineResourceSignatureMtlImpl>* pSignatures,
                                                        const MtlResourceCounters*                             pBaseBindings,
                                                        const Uint32                                           SignatureCount) const noexcept(false)
{
    VERIFY_EXPR(SignatureCount > 0);
    VERIFY_EXPR(pSignatures != nullptr);
    VERIFY_EXPR(pBaseBindings != nullptr);

    const auto RemoveTempFiles = []() {
        std::remove("Shader.metal");
        std::remove("Shader.air");
        std::remove("Shader.metallib");
        //std::remove("Shader.metallibsym");
    };
    RemoveTempFiles();
    
    auto*  pShaderMtl = static_cast<const CompiledShaderMtlImpl*>(m_pShaderMtl.get());
    String MslSource  = pShaderMtl->MslSource;
    MtlFunctionArguments::BufferTypeInfoMapType BufferTypeInfoMap;

    if (!pShaderMtl->SPIRV.empty())
    {
        try
        {
            // Shader can be patched as SPIRV
            VERIFY_EXPR(pShaderMtl->SPIRVResources != nullptr);
            ShaderMtlImpl::MtlResourceRemappingVectorType ResRemapping;

            PipelineStateMtlImpl::RemapShaderResources(pShaderMtl->SPIRV,
                                                       *pShaderMtl->SPIRVResources,
                                                       ResRemapping,
                                                       pSignatures,
                                                       SignatureCount,
                                                       pBaseBindings,
                                                       GetDesc(),
                                                       ""); // may throw exception
        
            MslSource = ShaderMtlImpl::SPIRVtoMSL(pShaderMtl->SPIRV,
                                                  GetCreateInfo(),
                                                  &ResRemapping,
                                                  BufferTypeInfoMap); // may throw exception
        }
        catch (...)
        {
            LOG_ERROR_AND_THROW("Failed to patch Metal shader");
        }
    }
    
    //if (!pDevice->GetMtlTempShaderFolder().empty())
    //    chdir(pDevice->GetMtlTempShaderFolder().c_str());
    
    // Save to 'Shader.metal'
    {
        FILE* File = fopen("Shader.metal", "wb");
        if (File == nullptr)
            LOG_ERROR_AND_THROW("Failed to save shader source");

        fwrite(MslSource.c_str(), sizeof(MslSource[0]), MslSource.size(), File);
        fclose(File);
    }

    // Run user-defined MSL preprocessor
    if (!m_pDevice->GetMslPreprocessorCmd().empty())
    {
        FILE* File = popen((m_pDevice->GetMslPreprocessorCmd() + " Shader.metal").c_str(), "r");
        if (File == nullptr)
            LOG_ERROR_AND_THROW("Failed to run command line Metal shader compiler");

        char Output[512];
        while (fgets(Output, _countof(Output), File) != nullptr)
            printf("%s", Output);
    
        auto status = pclose(File);
        if (status == -1)
            LOG_ERROR_MESSAGE("Failed to close process");
    }

    // https://developer.apple.com/documentation/metal/libraries/generating_and_loading_a_metal_library_symbol_file?language=objc
    
    // Compile MSL to AIR file
    {
        String cmd{"xcrun -sdk macosx metal "};
        cmd += m_pDevice->GetMtlCompileOptions();
        cmd += " -c Shader.metal -o Shader.air";

        FILE* File = popen(cmd.c_str(), "r");
        if (File == nullptr)
            LOG_ERROR_AND_THROW("Failed to compile MSL to AIR");

        char Output[512];
        while (fgets(Output, _countof(Output), File) != nullptr)
            printf("%s", Output);
    
        auto status = pclose(File);
        if (status == -1)
            LOG_ERROR_MESSAGE("Failed to close process");
    }

    // Generate a Metal library
    {
        String cmd{"xcrun -sdk macosx metallib "};
        cmd += m_pDevice->GetMtlLinkOptions();
        cmd += " Shader.air -o Shader.metallib";

        FILE* File = popen(cmd.c_str(), "r");
        if (File == nullptr)
            LOG_ERROR_AND_THROW("Failed to generate Metal library");

        char Output[512];
        while (fgets(Output, _countof(Output), File) != nullptr)
            printf("%s", Output);
    
        auto status = pclose(File);
        if (status == -1)
            LOG_ERROR_MESSAGE("Failed to close process");
    }

    // AZ TODO: separate debug info ?

    // Read 'default.metallib'
    std::unique_ptr<Uint8> Bytecode;
    size_t                 BytecodeSize = 0;
    {
        FILE* File = fopen("Shader.metallib", "rb");
        if (File == nullptr)
            LOG_ERROR_AND_THROW("Failed to read shader library");

        fseek(File, 0, SEEK_END);
        long size = ftell(File);
        fseek(File, 0, SEEK_SET);

        Bytecode.reset(new Uint8[size]);
        BytecodeSize = fread(Bytecode.get(), 1, size, File);

        fclose(File);
    }
    
    RemoveTempFiles();

    if (BytecodeSize == 0)
        LOG_ERROR_AND_THROW("Metal shader library is empty");
    
    // AZ TODO: serialize BufferTypeInfoMap

    return SerializedMemory{Bytecode.release(), BytecodeSize};
}

} // namespace Diligent
