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
/// Defines Diligent::IReferenceCounters interface

#include "InterfaceID.h"
#include "Atomics.h"

namespace Diligent
{

/// Base interface for a reference counter object that stores the number of strong and 
/// weak references and the pointer to the object. It is necessary to separate reference 
/// counters from the object to support weak pointers.
class IReferenceCounters
{
public:
    /// Increments the number of strong references by 1.

    /// \return The number of strong references after incrementing the counter.
    /// \remark The method is thread-safe and does not require explicit synchronization.
    /// \note   In a multithreaded environment, the returned number may not be reliable
    ///         as other threads may simultaneously change the actual value of the counter.
    virtual Atomics::Long AddStrongRef() = 0;


    /// Decrements the number of strong references by 1 and destroys the referenced object 
    /// when the counter reaches zero. If there are no more weak references, destroys the
    /// reference counters object itself.

    /// \return The number of strong references after decrementing the counter.
    /// \remark The referenced object is destroyed when the last strong reference is released.\n
    ///         If there are no more weak references, the reference counters object itself is
    ///         also destroyed.\n
    ///         The method is thread-safe and does not require explicit synchronization.
    /// \note   In a multithreaded environment, the returned number may not be reliable
    ///         as other threads may simultaneously change the actual value of the counter.
    ///         The only reliable value is 0 as the object is destroyed when the last 
    ///         strong reference is released.
    virtual Atomics::Long ReleaseStrongRef() = 0;


    /// Increments the number of weak references by 1.

    /// \return The number of weak references after incrementing the counter.
    /// \remark The method is thread-safe and does not require explicit synchronization.
    /// \note   In a multithreaded environment, the returned number may not be reliable
    ///         as other threads may simultaneously change the actual value of the counter.
    virtual Atomics::Long AddWeakRef() = 0;


    /// Decrements the number of weak references by 1. If there are no more strong and weak
    /// references, destroys the reference counters object itself.

    /// \return The number of weak references after decrementing the counter.
    /// \remark The method is thread-safe and does not require explicit synchronization.
    /// \note   In a multithreaded environment, the returned number may not be reliable
    ///         as other threads may simultaneously change the actual value of the counter.
    virtual Atomics::Long ReleaseWeakRef() = 0;


    /// Gets the pointer to the IUnknown interface of the referenced object.

    /// \param [out] ppObject - Memory address where the pointer to the object 
    ///                         will be stored.
    /// \remark If the object was destroyed, nullptr will be written to *ppObject. 
    ///         If the object was not released, the pointer to the object's IUnknown interface 
    ///         will be stored. In this case, the number of strong references to the object 
    ///         will be incremented by 1.\n
    ///         The method is thread-safe and does not require explicit synchronization.
    virtual void GetObject(class IObject **ppObject) = 0;


    /// Returns the number of outstanding strong references.

    /// \return The number of strong references.
    /// \note   In a multithreaded environment, the returned number may not be reliable
    ///         as other threads may simultaneously change the actual value of the counter.
    ///         The only reliable value is 0 as the object is destroyed when the last 
    ///         strong reference is released.
    virtual Atomics::Long GetNumStrongRefs()const = 0;


    /// Returns the number of outstanding weak references.

    /// \return The number of weak references.
    /// \note   In a multithreaded environment, the returned number may not be reliable
    ///         as other threads may simultaneously change the actual value of the counter.
    virtual Atomics::Long GetNumWeakRefs()const = 0;
};

}
