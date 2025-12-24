/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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

#include "Vulkan/TestingEnvironmentVk.hpp"
#include "Vulkan/TestingSwapChainVk.hpp"

#include "DeviceContextVk.h"
#include "RenderDeviceVk.h"

#include "GLSLangUtils.hpp"
#include "DXCompiler.hpp"

#include "volk.h"

#include "gtest/gtest.h"

// Forward declarations - avoid including SPIRVTools.hpp directly
namespace Diligent
{
std::vector<uint32_t> ConvertUBOToPushConstants(const std::vector<uint32_t>& SPIRV,
                                                const std::string&           BlockName);
}

namespace Diligent
{

namespace Testing
{

// Forward declaration of reference renderer
void RenderDrawCommandReferenceVk(ISwapChain* pSwapChain, const float* pClearColor);

namespace
{

class VkConvertUBOToPushConstantsTest : public ::testing::Test
{
public:
    static std::unique_ptr<IDXCompiler> DXCompiler;

protected:
    static void SetUpTestSuite()
    {
        GLSLangUtils::InitializeGlslang();

        DXCompiler = CreateDXCompiler(DXCompilerTarget::Vulkan, 0, nullptr);
    }

    static void TearDownTestSuite()
    {
        GLSLangUtils::FinalizeGlslang();

        DXCompiler.reset();
    }
};

std::unique_ptr<IDXCompiler> VkConvertUBOToPushConstantsTest::DXCompiler;

// GLSL Vertex Shader - procedural two triangles (same as reference)
const std::string GLSL_ProceduralTriangleVS = R"(
#version 450 core

layout(location = 0) out vec3 out_Color;

void main()
{
    vec4 Pos[6];
    Pos[0] = vec4(-1.0, -0.5, 0.0, 1.0);
    Pos[1] = vec4(-0.5, +0.5, 0.0, 1.0);
    Pos[2] = vec4( 0.0, -0.5, 0.0, 1.0);

    Pos[3] = vec4(+0.0, -0.5, 0.0, 1.0);
    Pos[4] = vec4(+0.5, +0.5, 0.0, 1.0);
    Pos[5] = vec4(+1.0, -0.5, 0.0, 1.0);

    vec3 Col[6];
    Col[0] = vec3(1.0, 0.0, 0.0);
    Col[1] = vec3(0.0, 1.0, 0.0);
    Col[2] = vec3(0.0, 0.0, 1.0);

    Col[3] = vec3(1.0, 0.0, 0.0);
    Col[4] = vec3(0.0, 1.0, 0.0);
    Col[5] = vec3(0.0, 0.0, 1.0);

    gl_Position = Pos[gl_VertexIndex];
    out_Color = Col[gl_VertexIndex];
}
)";

// GLSL Fragment Shader with UBO - will be patched to push constants
// Uses nested struct to test access chain propagation
const std::string GLSL_FragmentShaderWithUBO = R"(
#version 450 core

// Deeply nested structs to test multiple access chains and storage class propagation
struct Level3Data
{
    vec4 Factor;
};

struct Level2Data
{
    Level3Data Inner;
};

struct Level1Data
{
    Level2Data Nested;
};

// UBO named "CB1" with instance name "cb" - allows testing both name matching paths
layout(set = 0, binding = 0) uniform CB1
{
    Level1Data Data;
} cb;

layout(location = 0) in  vec3 in_Color;
layout(location = 0) out vec4 out_Color;

void main()
{
    // Access deeply nested member to generate multiple OpAccessChain instructions
    // This tests PropagateStorageClass with multiple levels of pointer indirection
    out_Color = vec4(in_Color, 1.0) * cb.Data.Nested.Inner.Factor;
}
)";

// Push constant data structure matching the UBO layout
struct PushConstantData
{
    float Factor[4]; // vec4 Factor in InnerData
};

// HLSL Vertex Shader - procedural two triangles (same as reference)
const std::string HLSL_ProceduralTriangleVS = R"(
struct PSInput
{
    float4 Pos   : SV_POSITION;
    float3 Color : COLOR;
};

PSInput main(uint VertexId : SV_VertexID)
{
    float4 Pos[6];
    Pos[0] = float4(-1.0, -0.5, 0.0, 1.0);
    Pos[1] = float4(-0.5, +0.5, 0.0, 1.0);
    Pos[2] = float4( 0.0, -0.5, 0.0, 1.0);

    Pos[3] = float4(+0.0, -0.5, 0.0, 1.0);
    Pos[4] = float4(+0.5, +0.5, 0.0, 1.0);
    Pos[5] = float4(+1.0, -0.5, 0.0, 1.0);

    float3 Col[6];
    Col[0] = float3(1.0, 0.0, 0.0);
    Col[1] = float3(0.0, 1.0, 0.0);
    Col[2] = float3(0.0, 0.0, 1.0);

    Col[3] = float3(1.0, 0.0, 0.0);
    Col[4] = float3(0.0, 1.0, 0.0);
    Col[5] = float3(0.0, 0.0, 1.0);

    PSInput Out;
    Out.Pos   = Pos[VertexId];
    Out.Color = Col[VertexId];
    return Out;
}
)";

