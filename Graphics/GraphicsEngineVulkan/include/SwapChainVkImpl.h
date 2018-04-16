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

#pragma once

/// \file
/// Declaration of Diligent::SwapChainVkImpl class

#include "SwapChainVk.h"
#include "SwapChainBase.h"
#include "VulkanUtilities/VulkanInstance.h"

namespace Diligent
{

class ITextureViewVk;
class IMemoryAllocator;
/// Implementation of the Diligent::ISwapChainVk interface
class SwapChainVkImpl : public SwapChainBase<ISwapChainVk>
{
public:
    typedef SwapChainBase<ISwapChainVk> TSwapChainBase;
    SwapChainVkImpl(IReferenceCounters *pRefCounters,
                       const SwapChainDesc& SwapChainDesc, 
                       class RenderDeviceVkImpl* pRenderDeviceVk,
                       class DeviceContextVkImpl* pDeviceContextVk,
                       void* pNativeWndHandle);
    ~SwapChainVkImpl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface );

    virtual void Present(Uint32 SyncInterval)override final;
    virtual void Resize( Uint32 NewWidth, Uint32 NewHeight )override final;

    virtual void SetFullscreenMode(const DisplayModeAttribs &DisplayMode)override final;

    virtual void SetWindowedMode()override final;

/*
    virtual IDXGISwapChain* GetDXGISwapChain()override final{ return m_pSwapChain; }
    virtual ITextureViewVk* GetCurrentBackBufferRTV()override final;
    virtual ITextureViewVk* GetDepthBufferDSV()override final{return m_pDepthBufferDSV;}
    */
private:
    void CreateVulkanSwapChain();
    void InitBuffersAndViews();

    std::shared_ptr<const VulkanUtilities::VulkanInstance> m_VulkanInstance;
    VkSurfaceKHR m_VkSurface = VK_NULL_HANDLE;
    VkSwapchainKHR m_VkSwapChain = VK_NULL_HANDLE;
    VkFormat m_VkColorFormat = VK_FORMAT_UNDEFINED;

    std::vector< RefCntAutoPtr<ITextureViewVk>, STDAllocatorRawMem<RefCntAutoPtr<ITextureViewVk>> > m_pBackBufferRTV;
    RefCntAutoPtr<ITextureViewVk> m_pDepthBufferDSV;

};

}
