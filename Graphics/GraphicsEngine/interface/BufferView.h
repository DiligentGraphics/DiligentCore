/*     Copyright 2015 Egor Yusov
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
/// Definition of the Diligent::IBufferView interface and related data structures

#include "DeviceObject.h"

namespace Diligent
{

// {E2E83490-E9D2-495B-9A83-ABB413A38B07}
static const Diligent::INTERFACE_ID IID_BufferView =
{ 0xe2e83490, 0xe9d2, 0x495b, { 0x9a, 0x83, 0xab, 0xb4, 0x13, 0xa3, 0x8b, 0x7 } };

/// Buffer view description
struct BufferViewDesc : DeviceObjectAttribs
{
    /// View type. See Diligent::BUFFER_VIEW_TYPE for details.
    BUFFER_VIEW_TYPE ViewType;

    /// Offset in bytes from the beginnig of the buffer to the start of the
    /// buffer region referenced by the view
    Uint32 ByteOffset;

    /// Size in bytes of the referenced buffer region
    Uint32 ByteWidth;

    /// Initializes the structure members with default values

    /// Default values:
    /// Member              | Default value
    /// --------------------|--------------
    /// ViewType            | BUFFER_VIEW_UNDEFINED
    /// ByteOffset          | 0
    /// ByteWidth           | 0
    BufferViewDesc() :
        ViewType( BUFFER_VIEW_UNDEFINED ),
        ByteOffset(0),
        ByteWidth(0)
    {
    }

    /// Comparison operator tests if two structures are equivalent

    /// \param [in] RHS - reference to the structure to perform comparison with
    /// \return 
    /// - True if all members of the two structures are equal.
    /// - False otherwise
    /// \remarks
    /// The operator ignores DeviceObjectAttribs::Name field.
    bool operator == (const BufferViewDesc& RHS)const
    {
               // Name is primarily used for debug purposes and does not affect the view.
               // It is ignored in comparison operation.
        return //strcmp(Name, RHS.Name) == 0 &&
               ViewType  == RHS.ViewType &&
               ByteOffset== RHS.ByteOffset   &&
               ByteWidth == RHS.ByteWidth;
    }
};

/// Buffer view interface

/// To create a buffer view, call IBuffer::CreateView().
/// \remarks
/// Buffer view holds strong references to the buffer. The buffer
/// will not be destroyed until all views are released.
class IBufferView : public IDeviceObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface ) = 0;

    /// Returns the buffer view description used to create the object
    virtual const BufferViewDesc& GetDesc()const = 0;

    /// Returns pointer to the referenced buffer object.

    /// The method does *NOT* call AddRef() on the returned interface, 
    /// so Release() must not be called.
    virtual IBuffer* GetBuffer() = 0;
};

}
