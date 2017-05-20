/*     Copyright 2015-2017 Egor Yusov
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
/// Declaration of Diligent::BufferD3D12Impl class

#include "BufferD3D12.h"
#include "RenderDeviceD3D12.h"
#include "BufferBase.h"
#include "BufferViewD3D12Impl.h"
#include "D3D12ResourceBase.h"
#include "DynamicUploadHeap.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Implementation of the Diligent::IBufferD3D12 interface
class BufferD3D12Impl : public BufferBase<IBufferD3D12, BufferViewD3D12Impl, FixedBlockMemoryAllocator, FixedBlockMemoryAllocator>, public D3D12ResourceBase
{
public:
    typedef BufferBase<IBufferD3D12, BufferViewD3D12Impl, FixedBlockMemoryAllocator, FixedBlockMemoryAllocator> TBufferBase;
    BufferD3D12Impl(FixedBlockMemoryAllocator &BufferObjMemAllocator, 
                    FixedBlockMemoryAllocator &BuffViewObjMemAllocator, 
                    class RenderDeviceD3D12Impl *pDeviceD3D12, 
                    const BufferDesc& BuffDesc, 
                    const BufferData &BuffData = BufferData());
    ~BufferD3D12Impl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;

    virtual void UpdateData( IDeviceContext *pContext, Uint32 Offset, Uint32 Size, const PVoid pData )override;
    virtual void CopyData( IDeviceContext *pContext, IBuffer *pSrcBuffer, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size )override;
    virtual void Map( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData )override;
    virtual void Unmap( IDeviceContext *pContext, MAP_TYPE MapType )override;

#ifdef _DEBUG
    void DbgVerifyDynamicAllocation(Uint32 ContextId);
#endif

    virtual ID3D12Resource *GetD3D12Buffer(size_t &DataStartByteOffset, Uint32 ContextId)override final
    { 
        if(m_Desc.Usage == USAGE_DYNAMIC)
        {
#ifdef _DEBUG
            DbgVerifyDynamicAllocation(ContextId);
#endif
            DataStartByteOffset = m_DynamicData[ContextId].Offset;
            return m_DynamicData[ContextId].pBuffer;
        }
        else
        {
            DataStartByteOffset = 0;
            return GetD3D12Resource(); 
        }
    }
    
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress(Uint32 ContextId)
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
            return GetD3D12Resource()->GetGPUVirtualAddress();
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCBVHandle(){return m_CBVDescriptorAllocation.GetCpuHandle();}

private:
    virtual void CreateViewInternal( const struct BufferViewDesc &ViewDesc, IBufferView **ppView, bool bIsDefaultView )override;

    void CreateUAV( struct BufferViewDesc &UAVDesc, D3D12_CPU_DESCRIPTOR_HANDLE UAVDescriptor );
    void CreateSRV( struct BufferViewDesc &SRVDesc, D3D12_CPU_DESCRIPTOR_HANDLE SRVDescriptor );
    void CreateCBV( D3D12_CPU_DESCRIPTOR_HANDLE CBVDescriptor );
    DescriptorHeapAllocation m_CBVDescriptorAllocation;

#ifdef _DEBUG
    std::vector<MAP_TYPE, STDAllocatorRawMem<MAP_TYPE> > m_DbgMapType;
#endif

    friend class DeviceContextD3D12Impl;
    // Array of dynamic allocations for every device context
    std::vector<DynamicAllocation,  STDAllocatorRawMem<DynamicAllocation> > m_DynamicData;
};

}
