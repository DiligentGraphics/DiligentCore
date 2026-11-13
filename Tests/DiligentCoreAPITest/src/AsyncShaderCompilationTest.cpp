/*
 *  Copyright 2024-2026 Diligent Graphics LLC
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

#if PLATFORM_WEB && WEBGPU_SUPPORTED
#    include <emscripten.h>
#endif

#include "ShaderMacroHelper.hpp"
#include "Timer.hpp"
#include "GraphicsTypesX.hpp"

#if ARCHIVER_SUPPORTED
#    include "GraphicsAccessories.hpp"
#    include "SerializationDevice.h"
#    include "ThreadPool.hpp"
#endif

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

RefCntAutoPtr<IShader> CreateShader(const char*          Path,
                                    const char*          Name,
                                    SHADER_TYPE          ShaderType,
                                    SHADER_COMPILE_FLAGS CompileFlags,
                                    bool                 SimplifiedShader = false)
{
    // Allocate shader CI on the heap to check that all data is copied correctly
    std::unique_ptr<ShaderCreateInfo> pShaderCI = std::make_unique<ShaderCreateInfo>();
    ShaderCreateInfo&                 ShaderCI  = *pShaderCI;

    GPUTestingEnvironment* pEnv    = GPUTestingEnvironment::GetInstance();
    IRenderDevice*         pDevice = pEnv->GetDevice();

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    ShaderCI.FilePath       = Path;
    ShaderCI.EntryPoint     = "main";
    ShaderCI.Desc           = {Name, ShaderType, true};
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);
    ShaderCI.CompileFlags   = CompileFlags;

    std::random_device Rand;

    ShaderMacroHelper Macros;
    Macros.Add("RANDOM", static_cast<int>(Rand()));
#ifdef DILIGENT_DEBUG
    if (pDevice->GetDeviceInfo().IsVulkanDevice() || pDevice->GetDeviceInfo().IsWebGPUDevice())
    {
        // In debug mode in Vulkan and WebGPU it takes a lot of time to compile full shader
        SimplifiedShader = true;
    }
#endif
    if (SimplifiedShader)
    {
        Macros.Add("SIMPLIFIED", 1);
    }
    ShaderCI.Macros = Macros;

    RefCntAutoPtr<IShader> pShader;
    pDevice->CreateShader(ShaderCI, &pShader);
    return pShader;
}

TEST(Shader, AsyncCompilation)
{
    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    const RenderDeviceInfo& DeviceInfo = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo();
    if (!DeviceInfo.Features.AsyncShaderCompilation)
    {
        GTEST_SKIP() << "Async shader compilation is not supported by this device";
    }

    std::vector<RefCntAutoPtr<IShader>> Shaders;
    for (Uint32 i = 0; i < 10; ++i)
    {
        RefCntAutoPtr<IShader> pShader = CreateShader("AsyncShaderCompilationTest.psh", "Async compilation test", SHADER_TYPE_PIXEL, SHADER_COMPILE_FLAG_ASYNCHRONOUS);
        ASSERT_NE(pShader, nullptr);
        Shaders.emplace_back(std::move(pShader));
    }

    Timer  T;
    double StartTime = T.GetElapsedTime();
    Uint32 Iter      = 0;
    while (true)
    {
        Uint32 NumShadersReady = 0;
        for (RefCntAutoPtr<IShader>& pShader : Shaders)
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

TEST(Shader, ReleaseWhileCompiling)
{
    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    const RenderDeviceInfo& DeviceInfo = GPUTestingEnvironment::GetInstance()->GetDevice()->GetDeviceInfo();
    if (!DeviceInfo.Features.AsyncShaderCompilation)
    {
        GTEST_SKIP() << "Async shader compilation is not supported by this device";
    }

    RefCntAutoPtr<IShader> pShader = CreateShader("AsyncShaderCompilationTest.psh", "Async pipeline test PS", SHADER_TYPE_PIXEL, SHADER_COMPILE_FLAG_ASYNCHRONOUS);
    ASSERT_NE(pShader, nullptr);

    // Release the shader while it is still compiling
    pShader.Release();
}

void TestAsyncPipeline(SHADER_COMPILE_FLAGS ShaderFlags, PSO_CREATE_FLAGS PSOFlags)
{
    GPUTestingEnvironment::ScopedReset EnvironmentAutoReset;

    IRenderDevice*          pDevice    = GPUTestingEnvironment::GetInstance()->GetDevice();
    const RenderDeviceInfo& DeviceInfo = pDevice->GetDeviceInfo();
    if (!DeviceInfo.Features.AsyncShaderCompilation)
    {
        GTEST_SKIP() << "Async shader compilation is not supported by this device";
    }

    std::vector<RefCntAutoPtr<IPipelineState>> pPSOs;
    {
        constexpr bool SimplifiedShader = true;

        RefCntAutoPtr<IShader> pVS = CreateShader("AsyncShaderCompilationTest.vsh", "Async pipeline test VS", SHADER_TYPE_VERTEX, ShaderFlags, SimplifiedShader);
        ASSERT_NE(pVS, nullptr);

        for (size_t i = 0; i < 16; ++i)
        {
            RefCntAutoPtr<IShader> pPS = CreateShader("AsyncShaderCompilationTest.psh", "Async pipeline test PS", SHADER_TYPE_PIXEL, ShaderFlags, SimplifiedShader);
            ASSERT_NE(pPS, nullptr);

            // Allocate pipeline CI on the heap to check that all data is copied correctly
            std::unique_ptr<GraphicsPipelineStateCreateInfoX> pPSOCreateInfo = std::make_unique<GraphicsPipelineStateCreateInfoX>();
            GraphicsPipelineStateCreateInfoX&                 PSOCreateInfo  = *pPSOCreateInfo;

            InputLayoutDescX InputLayout;
            InputLayout.Add(0u, 0u, 3u, VT_FLOAT32, False);

            PipelineResourceLayoutDescX ResourceLayout;
            ResourceLayout.AddVariable(SHADER_TYPE_PIXEL, "g_Tex2D", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);

            PSOCreateInfo
                .SetName("Async pipeline test PSO")
                .AddShader(pVS)
                .AddShader(pPS)
                .AddRenderTarget(TEX_FORMAT_RGBA8_UNORM)
                .SetDepthFormat(TEX_FORMAT_D32_FLOAT)
                .SetInputLayout(InputLayout)
                .SetResourceLayout(ResourceLayout)
                .SetFlags(PSOFlags);

            // Create multiple pipelines that use the same shaders.
            // In particular this reproduces the problem with the non-thread-safe ID3DBlob in D3D12.
            for (size_t j = 0; j < 4; ++j)
            {
                RefCntAutoPtr<IPipelineState> pPSO;
                pDevice->CreatePipelineState(PSOCreateInfo, &pPSO);
                ASSERT_NE(pPSO, nullptr);
                pPSOs.emplace_back(std::move(pPSO));
            }
        }
    }

    Timer  T;
    double StartTime = T.GetElapsedTime();
    Uint32 Iter      = 0;
    while (true)
    {
        Uint32 NumPSOsReady = 0;
        for (RefCntAutoPtr<IPipelineState>& pPSO : pPSOs)
        {
            const PIPELINE_STATE_STATUS Status = pPSO->GetStatus();
            ASSERT_NE(Status, PIPELINE_STATE_STATUS_FAILED);
            if (Status == PIPELINE_STATE_STATUS_READY)
                ++NumPSOsReady;
        }
        if (NumPSOsReady == pPSOs.size())
            break;
#if PLATFORM_WEB && WEBGPU_SUPPORTED
        // WebGPU delivers pipeline creation callbacks on the browser event loop.
        emscripten_sleep(10);
#else
        std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
#endif
        ++Iter;
    }
    LOG_INFO_MESSAGE(pPSOs.size(), " PSOs were compiled after ", Iter, " iterations (", (T.GetElapsedTime() - StartTime) * 1000, " ms)");
}

TEST(Shader, AsyncPipeline_SyncShaders)
{
    TestAsyncPipeline(SHADER_COMPILE_FLAG_NONE, PSO_CREATE_FLAG_ASYNCHRONOUS);
}

TEST(Shader, SyncPipeline_AsyncShaders)
{
    TestAsyncPipeline(SHADER_COMPILE_FLAG_ASYNCHRONOUS, PSO_CREATE_FLAG_NONE);
}

TEST(Shader, AsyncPipeline_AsyncShaders)
{
    TestAsyncPipeline(SHADER_COMPILE_FLAG_ASYNCHRONOUS, PSO_CREATE_FLAG_ASYNCHRONOUS);
}

#if ARCHIVER_SUPPORTED
// Record the task submitted by the archiver so the test can remove it before it runs.
class ManualCompilationPool final : public ObjectBase<IThreadPool>
{
public:
    using TBase = ObjectBase<IThreadPool>;
    explicit ManualCompilationPool(IReferenceCounters* pRefCounters) :
        TBase{pRefCounters}
    {}

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_ThreadPool, TBase)

    bool DILIGENT_CALL_TYPE EnqueueTask(IAsyncTask* pTask, IAsyncTask** ppPrerequisites, Uint32 NumPrerequisites) override
    {
        pQueuedTask = pTask;
        return m_pPool->EnqueueTask(pTask, ppPrerequisites, NumPrerequisites);
    }

    bool DILIGENT_CALL_TYPE   RemoveTask(IAsyncTask* pTask) override { return m_pPool->RemoveTask(pTask); }
    bool DILIGENT_CALL_TYPE   ReprioritizeTask(IAsyncTask* pTask) override { return m_pPool->ReprioritizeTask(pTask); }
    void DILIGENT_CALL_TYPE   ReprioritizeAllTasks() override { m_pPool->ReprioritizeAllTasks(); }
    void DILIGENT_CALL_TYPE   WaitForAllTasks() override { m_pPool->WaitForAllTasks(); }
    Uint32 DILIGENT_CALL_TYPE GetQueueSize() override { return m_pPool->GetQueueSize(); }
    Uint32 DILIGENT_CALL_TYPE GetRunningTaskCount() const override { return m_pPool->GetRunningTaskCount(); }
    void DILIGENT_CALL_TYPE   StopThreads() override { m_pPool->StopThreads(); }
    bool DILIGENT_CALL_TYPE   ProcessTask(Uint32 ThreadId, bool WaitForTask) override { return m_pPool->ProcessTask(ThreadId, WaitForTask); }

    RefCntAutoPtr<IAsyncTask> pQueuedTask;

private:
    const RefCntAutoPtr<IThreadPool> m_pPool = CreateThreadPool(ThreadPoolCreateInfo{0});
};

TEST(Shader, AsyncPipeline_CancelledSerializedPipeline)
{
    GPUTestingEnvironment* pEnv     = GPUTestingEnvironment::GetInstance();
    IArchiverFactory*      pFactory = pEnv->GetArchiverFactory();
    if (pFactory == nullptr)
        GTEST_SKIP() << "Archiver library is not loaded";

    RefCntAutoPtr<ManualCompilationPool> pPool{MakeNewRCObj<ManualCompilationPool>()()};
    SerializationDeviceCreateInfo        DeviceCI;
    DeviceCI.pAsyncShaderCompilationThreadPool = pPool;
    RefCntAutoPtr<ISerializationDevice> pDevice;
    pFactory->CreateSerializationDevice(DeviceCI, &pDevice);
    ASSERT_NE(pDevice, nullptr);

    const RenderDeviceInfo&         DeviceInfo = pEnv->GetDevice()->GetDeviceInfo();
    const ARCHIVE_DEVICE_DATA_FLAGS Target     = RenderDeviceTypeToArchiveDataFlag(DeviceInfo.Type);
    if ((pDevice->GetSerializationDeviceInfo().SupportedArchiveTargets & Target) == 0)
        GTEST_SKIP() << "The serialization device does not support the active backend";

    ShaderCreateInfo ShaderCI;
    ShaderCI.Desc           = {"Cancellation test CS", SHADER_TYPE_COMPUTE};
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Source         = "[numthreads(1, 1, 1)] void main() {}";
    if (DeviceInfo.IsWebGPUDevice())
    {
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_WGSL;
        ShaderCI.Source         = "@compute @workgroup_size(1) fn main() {}";
    }
    else if (DeviceInfo.IsGLDevice())
    {
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL;
        ShaderCI.Source         = "layout(local_size_x = 1) in; void main() {}";
    }
    ShaderCI.ShaderCompiler = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);

    RefCntAutoPtr<IShader> pCS;
    pDevice->CreateShader(ShaderCI, ShaderArchiveInfo{Target}, &pCS);
    ASSERT_NE(pCS, nullptr);

    ComputePipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name = "Cancelled serialized pipeline";
    PSOCreateInfo.Flags        = PSO_CREATE_FLAG_ASYNCHRONOUS;
    PSOCreateInfo.pCS          = pCS;
    PipelineStateArchiveInfo ArchiveInfo;
    ArchiveInfo.DeviceFlags = Target;
    for (bool RetainTask : {true, false})
    {
        for (bool Wait : {false, true})
        {
            SCOPED_TRACE(testing::Message() << "RetainTask=" << RetainTask << ", Wait=" << Wait);
            RefCntAutoPtr<IPipelineState> pPSO;
            pDevice->CreateComputePipelineState(PSOCreateInfo, ArchiveInfo, &pPSO);
            RefCntAutoPtr<IAsyncTask> pTask = std::move(pPool->pQueuedTask);
            ASSERT_NE(pPSO, nullptr);
            ASSERT_NE(pTask, nullptr);
            const PIPELINE_STATE_STATUS BeforeRemoval = pPSO->GetStatus(false);
            ASSERT_TRUE(pPool->RemoveTask(pTask));
            ASSERT_EQ(BeforeRemoval, PIPELINE_STATE_STATUS_COMPILING);
            ASSERT_EQ(pTask->GetStatus(), ASYNC_TASK_STATUS_CANCELLED);
            if (!RetainTask)
            {
                RefCntWeakPtr<IAsyncTask> wpTask{pTask};
                pTask.Release();
                ASSERT_FALSE(wpTask.Lock());
            }

            ASSERT_EQ(pPSO->GetStatus(Wait), PIPELINE_STATE_STATUS_FAILED);
            EXPECT_EQ(pPSO->GetStatus(!Wait), PIPELINE_STATE_STATUS_FAILED);
        }
    }
}
#endif

} // namespace
