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
#include "TextureVk.h"


#include "GLSLangUtils.hpp"
#include "DXCompiler.hpp"
#include "SPIRVTools.hpp"

#include "volk.h"

#include "gtest/gtest.h"

namespace Diligent
{

VkFormat TexFormatToVkFormat(TEXTURE_FORMAT TexFmt);

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
    PatchedPushConstantsRenderer(TestingSwapChainVk*          pSwapChain,
                                 const std::vector<uint32_t>& VS_SPIRV,
                                 const std::vector<uint32_t>& FS_SPIRV,
                                 uint32_t                     PushConstantSize,
                                 VkShaderStageFlags           PushConstantStages = VK_SHADER_STAGE_FRAGMENT_BIT)
    {
        m_pSwapChain = pSwapChain;

        auto* pEnv = TestingEnvironmentVk::GetInstance();
        m_vkDevice = pEnv->GetVkDevice();

        const auto& SCDesc = pSwapChain->GetDesc();

        CreateRenderPass();

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
        ShaderStages[0].sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ShaderStages[0].stage                           = VK_SHADER_STAGE_VERTEX_BIT;
        ShaderStages[0].module                          = m_vkVSModule;
        ShaderStages[0].pName                           = "main";

        ShaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ShaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        ShaderStages[1].module = m_vkFSModule;
        ShaderStages[1].pName  = "main";

        PipelineCI.pStages    = ShaderStages;
        PipelineCI.stageCount = _countof(ShaderStages);
        PipelineCI.layout     = m_vkLayout;

        VkPipelineVertexInputStateCreateInfo VertexInputStateCI = {};
        VertexInputStateCI.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        PipelineCI.pVertexInputState                            = &VertexInputStateCI;

        VkPipelineInputAssemblyStateCreateInfo InputAssemblyCI = {};
        InputAssemblyCI.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        InputAssemblyCI.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        InputAssemblyCI.primitiveRestartEnable                 = VK_FALSE;
        PipelineCI.pInputAssemblyState                         = &InputAssemblyCI;

        VkPipelineTessellationStateCreateInfo TessStateCI = {};
        TessStateCI.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        PipelineCI.pTessellationState                     = &TessStateCI;

        VkPipelineViewportStateCreateInfo ViewPortStateCI = {};
        ViewPortStateCI.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        ViewPortStateCI.viewportCount                     = 1;

        VkViewport Viewport        = {};
        Viewport.y                 = static_cast<float>(SCDesc.Height);
        Viewport.width             = static_cast<float>(SCDesc.Width);
        Viewport.height            = -static_cast<float>(SCDesc.Height);
        Viewport.maxDepth          = 1;
        ViewPortStateCI.pViewports = &Viewport;

        ViewPortStateCI.scissorCount = 1;
        VkRect2D ScissorRect         = {};
        ScissorRect.extent.width     = SCDesc.Width;
        ScissorRect.extent.height    = SCDesc.Height;
        ViewPortStateCI.pScissors    = &ScissorRect;
        PipelineCI.pViewportState    = &ViewPortStateCI;

        VkPipelineRasterizationStateCreateInfo RasterizerStateCI = {};
        RasterizerStateCI.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        RasterizerStateCI.polygonMode                            = VK_POLYGON_MODE_FILL;
        RasterizerStateCI.cullMode                               = VK_CULL_MODE_NONE;
        RasterizerStateCI.lineWidth                              = 1;
        PipelineCI.pRasterizationState                           = &RasterizerStateCI;

        // Multisample state (24)
        VkPipelineMultisampleStateCreateInfo MSStateCI = {};

        MSStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        MSStateCI.pNext = nullptr;
        MSStateCI.flags = 0; // reserved for future use
        // If subpass uses color and/or depth/stencil attachments, then the rasterizationSamples member of
        // pMultisampleState must be the same as the sample count for those subpass attachments
        MSStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        MSStateCI.sampleShadingEnable  = VK_FALSE;
        MSStateCI.minSampleShading     = 0;               // a minimum fraction of sample shading if sampleShadingEnable is set to VK_TRUE.
        uint32_t SampleMask[]          = {0xFFFFFFFF, 0}; // Vulkan spec allows up to 64 samples
        MSStateCI.pSampleMask          = SampleMask;      // an array of static coverage information that is ANDed with
                                                          // the coverage information generated during rasterization (25.3)
        MSStateCI.alphaToCoverageEnable = VK_FALSE;       // whether a temporary coverage value is generated based on
                                                          // the alpha component of the fragment's first color output
        MSStateCI.alphaToOneEnable   = VK_FALSE;          // whether the alpha component of the fragment's first color output is replaced with one
        PipelineCI.pMultisampleState = &MSStateCI;

        VkPipelineDepthStencilStateCreateInfo DepthStencilStateCI = {};
        DepthStencilStateCI.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        PipelineCI.pDepthStencilState                             = &DepthStencilStateCI;

        VkPipelineColorBlendStateCreateInfo BlendStateCI = {};
        VkPipelineColorBlendAttachmentState Attachment   = {};
        Attachment.colorWriteMask                        = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        BlendStateCI.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        BlendStateCI.pAttachments    = &Attachment;
        BlendStateCI.attachmentCount = 1;
        PipelineCI.pColorBlendState  = &BlendStateCI;

        VkPipelineDynamicStateCreateInfo DynamicStateCI = {};
        DynamicStateCI.sType                            = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        PipelineCI.pDynamicState                        = &DynamicStateCI;

        PipelineCI.renderPass         = m_vkRenderPass;
        PipelineCI.subpass            = 0;
        PipelineCI.basePipelineHandle = VK_NULL_HANDLE;
        PipelineCI.basePipelineIndex  = 0;

        res = vkCreateGraphicsPipelines(m_vkDevice, VK_NULL_HANDLE, 1, &PipelineCI, nullptr, &m_vkPipeline);
        VERIFY_EXPR(res == VK_SUCCESS);
        VERIFY_EXPR(m_vkPipeline != VK_NULL_HANDLE);

        m_PushConstantStages = PushConstantStages;

        CreateFramebuffer();
    }

