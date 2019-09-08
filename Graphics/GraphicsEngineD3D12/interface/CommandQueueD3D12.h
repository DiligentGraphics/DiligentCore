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

#pragma once

/// \file
/// Definition of the Diligent::ICommandQueueD3D12 interface

namespace Diligent
{

// {D89693CE-F3F4-44B5-B7EF-24115AAD085E}
static constexpr INTERFACE_ID IID_CommandQueueD3D12 =
{ 0xd89693ce, 0xf3f4, 0x44b5, { 0xb7, 0xef, 0x24, 0x11, 0x5a, 0xad, 0x8, 0x5e } };

/// Command queue interface
class ICommandQueueD3D12 : public Diligent::IObject
{
public:
	/// Returns the fence value that will be signaled next time
	virtual Uint64 GetNextFenceValue() = 0;

	/// Executes a given command list

    /// \return Fence value associated with the executed command list
	virtual Uint64 Submit(ID3D12GraphicsCommandList* commandList) = 0;

    /// Returns D3D12 command queue. May return null if queue is anavailable
    virtual ID3D12CommandQueue* GetD3D12CommandQueue() = 0;

    /// Returns value of the last completed fence
    virtual Uint64 GetCompletedFenceValue() = 0;

    /// Blocks execution until all pending GPU commands are complete
    virtual Uint64 WaitForIdle() = 0;

    /// Signals the given fence
    virtual void SignalFence(ID3D12Fence* pFence, Uint64 Value) = 0;

    /// Waits until the specified fence reaches or exceeds the specified value.
    virtual void Wait(ID3D12Fence* pFence, Uint64 Value) = 0;
};

}
