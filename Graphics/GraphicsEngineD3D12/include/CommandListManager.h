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

//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//
// Adapted to Diligent Engine: Egor Yusov

#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include <stdint.h>

namespace Diligent
{

class CommandListManager
{
public:
	CommandListManager(class RenderDeviceD3D12Impl *pDeviceD3D12);
	~CommandListManager();

    CommandListManager(const CommandListManager&) = delete;
    CommandListManager(CommandListManager&&) = delete;
    CommandListManager& operator = (const CommandListManager&) = delete;
    CommandListManager& operator = (CommandListManager&&) = delete;

    void CreateNewCommandList( ID3D12GraphicsCommandList** ppList, ID3D12CommandAllocator** ppAllocator );
    
    // Discards the allocator.
    // CmdListNum is the serial number of the command list that was created by this allocator
    void DiscardAllocator( Uint64 CmdListNum, ID3D12CommandAllocator* pAllocator );

    void RequestAllocator(ID3D12CommandAllocator** ppAllocator);

private:
    // fist    - the SERIAL NUMBER of command list that was created by the allocator 
    // second  - the allocator to be discarded
    typedef std::pair<Uint64, CComPtr<ID3D12CommandAllocator> > DiscardedAllocatorQueueElemType;
	std::deque< DiscardedAllocatorQueueElemType, STDAllocatorRawMem<DiscardedAllocatorQueueElemType> > m_DiscardedAllocators;

	std::mutex m_AllocatorMutex;
    RenderDeviceD3D12Impl *m_pDeviceD3D12;
};

}