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
/// Declaration of Diligent::BufferVkImpl class

#include "BufferVk.h"
#include "RenderDeviceVk.h"
#include "BufferBase.h"
#include "BufferViewVkImpl.h"
#include "DynamicUploadHeap.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Implementation of the Diligent::IBufferVk interface
class BufferVkImpl : public BufferBase<IBufferVk, BufferViewVkImpl, FixedBlockMemoryAllocator>
{
public:
    typedef BufferBase<IBufferVk, BufferViewVkImpl, FixedBlockMemoryAllocator> TBufferBase;
    BufferVkImpl(IReferenceCounters *pRefCounters, 
                 FixedBlockMemoryAllocator &BuffViewObjMemAllocator, 
                 class RenderDeviceVkImpl *pDeviceVk, 
                 const BufferDesc& BuffDesc, 
                 const BufferData &BuffData = BufferData());
    BufferVkImpl(IReferenceCounters *pRefCounters, 
                 FixedBlockMemoryAllocator &BuffViewObjMemAllocator, 
                 class RenderDeviceVkImpl *pDeviceVk, 
                 const BufferDesc& BuffDesc, 
                 void *pVkBuffer);
    ~BufferVkImpl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;

    virtual void UpdateData( IDeviceContext *pContext, Uint32 Offset, Uint32 Size, const PVoid pData )override;
    virtual void CopyData( IDeviceContext *pContext, IBuffer *pSrcBuffer, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size )override;
    virtual void Map( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData )override;
    virtual void Unmap( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags )override;

#ifdef _DEBUG
    void DbgVerifyDynamicAllocation(Uint32 ContextId);
#endif

    /*virtual IVkResource *GetVkBuffer(size_t &DataStartByteOffset, Uint32 ContextId)override final
    { 
        auto *pVkResource = GetVkResource();
        if(pVkResource != nullptr)
        {
            VERIFY(m_Desc.Usage != USAGE_DYNAMIC || (m_Desc.BindFlags | (BIND_SHADER_RESOURCE|BIND_UNORDERED_ACCESS)) != 0, "Expected non-dynamic buffer or a buffer with SRV or UAV bind flags");
            DataStartByteOffset = 0;
            return pVkResource; 
        }
        else
        {
            VERIFY(m_Desc.Usage == USAGE_DYNAMIC, "Dynamic buffer is expected");

#ifdef _DEBUG
            DbgVerifyDynamicAllocation(ContextId);
#endif
            DataStartByteOffset = m_DynamicData[ContextId].Offset;
            return m_DynamicData[ContextId].pBuffer;
        }
    }
    */

    VkBuffer GetVkBuffer()override final
    {
        return m_VulkanBuffer;
    }

    virtual void* GetNativeHandle()override final
    { 
        //VERIFY(GetVkResource() != nullptr, "The buffer is dynamic and has no pointer to Vk resource");
        //size_t DataStartByteOffset = 0;
        auto VkBuffer = GetVkBuffer(); 
        //VERIFY(DataStartByteOffset == 0, "0 offset expected");
        return VkBuffer;
    }

    virtual void SetAccessFlags(VkAccessFlags AccessFlags )override final{ m_AccessFlags = AccessFlags; }
    VkAccessFlags GetAccessFlags()const{return m_AccessFlags;}

/*
    Vk_GPU_VIRTUAL_ADDRESS GetGPUAddress(Uint32 ContextId)
    {
        if(m_Desc.Usage == USAGE_DYNAMIC)
        {
#ifdef _DEBUG
            DbgVerifyDynamicAllocation(ContextId);
#endif
            return m_DynamicData[ContextId].GPUAddress;
        }
        else
        {
            return GetVkResource()->GetGPUVirtualAddress();
        }
    }

    Vk_CPU_DESCRIPTOR_HANDLE GetCBVHandle(){return m_CBVDescriptorAllocation.GetCpuHandle();}
    */
private:
    virtual void CreateViewInternal( const struct BufferViewDesc &ViewDesc, IBufferView **ppView, bool bIsDefaultView )override;

    VulkanUtilities::BufferViewWrapper CreateView(struct BufferViewDesc &ViewDesc);
    VkAccessFlags m_AccessFlags = 0;

    /*   
#ifdef _DEBUG
    std::vector< std::pair<MAP_TYPE, Uint32>, STDAllocatorRawMem<std::pair<MAP_TYPE, Uint32>> > m_DbgMapType;
#endif

    friend class DeviceContextVkImpl;
    // Array of dynamic allocations for every device context
    std::vector<DynamicAllocation,  STDAllocatorRawMem<DynamicAllocation> > m_DynamicData;*/
    
    VulkanUtilities::BufferWrapper m_VulkanBuffer;
    VulkanUtilities::DeviceMemoryWrapper m_BufferMemory;
};

}
