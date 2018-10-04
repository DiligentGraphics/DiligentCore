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

#include <vector>
#include <mutex>
#include <stdint.h>

namespace Diligent
{

class RenderDeviceD3D12Impl;

class CommandListManager
{
public:
	CommandListManager(RenderDeviceD3D12Impl& DeviceD3D12Impl);
	~CommandListManager();

    CommandListManager             (const CommandListManager&)  = delete;
    CommandListManager             (      CommandListManager&&) = delete;
    CommandListManager& operator = (const CommandListManager&)  = delete;
    CommandListManager& operator = (      CommandListManager&&) = delete;

    void CreateNewCommandList( ID3D12GraphicsCommandList** ppList, ID3D12CommandAllocator** ppAllocator );
    
    void RequestAllocator(ID3D12CommandAllocator** ppAllocator);
    void ReleaseAllocator(CComPtr<ID3D12CommandAllocator>&& Allocator, Uint32 CmdQueue, Uint64 FenceValue);

    // Returns allocator to the list of available allocators. The GPU must have finished using the
    // allocator
    void FreeAllocator( CComPtr<ID3D12CommandAllocator>&& Allocator );

#ifdef DEVELOPMENT
    Atomics::Long GetAllocatorCounter() const {return m_AllocatorCounter;}
#endif

private:
    std::mutex m_AllocatorMutex;
	std::vector< CComPtr<ID3D12CommandAllocator>, STDAllocatorRawMem<CComPtr<ID3D12CommandAllocator>> > m_FreeAllocators;
	
    RenderDeviceD3D12Impl& m_DeviceD3D12Impl;

    Atomics::AtomicLong m_NumAllocators = 0; // For debug purposes only

#ifdef DEVELOPMENT
    Atomics::AtomicLong m_AllocatorCounter = 0;
#endif
};

}
