/*     Copyright 2015-2019 Egor Yusov
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

#include "pch.h"
#include "SwapChainVkImpl.h"
#include "RenderDeviceVkImpl.h"
#include "DeviceContextVkImpl.h"
#include "VulkanTypeConversions.h"
#include "TextureVkImpl.h"
#include "EngineMemory.h"

namespace Diligent
{

SwapChainVkImpl::SwapChainVkImpl(IReferenceCounters*    pRefCounters,
                                 const SwapChainDesc&   SCDesc, 
                                 RenderDeviceVkImpl*    pRenderDeviceVk, 
                                 DeviceContextVkImpl*   pDeviceContextVk, 
                                 void*                  pNativeWndHandle) : 
    TSwapChainBase(pRefCounters, pRenderDeviceVk, pDeviceContextVk, SCDesc),
    m_VulkanInstance(pRenderDeviceVk->GetVulkanInstance()),
    m_pBackBufferRTV(STD_ALLOCATOR_RAW_MEM(RefCntAutoPtr<ITextureView>, GetRawAllocator(), "Allocator for vector<RefCntAutoPtr<ITextureView>>")),
    m_SwapChainImagesInitialized(STD_ALLOCATOR_RAW_MEM(bool, GetRawAllocator(), "Allocator for vector<bool>"))
{
    // Create OS-specific surface
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.hinstance = GetModuleHandle(NULL);
    surfaceCreateInfo.hwnd = (HWND)pNativeWndHandle;
    auto err = vkCreateWin32SurfaceKHR(m_VulkanInstance->GetVkInstance(), &surfaceCreateInfo, nullptr, &m_VkSurface);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = (ANativeWindow*) pNativeWndHandle;
    auto err = vkCreateAndroidSurfaceKHR(m_VulkanInstance->GetVkInstance(), &surfaceCreateInfo, NULL, &m_VkSurface);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
    VkIOSSurfaceCreateInfoMVK surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
    surfaceCreateInfo.pView = pNativeWndHandle;
    auto err = vkCreateIOSSurfaceMVK(m_VulkanInstance->GetVkInstance(), &surfaceCreateInfo, nullptr, &m_VkSurface);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
    VkMacOSSurfaceCreateInfoMVK surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    surfaceCreateInfo.pView = pNativeWndHandle;
    auto err = vkCreateMacOSSurfaceMVK(m_VulkanInstance->GetVkInstance(), &surfaceCreateInfo, NULL, &m_VkSurface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    VkWaylandSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.display = display;
    surfaceCreatem_VkSurface = window;
    err = vkCreateWaylandSurfaceKHR(m_VulkanInstance->GetVkInstance(), &surfaceCreateInfo, nullptr, &m_VkSurface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    struct XCBInfo
    {
        xcb_connection_t* connection = nullptr;
        uint32_t window = 0;
    };
    XCBInfo& info = *reinterpret_cast<XCBInfo*>(pNativeWndHandle);
    surfaceCreateInfo.connection = info.connection;
    surfaceCreateInfo.window = info.window;
    auto err = vkCreateXcbSurfaceKHR(m_VulkanInstance->GetVkInstance(), &surfaceCreateInfo, nullptr, &m_VkSurface);
#endif

    CHECK_VK_ERROR_AND_THROW(err, "Failed to create OS-specific surface");
    const auto& PhysicalDevice = pRenderDeviceVk->GetPhysicalDevice();
    auto& CmdQueueVK = pRenderDeviceVk->GetCommandQueue(0);
    auto QueueFamilyIndex = CmdQueueVK.GetQueueFamilyIndex();
    if( !PhysicalDevice.CheckPresentSupport(QueueFamilyIndex, m_VkSurface) )
    {
        LOG_ERROR_AND_THROW("Selected physical device does not support present capability.\n"
                            "There could be few ways to mitigate this problem. One is to try to find another queue that supports present, but does not support graphics and compute capabilities."
                            "Another way is to find another physical device that exposes queue family that supports present and graphics capability. Neither apporach is currently implemented in Diligent Engine.");
    }

    CreateVulkanSwapChain();
    InitBuffersAndViews();
    auto res = AcquireNextImage(pDeviceContextVk);
    DEV_CHECK_ERR(res == VK_SUCCESS, "Failed to acquire next image for the newly created swap chain"); (void)res;
}

void SwapChainVkImpl::CreateVulkanSwapChain()
{
    auto *pRenderDeviceVk = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();
    const auto& PhysicalDevice = pRenderDeviceVk->GetPhysicalDevice();
    auto vkDeviceHandle = PhysicalDevice.GetVkDeviceHandle();
    // Get the list of VkFormats that are supported:
    uint32_t formatCount = 0;
    auto err = vkGetPhysicalDeviceSurfaceFormatsKHR(vkDeviceHandle, m_VkSurface, &formatCount, NULL);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to query number of supported formats");
    VERIFY_EXPR(formatCount > 0);
    std::vector<VkSurfaceFormatKHR> SupportedFormats(formatCount);
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(vkDeviceHandle, m_VkSurface, &formatCount, SupportedFormats.data());
    CHECK_VK_ERROR_AND_THROW(err, "Failed to query supported format properties");
    VERIFY_EXPR(formatCount == SupportedFormats.size());
    m_VkColorFormat = TexFormatToVkFormat(m_SwapChainDesc.ColorBufferFormat);
    VkColorSpaceKHR ColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    if (formatCount == 1 && SupportedFormats[0].format == VK_FORMAT_UNDEFINED) 
    {
        // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
        // the surface has no preferred format.  Otherwise, at least one
        // supported format will be returned.

        // Do nothing
    }
    else 
    {
        bool FmtFound = false;
        for(const auto& SrfFmt : SupportedFormats)
        {
            if(SrfFmt.format == m_VkColorFormat)
            {
                FmtFound = true;
                ColorSpace = SrfFmt.colorSpace;
                break;
            }
        }
        if(!FmtFound)
        {
            VkFormat VkReplacementColorFormat = VK_FORMAT_UNDEFINED;
            switch(m_VkColorFormat)
            {
                case VK_FORMAT_R8G8B8A8_UNORM: VkReplacementColorFormat = VK_FORMAT_B8G8R8A8_UNORM; break;
                case VK_FORMAT_B8G8R8A8_UNORM: VkReplacementColorFormat = VK_FORMAT_R8G8B8A8_UNORM; break;
                case VK_FORMAT_B8G8R8A8_SRGB: VkReplacementColorFormat = VK_FORMAT_R8G8B8A8_SRGB; break;
                case VK_FORMAT_R8G8B8A8_SRGB: VkReplacementColorFormat = VK_FORMAT_B8G8R8A8_SRGB; break;
                default: VkReplacementColorFormat = VK_FORMAT_UNDEFINED;
            }

            bool ReplacementFmtFound = false;
            for (const auto& SrfFmt : SupportedFormats)
            {
                if (SrfFmt.format == VkReplacementColorFormat)
                {
                    ReplacementFmtFound = true;
                    ColorSpace = SrfFmt.colorSpace;
                    break;
                }
            }

            if(ReplacementFmtFound)
            {
                m_VkColorFormat = VkReplacementColorFormat;
                auto NewColorBufferFormat = VkFormatToTexFormat(VkReplacementColorFormat);
                LOG_INFO_MESSAGE("Requested color buffer format ", GetTextureFormatAttribs(m_SwapChainDesc.ColorBufferFormat).Name, " is not supported by the surace and will be replaced with ", GetTextureFormatAttribs(NewColorBufferFormat).Name);
                m_SwapChainDesc.ColorBufferFormat = NewColorBufferFormat;
            }
            else
            {
                LOG_WARNING_MESSAGE("Requested color buffer format ", GetTextureFormatAttribs(m_SwapChainDesc.ColorBufferFormat).Name ,"is not supported by the surace");
            }
        }
    }
    
    VkSurfaceCapabilitiesKHR surfCapabilities = {};
    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkDeviceHandle, m_VkSurface, &surfCapabilities);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to query physical device surface capabilities");

    uint32_t presentModeCount = 0;
    err = vkGetPhysicalDeviceSurfacePresentModesKHR(vkDeviceHandle, m_VkSurface, &presentModeCount, NULL);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to query surface present mode count");
    VERIFY_EXPR(presentModeCount > 0);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    err = vkGetPhysicalDeviceSurfacePresentModesKHR(vkDeviceHandle, m_VkSurface, &presentModeCount, presentModes.data());
    CHECK_VK_ERROR_AND_THROW(err, "Failed to query surface present modes");
    VERIFY_EXPR(presentModeCount == presentModes.size());

    VkExtent2D swapchainExtent = {};
    // width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
    if (surfCapabilities.currentExtent.width == 0xFFFFFFFF && m_SwapChainDesc.Width != 0 && m_SwapChainDesc.Height != 0)
    {
        // If the surface size is undefined, the size is set to
        // the size of the images requested.
        swapchainExtent.width  = std::min(std::max(m_SwapChainDesc.Width,  surfCapabilities.minImageExtent.width),  surfCapabilities.maxImageExtent.width);
        swapchainExtent.height = std::min(std::max(m_SwapChainDesc.Height, surfCapabilities.minImageExtent.height), surfCapabilities.maxImageExtent.height);
    }
    else 
    {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = surfCapabilities.currentExtent;
    }
    swapchainExtent.width  = std::max(swapchainExtent.width,  1u);
    swapchainExtent.height = std::max(swapchainExtent.height, 1u);
    m_SwapChainDesc.Width  = swapchainExtent.width;
    m_SwapChainDesc.Height = swapchainExtent.height;

    // Mailbox is the lowest latency non-tearing presentation mode
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    bool PresentModeSupported = std::find(presentModes.begin(), presentModes.end(), swapchainPresentMode) != presentModes.end();
    if(!PresentModeSupported)
    {
        swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        // The FIFO present mode is guaranteed by the spec to be supported
        VERIFY(std::find(presentModes.begin(), presentModes.end(), swapchainPresentMode) != presentModes.end(), "FIFO present mode must be supported" );
    }

    // Determine the number of VkImage's to use in the swap chain.
    // We need to acquire only 1 presentable image at at time.
    // Asking for minImageCount images ensures that we can acquire
    // 1 presentable image as long as we present it before attempting
    // to acquire another.
    if(m_SwapChainDesc.BufferCount < surfCapabilities.minImageCount)
    {
        LOG_INFO_MESSAGE("Requested back buffer count (", m_SwapChainDesc.BufferCount, ") is smaller than the minimal image count supported for this surface (", surfCapabilities.minImageCount, "). Resetting to ", surfCapabilities.minImageCount);
        m_SwapChainDesc.BufferCount = surfCapabilities.minImageCount;
    }
    if (surfCapabilities.maxImageCount != 0 && m_SwapChainDesc.BufferCount > surfCapabilities.maxImageCount)
    {
        LOG_INFO_MESSAGE("Requested back buffer count (", m_SwapChainDesc.BufferCount, ") is greater than the maximal image count supported for this surface (", surfCapabilities.maxImageCount, "). Resetting to ", surfCapabilities.maxImageCount);
        m_SwapChainDesc.BufferCount = surfCapabilities.maxImageCount;
    }
    uint32_t desiredNumberOfSwapChainImages = m_SwapChainDesc.BufferCount;

    VkSurfaceTransformFlagBitsKHR preTransform = 
        (surfCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? 
            VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : 
            surfCapabilities.currentTransform;

    // Find a supported composite alpha mode - one of these is guaranteed to be set
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[4] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (uint32_t i = 0; i < _countof(compositeAlphaFlags); i++) {
        if (surfCapabilities.supportedCompositeAlpha & compositeAlphaFlags[i]) {
            compositeAlpha = compositeAlphaFlags[i];
            break;
        }
    }

    auto oldSwapchain = m_VkSwapChain;
    m_VkSwapChain = VK_NULL_HANDLE;
    VkSwapchainCreateInfoKHR swapchain_ci = {};
    swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_ci.pNext = NULL;
    swapchain_ci.surface = m_VkSurface;
    swapchain_ci.minImageCount = desiredNumberOfSwapChainImages;
    swapchain_ci.imageFormat = m_VkColorFormat;
    swapchain_ci.imageExtent.width = swapchainExtent.width;
    swapchain_ci.imageExtent.height = swapchainExtent.height;
    swapchain_ci.preTransform = preTransform;
    swapchain_ci.compositeAlpha = compositeAlpha;
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.presentMode = swapchainPresentMode;
    swapchain_ci.oldSwapchain = oldSwapchain;
    swapchain_ci.clipped = VK_TRUE;
    swapchain_ci.imageColorSpace = ColorSpace;
    swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // vkCmdClearColorImage() command requires the image to use VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL layout
    // that requires  VK_IMAGE_USAGE_TRANSFER_DST_BIT to be set
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_ci.queueFamilyIndexCount = 0;
    swapchain_ci.pQueueFamilyIndices = NULL;
    //uint32_t queueFamilyIndices[] = { (uint32_t)info.graphics_queue_family_index, (uint32_t)info.present_queue_family_index };
    //if (info.graphics_queue_family_index != info.present_queue_family_index) {
    //    // If the graphics and present queues are from different queue families,
    //    // we either have to explicitly transfer ownership of images between
    //    // the queues, or we have to create the swapchain with imageSharingMode
    //    // as VK_SHARING_MODE_CONCURRENT
    //    swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    //    swapchain_ci.queueFamilyIndexCount = 2;
    //    swapchain_ci.pQueueFamilyIndices = queueFamilyIndices;
    //}

    const auto& LogicalDevice = pRenderDeviceVk->GetLogicalDevice();
    auto vkDevice = pRenderDeviceVk->GetVkDevice();
    err = vkCreateSwapchainKHR(vkDevice, &swapchain_ci, NULL, &m_VkSwapChain);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to create Vulkan swapchain");

    if(oldSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(vkDevice, oldSwapchain, NULL);
    }

    uint32_t swapchainImageCount = 0;
    err = vkGetSwapchainImagesKHR(vkDevice, m_VkSwapChain, &swapchainImageCount, NULL);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to request swap chain image count");
    VERIFY_EXPR(swapchainImageCount > 0);
    if (swapchainImageCount != m_SwapChainDesc.BufferCount)
    {
        LOG_INFO_MESSAGE("Actual number of images in the created swap chain: ", m_SwapChainDesc.BufferCount);
        m_SwapChainDesc.BufferCount = swapchainImageCount;
    }

    m_ImageAcquiredSemaphores.resize(swapchainImageCount);
    m_DrawCompleteSemaphores.resize(swapchainImageCount);
    for(uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        VkSemaphoreCreateInfo SemaphoreCI = {};
        SemaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        SemaphoreCI.pNext = nullptr;
        SemaphoreCI.flags = 0; // reserved for future use
        m_ImageAcquiredSemaphores[i] = LogicalDevice.CreateSemaphore(SemaphoreCI);
        m_DrawCompleteSemaphores[i] = LogicalDevice.CreateSemaphore(SemaphoreCI);
    }
}

SwapChainVkImpl::~SwapChainVkImpl()
{
    if (m_VkSwapChain != VK_NULL_HANDLE)
    {
        auto *pDeviceVkImpl = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();
        pDeviceVkImpl->IdleGPU();
        vkDestroySwapchainKHR(pDeviceVkImpl->GetVkDevice(), m_VkSwapChain, NULL);
    }
    if (m_VkSurface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_VulkanInstance->GetVkInstance(), m_VkSurface, NULL);
    }
}

void SwapChainVkImpl::InitBuffersAndViews()
{
    auto *pDeviceVkImpl = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();
    auto LogicalVkDevice = pDeviceVkImpl->GetVkDevice();

#ifdef _DEBUG
    {
        uint32_t swapchainImageCount = 0;
        auto err = vkGetSwapchainImagesKHR(LogicalVkDevice, m_VkSwapChain, &swapchainImageCount, NULL);
        VERIFY_EXPR(err == VK_SUCCESS);
        VERIFY(swapchainImageCount == m_SwapChainDesc.BufferCount, "Unexpected swap chain buffer count");
    }
#endif

    m_pBackBufferRTV.resize(m_SwapChainDesc.BufferCount);
    m_SwapChainImagesInitialized.resize(m_pBackBufferRTV.size(), false);

    uint32_t swapchainImageCount = m_SwapChainDesc.BufferCount;
    std::vector<VkImage> swapchainImages(swapchainImageCount);
    auto err = vkGetSwapchainImagesKHR(LogicalVkDevice, m_VkSwapChain, &swapchainImageCount, swapchainImages.data());
    CHECK_VK_ERROR_AND_THROW(err, "Failed to get swap chain images");
    VERIFY_EXPR(swapchainImageCount == swapchainImages.size());

    for (uint32_t i = 0; i < swapchainImageCount; i++) 
    {
        TextureDesc BackBufferDesc;
        BackBufferDesc.Format = m_SwapChainDesc.ColorBufferFormat;
        std::stringstream name_ss;
        name_ss << "Main back buffer " << i;
        auto name = name_ss.str();
        BackBufferDesc.Name = name.c_str();
        BackBufferDesc.Type = RESOURCE_DIM_TEX_2D;
        BackBufferDesc.Width  = m_SwapChainDesc.Width;
        BackBufferDesc.Height = m_SwapChainDesc.Height;
        BackBufferDesc.Format = m_SwapChainDesc.ColorBufferFormat;
        BackBufferDesc.BindFlags = BIND_RENDER_TARGET;
        BackBufferDesc.MipLevels = 1;

        RefCntAutoPtr<TextureVkImpl> pBackBufferTex;
        m_pRenderDevice.RawPtr<RenderDeviceVkImpl>()->CreateTexture(BackBufferDesc, swapchainImages[i], RESOURCE_STATE_UNDEFINED, &pBackBufferTex);
        
        TextureViewDesc RTVDesc;
        RTVDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
        RefCntAutoPtr<ITextureView> pRTV;
        pBackBufferTex->CreateView(RTVDesc, &pRTV);
        m_pBackBufferRTV[i] = RefCntAutoPtr<ITextureViewVk>(pRTV, IID_TextureViewVk);
    }

    TextureDesc DepthBufferDesc;
    DepthBufferDesc.Type = RESOURCE_DIM_TEX_2D;
    DepthBufferDesc.Width = m_SwapChainDesc.Width;
    DepthBufferDesc.Height = m_SwapChainDesc.Height;
    DepthBufferDesc.Format = m_SwapChainDesc.DepthBufferFormat;
    DepthBufferDesc.SampleCount = m_SwapChainDesc.SamplesCount;
    DepthBufferDesc.Usage = USAGE_DEFAULT;
    DepthBufferDesc.BindFlags = BIND_DEPTH_STENCIL;

    DepthBufferDesc.ClearValue.Format = DepthBufferDesc.Format;
    DepthBufferDesc.ClearValue.DepthStencil.Depth = m_SwapChainDesc.DefaultDepthValue;
    DepthBufferDesc.ClearValue.DepthStencil.Stencil = m_SwapChainDesc.DefaultStencilValue;
    DepthBufferDesc.Name = "Main depth buffer";
    RefCntAutoPtr<ITexture> pDepthBufferTex;
    m_pRenderDevice->CreateTexture(DepthBufferDesc, nullptr, static_cast<ITexture**>(&pDepthBufferTex) );
    auto pDSV = pDepthBufferTex->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    m_pDepthBufferDSV = RefCntAutoPtr<ITextureViewVk>(pDSV, IID_TextureViewVk);
}

VkResult SwapChainVkImpl::AcquireNextImage(DeviceContextVkImpl* pDeviceCtxVk)
{
    auto* pDeviceVk = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();
    const auto& LogicalDevice = pDeviceVk->GetLogicalDevice();

    auto res = vkAcquireNextImageKHR(LogicalDevice.GetVkDevice(), m_VkSwapChain, UINT64_MAX, m_ImageAcquiredSemaphores[m_SemaphoreIndex], (VkFence)nullptr, &m_BackBufferIndex);
    if (res == VK_SUCCESS)
    {
        // Next command in the device context must wait for the next image to be acquired
        // Unlike fences or events, the act of waiting for a semaphore also unsignals that semaphore (6.4.2)
        pDeviceCtxVk->AddWaitSemaphore(m_ImageAcquiredSemaphores[m_SemaphoreIndex], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        if (!m_SwapChainImagesInitialized[m_BackBufferIndex])
        {
            // Vulkan validation layers do not like uninitialized memory.
            // Clear back buffer first time we acquire it. This will use vkCmdClearColorImage()
            pDeviceCtxVk->ClearRenderTarget(GetCurrentBackBufferRTV(), nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_SwapChainImagesInitialized[m_BackBufferIndex] = true;
        }
    }
    
    return res;
}

IMPLEMENT_QUERY_INTERFACE( SwapChainVkImpl, IID_SwapChainVk, TSwapChainBase )


void SwapChainVkImpl::Present(Uint32 SyncInterval)
{
    auto pDeviceContext = m_wpDeviceContext.Lock();
    if( !pDeviceContext )
    {
        LOG_ERROR_MESSAGE( "Immediate context has been released" );
        return;
    }

    auto* pImmediateCtxVk = pDeviceContext.RawPtr<DeviceContextVkImpl>();
    auto* pDeviceVk = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();

    if (!m_IsMinimized)
    {
        // TransitionImageLayout() never triggers flush
        pImmediateCtxVk->TransitionImageLayout(GetCurrentBackBufferRTV()->GetTexture(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        // The context can be empty if no render commands were issued by the app
        //VERIFY(pImmediateCtxVk->GetNumCommandsInCtx() != 0, "The context must not be flushed");
        pImmediateCtxVk->AddSignalSemaphore(m_DrawCompleteSemaphores[m_SemaphoreIndex]);
    }

    pImmediateCtxVk->Flush();
    // If present fails, default FB will be undbound by RecreateVulkanSwapchain(), so we need to check it now
    bool IsDefaultFBBound = pImmediateCtxVk->IsDefaultFBBound();

    if (!m_IsMinimized)
    {
        VkPresentInfoKHR PresentInfo = {};
        PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        PresentInfo.pNext = nullptr;
        PresentInfo.waitSemaphoreCount = 1;
        // Unlike fences or events, the act of waiting for a semaphore also unsignals that semaphore (6.4.2)
        VkSemaphore WaitSemaphore[] = { m_DrawCompleteSemaphores[m_SemaphoreIndex] };
        PresentInfo.pWaitSemaphores = WaitSemaphore;
        PresentInfo.swapchainCount = 1;
        PresentInfo.pSwapchains = &m_VkSwapChain;
        PresentInfo.pImageIndices = &m_BackBufferIndex;
        VkResult Result = VK_SUCCESS;
        PresentInfo.pResults = &Result;
        pDeviceVk->LockCommandQueue(0, 
            [&PresentInfo](ICommandQueueVk* pCmdQueueVk)
            {
                pCmdQueueVk->Present(PresentInfo);
            }
        );

        if (Result == VK_SUBOPTIMAL_KHR || Result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateVulkanSwapchain(pImmediateCtxVk);
            m_SemaphoreIndex = m_SwapChainDesc.BufferCount-1; // To start with 0 index when acquire next image
        }
        else
        {
            DEV_CHECK_ERR(Result == VK_SUCCESS, "Present failed");
        }
    }

    pImmediateCtxVk->FinishFrame();
    pDeviceVk->ReleaseStaleResources();

    if (!m_IsMinimized)
    {
        ++m_SemaphoreIndex;
        if (m_SemaphoreIndex >= m_SwapChainDesc.BufferCount)
            m_SemaphoreIndex = 0;

        auto res = AcquireNextImage(pImmediateCtxVk);
        if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateVulkanSwapchain(pImmediateCtxVk);
            m_SemaphoreIndex = m_SwapChainDesc.BufferCount-1; // To start with 0 index when acquire next image
            res = AcquireNextImage(pImmediateCtxVk);
        }
        DEV_CHECK_ERR(res == VK_SUCCESS, "Failed to acquire next swap chain image");

        if (IsDefaultFBBound)
        {
            // If default framebuffer is bound, we need to call SetRenderTargets()
            // to bind new back buffer RTV
            pImmediateCtxVk->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    }
}

void SwapChainVkImpl::RecreateVulkanSwapchain(DeviceContextVkImpl* pImmediateCtxVk)
{
    if (pImmediateCtxVk->IsDefaultFBBound())
        pImmediateCtxVk->ResetRenderTargets();

    // All references to the swap chain must be released before it can be resized
    m_pBackBufferRTV.clear();
    m_SwapChainImagesInitialized.clear();
    m_pDepthBufferDSV.Release();

    RenderDeviceVkImpl* pDeviceVk = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();
    // This will release references to Vk swap chain buffers hold by
    // m_pBackBufferRTV[]
    pDeviceVk->IdleGPU();

    // We must wait unitl GPU is idled before destroying semaphores as they
    // are destroyed immediately
    m_ImageAcquiredSemaphores.clear();
    m_DrawCompleteSemaphores.clear();
    m_SemaphoreIndex = 0;

    CreateVulkanSwapChain();
    InitBuffersAndViews();
}

void SwapChainVkImpl::Resize( Uint32 NewWidth, Uint32 NewHeight )
{
    if( TSwapChainBase::Resize(NewWidth, NewHeight) )
    {
        auto pDeviceContext = m_wpDeviceContext.Lock();
        VERIFY( pDeviceContext, "Immediate context has been released" );
        if( pDeviceContext )
        {
            pDeviceContext->Flush();

            try
            {
            	auto* pImmediateCtxVk = pDeviceContext.RawPtr<DeviceContextVkImpl>();
                // RecreateVulkanSwapchain() unbinds default FB
                bool bIsDefaultFBBound = pImmediateCtxVk->IsDefaultFBBound();
                RecreateVulkanSwapchain(pImmediateCtxVk);

                auto res = AcquireNextImage(pImmediateCtxVk);
                DEV_CHECK_ERR(res == VK_SUCCESS, "Failed to acquire next image for the just resized swap chain"); (void)res;

                if( bIsDefaultFBBound )
                {
                    // Set default render target and viewport
                    pDeviceContext->SetRenderTargets( 0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                    pDeviceContext->SetViewports( 1, nullptr, 0, 0 );
                }
            }
            catch( const std::runtime_error & )
            {
                LOG_ERROR( "Failed to resize the swap chain" );
            }
        }
    }

    m_IsMinimized = (NewWidth == 0 && NewHeight == 0);
}


void SwapChainVkImpl::SetFullscreenMode(const DisplayModeAttribs &DisplayMode)
{
}

void SwapChainVkImpl::SetWindowedMode()
{
}

}
