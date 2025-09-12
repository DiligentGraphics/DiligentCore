/*
 *  Copyright 2025 Diligent Graphics LLC
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

#include "ObjectBase.hpp"
#include "MemoryFileStream.hpp"
#include "ProxyDataBlob.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

constexpr char g_PS[] = R"(
Texture2D    g_Texture;
SamplerState g_Texture_sampler;

float4 main(in float2 UV : TEXCOORD) : SV_Target
{
    return g_Texture.Sample(g_Texture_sampler, UV);
}
)";

TEST(ShaderCreationTest, FromSource)
{
    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();

    ShaderCreateInfo ShaderCI;
    ShaderCI.Desc           = {"ShaderCreationTest.FromSource", SHADER_TYPE_PIXEL, true};
    ShaderCI.Source         = g_PS;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    {
        RefCntAutoPtr<IShader> pShader;
        pDevice->CreateShader(ShaderCI, &pShader);
        EXPECT_NE(pShader, nullptr);
    }

    {
        std::string Source = g_PS;
        Source += "invalid syntax";

        ShaderCI.Source       = Source.c_str();
        ShaderCI.SourceLength = sizeof(g_PS) - 1;

        RefCntAutoPtr<IShader> pShader;
        pDevice->CreateShader(ShaderCI, &pShader);
        EXPECT_NE(pShader, nullptr);
    }
}

static std::vector<uint8_t> CompilePS(IRenderDevice* pDevice)
{
    ShaderCreateInfo ShaderCI;
    ShaderCI.Desc           = {"ShaderCreationTest.FromSource", SHADER_TYPE_PIXEL, true};
    ShaderCI.Source         = g_PS;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    std::vector<uint8_t>   Bytecode;
    RefCntAutoPtr<IShader> pShader;
    pDevice->CreateShader(ShaderCI, &pShader);
    if (pShader)
    {
        const void* pBytecode = nullptr;
        Uint64      Size      = 0;
        pShader->GetBytecode(&pBytecode, Size);

        Bytecode.resize(static_cast<size_t>(Size));
        memcpy(Bytecode.data(), pBytecode, static_cast<size_t>(Size));
    }

    return Bytecode;
}

TEST(ShaderCreationTest, FromBytecode)
{
    GPUTestingEnvironment*  pEnv       = GPUTestingEnvironment::GetInstance();
    IRenderDevice*          pDevice    = pEnv->GetDevice();
    const RenderDeviceInfo& DeviceInfo = pDevice->GetDeviceInfo();

    if (!(DeviceInfo.IsD3DDevice() || DeviceInfo.IsVulkanDevice()))
    {
        GTEST_SKIP() << "Creating shader from bytecode is not supported on this device type";
    }

    std::vector<uint8_t> Bytecode = CompilePS(pDevice);
    ASSERT_FALSE(Bytecode.empty()) << "Failed to compile shader";

    {
        ShaderCreateInfo ShaderCI;
        ShaderCI.Desc         = {"ShaderCreationTest.FromBytecode - Src", SHADER_TYPE_PIXEL, true};
        ShaderCI.ByteCode     = Bytecode.data();
        ShaderCI.ByteCodeSize = Bytecode.size();

        RefCntAutoPtr<IShader> pShader;
        pDevice->CreateShader(ShaderCI, &pShader);
        EXPECT_NE(pShader, nullptr);
    }

    {
        class TestShaderSourceFactoryImpl : public ObjectBase<IShaderSourceInputStreamFactory>
        {
        public:
            static RefCntAutoPtr<IShaderSourceInputStreamFactory> Create(const std::vector<uint8_t>& Bytecode)
            {
                return RefCntAutoPtr<IShaderSourceInputStreamFactory>{MakeNewRCObj<TestShaderSourceFactoryImpl>()(Bytecode)};
            }

            TestShaderSourceFactoryImpl(IReferenceCounters* pRefCounters, const std::vector<uint8_t>& Bytecode) :
                ObjectBase<IShaderSourceInputStreamFactory>(pRefCounters),
                m_Bytecode{Bytecode}
            {
            }

            virtual void DILIGENT_CALL_TYPE CreateInputStream(const Char* Name, IFileStream** ppStream) override final
            {
                CreateInputStream2(Name, CREATE_SHADER_SOURCE_INPUT_STREAM_FLAG_NONE, ppStream);
            }

            virtual void DILIGENT_CALL_TYPE CreateInputStream2(const Char*                             Name,
                                                               CREATE_SHADER_SOURCE_INPUT_STREAM_FLAGS Flags,
                                                               IFileStream**                           ppStream) override final
            {
                if (strcmp(Name, "ps.bc") != 0)
                    return;

                RefCntAutoPtr<IDataBlob>        pDataBlob = ProxyDataBlob::Create(m_Bytecode.data(), m_Bytecode.size());
                RefCntAutoPtr<MemoryFileStream> pMemStream{MakeNewRCObj<MemoryFileStream>()(pDataBlob)};

                pMemStream->QueryInterface(IID_FileStream, reinterpret_cast<IObject**>(ppStream));
            }

        private:
            const std::vector<uint8_t>& m_Bytecode;
        };

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory = TestShaderSourceFactoryImpl::Create(Bytecode);

        ShaderCreateInfo ShaderCI;
        ShaderCI.Desc                       = {"ShaderCreationTest.FromBytecode", SHADER_TYPE_PIXEL, true};
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        ShaderCI.FilePath                   = "ps.bc";
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_BYTECODE;

        RefCntAutoPtr<IShader> pShader;
        pDevice->CreateShader(ShaderCI, &pShader);
        EXPECT_NE(pShader, nullptr);
    }
}

} // namespace
