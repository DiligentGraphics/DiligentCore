/*     Copyright 2015-2018 Egor Yusov
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

/// \file
/// Routines that initialize Vulkan-based engine implementation

#include "pch.h"
#include "RenderDeviceFactoryVk.h"
#include "RenderDeviceVkImpl.h"
#include "DeviceContextVkImpl.h"
#include "SwapChainVkImpl.h"
#include "VulkanTypeConversions.h"
#include "StringTools.h"
#include "EngineMemory.h"
#include "CommandQueueVkImpl.h"
#include "VulkanUtilities/VulkanInstance.h"
#include "VulkanUtilities/VulkanPhysicalDevice.h"

namespace Diligent
{

/// Engine factory for Vk implementation
class EngineFactoryVkImpl : public IEngineFactoryVk
{
public:
    static EngineFactoryVkImpl* GetInstance()
    {
        static EngineFactoryVkImpl TheFactory;
        return &TheFactory;
    }

    void CreateDeviceAndContextsVk( const EngineVkAttribs& CreationAttribs, 
                                       IRenderDevice **ppDevice, 
                                       IDeviceContext **ppContexts,
                                       Uint32 NumDeferredContexts)override final;

    /*void AttachToVkDevice(void *pVkNativeDevice, 
                             ICommandQueueVk *pCommandQueue,
                             const EngineVkAttribs& EngineAttribs, 
                             IRenderDevice **ppDevice, 
                             IDeviceContext **ppContexts,
                             Uint32 NumDeferredContexts)override final;
                             */

    void CreateSwapChainVk( IRenderDevice *pDevice, 
                                       IDeviceContext *pImmediateContext, 
                                       const SwapChainDesc& SwapChainDesc, 
                                       void* pNativeWndHandle, 
                                       ISwapChain **ppSwapChain )override final;
};

#if 0
void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
{
	CComPtr<IDXGIAdapter1> adapter;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			// If you want a software adapter, pass in "/warp" on the command line.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(VkCreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(IVkDevice), nullptr)))
		{
            LOG_INFO_MESSAGE("Vk-capabale hardware found: ", NarrowString(desc.Description), " (", desc.DedicatedVideoMemory>>20, " MB)");
			break;
		}
	}

	*ppAdapter = adapter.Detach();
}
#endif


/// Creates render device and device contexts for Vulkan backend

