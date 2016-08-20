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
/// Defines Diligent::IDataBlob interface

#include "Object.h"

namespace Diligent
{

// {F578FF0D-ABD2-4514-9D32-7CB454D4A73B}
static const Diligent::INTERFACE_ID IID_DataBlob = 
{ 0xf578ff0d, 0xabd2, 0x4514, { 0x9d, 0x32, 0x7c, 0xb4, 0x54, 0xd4, 0xa7, 0x3b } };

/// Base interface for a file stream
class IDataBlob : public Diligent::IObject
{
public:
    
    /// Sets the size of the internal data buffer
    virtual void Resize( size_t NewSize ) = 0;

    /// Returns the size of the internal data buffer
    virtual size_t GetSize() = 0;

    /// Returns the pointer to the internal data buffer
    virtual void* GetDataPtr() = 0;
};

}
