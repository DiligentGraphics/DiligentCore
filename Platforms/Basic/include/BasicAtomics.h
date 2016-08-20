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

#include <atomic>

struct BasicAtomics
{
    typedef long Long;
    typedef std::atomic<Long> AtomicLong;
    typedef std::atomic<int64_t> AtomicInt64;

    // The function returns the resulting INCREMENTED value.
    template<typename Type>
    static inline Type AtomicIncrement(std::atomic<Type> &Val)
    {
        return ++Val;
    }

    // The function returns the resulting DECREMENTED value.
    template<typename Type>
    static inline Type AtomicDecrement(std::atomic<Type> &Val)
    {
        return --Val;
    }
    
    // The function compares the Destination value with the Comparand value. If the Destination value is equal 
    // to the Comparand value, the Exchange value is stored in the address specified by Destination. 
    // Otherwise, no operation is performed.
    // The function returns the initial value of the Destination parameter
    template<typename Type>
    static inline Type AtomicCompareExchange( std::atomic<Type> &Destination, Type Exchange, Type Comparand)
    {
        Destination.compare_exchange_strong(Comparand, Exchange);
        return Comparand;
    }

    template<typename Type>
    static inline Type AtomicAdd( std::atomic<Type> &Destination, Type Val)
    {
        return std::atomic_fetch_add(&Destination, Val);
    }

};
