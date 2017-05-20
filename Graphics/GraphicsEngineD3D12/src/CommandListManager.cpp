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

#include "pch.h"
#include "CommandListManager.h"
#include "RenderDeviceD3D12Impl.h"

namespace Diligent
{

CommandListManager::CommandListManager(RenderDeviceD3D12Impl *pDeviceD3D12) : 
    m_pDeviceD3D12(pDeviceD3D12),
    m_DiscardedAllocators(STD_ALLOCATOR_RAW_MEM(DiscardedAllocatorQueueElemType, GetRawAllocator(), "Allocator for deque<DiscardedAllocatorQueueElemType>"))
{
}

CommandListManager::~CommandListManager()
{
}


void CommandListManager::CreateNewCommandList( ID3D12GraphicsCommandList** List, ID3D12CommandAllocator** Allocator )
{
	RequestAllocator(Allocator);
    auto *pd3d12Device = m_pDeviceD3D12->GetD3D12Device();
	auto hr = pd3d12Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, *Allocator, nullptr, __uuidof(*List), reinterpret_cast<void**>(List) );
    VERIFY(SUCCEEDED(hr), "Failed to create command list");
	(*List)->SetName(L"CommandList");
}


void CommandListManager::RequestAllocator(ID3D12CommandAllocator** ppAllocator)
{
	std::lock_guard<std::mutex> LockGuard(m_AllocatorMutex);

    VERIFY( (*ppAllocator) == nullptr, "Allocator pointer is not null" );
    (*ppAllocator) = nullptr;

    if (!m_DiscardedAllocators.empty())
	{
        // Pick the oldest allocator at the front of the deque
        // If this allocator is not yet available, there is no sense in
        // looking at the rest of allocators
		auto& AllocatorPair = m_DiscardedAllocators.front();
        
        // Get the NUMBER OF COMPLETED cmd lists
        auto NumCompeltedCmdLists = m_pDeviceD3D12->GetNumCompletedCmdLists();
        // Note that NumCompeltedCmdLists only grows. So if after we queried
        // the value, the actual value is increased in other thread, this will not
        // be an issue as the only consequence is that the allocator that is 
        // potentially available may not be used.

        // AllocatorPair.first is the NUMBER of the command list.
        // If command list number is N, it can be reused when
        // NumCompeltedCmdLists is at least N+1
        // (To reuse allocator used to create CmdList 0, NumCompeltedCmdLists must be at least 1)
		if ( AllocatorPair.first < NumCompeltedCmdLists )
		{
			*ppAllocator = AllocatorPair.second.Detach();
			auto hr = (*ppAllocator)->Reset();
            VERIFY_EXPR(SUCCEEDED(hr));
			m_DiscardedAllocators.pop_front();
		}
	}

	// If no allocators were ready to be reused, create a new one
	if ((*ppAllocator) == nullptr)
	{
        auto *pd3d12Device = m_pDeviceD3D12->GetD3D12Device();
		auto hr = pd3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(*ppAllocator), reinterpret_cast<void**>(ppAllocator));
        VERIFY(SUCCEEDED(hr), "Failed to create command allocator")
		//wchar_t AllocatorName[32];
		//swprintf(AllocatorName, 32, L"CommandAllocator %zu", m_AllocatorPool.size());
		//(*ppAllocator)->SetName(AllocatorName);
	}
}

void CommandListManager::DiscardAllocator( Uint64 CmdListNum, ID3D12CommandAllocator* pAllocator )
{
	std::lock_guard<std::mutex> LockGuard(m_AllocatorMutex);

    // CmdListNum is the number of the command list that was created by this allocator.
    // This value is used to understand when it is safe to reuse the allocator
	m_DiscardedAllocators.emplace_back( CmdListNum, CComPtr<ID3D12CommandAllocator>(pAllocator) );
}

}