// HLSL Fragment Shader with constant buffer - will be patched to push constants
// struct CB1 with instance name cb - allows testing both name matching paths
const std::string HLSL_FragmentShaderWithCB = R"(
// Deeply nested structs to test multiple access chains
struct Level3Data
{
    float4 Factor;
};

struct Level2Data
{
    Level3Data Inner;
};

struct Level1Data
{
    Level2Data Nested;
};

// Constant buffer named "CB1"
cbuffer CB1 : register(b0)
{
    Level1Data Data;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float3 Color : COLOR;
};

float4 main(PSInput In) : SV_Target
{
    // Access deeply nested member to generate multiple OpAccessChain instructions
    // This tests PropagateStorageClass with multiple levels of pointer indirection
    return float4(In.Color, 1.0) * Data.Nested.Inner.Factor;
}
)";

// Helper to create VkShaderModule from SPIR-V bytecode
VkShaderModule CreateVkShaderModuleFromSPIRV(VkDevice vkDevice, const std::vector<uint32_t>& SPIRV)
{
    VkShaderModuleCreateInfo ShaderModuleCI = {};
    ShaderModuleCI.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCI.pNext                    = nullptr;
    ShaderModuleCI.flags                    = 0;
    ShaderModuleCI.codeSize                 = SPIRV.size() * sizeof(uint32_t);
    ShaderModuleCI.pCode                    = SPIRV.data();

    VkShaderModule vkShaderModule = VK_NULL_HANDLE;
    VkResult       res            = vkCreateShaderModule(vkDevice, &ShaderModuleCI, nullptr, &vkShaderModule);
    VERIFY_EXPR(res == VK_SUCCESS);
    (void)res;

    return vkShaderModule;
}

std::vector<unsigned int> LoadSPIRVFromHLSL(const std::string& ShaderSource,
                                            SHADER_TYPE        ShaderType,
                                            SHADER_COMPILER    Compiler = SHADER_COMPILER_DEFAULT)
{
    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Source         = ShaderSource.data();
    ShaderCI.SourceLength   = ShaderSource.size();
    ShaderCI.Desc           = {"SPIRV test shader", ShaderType};
    ShaderCI.EntryPoint     = "main";

    std::vector<unsigned int> SPIRV;

    if (Compiler == SHADER_COMPILER_DXC)
    {
        if (!VkConvertUBOToPushConstantsTest::DXCompiler || !VkConvertUBOToPushConstantsTest::DXCompiler->IsLoaded())
        {
            UNEXPECTED("Test should be skipped if DXCompiler is not available");
            return {};
        }

        RefCntAutoPtr<IDataBlob> pCompilerOutput;
        VkConvertUBOToPushConstantsTest::DXCompiler->Compile(ShaderCI, ShaderVersion{6, 0}, nullptr, nullptr, &SPIRV, &pCompilerOutput);

        if (pCompilerOutput && pCompilerOutput->GetSize() > 0)
        {
            const char* CompilerOutput = static_cast<const char*>(pCompilerOutput->GetConstDataPtr());
            if (*CompilerOutput != 0)
                LOG_INFO_MESSAGE("DXC compiler output:\n", CompilerOutput);
        }
    }
    else
    {
        SPIRV = GLSLangUtils::HLSLtoSPIRV(ShaderCI, GLSLangUtils::SpirvVersion::Vk100, nullptr, nullptr);
    }

    return SPIRV;
}

