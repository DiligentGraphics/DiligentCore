/*     Copyright 2019 Diligent Graphics LLC
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

#include "TestingEnvironment.h"

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

TestingEnvironment* TestingEnvironment::m_pTheEnvironment = nullptr;

TestingEnvironment::TestingEnvironment(DeviceType deviceType, ADAPTER_TYPE AdapterType) :
    m_DeviceType{deviceType}
{
    VERIFY(m_pTheEnvironment == nullptr, "Testing environment object has already been initialized!");
    m_pTheEnvironment = this;

    SwapChainDesc SCDesc;

    Uint32 NumDeferredCtx     = 0;
    void*  NativeWindowHandle = nullptr;

    std::vector<IDeviceContext*> ppContexts;
    std::vector<AdapterAttribs>  Adapters;

    auto PrintAdapterInfo = [](Uint32 AdapterId, const AdapterAttribs& AdapterInfo, const std::vector<DisplayModeAttribs>& DisplayModes) //
    {
        const char* AdapterTypeStr = nullptr;
        switch (AdapterInfo.AdapterType)
        {
            case ADAPTER_TYPE_HARDWARE: AdapterTypeStr = "HW"; break;
            case ADAPTER_TYPE_SOFTWARE: AdapterTypeStr = "SW"; break;
            default: AdapterTypeStr = "Type unknown";
        }
        LOG_INFO_MESSAGE("Adapter ", AdapterId, ": '", AdapterInfo.Description, "' (",
                         AdapterTypeStr, ", ", AdapterInfo.DedicatedVideoMemory / (1 << 20), " MB); ",
                         DisplayModes.size(), (DisplayModes.size() == 1 ? " display mode" : " display modes"));
    };
    switch (m_DeviceType)
    {
#if D3D11_SUPPORTED
        case DeviceType::D3D11:
        {
#    if ENGINE_DLL
            GetEngineFactoryD3D11Type GetEngineFactoryD3D11 = nullptr;
            // Load the dll and import GetEngineFactoryD3D11() function
            LoadGraphicsEngineD3D11(GetEngineFactoryD3D11);
            if (GetEngineFactoryD3D11 == nullptr)
            {
                LOG_ERROR_AND_THROW("Failed to load the engine");
            }
#    endif

            EngineD3D11CreateInfo CreateInfo;
#    ifdef _DEBUG
            CreateInfo.DebugFlags =
                D3D11_DEBUG_FLAG_CREATE_DEBUG_DEVICE |
                D3D11_DEBUG_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE |
                D3D11_DEBUG_FLAG_VERIFY_COMMITTED_SHADER_RESOURCES;
#    endif
            auto*  pFactoryD3D11 = GetEngineFactoryD3D11();
            Uint32 NumAdapters   = 0;
            pFactoryD3D11->EnumerateAdapters(DIRECT3D_FEATURE_LEVEL_11_0, NumAdapters, 0);
            Adapters.resize(NumAdapters);
            pFactoryD3D11->EnumerateAdapters(DIRECT3D_FEATURE_LEVEL_11_0, NumAdapters, Adapters.data());

            LOG_INFO_MESSAGE("Found ", Adapters.size(), " compatible adapters");
            for (Uint32 i = 0; i < Adapters.size(); ++i)
            {
                const auto& AdapterInfo = Adapters[i];

                std::vector<DisplayModeAttribs> DisplayModes;
                if (AdapterInfo.NumOutputs > 0)
                {
                    Uint32 NumDisplayModes = 0;
                    pFactoryD3D11->EnumerateDisplayModes(DIRECT3D_FEATURE_LEVEL_11_0, i, 0, TEX_FORMAT_RGBA8_UNORM, NumDisplayModes, nullptr);
                    DisplayModes.resize(NumDisplayModes);
                    pFactoryD3D11->EnumerateDisplayModes(DIRECT3D_FEATURE_LEVEL_11_0, i, 0, TEX_FORMAT_RGBA8_UNORM, NumDisplayModes, DisplayModes.data());
                }

                PrintAdapterInfo(i, AdapterInfo, DisplayModes);
            }
            if (AdapterType != ADAPTER_TYPE_UNKNOWN)
            {
                for (Uint32 i = 0; i < Adapters.size(); ++i)
                {
                    if (Adapters[i].AdapterType == AdapterType &&
                        CreateInfo.AdapterId == EngineD3D11CreateInfo::DefaultAdapterId)
                    {
                        CreateInfo.AdapterId = i;
                        LOG_INFO_MESSAGE("Using adapter ", i, ": '", Adapters[i].Description, "'");
                        break;
                    }
                }
            }


            CreateInfo.NumDeferredContexts = NumDeferredCtx;
            ppContexts.resize(1 + NumDeferredCtx);
            pFactoryD3D11->CreateDeviceAndContextsD3D11(CreateInfo, &m_pDevice, ppContexts.data());

            if (NativeWindowHandle != nullptr)
                pFactoryD3D11->CreateSwapChainD3D11(m_pDevice, ppContexts[0], SCDesc, FullScreenModeDesc{}, NativeWindowHandle, &m_pSwapChain);
        }
        break;
#endif

#if D3D12_SUPPORTED
        case DeviceType::D3D12:
        {
#    if ENGINE_DLL
            GetEngineFactoryD3D12Type GetEngineFactoryD3D12 = nullptr;
            // Load the dll and import GetEngineFactoryD3D12() function
            LoadGraphicsEngineD3D12(GetEngineFactoryD3D12);
            if (GetEngineFactoryD3D12 == nullptr)
            {
                LOG_ERROR_AND_THROW("Failed to load the engine");
            }
#    endif
            auto* pFactoryD3D12 = GetEngineFactoryD3D12();

            Uint32 NumAdapters = 0;
            pFactoryD3D12->EnumerateAdapters(DIRECT3D_FEATURE_LEVEL_11_0, NumAdapters, 0);
            Adapters.resize(NumAdapters);
            pFactoryD3D12->EnumerateAdapters(DIRECT3D_FEATURE_LEVEL_11_0, NumAdapters, Adapters.data());

            EngineD3D12CreateInfo CreateInfo;

            LOG_INFO_MESSAGE("Found ", Adapters.size(), " compatible adapters");
            for (Uint32 i = 0; i < Adapters.size(); ++i)
            {
                const auto& AdapterInfo = Adapters[i];

                std::vector<DisplayModeAttribs> DisplayModes;
                if (AdapterInfo.NumOutputs > 0)
                {
                    Uint32 NumDisplayModes = 0;
                    pFactoryD3D12->EnumerateDisplayModes(DIRECT3D_FEATURE_LEVEL_11_0, i, 0, TEX_FORMAT_RGBA8_UNORM, NumDisplayModes, nullptr);
                    DisplayModes.resize(NumDisplayModes);
                    pFactoryD3D12->EnumerateDisplayModes(DIRECT3D_FEATURE_LEVEL_11_0, i, 0, TEX_FORMAT_RGBA8_UNORM, NumDisplayModes, DisplayModes.data());
                }

                PrintAdapterInfo(i, AdapterInfo, DisplayModes);
            }
            if (AdapterType != ADAPTER_TYPE_UNKNOWN)
            {
                for (Uint32 i = 0; i < Adapters.size(); ++i)
                {
                    if (Adapters[i].AdapterType == AdapterType &&
                        CreateInfo.AdapterId == EngineD3D11CreateInfo::DefaultAdapterId)
                    {
                        CreateInfo.AdapterId = i;
                        LOG_INFO_MESSAGE("Using adapter ", i, ": '", Adapters[i].Description, "'");
                        break;
                    }
                }
            }
            CreateInfo.EnableDebugLayer = true;
            //CreateInfo.EnableGPUBasedValidation = true;
            CreateInfo.CPUDescriptorHeapAllocationSize[0]      = 64; // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            CreateInfo.CPUDescriptorHeapAllocationSize[1]      = 32; // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
            CreateInfo.CPUDescriptorHeapAllocationSize[2]      = 16; // D3D12_DESCRIPTOR_HEAP_TYPE_RTV
            CreateInfo.CPUDescriptorHeapAllocationSize[3]      = 16; // D3D12_DESCRIPTOR_HEAP_TYPE_DSV
            CreateInfo.DynamicDescriptorAllocationChunkSize[0] = 8;  // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            CreateInfo.DynamicDescriptorAllocationChunkSize[1] = 8;  // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
            ppContexts.resize(1 + NumDeferredCtx);

            CreateInfo.NumDeferredContexts = NumDeferredCtx;
            pFactoryD3D12->CreateDeviceAndContextsD3D12(CreateInfo, &m_pDevice, ppContexts.data());

            if (!m_pSwapChain && NativeWindowHandle != nullptr)
                pFactoryD3D12->CreateSwapChainD3D12(m_pDevice, ppContexts[0], SCDesc, FullScreenModeDesc{}, NativeWindowHandle, &m_pSwapChain);
        }
        break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case DeviceType::OpenGL:
        case DeviceType::OpenGLES:
        {
#    if !PLATFORM_MACOS
            VERIFY_EXPR(NativeWindowHandle != nullptr);
#    endif
#    if EXPLICITLY_LOAD_ENGINE_GL_DLL
            // Declare function pointer
            GetEngineFactoryOpenGLType GetEngineFactoryOpenGL = nullptr;
            // Load the dll and import GetEngineFactoryOpenGL() function
            LoadGraphicsEngineOpenGL(GetEngineFactoryOpenGL);
            if (GetEngineFactoryOpenGL == nullptr)
            {
                LOG_ERROR_AND_THROW("Failed to load the engine");
            }
#    endif
            auto* pFactoryOpenGL = GetEngineFactoryOpenGL();

            EngineGLCreateInfo CreateInfo;
            CreateInfo.pNativeWndHandle = NativeWindowHandle;
#    if PLATFORM_LINUX
            CreateInfo.pDisplay = display;
#    endif
            if (NumDeferredCtx != 0)
            {
                LOG_ERROR_MESSAGE("Deferred contexts are not supported in OpenGL mode");
                NumDeferredCtx = 0;
            }
            ppContexts.resize(1 + NumDeferredCtx);
            pFactoryOpenGL->CreateDeviceAndSwapChainGL(
                CreateInfo, &m_pDevice, ppContexts.data(), SCDesc, &m_pSwapChain);
        }
        break;
#endif

#if VULKAN_SUPPORTED
        case DeviceType::Vulkan:
        {
#    if EXPLICITLY_LOAD_ENGINE_VK_DLL
            GetEngineFactoryVkType GetEngineFactoryVk = nullptr;
            // Load the dll and import GetEngineFactoryVk() function
            LoadGraphicsEngineVk(GetEngineFactoryVk);
            if (GetEngineFactoryVk == nullptr)
            {
                LOG_ERROR_AND_THROW("Failed to load the engine");
            }
#    endif

            EngineVkCreateInfo CreateInfo;

            CreateInfo.EnableValidation          = true;
            CreateInfo.MainDescriptorPoolSize    = EngineVkCreateInfo::DescriptorPoolSize{64, 64, 256, 256, 64, 32, 32, 32, 32};
            CreateInfo.DynamicDescriptorPoolSize = EngineVkCreateInfo::DescriptorPoolSize{64, 64, 256, 256, 64, 32, 32, 32, 32};
            CreateInfo.UploadHeapPageSize        = 32 * 1024;
            //CreateInfo.DeviceLocalMemoryReserveSize = 32 << 20;
            //CreateInfo.HostVisibleMemoryReserveSize = 48 << 20;

            auto& Features                          = CreateInfo.EnabledFeatures;
            Features.depthBiasClamp                 = true;
            Features.fillModeNonSolid               = true;
            Features.depthClamp                     = true;
            Features.independentBlend               = true;
            Features.samplerAnisotropy              = true;
            Features.geometryShader                 = true;
            Features.tessellationShader             = true;
            Features.dualSrcBlend                   = true;
            Features.multiViewport                  = true;
            Features.imageCubeArray                 = true;
            Features.textureCompressionBC           = true;
            Features.vertexPipelineStoresAndAtomics = true;
            Features.fragmentStoresAndAtomics       = true;

            CreateInfo.NumDeferredContexts = NumDeferredCtx;
            ppContexts.resize(1 + NumDeferredCtx);
            auto* pFactoryVk = GetEngineFactoryVk();
            pFactoryVk->CreateDeviceAndContextsVk(CreateInfo, &m_pDevice, ppContexts.data());

            if (!m_pSwapChain && NativeWindowHandle != nullptr)
                pFactoryVk->CreateSwapChainVk(m_pDevice, ppContexts[0], SCDesc, NativeWindowHandle, &m_pSwapChain);
        }
        break;
#endif

#if METAL_SUPPORTED
        case DeviceType::Metal:
        {
            EngineMtlCreateInfo MtlAttribs;

            MtlAttribs.NumDeferredContexts = NumDeferredCtx;
            ppContexts.resize(1 + NumDeferredCtx);
            auto* pFactoryMtl = GetEngineFactoryMtl();
            pFactoryMtl->CreateDeviceAndContextsMtl(MtlAttribs, &m_pDevice, ppContexts.data());

            if (!m_pSwapChain && NativeWindowHandle != nullptr)
                pFactoryMtl->CreateSwapChainMtl(m_pDevice, ppContexts[0], SCDesc, NativeWindowHandle, &m_pSwapChain);
        }
        break;
#endif

        default:
            LOG_ERROR_AND_THROW("Unknown device type");
            break;
    }

    m_pDeviceContext.Attach(ppContexts[0]);
}

TestingEnvironment::~TestingEnvironment()
{
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
    m_pDeviceContext->Flush();
    m_pDeviceContext->FinishFrame();
    m_pDevice->ReleaseStaleResources();
}

void TestingEnvironment::Reset()
{
    m_pDeviceContext->Flush();
    m_pDeviceContext->FinishFrame();
    m_pDevice->IdleGPU();
    m_pDevice->ReleaseStaleResources();
    m_pDeviceContext->InvalidateState();
}

} // namespace Diligent