    void CreateRenderPass()
    {
        VkFormat ColorFormat = TexFormatToVkFormat(m_pSwapChain->GetCurrentBackBufferRTV()->GetDesc().Format);
        VkFormat DepthFormat = TexFormatToVkFormat(m_pSwapChain->GetDepthBufferDSV()->GetDesc().Format);

        std::array<VkAttachmentDescription, MAX_RENDER_TARGETS + 1> Attachments;
        std::array<VkAttachmentReference, MAX_RENDER_TARGETS + 1>   AttachmentReferences;

        VkSubpassDescription Subpass;

        VkRenderPassCreateInfo RenderPassCI =
            TestingEnvironmentVk::GetRenderPassCreateInfo(1, &ColorFormat, DepthFormat, 1,
                                                          VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                          Attachments, AttachmentReferences, Subpass);
        VkResult res = vkCreateRenderPass(m_vkDevice, &RenderPassCI, nullptr, &m_vkRenderPass);
        VERIFY_EXPR(res >= 0);
        (void)res;
    }

    void CreateFramebuffer()
    {
        // Use Diligent Engine managed images (different from TestingSwapChainVk's internal images).
        // The test compares rendering to Diligent Engine images against TestingSwapChainVk's internal images.
        m_vkRenderTargetImage = (VkImage)m_pSwapChain->GetCurrentBackBufferRTV()->GetTexture()->GetNativeHandle();
        m_vkDepthBufferImage  = (VkImage)m_pSwapChain->GetDepthBufferDSV()->GetTexture()->GetNativeHandle();

        VkFormat ColorFormat = TexFormatToVkFormat(m_pSwapChain->GetCurrentBackBufferRTV()->GetDesc().Format);
        VkFormat DepthFormat = TexFormatToVkFormat(m_pSwapChain->GetDepthBufferDSV()->GetDesc().Format);

        {
            VkImageViewCreateInfo ImageViewCI = {};

            ImageViewCI.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ImageViewCI.pNext        = nullptr;
            ImageViewCI.flags        = 0; // reserved for future use.
            ImageViewCI.image        = m_vkRenderTargetImage;
            ImageViewCI.format       = ColorFormat;
            ImageViewCI.viewType     = VK_IMAGE_VIEW_TYPE_2D;
            ImageViewCI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewCI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewCI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewCI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            ImageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ImageViewCI.subresourceRange.levelCount = 1;
            ImageViewCI.subresourceRange.layerCount = 1;

            VkResult res = vkCreateImageView(m_vkDevice, &ImageViewCI, nullptr, &m_vkRenderTargetView);
            VERIFY_EXPR(res >= 0);

            ImageViewCI.image                       = m_vkDepthBufferImage;
            ImageViewCI.format                      = DepthFormat;
            ImageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            res = vkCreateImageView(m_vkDevice, &ImageViewCI, nullptr, &m_vkDepthBufferView);
            VERIFY_EXPR(res >= 0);
        }

        {
            VkFramebufferCreateInfo FramebufferCI = {};

            FramebufferCI.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            FramebufferCI.pNext           = nullptr;
            FramebufferCI.flags           = 0; // reserved for future use
            FramebufferCI.renderPass      = m_vkRenderPass;
            FramebufferCI.attachmentCount = 2;
            VkImageView Attachments[2]    = {m_vkDepthBufferView, m_vkRenderTargetView};
            FramebufferCI.pAttachments    = Attachments;
            FramebufferCI.width           = m_pSwapChain->GetDesc().Width;
            FramebufferCI.height          = m_pSwapChain->GetDesc().Height;
            FramebufferCI.layers          = 1;

            VkResult res = vkCreateFramebuffer(m_vkDevice, &FramebufferCI, nullptr, &m_vkFramebuffer);
            VERIFY_EXPR(res >= 0);
            (void)res;
        }
    }