std::vector<unsigned int> LoadSPIRVFromGLSL(const std::string& ShaderSource, SHADER_TYPE ShaderType = SHADER_TYPE_PIXEL)
{
    // Ray tracing shaders require Vulkan 1.1 or higher SPIR-V version
    GLSLangUtils::SpirvVersion Version = GLSLangUtils::SpirvVersion::Vk100;
    if (ShaderType == SHADER_TYPE_RAY_GEN ||
        ShaderType == SHADER_TYPE_RAY_MISS ||
        ShaderType == SHADER_TYPE_RAY_CLOSEST_HIT ||
        ShaderType == SHADER_TYPE_RAY_ANY_HIT ||
        ShaderType == SHADER_TYPE_RAY_INTERSECTION ||
        ShaderType == SHADER_TYPE_CALLABLE)
    {
        Version = GLSLangUtils::SpirvVersion::Vk110_Spirv14;
    }

    GLSLangUtils::GLSLtoSPIRVAttribs Attribs;
    Attribs.ShaderType                 = ShaderType;
    Attribs.ShaderSource               = ShaderSource.data();
    Attribs.SourceCodeLen              = static_cast<int>(ShaderSource.size());
    Attribs.Version                    = Version;
    Attribs.AssignBindings             = true;

    return GLSLangUtils::GLSLtoSPIRV(Attribs);
}

void CompileSPIRV(const std::string&         ShaderSource,
                  const std::string&         ShaderIdentifier,
                  SHADER_COMPILER            Compiler,
                  SHADER_TYPE                ShaderType,
                  SHADER_SOURCE_LANGUAGE     SourceLanguage,
                  std::vector<unsigned int>& SPIRV)
{
    if (Compiler == SHADER_COMPILER_DXC)
    {
        VERIFY(SourceLanguage == SHADER_SOURCE_LANGUAGE_HLSL, "DXC only supports HLSL");
        if (!VkConvertUBOToPushConstantsTest::DXCompiler || !VkConvertUBOToPushConstantsTest::DXCompiler->IsLoaded())
        {
            GTEST_SKIP() << "DXC compiler is not available";
        }
    }

    SPIRV = (SourceLanguage == SHADER_SOURCE_LANGUAGE_GLSL) ?
        LoadSPIRVFromGLSL(ShaderSource, ShaderType) :
        LoadSPIRVFromHLSL(ShaderSource, ShaderType, Compiler);
    ASSERT_FALSE(SPIRV.empty()) << "Failed to compile shader " << ShaderIdentifier;
}

