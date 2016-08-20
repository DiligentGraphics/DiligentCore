/*     Copyright 2015-2016 Egor Yusov
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
/// Declaration of Diligent::BufferViewD3D12Impl class

#include "BufferViewD3D12.h"
#include "RenderDeviceD3D12.h"
#include "BufferViewBase.h"
#include "DescriptorHeap.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;
/// Implementation of the Diligent::IBufferViewD3D12 interface
class BufferViewD3D12Impl : public BufferViewBase<IBufferViewD3D12, FixedBlockMemoryAllocator>
{
public:
    typedef BufferViewBase<IBufferViewD3D12, FixedBlockMemoryAllocator> TBufferViewBase;

    BufferViewD3D12Impl( FixedBlockMemoryAllocator &BuffViewAllocator,
                         IRenderDevice *pDevice, 
                         const BufferViewDesc& ViewDesc, 
                         class IBuffer *pBuffer,
                         DescriptorHeapAllocation &&HandleAlloc,
                         bool bIsDefaultView);

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface );

    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle()override{return m_DescriptorHandle.GetCpuHandle();}
   
protected:
    DescriptorHeapAllocation m_DescriptorHandle;
};

}