/// \param [in] CreationAttribs - Engine creation attributes.
/// \param [out] ppDevice - Address of the memory location where pointer to 
///                         the created device will be written
/// \param [out] ppContexts - Address of the memory location where pointers to 
///                           the contexts will be written. The new immediate 
///                           context goes at position 0. If NumDeferredContexts > 0,
///                           pointers to the deferred contexts are written afterwards.
/// \param [in] NumDeferredContexts - Number of deferred contexts. If non-zero number
///                                   of deferred contexts is requested, pointers to the
///                                   contexts are written to ppContexts array starting 
///                                   at position 1
void EngineFactoryVkImpl::CreateDeviceAndContextsVk( const EngineVkAttribs& CreationAttribs, 
                                                     IRenderDevice **ppDevice, 
                                                     IDeviceContext **ppContexts,
                                                     Uint32 NumDeferredContexts)
{
    VERIFY( ppDevice && ppContexts, "Null pointer provided" );
    if( !ppDevice || !ppContexts )
        return;

    SetRawAllocator(CreationAttribs.pRawMemAllocator);

    *ppDevice = nullptr;
    memset(ppContexts, 0, sizeof(*ppContexts) * (1 + NumDeferredContexts));

    std::shared_ptr<VulkanUtilities::VulkanInstance> Instance;
    try
    {
        Instance = std::make_shared<VulkanUtilities::VulkanInstance>(
            CreationAttribs.EnableValidation, 
            CreationAttribs.GlobalExtensionCount, 
            CreationAttribs.ppGlobalExtensionNames,
            reinterpret_cast<VkAllocationCallbacks*>(CreationAttribs.pVkAllocator));

        auto vkDevice = Instance->SelectPhysicalDevice();
        std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice> PhysicalDevice(new VulkanUtilities::VulkanPhysicalDevice(vkDevice));

        // If an implementation exposes any queue family that supports graphics operations, 
        // at least one queue family of at least one physical device exposed by the implementation 
        // must support both graphics and compute operations.

        VkDeviceQueueCreateInfo QueueInfo{};
        QueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QueueInfo.flags = 0; // reserved for future use
        // All commands that are allowed on a queue that supports transfer operations are also allowed on a 
        // queue that supports either graphics or compute operations.Thus, if the capabilities of a queue family 
        // include VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT, then reporting the VK_QUEUE_TRANSFER_BIT 
        // capability separately for that queue family is optional.
        QueueInfo.queueFamilyIndex = PhysicalDevice->FindQueueFamily(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
        QueueInfo.queueCount = 1;
        const float defaultQueuePriority = 1.0f; // Ask for highest priority for our queue. (range [0,1])
        QueueInfo.pQueuePriorities = &defaultQueuePriority;

        VkDeviceCreateInfo DeviceCreateInfo = {};
        DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        DeviceCreateInfo.flags = 0; // Reserved for future use
        // https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#extended-functionality-device-layer-deprecation
        DeviceCreateInfo.enabledLayerCount = 0; // Deprecated and ignored.
        DeviceCreateInfo.ppEnabledLayerNames = nullptr; // Deprecated and ignored
        DeviceCreateInfo.queueCreateInfoCount = 1;
        DeviceCreateInfo.pQueueCreateInfos = &QueueInfo;
        DeviceCreateInfo.pEnabledFeatures = nullptr; // NULL or a pointer to a VkPhysicalDeviceFeatures structure that contains 
                                                     // boolean indicators of all the features to be enabled.

        std::vector<const char*> DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        if (PhysicalDevice->IsExtensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
        {
            DeviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
        }

        DeviceCreateInfo.ppEnabledExtensionNames = DeviceExtensions.empty() ? nullptr : DeviceExtensions.data();
        DeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
        
        VkDevice VulkanDevice = VK_NULL_HANDLE;
        auto res = vkCreateDevice(PhysicalDevice->GetVkDeviceHandle(), &DeviceCreateInfo, Instance->GetVkAllocator(), &VulkanDevice);
        CHECK_VK_ERROR_AND_THROW(res, "Failed to create logical device");

        VkQueue Queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(VulkanDevice, 
            QueueInfo.queueFamilyIndex, // Index of the queue family to which the queue belongs
            0,                          // Index within this queue family of the queue to retrieve
            &Queue);
        VERIFY_EXPR(Queue != VK_NULL_HANDLE);

        //vkDestroyDevice(VulkanDevice, Instance->GetVkAllocator());

    }
    catch(std::runtime_error& )
    {
        return;
    }




#if 0
    for(Uint32 Type=Vk_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; Type < Vk_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++Type)
    {
        auto CPUHeapAllocSize = CreationAttribs.CPUDescriptorHeapAllocationSize[Type];
        Uint32 MaxSize = 1 << 20;
        if( CPUHeapAllocSize > 1 << 20 )
        {
            LOG_ERROR( "CPU Heap allocation size is too large (", CPUHeapAllocSize, "). Max allowed size is ", MaxSize );
            return;
        }

        if( (CPUHeapAllocSize % 16) != 0 )
        {
            LOG_ERROR( "CPU Heap allocation size (", CPUHeapAllocSize, ") is expected to be multiple of 16" );
            return;
        }
    }


    RefCntAutoPtr<CommandQueueVkImpl> pCmdQueueVk;
    CComPtr<IVkDevice> VkDevice;
    try
    {
#if defined(_DEBUG)
	    // Enable the Vk debug layer.
	    {
		    CComPtr<IVkDebug> debugController;
		    if (SUCCEEDED(VkGetDebugInterface(__uuidof(debugController), reinterpret_cast<void**>(static_cast<IVkDebug**>(&debugController)) )))
		    {
			    debugController->EnableDebugLayer();
		    }
	    }
#endif

	    CComPtr<IDXGIFactory4> factory;
        HRESULT hr = CreateDXGIFactory1(__uuidof(factory), reinterpret_cast<void**>(static_cast<IDXGIFactory4**>(&factory)) );
        CHECK_D3D_RESULT_THROW(hr, "Failed to create DXGI factory")

	    CComPtr<IDXGIAdapter1> hardwareAdapter;
	    GetHardwareAdapter(factory, &hardwareAdapter);
    
        hr = VkCreateDevice(hardwareAdapter, D3D_FEATURE_LEVEL_11_0, __uuidof(VkDevice), reinterpret_cast<void**>(static_cast<IVkDevice**>(&VkDevice)) );
        if( FAILED(hr))
        {
            LOG_WARNING_MESSAGE("Failed to create hardware device. Attempting to create WARP device");

		    CComPtr<IDXGIAdapter> warpAdapter;
		    hr = factory->EnumWarpAdapter( __uuidof(warpAdapter),  reinterpret_cast<void**>(static_cast<IDXGIAdapter**>(&warpAdapter)) );
            CHECK_D3D_RESULT_THROW(hr, "Failed to enum warp adapter")

		    hr = VkCreateDevice( warpAdapter, D3D_FEATURE_LEVEL_11_0, __uuidof(VkDevice), reinterpret_cast<void**>(static_cast<IVkDevice**>(&VkDevice)) );
            CHECK_D3D_RESULT_THROW(hr, "Failed to crate warp device")
        }

#if _DEBUG
        {
	        CComPtr<IVkInfoQueue> pInfoQueue;
            hr = VkDevice->QueryInterface(__uuidof(pInfoQueue), reinterpret_cast<void**>(static_cast<IVkInfoQueue**>(&pInfoQueue)));
	        if( SUCCEEDED(hr) )
	        {
		        // Suppress whole categories of messages
		        //Vk_MESSAGE_CATEGORY Categories[] = {};

		        // Suppress messages based on their severity level
		        Vk_MESSAGE_SEVERITY Severities[] = 
		        {
			        Vk_MESSAGE_SEVERITY_INFO
		        };

		        // Suppress individual messages by their ID
		        //Vk_MESSAGE_ID DenyIds[] = {};

		        Vk_INFO_QUEUE_FILTER NewFilter = {};
		        //NewFilter.DenyList.NumCategories = _countof(Categories);
		        //NewFilter.DenyList.pCategoryList = Categories;
		        NewFilter.DenyList.NumSeverities = _countof(Severities);
		        NewFilter.DenyList.pSeverityList = Severities;
		        //NewFilter.DenyList.NumIDs = _countof(DenyIds);
		        //NewFilter.DenyList.pIDList = DenyIds;

		        hr = pInfoQueue->PushStorageFilter(&NewFilter);
                VERIFY(SUCCEEDED(hr), "Failed to push storage filter");
            }
        }
#endif

#ifndef RELEASE
	    // Prevent the GPU from overclocking or underclocking to get consistent timings
	    //VkDevice->SetStablePowerState(TRUE);
#endif

	    // Describe and create the command queue.
	    Vk_COMMAND_QUEUE_DESC queueDesc = {};
	    queueDesc.Flags = Vk_COMMAND_QUEUE_FLAG_NONE;
	    queueDesc.Type = Vk_COMMAND_LIST_TYPE_DIRECT;

        CComPtr<IVkCommandQueue> pVkCmdQueue;
        hr = VkDevice->CreateCommandQueue(&queueDesc, __uuidof(pVkCmdQueue), reinterpret_cast<void**>(static_cast<IVkCommandQueue**>(&pVkCmdQueue)));
        CHECK_D3D_RESULT_THROW(hr, "Failed to create command queue");
        hr = pVkCmdQueue->SetName(L"Main Command Queue");
        VERIFY_EXPR(SUCCEEDED(hr));

        CComPtr<IVkFence> pVkFence;
        hr = VkDevice->CreateFence(0, Vk_FENCE_FLAG_NONE, __uuidof(pVkFence), reinterpret_cast<void**>(static_cast<IVkFence**>(&pVkFence)));
        CHECK_D3D_RESULT_THROW(hr, "Failed to create main command queue fence");
	    VkDevice->SetName(L"Main Command Queue fence");

        auto &RawMemAllocator = GetRawAllocator();
        pCmdQueueVk = NEW_RC_OBJ(RawMemAllocator, "CommandQueueVk instance", CommandQueueVkImpl)(pVkCmdQueue, pVkFence);
    }
    catch( const std::runtime_error & )
    {
        LOG_ERROR( "Failed to initialize Vk resources" );
        return;
    }
        
    AttachToVkDevice(VkDevice, pCmdQueueVk, CreationAttribs, ppDevice, ppContexts, NumDeferredContexts);
#endif
}

#if 0
/// Attaches to existing Vk device

/// \param [in] pVkNativeDevice - pointer to native Vk device
/// \param [in] pCommandQueue - pointer to the implementation of command queue
/// \param [in] EngineAttribs - Engine creation attributes.
/// \param [out] ppDevice - Address of the memory location where pointer to 
///                         the created device will be written
/// \param [out] ppContexts - Address of the memory location where pointers to 
///                           the contexts will be written. Pointer to the immediate 
///                           context goes at position 0. If NumDeferredContexts > 0,
///                           pointers to the deferred contexts go afterwards.
/// \param [in] NumDeferredContexts - Number of deferred contexts. If non-zero number
///                                   of deferred contexts is requested, pointers to the
///                                   contexts are written to ppContexts array starting 
///                                   at position 1
void EngineFactoryVkImpl::AttachToVkDevice(void *pVkNativeDevice, 
                                           ICommandQueueVk *pCommandQueue,
                                           const EngineVkAttribs& EngineAttribs, 
                                           IRenderDevice **ppDevice, 
                                           IDeviceContext **ppContexts,
                                           Uint32 NumDeferredContexts)
{
    VERIFY( pVkNativeDevice && pCommandQueue && ppDevice && ppContexts, "Null pointer provided" );
    if( !pVkNativeDevice || !pCommandQueue || !ppDevice || !ppContexts )
        return;

    *ppDevice = nullptr;
    memset(ppContexts, 0, sizeof(*ppContexts) * (1+NumDeferredContexts));

    try
    {
        auto &RawMemAllocator = GetRawAllocator();
        auto VkDevice = reinterpret_cast<IVkDevice*>(pVkNativeDevice);
        RenderDeviceVkImpl *pRenderDeviceVk( NEW_RC_OBJ(RawMemAllocator, "RenderDeviceVkImpl instance", RenderDeviceVkImpl)(RawMemAllocator, EngineAttribs, VkDevice, pCommandQueue, NumDeferredContexts ) );
        pRenderDeviceVk->QueryInterface(IID_RenderDevice, reinterpret_cast<IObject**>(ppDevice) );

        RefCntAutoPtr<DeviceContextVkImpl> pImmediateCtxVk( NEW_RC_OBJ(RawMemAllocator, "DeviceContextVkImpl instance", DeviceContextVkImpl)(pRenderDeviceVk, false, EngineAttribs, 0) );
        // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceVk will
        // keep a weak reference to the context
        pImmediateCtxVk->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppContexts) );
        pRenderDeviceVk->SetImmediateContext(pImmediateCtxVk);

        for (Uint32 DeferredCtx = 0; DeferredCtx < NumDeferredContexts; ++DeferredCtx)
        {
            RefCntAutoPtr<DeviceContextVkImpl> pDeferredCtxVk( NEW_RC_OBJ(RawMemAllocator, "DeviceContextVkImpl instance", DeviceContextVkImpl)(pRenderDeviceVk, true, EngineAttribs, 1+DeferredCtx) );
            // We must call AddRef() (implicitly through QueryInterface()) because pRenderDeviceVk will
            // keep a weak reference to the context
            pDeferredCtxVk->QueryInterface(IID_DeviceContext, reinterpret_cast<IObject**>(ppContexts + 1 + DeferredCtx) );
            pRenderDeviceVk->SetDeferredContext(DeferredCtx, pDeferredCtxVk);
        }
    }
    catch( const std::runtime_error & )
    {
        if( *ppDevice )
        {
            (*ppDevice)->Release();
            *ppDevice = nullptr;
        }
        for(Uint32 ctx=0; ctx < 1 + NumDeferredContexts; ++ctx)
        {
            if( ppContexts[ctx] != nullptr )
            {
                ppContexts[ctx]->Release();
                ppContexts[ctx] = nullptr;
            }
        }

        LOG_ERROR( "Failed to create device and contexts" );
    }
}
#endif


