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

/// \file
/// Implementation for the IDataBlob interface

#include "DataBlob.h"
#include "BasicTypes.h"
#include "ObjectBase.h"
#include <vector>

namespace Diligent
{
    
/// Base interface for a file stream
class StringDataBlobImpl : public Diligent::ObjectBase<IDataBlob>
{
public:
    typedef Diligent::ObjectBase<IDataBlob> TBase;

    StringDataBlobImpl( IReferenceCounters* pRefCounters, const String &str ) : TBase(pRefCounters), m_String(str) {}
    StringDataBlobImpl( IReferenceCounters* pRefCounters, String &&str ) : TBase(pRefCounters), m_String( std::move(str) ) {}

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_DataBlob, TBase )

    /// Sets the size of the internal data buffer
    virtual void Resize(size_t NewSize)override
    {
        m_String.resize(NewSize);
    }

    /// Returns the size of the internal data buffer
    virtual size_t GetSize()override
    {
        return m_String.length();
    }

    /// Returns the pointer to the internal data buffer
    virtual void* GetDataPtr()override
    {
        return &m_String[0];
    }

private:
    String m_String;
};

}
