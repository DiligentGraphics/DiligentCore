/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#include <algorithm>

#include "TestingEnvironment.hpp"
#include "PlatformDebug.hpp"
#include "TestingSwapChainBase.hpp"

#if D3D11_SUPPORTED
#    include "EngineFactoryD3D11.h"
#endif

#if D3D12_SUPPORTED
#    include "EngineFactoryD3D12.h"
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
#    include "EngineFactoryOpenGL.h"
#endif

#if VULKAN_SUPPORTED
#    include "EngineFactoryVk.h"
#endif

#if METAL_SUPPORTED
#    include "EngineFactoryMtl.h"
#endif

namespace Diligent
{

namespace Testing
{

TestingEnvironment* TestingEnvironment::m_pTheEnvironment = nullptr;
std::atomic_int     TestingEnvironment::m_NumAllowedErrors;

std::vector<std::string> TestingEnvironment::m_ExpectedErrorSubstrings;

void TestingEnvironment::MessageCallback(DEBUG_MESSAGE_SEVERITY Severity,
                                         const Char*            Message,
                                         const char*            Function,
                                         const char*            File,
                                         int                    Line)
{
    if (Severity == DEBUG_MESSAGE_SEVERITY_ERROR || Severity == DEBUG_MESSAGE_SEVERITY_FATAL_ERROR)
    {
        if (m_NumAllowedErrors == 0)
        {
            ADD_FAILURE() << "Unexpected error";
        }
        else
        {
            m_NumAllowedErrors--;
            if (!m_ExpectedErrorSubstrings.empty())
            {
                const auto& ErrorSubstring = m_ExpectedErrorSubstrings.back();
                if (strstr(Message, ErrorSubstring.c_str()) == nullptr)
                {
                    ADD_FAILURE() << "Expected error substring '" << ErrorSubstring << "' was not found in the error message";
                }
                m_ExpectedErrorSubstrings.pop_back();
            }
        }
    }

    PlatformDebug::OutputDebugMessage(Severity, Message, Function, File, Line);
}

void TestingEnvironment::SetErrorAllowance(int NumErrorsToAllow, const char* InfoMessage)
{
    m_NumAllowedErrors = NumErrorsToAllow;
    if (InfoMessage != nullptr)
    {
        std::cout << InfoMessage;
    }
    if (m_NumAllowedErrors == 0)
    {
        m_ExpectedErrorSubstrings.clear();
    }
}

void TestingEnvironment::PushExpectedErrorSubstring(const char* Str, bool ClearStack)
{
    if (ClearStack)
        m_ExpectedErrorSubstrings.clear();
    VERIFY_EXPR(Str != nullptr && Str[0] != '\0');
    m_ExpectedErrorSubstrings.push_back(Str);
}

Uint32 TestingEnvironment::FindAdapater(const std::vector<GraphicsAdapterInfo>& Adapters,
                                        ADAPTER_TYPE                            AdapterType,
                                        Uint32                                  AdapterId)
{
    if (AdapterId != DEFAULT_ADAPTER_ID && AdapterId >= Adapters.size())
    {
        LOG_ERROR_MESSAGE("Adapter ID (", AdapterId, ") is invalid. Only ", Adapters.size(), " adapter(s) found on the system");
        AdapterId = DEFAULT_ADAPTER_ID;
    }

    if (AdapterId == DEFAULT_ADAPTER_ID && AdapterType != ADAPTER_TYPE_UNKNOWN)
    {
        for (Uint32 i = 0; i < Adapters.size(); ++i)
        {
            if (Adapters[i].Type == AdapterType)
            {
                AdapterId     = i;
                m_AdapterType = AdapterType;
                break;
            }
        }
        if (AdapterId == DEFAULT_ADAPTER_ID)
            LOG_WARNING_MESSAGE("Unable to find the requested adapter type. Using default adapter.");
    }

    if (AdapterId != DEFAULT_ADAPTER_ID)
        LOG_INFO_MESSAGE("Using adapter ", AdapterId, ": '", Adapters[AdapterId].Description, "'");

    return AdapterId;
}

TestingEnvironment::TestingEnvironment(const CreateInfo& CI, const SwapChainDesc& SCDesc) :
    m_DeviceType{CI.deviceType}
{
    VERIFY(m_pTheEnvironment == nullptr, "Testing environment object has already been initialized!");
    m_pTheEnvironment = this;

    Uint32 NumDeferredCtx = 0;

    std::vector<IDeviceContext*>            ppContexts;
    std::vector<GraphicsAdapterInfo>        Adapters;
    std::vector<ImmediateContextCreateInfo> ContextCI;

    auto EnumerateAdapters = [&Adapters](IEngineFactory* pFactory, Version MinVersion) //
    {
        Uint32 NumAdapters = 0;
        pFactory->EnumerateAdapters(MinVersion, NumAdapters, 0);
        if (NumAdapters > 0)
        {
            Adapters.resize(NumAdapters);
            pFactory->EnumerateAdapters(MinVersion, NumAdapters, Adapters.data());

            // Validate adapter info
            for (auto& Adapter : Adapters)
            {
                VERIFY_EXPR(Adapter.NumQueues >= 1);
            }
        }
    };

    auto AddContext = [&ContextCI, &Adapters](COMMAND_QUEUE_TYPE Type, const char* Name, Uint32 AdapterId) //
    {
        if (AdapterId >= Adapters.size())
            AdapterId = 0;

        constexpr auto QueueMask = COMMAND_QUEUE_TYPE_PRIMARY_MASK;
        auto*          Queues    = Adapters[AdapterId].Queues;
        for (Uint32 q = 0, Count = Adapters[AdapterId].NumQueues; q < Count; ++q)
        {
            auto& CurQueue = Queues[q];
            if (CurQueue.MaxDeviceContexts == 0)
                continue;

            if ((CurQueue.QueueType & QueueMask) == Type)
            {
                CurQueue.MaxDeviceContexts -= 1;

                ImmediateContextCreateInfo Ctx{};
                Ctx.QueueId  = static_cast<Uint8>(q);
                Ctx.Name     = Name;
                Ctx.Priority = QUEUE_PRIORITY_MEDIUM;
                ContextCI.push_back(Ctx);
                return;
            }
        }
    };

#if D3D11_SUPPORTED || D3D12_SUPPORTED
    auto PrintAdapterInfo = [](Uint32 AdapterId, const GraphicsAdapterInfo& AdapterInfo, const std::vector<DisplayModeAttribs>& DisplayModes) //
    {
        const char* AdapterTypeStr = nullptr;
        switch (AdapterInfo.Type)
        {
            case ADAPTER_TYPE_DISCRETE:
            case ADAPTER_TYPE_INTEGRATED: AdapterTypeStr = "HW"; break;
            case ADAPTER_TYPE_SOFTWARE: AdapterTypeStr = "SW"; break;
            default: AdapterTypeStr = "Type unknown";
        }
        LOG_INFO_MESSAGE("Adapter ", AdapterId, ": '", AdapterInfo.Description, "' (",
                         AdapterTypeStr, ", ", AdapterInfo.Memory.LocalMemory / (1 << 20), " MB); ",
                         DisplayModes.size(), (DisplayModes.size() == 1 ? " display mode" : " display modes"));
    };
#endif

    switch (m_DeviceType)
    {
#if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11:
        {
#    if ENGINE_DLL
            // Load the dll and import GetEngineFactoryD3D11() function
            auto GetEngineFactoryD3D11 = LoadGraphicsEngineD3D11();
            if (GetEngineFactoryD3D11 == nullptr)
            {
                LOG_ERROR_AND_THROW("Failed to load the engine");
            }
#    endif

            EngineD3D11CreateInfo CreateInfo;
            CreateInfo.GraphicsAPIVersion   = Version{11, 0};
            CreateInfo.DebugMessageCallback = MessageCallback;
            CreateInfo.Features             = DeviceFeatures{DEVICE_FEATURE_STATE_OPTIONAL};
#    ifdef DILIGENT_DEVELOPMENT
            CreateInfo.SetValidationLevel(VALIDATION_LEVEL_2);
#    endif
            auto* pFactoryD3D11 = GetEngineFactoryD3D11();
            EnumerateAdapters(pFactoryD3D11, CreateInfo.GraphicsAPIVersion);

            LOG_INFO_MESSAGE("Found ", Adapters.size(), " compatible adapters");
            for (Uint32 i = 0; i < Adapters.size(); ++i)
            {
                const auto& AdapterInfo = Adapters[i];

                std::vector<DisplayModeAttribs> DisplayModes;
                if (AdapterInfo.NumOutputs > 0)
                {
                    Uint32 NumDisplayModes = 0;
                    pFactoryD3D11->EnumerateDisplayModes(CreateInfo.GraphicsAPIVersion, i, 0, TEX_FORMAT_RGBA8_UNORM, NumDisplayModes, nullptr);
                    DisplayModes.resize(NumDisplayModes);
                    pFactoryD3D11->EnumerateDisplayModes(CreateInfo.GraphicsAPIVersion, i, 0, TEX_FORMAT_RGBA8_UNORM, NumDisplayModes, DisplayModes.data());
                }

                PrintAdapterInfo(i, AdapterInfo, DisplayModes);
            }

            CreateInfo.AdapterId           = FindAdapater(Adapters, CI.AdapterType, CI.AdapterId);
            NumDeferredCtx                 = CI.NumDeferredContexts;
            CreateInfo.NumDeferredContexts = NumDeferredCtx;
            ppContexts.resize(std::max(size_t{1}, ContextCI.size()) + NumDeferredCtx);
            pFactoryD3D11->CreateDeviceAndContextsD3D11(CreateInfo, &m_pDevice, ppContexts.data());
        }
        break;
#endif

#if D3D12_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12:
        {
#    if ENGINE_DLL
            // Load the dll and import GetEngineFactoryD3D12() function
            auto GetEngineFactoryD3D12 = LoadGraphicsEngineD3D12();
            if (GetEngineFactoryD3D12 == nullptr)
            {
                LOG_ERROR_AND_THROW("Failed to load the engine");
            }
#    endif
            auto* pFactoryD3D12 = GetEngineFactoryD3D12();
            if (!pFactoryD3D12->LoadD3D12())
            {
                LOG_ERROR_AND_THROW("Failed to load d3d12 dll");
            }

            EngineD3D12CreateInfo CreateInfo;
            CreateInfo.GraphicsAPIVersion = Version{11, 0};

            EnumerateAdapters(pFactoryD3D12, CreateInfo.GraphicsAPIVersion);

            // Always enable validation
            CreateInfo.SetValidationLevel(VALIDATION_LEVEL_1);

            CreateInfo.DebugMessageCallback = MessageCallback;
            CreateInfo.Features             = DeviceFeatures{DEVICE_FEATURE_STATE_OPTIONAL};

            LOG_INFO_MESSAGE("Found ", Adapters.size(), " compatible adapters");
            for (Uint32 i = 0; i < Adapters.size(); ++i)
            {
                const auto& AdapterInfo = Adapters[i];

                std::vector<DisplayModeAttribs> DisplayModes;
                if (AdapterInfo.NumOutputs > 0)
                {
                    Uint32 NumDisplayModes = 0;
                    pFactoryD3D12->EnumerateDisplayModes(CreateInfo.GraphicsAPIVersion, i, 0, TEX_FORMAT_RGBA8_UNORM, NumDisplayModes, nullptr);
                    DisplayModes.resize(NumDisplayModes);
                    pFactoryD3D12->EnumerateDisplayModes(CreateInfo.GraphicsAPIVersion, i, 0, TEX_FORMAT_RGBA8_UNORM, NumDisplayModes, DisplayModes.data());
                }

                PrintAdapterInfo(i, AdapterInfo, DisplayModes);
            }

            CreateInfo.AdapterId = FindAdapater(Adapters, CI.AdapterType, CI.AdapterId);
            AddContext(COMMAND_QUEUE_TYPE_GRAPHICS, "Graphics", CI.AdapterId);
            AddContext(COMMAND_QUEUE_TYPE_COMPUTE, "Compute", CI.AdapterId);
            AddContext(COMMAND_QUEUE_TYPE_TRANSFER, "Transfer", CI.AdapterId);
            AddContext(COMMAND_QUEUE_TYPE_GRAPHICS, "Graphics 2", CI.AdapterId);
            CreateInfo.NumImmediateContexts  = static_cast<Uint32>(ContextCI.size());
            CreateInfo.pImmediateContextInfo = CreateInfo.NumImmediateContexts > 0 ? ContextCI.data() : nullptr;

            //CreateInfo.EnableGPUBasedValidation                = true;
            CreateInfo.CPUDescriptorHeapAllocationSize[0]      = 64; // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            CreateInfo.CPUDescriptorHeapAllocationSize[1]      = 32; // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
            CreateInfo.CPUDescriptorHeapAllocationSize[2]      = 16; // D3D12_DESCRIPTOR_HEAP_TYPE_RTV
            CreateInfo.CPUDescriptorHeapAllocationSize[3]      = 16; // D3D12_DESCRIPTOR_HEAP_TYPE_DSV
            CreateInfo.DynamicDescriptorAllocationChunkSize[0] = 8;  // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            CreateInfo.DynamicDescriptorAllocationChunkSize[1] = 8;  // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER

            NumDeferredCtx                 = CI.NumDeferredContexts;
            CreateInfo.NumDeferredContexts = NumDeferredCtx;
            ppContexts.resize(std::max(size_t{1}, ContextCI.size()) + NumDeferredCtx);
            pFactoryD3D12->CreateDeviceAndContextsD3D12(CreateInfo, &m_pDevice, ppContexts.data());
        }
        break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
        {
#    if EXPLICITLY_LOAD_ENGINE_GL_DLL
            // Declare function pointer
            // Load the dll and import GetEngineFactoryOpenGL() function
            auto GetEngineFactoryOpenGL = LoadGraphicsEngineOpenGL();
            if (GetEngineFactoryOpenGL == nullptr)
            {
                LOG_ERROR_AND_THROW("Failed to load the engine");
            }
#    endif
            auto* pFactoryOpenGL = GetEngineFactoryOpenGL();
            EnumerateAdapters(pFactoryOpenGL, Version{});

            auto Window = CreateNativeWindow();

            EngineGLCreateInfo CreateInfo;

            // Always enable validation
            CreateInfo.SetValidationLevel(VALIDATION_LEVEL_1);

            CreateInfo.DebugMessageCallback = MessageCallback;
            CreateInfo.Window               = Window;
            CreateInfo.Features             = DeviceFeatures{DEVICE_FEATURE_STATE_OPTIONAL};
            if (CI.ForceNonSeparablePrograms)
                CreateInfo.Features.SeparablePrograms = DEVICE_FEATURE_STATE_DISABLED;
            NumDeferredCtx = 0;
            ppContexts.resize(std::max(size_t{1}, ContextCI.size()) + NumDeferredCtx);
            RefCntAutoPtr<ISwapChain> pSwapChain; // We will use testing swap chain instead
            pFactoryOpenGL->CreateDeviceAndSwapChainGL(
                CreateInfo, &m_pDevice, ppContexts.data(), SCDesc, &pSwapChain);
        }
        break;
#endif

#if VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_VULKAN:
        {
#    if EXPLICITLY_LOAD_ENGINE_VK_DLL
            // Load the dll and import GetEngineFactoryVk() function
            auto GetEngineFactoryVk = LoadGraphicsEngineVk();
            if (GetEngineFactoryVk == nullptr)
            {
                LOG_ERROR_AND_THROW("Failed to load the engine");
            }
#    endif

            auto* pFactoryVk = GetEngineFactoryVk();

            if (CI.EnableDeviceSimulation)
                pFactoryVk->EnableDeviceSimulation();

            EnumerateAdapters(pFactoryVk, Version{});
            AddContext(COMMAND_QUEUE_TYPE_GRAPHICS, "Graphics", CI.AdapterId);
            AddContext(COMMAND_QUEUE_TYPE_COMPUTE, "Compute", CI.AdapterId);
            AddContext(COMMAND_QUEUE_TYPE_TRANSFER, "Transfer", CI.AdapterId);
            AddContext(COMMAND_QUEUE_TYPE_GRAPHICS, "Graphics 2", CI.AdapterId);

            EngineVkCreateInfo CreateInfo;

            // Always enable validation
            CreateInfo.SetValidationLevel(VALIDATION_LEVEL_1);

            CreateInfo.AdapterId                 = CI.AdapterId;
            CreateInfo.NumImmediateContexts      = static_cast<Uint32>(ContextCI.size());
            CreateInfo.pImmediateContextInfo     = CreateInfo.NumImmediateContexts > 0 ? ContextCI.data() : nullptr;
            CreateInfo.DebugMessageCallback      = MessageCallback;
            CreateInfo.MainDescriptorPoolSize    = VulkanDescriptorPoolSize{64, 64, 256, 256, 64, 32, 32, 32, 32, 16, 16};
            CreateInfo.DynamicDescriptorPoolSize = VulkanDescriptorPoolSize{64, 64, 256, 256, 64, 32, 32, 32, 32, 16, 16};
            CreateInfo.UploadHeapPageSize        = 32 * 1024;
            //CreateInfo.DeviceLocalMemoryReserveSize = 32 << 20;
            //CreateInfo.HostVisibleMemoryReserveSize = 48 << 20;
            CreateInfo.Features = DeviceFeatures{DEVICE_FEATURE_STATE_OPTIONAL};

            NumDeferredCtx                 = CI.NumDeferredContexts;
            CreateInfo.NumDeferredContexts = NumDeferredCtx;
            ppContexts.resize(std::max(size_t{1}, ContextCI.size()) + NumDeferredCtx);
            pFactoryVk->CreateDeviceAndContextsVk(CreateInfo, &m_pDevice, ppContexts.data());
        }
        break;
#endif

#if METAL_SUPPORTED
        case RENDER_DEVICE_TYPE_METAL:
        {
            EngineMtlCreateInfo CreateInfo;

            auto* pFactoryMtl = GetEngineFactoryMtl();
            EnumerateAdapters(pFactoryMtl, Version{});
            AddContext(COMMAND_QUEUE_TYPE_GRAPHICS, "Graphics", CI.AdapterId);
            AddContext(COMMAND_QUEUE_TYPE_COMPUTE, "Compute", CI.AdapterId);
            AddContext(COMMAND_QUEUE_TYPE_TRANSFER, "Transfer", CI.AdapterId);
            AddContext(COMMAND_QUEUE_TYPE_GRAPHICS, "Graphics 2", CI.AdapterId);

            CreateInfo.AdapterId             = CI.AdapterId;
            CreateInfo.NumImmediateContexts  = static_cast<Uint32>(ContextCI.size());
            CreateInfo.pImmediateContextInfo = CreateInfo.NumImmediateContexts ? ContextCI.data() : nullptr;
            CreateInfo.Features              = DeviceFeatures{DEVICE_FEATURE_STATE_OPTIONAL};

            // Always enable validation
            CreateInfo.SetValidationLevel(VALIDATION_LEVEL_1);

            CreateInfo.DebugMessageCallback = MessageCallback;
            NumDeferredCtx                  = CI.NumDeferredContexts;
            CreateInfo.NumDeferredContexts  = NumDeferredCtx;
            ppContexts.resize(std::max(size_t{1}, ContextCI.size()) + NumDeferredCtx);
            pFactoryMtl->CreateDeviceAndContextsMtl(CreateInfo, &m_pDevice, ppContexts.data());
        }
        break;
#endif

        default:
            LOG_ERROR_AND_THROW("Unknown device type");
            break;
    }

    constexpr Uint8 InvalidQueueId = 64; // MAX_COMMAND_QUEUES
    m_NumImmediateContexts         = std::max(1u, static_cast<Uint32>(ContextCI.size()));
    m_pDeviceContexts.resize(ppContexts.size());
    for (size_t i = 0; i < ppContexts.size(); ++i)
    {
        if (ppContexts[i] == nullptr)
            LOG_ERROR_AND_THROW("Context must not be null");

        const auto CtxDesc = ppContexts[i]->GetDesc();
        VERIFY(CtxDesc.ContextId == static_cast<Uint8>(i), "Invalid context index");
        if (i < m_NumImmediateContexts)
        {
            VERIFY(!CtxDesc.IsDeferred, "Immediate context expected");
        }
        else
        {
            VERIFY(CtxDesc.IsDeferred, "Deferred context expected");
            VERIFY(CtxDesc.QueueId >= InvalidQueueId, "Hardware queue id must be invalid");
        }
        m_pDeviceContexts[i].Attach(ppContexts[i]);
    }

    for (size_t i = 0; i < ContextCI.size(); ++i)
    {
        const auto& CtxCI   = ContextCI[i];
        const auto  CtxDesc = m_pDeviceContexts[i]->GetDesc();
        if (CtxCI.QueueId != CtxDesc.QueueId)
            LOG_ERROR_MESSAGE("QueueId mismatch");
        if (i != CtxDesc.ContextId)
            LOG_ERROR_MESSAGE("CommandQueueId mismatch");
    }

    const auto& AdapterInfo = m_pDevice->GetAdapterInfo();
    std::string AdapterInfoStr;
    AdapterInfoStr = "Adapter description: ";
    AdapterInfoStr += AdapterInfo.Description;
    AdapterInfoStr += ". Vendor: ";
    switch (AdapterInfo.Vendor)
    {
        case ADAPTER_VENDOR_NVIDIA:
            AdapterInfoStr += "NVidia";
            break;
        case ADAPTER_VENDOR_AMD:
            AdapterInfoStr += "AMD";
            break;
        case ADAPTER_VENDOR_INTEL:
            AdapterInfoStr += "Intel";
            break;
        case ADAPTER_VENDOR_ARM:
            AdapterInfoStr += "ARM";
            break;
        case ADAPTER_VENDOR_QUALCOMM:
            AdapterInfoStr += "Qualcomm";
            break;
        case ADAPTER_VENDOR_IMGTECH:
            AdapterInfoStr += "Imagination tech";
            break;
        case ADAPTER_VENDOR_MSFT:
            AdapterInfoStr += "Microsoft";
            break;
        case ADAPTER_VENDOR_APPLE:
            AdapterInfoStr += "Apple";
            break;
        case ADAPTER_VENDOR_MESA:
            AdapterInfoStr += "Mesa";
            break;
        case ADAPTER_VENDOR_BROADCOM:
            AdapterInfoStr += "Broadcom";
            break;
        default:
            AdapterInfoStr += "Unknown";
    }
    AdapterInfoStr += ". Local memory: ";
    AdapterInfoStr += std::to_string(AdapterInfo.Memory.LocalMemory >> 20);
    AdapterInfoStr += " MB. Host-visible memory: ";
    AdapterInfoStr += std::to_string(AdapterInfo.Memory.HostVisibileMemory >> 20);
    AdapterInfoStr += " MB. Unified memory: ";
    AdapterInfoStr += std::to_string(AdapterInfo.Memory.UnifiedMemory >> 20);
    AdapterInfoStr += " MB.";
    LOG_INFO_MESSAGE(AdapterInfoStr);
}

TestingEnvironment::~TestingEnvironment()
{
    for (Uint32 i = 0; i < GetNumImmediateContexts(); ++i)
    {
        auto* pCtx = GetDeviceContext(i);
        pCtx->Flush();
        pCtx->FinishFrame();
    }
}

// Override this to define how to set up the environment.
void TestingEnvironment::SetUp()
{
}

// Override this to define how to tear down the environment.
void TestingEnvironment::TearDown()
{
}

void TestingEnvironment::ReleaseResources()
{
    // It is necessary to call Flush() to force the driver to release resources.
    // Without flushing the command buffer, the memory may not be released until sometimes
    // later causing out-of-memory error.
    for (Uint32 i = 0; i < GetNumImmediateContexts(); ++i)
    {
        auto* pCtx = GetDeviceContext(i);
        pCtx->Flush();
        pCtx->FinishFrame();
        pCtx->WaitForIdle();
    }
    m_pDevice->ReleaseStaleResources();
}

void TestingEnvironment::Reset()
{
    for (Uint32 i = 0; i < GetNumImmediateContexts(); ++i)
    {
        auto* pCtx = GetDeviceContext(i);
        pCtx->Flush();
        pCtx->FinishFrame();
        pCtx->InvalidateState();
    }
    m_pDevice->IdleGPU();
    m_pDevice->ReleaseStaleResources();
    m_NumAllowedErrors = 0;
}

RefCntAutoPtr<ITexture> TestingEnvironment::CreateTexture(const char* Name, TEXTURE_FORMAT Fmt, BIND_FLAGS BindFlags, Uint32 Width, Uint32 Height, void* pInitData)
{
    TextureDesc TexDesc;

    TexDesc.Name      = Name;
    TexDesc.Type      = RESOURCE_DIM_TEX_2D;
    TexDesc.Format    = Fmt;
    TexDesc.BindFlags = BindFlags;
    TexDesc.Width     = Width;
    TexDesc.Height    = Height;

    const auto        FmtAttribs = GetTextureFormatAttribs(Fmt);
    TextureSubResData Mip0Data{pInitData, FmtAttribs.ComponentSize * FmtAttribs.NumComponents * Width};
    TextureData       TexData{&Mip0Data, 1};

    RefCntAutoPtr<ITexture> pTexture;
    m_pDevice->CreateTexture(TexDesc, pInitData ? &TexData : nullptr, &pTexture);
    VERIFY_EXPR(pTexture != nullptr);

    return pTexture;
}

RefCntAutoPtr<ISampler> TestingEnvironment::CreateSampler(const SamplerDesc& Desc)
{
    RefCntAutoPtr<ISampler> pSampler;
    m_pDevice->CreateSampler(Desc, &pSampler);
    return pSampler;
}

void TestingEnvironment::SetDefaultCompiler(SHADER_COMPILER compiler)
{
    switch (m_pDevice->GetDeviceInfo().Type)
    {
        case RENDER_DEVICE_TYPE_D3D12:
            switch (compiler)
            {
                case SHADER_COMPILER_DEFAULT:
                case SHADER_COMPILER_FXC:
                case SHADER_COMPILER_DXC:
                    m_ShaderCompiler = compiler;
                    break;

                default:
                    LOG_WARNING_MESSAGE(GetShaderCompilerTypeString(compiler), " is not supported by Direct3D12 backend. Using default compiler");
                    m_ShaderCompiler = SHADER_COMPILER_DEFAULT;
            }
            break;

        case RENDER_DEVICE_TYPE_D3D11:
            switch (compiler)
            {
                case SHADER_COMPILER_DEFAULT:
                case SHADER_COMPILER_FXC:
                    m_ShaderCompiler = compiler;
                    break;

                default:
                    LOG_WARNING_MESSAGE(GetShaderCompilerTypeString(compiler), " is not supported by Direct3D11 backend. Using default compiler");
                    m_ShaderCompiler = SHADER_COMPILER_DEFAULT;
            }
            break;

        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
            switch (compiler)
            {
                case SHADER_COMPILER_DEFAULT:
                    m_ShaderCompiler = compiler;
                    break;

                default:
                    LOG_WARNING_MESSAGE(GetShaderCompilerTypeString(compiler), " is not supported by OpenGL/GLES backend. Using default compiler");
                    m_ShaderCompiler = SHADER_COMPILER_DEFAULT;
            }
            break;

        case RENDER_DEVICE_TYPE_VULKAN:
            switch (compiler)
            {
                case SHADER_COMPILER_DEFAULT:
                case SHADER_COMPILER_GLSLANG:
                    m_ShaderCompiler = compiler;

                case SHADER_COMPILER_DXC:
                    if (HasDXCompiler())
                        m_ShaderCompiler = compiler;
                    break;

                default:
                    LOG_WARNING_MESSAGE(GetShaderCompilerTypeString(compiler), " is not supported by Vulkan backend. Using default compiler");
                    m_ShaderCompiler = SHADER_COMPILER_DEFAULT;
            }
            break;

        case RENDER_DEVICE_TYPE_METAL:
            switch (compiler)
            {
                case SHADER_COMPILER_DEFAULT:
                    m_ShaderCompiler = compiler;
                    break;

                default:
                    LOG_WARNING_MESSAGE(GetShaderCompilerTypeString(compiler), " is not supported by Metal backend. Using default compiler");
                    m_ShaderCompiler = SHADER_COMPILER_DEFAULT;
            }
            break;

        default:
            LOG_WARNING_MESSAGE("Unepxected device type");
            m_ShaderCompiler = SHADER_COMPILER_DEFAULT;
    }

    LOG_INFO_MESSAGE("Selected shader compiler: ", GetShaderCompilerTypeString(m_ShaderCompiler));
}

SHADER_COMPILER TestingEnvironment::GetDefaultCompiler(SHADER_SOURCE_LANGUAGE lang) const
{
    if (m_pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_VULKAN &&
        lang != SHADER_SOURCE_LANGUAGE_HLSL)
        return SHADER_COMPILER_GLSLANG;
    else
        return m_ShaderCompiler;
}

const char* TestingEnvironment::GetCurrentTestStatusString()
{
    static constexpr char TestFailedString[] = "\033[0;91m"
                                               "[  FAILED  ]"
                                               "\033[0;0m";
    static constexpr char TestPassedString[] = "\033[0;92m"
                                               "[  PASSED  ]"
                                               "\033[0;0m";
    return testing::Test::HasFailure() ? TestFailedString : TestPassedString;
}

const char* TestingEnvironment::GetTestSkippedString()
{
    return "\033[0;32m"
           "[  SKIPPED ]"
           "\033[0;0m";
}

} // namespace Testing

} // namespace Diligent