/// Creates a swap chain for Direct3D12-based engine implementation

/// \param [in] pDevice - Pointer to the render device
/// \param [in] pImmediateContext - Pointer to the immediate device context
/// \param [in] SCDesc - Swap chain description
/// \param [in] pNativeWndHandle - Platform-specific native handle of the window 
///                                the swap chain will be associated with:
///                                * On Win32 platform, this should be window handle (HWND)
///                                * On Universal Windows Platform, this should be reference to the 
///                                  core window (Windows::UI::Core::CoreWindow)
///                                
/// \param [out] ppSwapChain    - Address of the memory location where pointer to the new 
///                               swap chain will be written
void EngineFactoryVkImpl::CreateSwapChainVk( IRenderDevice *pDevice, 
                                                   IDeviceContext *pImmediateContext, 
                                                   const SwapChainDesc& SCDesc, 
                                                   void* pNativeWndHandle, 
                                                   ISwapChain **ppSwapChain )
{
    VERIFY( ppSwapChain, "Null pointer provided" );
    if( !ppSwapChain )
        return;

    *ppSwapChain = nullptr;

#if 0
    try
    {
        auto *pDeviceVk = ValidatedCast<RenderDeviceVkImpl>( pDevice );
        auto *pDeviceContextVk = ValidatedCast<DeviceContextVkImpl>(pImmediateContext);
        auto &RawMemAllocator = GetRawAllocator();
        auto *pSwapChainVk = NEW_RC_OBJ(RawMemAllocator, "SwapChainVkImpl instance", SwapChainVkImpl)(SCDesc, pDeviceVk, pDeviceContextVk, pNativeWndHandle);
        pSwapChainVk->QueryInterface( IID_SwapChain, reinterpret_cast<IObject**>(ppSwapChain) );

        pDeviceContextVk->SetSwapChain(pSwapChainVk);
        // Bind default render target
        pDeviceContextVk->SetRenderTargets( 0, nullptr, nullptr );
        // Set default viewport
        pDeviceContextVk->SetViewports( 1, nullptr, 0, 0 );
        
        auto NumDeferredCtx = pDeviceVk->GetNumDeferredContexts();
        for (size_t ctx = 0; ctx < NumDeferredCtx; ++ctx)
        {
            if (auto pDeferredCtx = pDeviceVk->GetDeferredContext(ctx))
            {
                auto *pDeferredCtxVk = ValidatedCast<DeviceContextVkImpl>(pDeferredCtx.RawPtr());
                pDeferredCtxVk->SetSwapChain(pSwapChainVk);
                // We cannot bind default render target here because
                // there is no guarantee that deferred context will be used
                // in this frame. It is an error to bind 
                // RTV of an inactive buffer in the swap chain
            }
        }
    }
    catch( const std::runtime_error & )
    {
        if( *ppSwapChain )
        {
            (*ppSwapChain)->Release();
            *ppSwapChain = nullptr;
        }

        LOG_ERROR( "Failed to create the swap chain" );
    }
#endif
}


#ifdef DOXYGEN
/// Loads Direct3D12-based engine implementation and exports factory functions
/// \param [out] GetFactoryFunc - Pointer to the function that returns factory for Vk engine implementation.
///                               See EngineFactoryVkImpl.
/// \remarks Depending on the configuration and platform, the function loads different dll:
/// Platform\\Configuration   |           Debug               |        Release
/// --------------------------|-------------------------------|----------------------------
///         x86               | GraphicsEngineVk_32d.dll   |    GraphicsEngineVk_32r.dll
///         x64               | GraphicsEngineVk_64d.dll   |    GraphicsEngineVk_64r.dll
///
void LoadGraphicsEngineVk(GetEngineFactoryVkType &GetFactoryFunc)
{
    // This function is only required because DoxyGen refuses to generate documentation for a static function when SHOW_FILES==NO
    #error This function must never be compiled;    
}
#endif


IEngineFactoryVk* GetEngineFactoryVk()
{
    return EngineFactoryVkImpl::GetInstance();
}

}
