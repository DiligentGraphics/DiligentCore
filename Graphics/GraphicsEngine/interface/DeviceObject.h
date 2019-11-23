/*     Copyright 2019 Diligent Graphics LLC
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
/// Defines Diligent::IDeviceObject interface

#include "../../../Primitives/interface/Object.h"
#include "GraphicsTypes.h"

namespace Diligent
{

// {5B4CCA0B-5075-4230-9759-F48769EE5502}
static constexpr INTERFACE_ID IID_DeviceObject =
    {0x5b4cca0b, 0x5075, 0x4230, {0x97, 0x59, 0xf4, 0x87, 0x69, 0xee, 0x55, 0x2}};

/// Base interface for all objects created by the render device Diligent::IRenderDevice
class IDeviceObject : public IObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override = 0;


    /// Returns the object description
    virtual const DeviceObjectAttribs& GetDesc() const = 0;


    /// Returns unique identifier assigned to an object

    /// \remarks Unique identifiers can be used to reliably check if two objects are identical.
    ///          Note that the engine resuses memory reclaimed after an object has been released.
    ///          For example, if a texture object is released and then another texture is created,
    ///          the engine may return the same pointer, so pointer-to-pointer comparisons are not
    ///          reliable. Unique identifiers, on the other hand, are guaranteed to be, well, unique.
    ///
    ///          Unique identifiers are object-specifics, so, for instance, buffer identifiers
    ///          are not comparable to texture identifiers.
    ///
    ///          Unique identifiers are only meaningful within one session. After an application
    ///          restarts, all identifiers become invalid.
    ///
    ///          Valid identifiers are always positive values. Zero and negative values can never be
    ///          assigned to an object and are always guaranteed to be invalid.
    virtual Int32 GetUniqueID() const = 0;
};

} // namespace Diligent
