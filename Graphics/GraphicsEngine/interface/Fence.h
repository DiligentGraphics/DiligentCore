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
/// Defines Diligent::IFence interface and related data structures

#include "DeviceObject.h"

namespace Diligent
{

// {3B19184D-32AB-4701-84F4-9A0C03AE1672}
static constexpr INTERFACE_ID IID_Fence =
{ 0x3b19184d, 0x32ab, 0x4701, { 0x84, 0xf4, 0x9a, 0xc, 0x3, 0xae, 0x16, 0x72 } };

/// Buffer description
struct FenceDesc : DeviceObjectAttribs
{

};

/// Fence interface

/// Fence the methods to manipulate a fence object
class IFence : public IDeviceObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface)override = 0;

    /// Returns the fence description used to create the object
    virtual const FenceDesc& GetDesc()const = 0;

    /// Returns the last completed value signaled by the GPU
    virtual Uint64 GetCompletedValue() = 0;

    /// Resets the fence to the specified value. 
    virtual void Reset(Uint64 Value) = 0;
};

}
