/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "gtest/gtest.h"

#include <thread>
#include <random>
#include <vector>

#include "ShaderMacroHelper.hpp"
#include "Timer.hpp"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

RefCntAutoPtr<IShader> CreateShader(const char*          Path,
                                    const char*          Name,
                                    SHADER_COMPILE_FLAGS CompileFlags)
{
    ShaderCreateInfo ShaderCI;

    auto* pEnv    = GPUTestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    ShaderCI.FilePath       = Path;
    ShaderCI.EntryPoint     = "main";
    ShaderCI.Desc           = {Name, SHADER_TYPE_PIXEL, true};
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);
    ShaderCI.CompileFlags   = CompileFlags;

    std::random_device Rand;

    ShaderMacroHelper Macros;
    Macros.Add("RANDOM", static_cast<int>(Rand()));
    ShaderCI.Macros = Macros;

    RefCntAutoPtr<IShader> pShader;
    pDevice->CreateShader(ShaderCI, &pShader);
    return pShader;
}

TEST(Shader, AsyncCompilation)
{
    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    const auto& DeviceInfo = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo();
    if (!DeviceInfo.Features.AsyncShaderCompilation)
    {
        GTEST_SKIP() << "Async shader compilation is not supported by this device";
    }

    std::vector<RefCntAutoPtr<IShader>> Shaders;
    for (Uint32 i = 0; i < 10; ++i)
    {
        auto pShader = CreateShader("AsyncShaderCompilationTest.psh", "Async compilation test", SHADER_COMPILE_FLAG_ASYNCHRONOUS);
        ASSERT_NE(pShader, nullptr);
        Shaders.emplace_back(std::move(pShader));
    }

    Timer  T;
    auto   StartTime = T.GetElapsedTime();
    Uint32 Iter      = 0;
    while (true)
    {
        Uint32 NumShadersReady = 0;
        for (auto& pShader : Shaders)
        {
            if (pShader->GetStatus() == SHADER_STATUS_READY)
                ++NumShadersReady;
        }
        if (NumShadersReady == Shaders.size())
            break;
        std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        ++Iter;
    }
    LOG_INFO_MESSAGE(Shaders.size(), " shaders were compiled after ", Iter, " iterations (", (T.GetElapsedTime() - StartTime) * 1000, " ms)");
}

} // namespace