// Renderer that uses patched push constants shader
class PatchedPushConstantsRenderer
{
public:
    PatchedPushConstantsRenderer(ISwapChain*                    pSwapChain,
                                 VkRenderPass                   vkRenderPass,
                                 const std::vector<uint32_t>&   VS_SPIRV,
                                 const std::vector<uint32_t>&   FS_SPIRV,
                                 uint32_t                       PushConstantSize,
                                 VkShaderStageFlags             PushConstantStages = VK_SHADER_STAGE_FRAGMENT_BIT)
    {
        auto* pEnv     = TestingEnvironmentVk::GetInstance();
        m_vkDevice     = pEnv->GetVkDevice();

        const auto& SCDesc = pSwapChain->GetDesc();

        // Create shader modules from SPIR-V
        m_vkVSModule = CreateVkShaderModuleFromSPIRV(m_vkDevice, VS_SPIRV);
        VERIFY_EXPR(m_vkVSModule != VK_NULL_HANDLE);

        m_vkFSModule = CreateVkShaderModuleFromSPIRV(m_vkDevice, FS_SPIRV);
        VERIFY_EXPR(m_vkFSModule != VK_NULL_HANDLE);

        // Pipeline layout with push constants (no descriptor sets)
        VkPushConstantRange PushConstantRange = {};
        PushConstantRange.stageFlags          = PushConstantStages;
        PushConstantRange.offset              = 0;
        PushConstantRange.size                = PushConstantSize;

        VkPipelineLayoutCreateInfo PipelineLayoutCI = {};
        PipelineLayoutCI.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        PipelineLayoutCI.setLayoutCount             = 0;
        PipelineLayoutCI.pSetLayouts                = nullptr;
        PipelineLayoutCI.pushConstantRangeCount     = 1;
        PipelineLayoutCI.pPushConstantRanges        = &PushConstantRange;

        VkResult res = vkCreatePipelineLayout(m_vkDevice, &PipelineLayoutCI, nullptr, &m_vkLayout);
        VERIFY_EXPR(res == VK_SUCCESS);
        (void)res;

        // Create graphics pipeline
        VkGraphicsPipelineCreateInfo PipelineCI = {};
        PipelineCI.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        VkPipelineShaderStageCreateInfo ShaderStages[2] = {};
        ShaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ShaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        ShaderStages[0].module = m_vkVSModule;
        ShaderStages[0].pName  = "main";

        ShaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ShaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        ShaderStages[1].module = m_vkFSModule;
        ShaderStages[1].pName  = "main";

        PipelineCI.pStages    = ShaderStages;
        PipelineCI.stageCount = 2;
        PipelineCI.layout     = m_vkLayout;

        VkPipelineVertexInputStateCreateInfo VertexInputStateCI = {};
        VertexInputStateCI.sType     = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        PipelineCI.pVertexInputState = &VertexInputStateCI;

        VkPipelineInputAssemblyStateCreateInfo InputAssemblyCI = {};
        InputAssemblyCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        InputAssemblyCI.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        InputAssemblyCI.primitiveRestartEnable = VK_FALSE;
        PipelineCI.pInputAssemblyState         = &InputAssemblyCI;

        VkPipelineTessellationStateCreateInfo TessStateCI = {};
        TessStateCI.sType             = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        PipelineCI.pTessellationState = &TessStateCI;

        VkPipelineViewportStateCreateInfo ViewPortStateCI = {};
        ViewPortStateCI.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        ViewPortStateCI.viewportCount = 1;

        VkViewport Viewport = {};
        Viewport.y          = static_cast<float>(SCDesc.Height);
        Viewport.width      = static_cast<float>(SCDesc.Width);
        Viewport.height     = -static_cast<float>(SCDesc.Height);
        Viewport.maxDepth   = 1;
        ViewPortStateCI.pViewports = &Viewport;

        ViewPortStateCI.scissorCount = 1;
        VkRect2D ScissorRect         = {};
        ScissorRect.extent.width     = SCDesc.Width;
        ScissorRect.extent.height    = SCDesc.Height;
        ViewPortStateCI.pScissors    = &ScissorRect;
        PipelineCI.pViewportState    = &ViewPortStateCI;

        VkPipelineRasterizationStateCreateInfo RasterizerStateCI = {};
        RasterizerStateCI.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        RasterizerStateCI.polygonMode = VK_POLYGON_MODE_FILL;
        RasterizerStateCI.cullMode    = VK_CULL_MODE_NONE;
        RasterizerStateCI.lineWidth   = 1;
        PipelineCI.pRasterizationState = &RasterizerStateCI;

        VkPipelineMultisampleStateCreateInfo MSStateCI = {};
        MSStateCI.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        MSStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        uint32_t SampleMask[]          = {0xFFFFFFFF, 0};
        MSStateCI.pSampleMask          = SampleMask;
        PipelineCI.pMultisampleState   = &MSStateCI;

        VkPipelineDepthStencilStateCreateInfo DepthStencilStateCI = {};
        DepthStencilStateCI.sType     = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        PipelineCI.pDepthStencilState = &DepthStencilStateCI;

        VkPipelineColorBlendStateCreateInfo BlendStateCI = {};
        VkPipelineColorBlendAttachmentState Attachment   = {};
        Attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        BlendStateCI.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        BlendStateCI.pAttachments    = &Attachment;
        BlendStateCI.attachmentCount = 1;
        PipelineCI.pColorBlendState  = &BlendStateCI;

        VkPipelineDynamicStateCreateInfo DynamicStateCI = {};
        DynamicStateCI.sType     = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        PipelineCI.pDynamicState = &DynamicStateCI;

        PipelineCI.renderPass         = vkRenderPass;
        PipelineCI.subpass            = 0;
        PipelineCI.basePipelineHandle = VK_NULL_HANDLE;
        PipelineCI.basePipelineIndex  = 0;

        res = vkCreateGraphicsPipelines(m_vkDevice, VK_NULL_HANDLE, 1, &PipelineCI, nullptr, &m_vkPipeline);
        VERIFY_EXPR(res == VK_SUCCESS);
        VERIFY_EXPR(m_vkPipeline != VK_NULL_HANDLE);

        m_PushConstantStages = PushConstantStages;
    }