    void BeginRenderPass(VkCommandBuffer vkCmdBuffer)
    {
        // Manually transition Diligent Engine managed images to the required layouts.
        // We cannot use TestingSwapChainVk::TransitionRenderTarget/TransitionDepthBuffer
        // because they operate on TestingSwapChainVk's internal images, not the Diligent Engine images.
        VkImageMemoryBarrier ImageBarriers[2] = {};

        // Render target barrier: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
        ImageBarriers[0].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        ImageBarriers[0].srcAccessMask                   = 0;
        ImageBarriers[0].dstAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        ImageBarriers[0].oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        ImageBarriers[0].newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ImageBarriers[0].srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        ImageBarriers[0].dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        ImageBarriers[0].image                           = m_vkRenderTargetImage;
        ImageBarriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageBarriers[0].subresourceRange.baseMipLevel   = 0;
        ImageBarriers[0].subresourceRange.levelCount     = 1;
        ImageBarriers[0].subresourceRange.baseArrayLayer = 0;
        ImageBarriers[0].subresourceRange.layerCount     = 1;

        // Depth buffer barrier: UNDEFINED -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        ImageBarriers[1].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        ImageBarriers[1].srcAccessMask                   = 0;
        ImageBarriers[1].dstAccessMask                   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        ImageBarriers[1].oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        ImageBarriers[1].newLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        ImageBarriers[1].srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        ImageBarriers[1].dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        ImageBarriers[1].image                           = m_vkDepthBufferImage;
        ImageBarriers[1].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        ImageBarriers[1].subresourceRange.baseMipLevel   = 0;
        ImageBarriers[1].subresourceRange.levelCount     = 1;
        ImageBarriers[1].subresourceRange.baseArrayLayer = 0;
        ImageBarriers[1].subresourceRange.layerCount     = 1;

        vkCmdPipelineBarrier(vkCmdBuffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             2, ImageBarriers);

        VkRenderPassBeginInfo BeginInfo = {};

        BeginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        BeginInfo.renderPass        = m_vkRenderPass;
        BeginInfo.framebuffer       = m_vkFramebuffer;
        BeginInfo.renderArea.extent = VkExtent2D{m_pSwapChain->GetDesc().Width, m_pSwapChain->GetDesc().Height};

        VkClearValue ClearValues[2] = {};

        ClearValues[0].depthStencil.depth = 1;
        ClearValues[1].color.float32[0]   = 0;
        ClearValues[1].color.float32[1]   = 0;
        ClearValues[1].color.float32[2]   = 0;
        ClearValues[1].color.float32[3]   = 0;

        BeginInfo.clearValueCount = 2;
        BeginInfo.pClearValues    = ClearValues;

        vkCmdBeginRenderPass(vkCmdBuffer, &BeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void Draw(VkCommandBuffer vkCmdBuffer, const void* pPushConstantData, uint32_t PushConstantSize)
    {
        vkCmdBindPipeline(vkCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);
        vkCmdPushConstants(vkCmdBuffer, m_vkLayout, m_PushConstantStages, 0, PushConstantSize, pPushConstantData);
        vkCmdDraw(vkCmdBuffer, 6, 1, 0, 0);
    }

    void EndRenderPass(VkCommandBuffer vkCmdBuffer)
    {
        vkCmdEndRenderPass(vkCmdBuffer);
    }

    ~PatchedPushConstantsRenderer()
    {
        vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
        vkDestroyPipelineLayout(m_vkDevice, m_vkLayout, nullptr);
        vkDestroyShaderModule(m_vkDevice, m_vkVSModule, nullptr);
        vkDestroyShaderModule(m_vkDevice, m_vkFSModule, nullptr);
        vkDestroyRenderPass(m_vkDevice, m_vkRenderPass, nullptr);
        vkDestroyFramebuffer(m_vkDevice, m_vkFramebuffer, nullptr);
        vkDestroyImageView(m_vkDevice, m_vkDepthBufferView, nullptr);
        vkDestroyImageView(m_vkDevice, m_vkRenderTargetView, nullptr);
    }

private:
    TestingSwapChainVk* m_pSwapChain          = nullptr;
    VkDevice            m_vkDevice            = VK_NULL_HANDLE;
    VkShaderModule      m_vkVSModule          = VK_NULL_HANDLE;
    VkShaderModule      m_vkFSModule          = VK_NULL_HANDLE;
    VkPipeline          m_vkPipeline          = VK_NULL_HANDLE;
    VkPipelineLayout    m_vkLayout            = VK_NULL_HANDLE;
    VkRenderPass        m_vkRenderPass        = VK_NULL_HANDLE;
    VkFramebuffer       m_vkFramebuffer       = VK_NULL_HANDLE;
    VkImage             m_vkRenderTargetImage = VK_NULL_HANDLE; // Diligent Engine managed render target
    VkImage             m_vkDepthBufferImage  = VK_NULL_HANDLE; // Diligent Engine managed depth buffer
    VkImageView         m_vkRenderTargetView  = VK_NULL_HANDLE;
    VkImageView         m_vkDepthBufferView   = VK_NULL_HANDLE;
    VkShaderStageFlags  m_PushConstantStages  = 0;
};

// Test helper that runs the full test flow
void RunConvertUBOToPushConstantsTest(SHADER_COMPILER Compiler, SHADER_SOURCE_LANGUAGE SourceLanguage, const std::string& BlockName)
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

    auto* pTestingSwapChainVk = ClassPtrCast<TestingSwapChainVk>(pSwapChain);

    // Step 1: Render reference using existing ReferenceTriangleRenderer
    pContext->Flush();
    pContext->InvalidateState();

    const float ClearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
    RenderDrawCommandReferenceVk(pSwapChain, ClearColor);

    // Take snapshot of reference image
    pTestingSwapChain->TakeSnapshot();

    // Step 2: Compile shaders to SPIR-V
    std::vector<uint32_t> VS_SPIRV, FS_SPIRV;

    if (SourceLanguage == SHADER_SOURCE_LANGUAGE_HLSL)
    {
        CompileSPIRV(HLSL_ProceduralTriangleVS, "HLSL_ProceduralTriangleVS", Compiler, SHADER_TYPE_VERTEX, SHADER_SOURCE_LANGUAGE_HLSL, VS_SPIRV);
        CompileSPIRV(HLSL_FragmentShaderWithCB, "HLSL_FragmentShaderWithCB", Compiler, SHADER_TYPE_PIXEL, SHADER_SOURCE_LANGUAGE_HLSL, FS_SPIRV);
    }
    else
    {
        CompileSPIRV(GLSL_ProceduralTriangleVS, "GLSL_ProceduralTriangleVS", Compiler, SHADER_TYPE_VERTEX, SHADER_SOURCE_LANGUAGE_GLSL, VS_SPIRV);
        CompileSPIRV(GLSL_FragmentShaderWithUBO, "GLSL_FragmentShaderWithUBO", Compiler, SHADER_TYPE_PIXEL, SHADER_SOURCE_LANGUAGE_GLSL, FS_SPIRV);
    }

    ASSERT_FALSE(VS_SPIRV.empty()) << "Failed to compile vertex shader";
    ASSERT_FALSE(FS_SPIRV.empty()) << "Failed to compile fragment shader";

    // Step 3: Patch fragment shader to use push constants
    std::vector<uint32_t> FS_SPIRV_Patched = ConvertUBOToPushConstants(FS_SPIRV, BlockName);
    ASSERT_FALSE(FS_SPIRV_Patched.empty()) << "Failed to patch UBO to push constants with BlockName: " << BlockName;

    if (SourceLanguage == SHADER_SOURCE_LANGUAGE_HLSL)
    {
        // SPIR-V bytecode generated from HLSL must be legalized to
        // turn it into a valid vulkan SPIR-V shader.
        SPIRV_OPTIMIZATION_FLAGS OptimizationFlags = SPIRV_OPTIMIZATION_FLAG_LEGALIZATION | SPIRV_OPTIMIZATION_FLAG_STRIP_REFLECTION;
        VS_SPIRV                                   = OptimizeSPIRV(VS_SPIRV, SPV_ENV_MAX, OptimizationFlags);
        FS_SPIRV_Patched                           = OptimizeSPIRV(FS_SPIRV_Patched, SPV_ENV_MAX, OptimizationFlags);
    }

    // Step 4: Render with push constants
    {
        PatchedPushConstantsRenderer Renderer{
            pTestingSwapChainVk,
            VS_SPIRV,
            FS_SPIRV_Patched,
            sizeof(PushConstantData),
            VK_SHADER_STAGE_FRAGMENT_BIT};

        VkCommandBuffer vkCmdBuffer = pEnv->AllocateCommandBuffer();

        Renderer.BeginRenderPass(vkCmdBuffer);

        // Set push constant data - Factor = (1,1,1,1) to make output identical to reference
        PushConstantData PushData = {};
        PushData.Factor[0]        = 1.0f;
        PushData.Factor[1]        = 1.0f;
        PushData.Factor[2]        = 1.0f;
        PushData.Factor[3]        = 1.0f;

        Renderer.Draw(vkCmdBuffer, &PushData, sizeof(PushData));

        Renderer.EndRenderPass(vkCmdBuffer);

        vkEndCommandBuffer(vkCmdBuffer);

        pEnv->SubmitCommandBuffer(vkCmdBuffer, true);
    }

    // Sync Diligent Engine's internal layout tracking with the actual image layouts.
    // After our native Vulkan rendering, the images are in COLOR_ATTACHMENT_OPTIMAL
    // and DEPTH_STENCIL_ATTACHMENT_OPTIMAL layouts, but Diligent Engine doesn't know this.
    // We need to update the tracked layouts so that CompareWithSnapshot() can correctly
    // transition the images for the copy operation.
    {
        RefCntAutoPtr<ITextureVk> pRenderTargetVk{pTestingSwapChainVk->GetCurrentBackBufferRTV()->GetTexture(), IID_TextureVk};
        RefCntAutoPtr<ITextureVk> pDepthBufferVk{pTestingSwapChainVk->GetDepthBufferDSV()->GetTexture(), IID_TextureVk};
        if (pRenderTargetVk)
            pRenderTargetVk->SetLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        if (pDepthBufferVk)
            pDepthBufferVk->SetLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    // Step 5: Comparison native draw image with ref snapshot
    pTestingSwapChainVk->Present();
}

} // namespace

// Test patching UBO using struct type name "CB1"
TEST_F(VkConvertUBOToPushConstantsTest, PatchByStructTypeName_GLSLang_GLSL)
{
    RunConvertUBOToPushConstantsTest(SHADER_COMPILER_GLSLANG, SHADER_SOURCE_LANGUAGE_GLSL, "CB1");
}

// Test patching UBO using variable instance name "cb"
TEST_F(VkConvertUBOToPushConstantsTest, PatchByVariableName_GLSLang_GLSL)
{
    RunConvertUBOToPushConstantsTest(SHADER_COMPILER_GLSLANG, SHADER_SOURCE_LANGUAGE_GLSL, "cb");
}

// Test patching CB using cbuffer block name "CB1" with DXC compiler
// Note: In HLSL, cbuffer name and struct name may be the same or different.
// DXC typically generates both OpName for the struct type and the variable.

TEST_F(VkConvertUBOToPushConstantsTest, PatchByStructTypeName_GLSLang_HLSL)
{
    RunConvertUBOToPushConstantsTest(SHADER_COMPILER_GLSLANG, SHADER_SOURCE_LANGUAGE_HLSL, "CB1");
}

TEST_F(VkConvertUBOToPushConstantsTest, PatchByStructTypeName_DXC_HLSL)
{
    RunConvertUBOToPushConstantsTest(SHADER_COMPILER_DXC, SHADER_SOURCE_LANGUAGE_HLSL, "CB1");
}


} // namespace Testing

} // namespace Diligent
