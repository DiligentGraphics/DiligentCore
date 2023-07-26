/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

#include "GPUTestingEnvironment.hpp"
#include "FileWrapper.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

static constexpr Uint32 ContentVersion = 1234;

constexpr ARCHIVE_DEVICE_DATA_FLAGS GetDeviceBits()
{
    ARCHIVE_DEVICE_DATA_FLAGS DeviceBits = ARCHIVE_DEVICE_DATA_FLAG_NONE;
#if D3D11_SUPPORTED
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_D3D11;
#endif
#if D3D12_SUPPORTED
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_D3D12;
#endif
#if GL_SUPPORTED
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_GL;
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_GLES;
#endif
#if VULKAN_SUPPORTED
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_VULKAN;
#endif
#if METAL_SUPPORTED
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_METAL_MACOS;
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS;
#endif
    return DeviceBits;
}

TEST(GenerateArchiveDotNetTest, GenerateCubeArchive)
{
    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto* pEnv             = GPUTestingEnvironment::GetInstance();
    auto* pArchiverFactory = pEnv->GetArchiverFactory();
    auto* pSwapChain       = pEnv->GetSwapChain();

    RefCntAutoPtr<ISerializationDevice> pDevice;
    SerializationDeviceCreateInfo       DeviceCI{};
    pArchiverFactory->CreateSerializationDevice(DeviceCI, &pDevice);
    ASSERT_NE(pDevice, nullptr);

    RefCntAutoPtr<IArchiver> pArchiver;
    pArchiverFactory->CreateArchiver(pDevice, &pArchiver);
    ASSERT_NE(pArchiver, nullptr);

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pEnv->GetDevice()->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);

    RefCntAutoPtr<IPipelineState> pGraphicsPSO;
    {
        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCreateInfo ShaderCI{};
            ShaderCI.FilePath                        = "DotNetCube.vsh";
            ShaderCI.pShaderSourceStreamFactory      = pShaderSourceFactory;
            ShaderCI.Desc.Name                       = "Cube vertex shader";
            ShaderCI.Desc.ShaderType                 = SHADER_TYPE_VERTEX;
            ShaderCI.Desc.UseCombinedTextureSamplers = true;
            ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
            pDevice->CreateShader(ShaderCI, {GetDeviceBits()}, &pVS);
            ASSERT_NE(pVS, nullptr);
            ASSERT_TRUE(pArchiver->AddShader(pVS));
        }
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCreateInfo ShaderCI{};
            ShaderCI.FilePath                        = "DotNetCube.psh";
            ShaderCI.pShaderSourceStreamFactory      = pShaderSourceFactory;
            ShaderCI.Desc.Name                       = "Cube pixel shader";
            ShaderCI.Desc.ShaderType                 = SHADER_TYPE_PIXEL;
            ShaderCI.Desc.UseCombinedTextureSamplers = true;
            ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
            pDevice->CreateShader(ShaderCI, {GetDeviceBits()}, &pPS);
            ASSERT_NE(pPS, nullptr);
            ASSERT_TRUE(pArchiver->AddShader(pPS));
        }

        LayoutElement LayoutElements[] = {
            LayoutElement{0, 0, 3, VT_FLOAT32, false},
            LayoutElement{1, 0, 4, VT_FLOAT32, false}};

        InputLayoutDesc InputLayout{};
        InputLayout.LayoutElements = LayoutElements;
        InputLayout.NumElements    = _countof(LayoutElements);

        GraphicsPipelineStateCreateInfo PipelineCI{};
        PipelineCI.PSODesc.Name                                          = "Cube Graphics PSO";
        PipelineCI.pVS                                                   = pVS;
        PipelineCI.pPS                                                   = pPS;
        PipelineCI.GraphicsPipeline.InputLayout                          = InputLayout;
        PipelineCI.GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PipelineCI.GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
        PipelineCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = false;
        PipelineCI.GraphicsPipeline.DepthStencilDesc.DepthEnable         = true;
        PipelineCI.GraphicsPipeline.NumRenderTargets                     = 1;
        PipelineCI.GraphicsPipeline.RTVFormats[0]                        = pSwapChain->GetDesc().ColorBufferFormat;
        PipelineCI.GraphicsPipeline.DSVFormat                            = pSwapChain->GetDesc().DepthBufferFormat;
        pDevice->CreateGraphicsPipelineState(PipelineCI, {PSO_ARCHIVE_FLAG_NONE, GetDeviceBits()}, &pGraphicsPSO);
        ASSERT_NE(pGraphicsPSO, nullptr);
        ASSERT_TRUE(pArchiver->AddPipelineState(pGraphicsPSO));
    }

    RefCntAutoPtr<IDataBlob> pDataBlob;
    pArchiver->SerializeToBlob(ContentVersion, &pDataBlob);
    ASSERT_NE(pDataBlob, nullptr);

    FileWrapper File{"DotNetArchive.bin", EFileAccessMode::Overwrite};
    ASSERT_TRUE(File);
    ASSERT_TRUE(File->Write(pDataBlob->GetDataPtr(), pDataBlob->GetSize()));
}

} // namespace
