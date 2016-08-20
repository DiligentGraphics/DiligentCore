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
/// Defines Diligent::IObject interface

#include "InterfaceID.h"
#include "Atomics.h"
#include "ReferenceCounters.h"

namespace Diligent
{

/// Base interface for all dynamic objects in the engine
class IObject
{
public:
    /// Queries the specific interface. 

    /// \param [in] IID - Unique identifier of the requested interface.
    /// \param [out] ppInterface - Memory address where the pointer to the requested interface will be written.
    ///                            If the interface is not supported, null pointer will be returned.
    /// \remark The method increments the number of strong references by 1. The interface must be 
    ///         released by a call to Release() method when it is no longer needed.
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface ) = 0;


    /// Increments the number of strong references by 1.

    /// \remark This method is equivalent to GetReferenceCounters()->AddStrongRef().\n
    ///         The method is thread-safe and does not require explicit synchronization.
    /// \return The number of strong references after incrementing the counter.
    /// \note   In a multithreaded environment, the returned number may not be reliable
    ///         as other threads may simultaneously change the actual value of the counter.
    virtual Atomics::Long AddRef() = 0;


    /// Decrements the number of strong references by 1 and destroys the object when the 
    /// counter reaches zero.

    /// \remark This method is equivalent to GetReferenceCounters()->ReleaseStrongRef().\n
    ///         The method is thread-safe and does not require explicit synchronization.
    /// \return The number of strong references after decrementing the counter.
    /// \note   In a multithreaded environment, the returned number may not be reliable
    ///         as other threads may simultaneously change the actual value of the counter.
    ///         The only reliable value is 0 as the object is destroyed when the last 
    ///         strong reference is released.
    virtual Atomics::Long Release() = 0;


    /// Returns the pointer to IReferenceCounters interface of the associated 
    /// reference counters object. The metod does *NOT* increment
    /// the number of strong references to the returned object.
    virtual IReferenceCounters* GetReferenceCounters()const = 0;
};

}