    void Draw(VkCommandBuffer vkCmdBuffer, const void* pPushConstantData, uint32_t PushConstantSize)
    {
        vkCmdBindPipeline(vkCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);
        vkCmdPushConstants(vkCmdBuffer, m_vkLayout, m_PushConstantStages, 0, PushConstantSize, pPushConstantData);
        vkCmdDraw(vkCmdBuffer, 6, 1, 0, 0);
    }

    ~PatchedPushConstantsRenderer()
    {
        vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
        vkDestroyPipelineLayout(m_vkDevice, m_vkLayout, nullptr);
        vkDestroyShaderModule(m_vkDevice, m_vkVSModule, nullptr);
        vkDestroyShaderModule(m_vkDevice, m_vkFSModule, nullptr);
    }

private:
    VkDevice           m_vkDevice           = VK_NULL_HANDLE;
    VkShaderModule     m_vkVSModule         = VK_NULL_HANDLE;
    VkShaderModule     m_vkFSModule         = VK_NULL_HANDLE;
    VkPipeline         m_vkPipeline         = VK_NULL_HANDLE;
    VkPipelineLayout   m_vkLayout           = VK_NULL_HANDLE;
    VkShaderStageFlags m_PushConstantStages = 0;
};

// Test helper that runs the full test flow
void RunConvertUBOToPushConstantsTest(SHADER_COMPILER Compiler, const std::string& BlockName)
{
    auto* pEnv = TestingEnvironmentVk::GetInstance();
    if (!pEnv)
    {
        GTEST_SKIP() << "Vulkan environment not available";
        return;
    }

    auto* pDevice = pEnv->GetDevice();
    if (pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_VULKAN)
    {
        GTEST_SKIP() << "This test requires Vulkan device";
        return;
    }

    if (Compiler == SHADER_COMPILER_DXC)
    {
        if (!VkConvertUBOToPushConstantsTest::DXCompiler || 
            !VkConvertUBOToPushConstantsTest::DXCompiler->IsLoaded())
        {
            GTEST_SKIP() << "Skipped because DXCompiler not available";
            return;
        }
    }


    auto* pContext   = pEnv->GetDeviceContext();
    auto* pSwapChain = pEnv->GetSwapChain();

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    if (!pTestingSwapChain)
    {
        GTEST_SKIP() << "Testing swap chain not available";
        return;
    }

    auto* pTestingSwapChainVk = ClassPtrCast<TestingSwapChainVk>(pSwapChain);

    // Step 1: Render reference using existing ReferenceTriangleRenderer
    pContext->Flush();
    pContext->InvalidateState();

    const float ClearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
    RenderDrawCommandReferenceVk(pSwapChain, ClearColor);

    // Take snapshot of reference
    pTestingSwapChain->TakeSnapshot();

    // Step 2: Compile shaders to SPIR-V
    std::vector<uint32_t> VS_SPIRV;
    CompileSPIRV(GLSL_ProceduralTriangleVS, "GLSL_ProceduralTriangleVS", Compiler, SHADER_TYPE_VERTEX, SHADER_SOURCE_LANGUAGE_GLSL, VS_SPIRV);
    ASSERT_FALSE(VS_SPIRV.empty()) << "Failed to compile vertex shader";

    std::vector<uint32_t> FS_SPIRV;
    CompileSPIRV(GLSL_FragmentShaderWithUBO, "GLSL_FragmentShaderWithUBO", Compiler, SHADER_TYPE_PIXEL, SHADER_SOURCE_LANGUAGE_GLSL, FS_SPIRV);
    ASSERT_FALSE(FS_SPIRV.empty()) << "Failed to compile fragment shader";

    {
        auto fp = fopen("d:/unpatched.spv", "wb");
        fwrite(FS_SPIRV.data(), FS_SPIRV.size() * 4, 1, fp);
        fclose(fp);
    }

    // Step 3: Patch fragment shader to use push constants
    std::vector<uint32_t> FS_SPIRV_Patched = ConvertUBOToPushConstants(FS_SPIRV, BlockName);
    ASSERT_FALSE(FS_SPIRV_Patched.empty()) << "Failed to patch UBO to push constants with BlockName: " << BlockName;

    {
        auto fp = fopen("d:/patched.spv", "wb");
        fwrite(FS_SPIRV_Patched.data(), FS_SPIRV_Patched.size() * 4, 1, fp);
        fclose(fp);
    }

    // Step 4: Create renderer with patched shaders
    PatchedPushConstantsRenderer Renderer{
        pSwapChain,
        pTestingSwapChainVk->GetRenderPass(),
        VS_SPIRV,
        FS_SPIRV_Patched,
        sizeof(PushConstantData),
        VK_SHADER_STAGE_FRAGMENT_BIT};

    // Step 5: Render with push constants
    VkCommandBuffer vkCmdBuffer = pEnv->AllocateCommandBuffer();

    pTestingSwapChainVk->BeginRenderPass(vkCmdBuffer,
                                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                         ClearColor);

    // Set push constant data - Factor = (1,1,1,1) to make output identical to reference
    PushConstantData PushData = {};
    PushData.Factor[0]        = 1.0f;
    PushData.Factor[1]        = 1.0f;
    PushData.Factor[2]        = 1.0f;
    PushData.Factor[3]        = 1.0f;

    Renderer.Draw(vkCmdBuffer, &PushData, sizeof(PushData));

    pTestingSwapChainVk->EndRenderPass(vkCmdBuffer);
    vkEndCommandBuffer(vkCmdBuffer);
    pEnv->SubmitCommandBuffer(vkCmdBuffer, true);

    // Step 6: Present triggers comparison with snapshot
    pSwapChain->Present();
}

} // namespace

// Test patching UBO using struct type name "CB1"
TEST(VkConvertUBOToPushConstantsTest, PatchByStructTypeName_GLSLang)
{
    RunConvertUBOToPushConstantsTest(SHADER_COMPILER_GLSLANG, "CB1");
}

// Test patching UBO using variable instance name "cb"
TEST(VkConvertUBOToPushConstantsTest, PatchByVariableName_GLSLang)
{
    RunConvertUBOToPushConstantsTest(SHADER_COMPILER_GLSLANG, "cb");
}

// Test patching CB using cbuffer block name "CB1" with DXC compiler
// Note: In HLSL, cbuffer name and struct name may be the same or different.
// DXC typically generates both OpName for the struct type and the variable.
TEST(VkConvertUBOToPushConstantsTest, PatchByStructTypeName_DXC)
{
    RunConvertUBOToPushConstantsTest(SHADER_COMPILER_DXC, "CB1");
}


} // namespace Testing

} // namespace Diligent

