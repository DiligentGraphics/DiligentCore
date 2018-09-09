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
/// Defines Diligent::DefaultRawMemoryAllocator class
#include <limits>

#include "../../Primitives/interface/BasicTypes.h"
#include "../../Primitives/interface/MemoryAllocator.h"
#include "../../Platforms/Basic/interface/DebugUtilities.h"

namespace Diligent
{

template <typename T, typename AllocatorType>
struct STDAllocator
{
    typedef T value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    STDAllocator(AllocatorType& Allocator, const Char* Description, const Char* FileName, const  Int32 LineNumber)noexcept : 
        m_Allocator     (Allocator)
#ifdef DEVELOPMENT
      , m_dvpDescription(Description)
      , m_dvpFileName   (FileName)
      , m_dvpLineNumber (LineNumber)
#endif
    {
    }

    template <class U> 
    STDAllocator(const STDAllocator<U, AllocatorType>& other)noexcept : 
        m_Allocator     (other.m_Allocator)
#ifdef DEVELOPMENT
      , m_dvpDescription(other.m_dvpDescription)
      , m_dvpFileName   (other.m_dvpFileName)
      , m_dvpLineNumber (other.m_dvpLineNumber)
#endif
    {
    }

    template <class U> 
    STDAllocator(STDAllocator<U, AllocatorType>&& other)noexcept : 
        m_Allocator     (other.m_Allocator)
#ifdef DEVELOPMENT
      , m_dvpDescription(other.m_dvpDescription)
      , m_dvpFileName   (other.m_dvpFileName)
      , m_dvpLineNumber (other.m_dvpLineNumber)
#endif
    {
    }

    template <class U> 
    STDAllocator& operator = (STDAllocator<U, AllocatorType>&& other)noexcept
    {
        // Android build requires this operator to be defined - I have no idea why
        VERIFY_EXPR(&m_Allocator == &other.m_Allocator);
#ifdef DEVELOPMENT
        m_dvpDescription = other.m_dvpDescription;
        m_dvpFileName    = other.m_dvpFileName;
        m_dvpLineNumber  = other.m_dvpLineNumber;
#endif
        return *this;
    }

    template< class U > struct rebind
    { 
        typedef STDAllocator<U, AllocatorType> other;
    };

    T* allocate(std::size_t count)
    {
#ifndef DEVELOPMENT
        static constexpr char* m_dvpDescription = "<Unavailable in release build>";
        static constexpr char* m_dvpFileName    = "<Unavailable in release build>";
        static constexpr Int32 m_dvpLineNumber  = -1;
#endif
        return reinterpret_cast<T*>( m_Allocator.Allocate(count * sizeof(T), m_dvpDescription, m_dvpFileName, m_dvpLineNumber ) );
    }

    void deallocate(T* p, std::size_t count)
    {
        m_Allocator.Free(p);
    }

    inline size_type max_size() const 
    { 
        return std::numeric_limits<size_type>::max() / sizeof(T); 
    }

    //    construction/destruction
    template< class U, class... Args >
    void construct( U* p, Args&&... args )
    { 
        ::new(p) U(std::forward<Args>(args)...); 
    }

    inline void destroy(pointer p)
    { 
        p->~T(); 
    }

    AllocatorType &m_Allocator;
#ifdef DEVELOPMENT
    const Char* const m_dvpDescription;
    const Char* const m_dvpFileName;
    Int32       const m_dvpLineNumber;
#endif
};

#define STD_ALLOCATOR(Type, AllocatorType, Allocator, Description) STDAllocator<Type, AllocatorType>(Allocator, Description, __FILE__, __LINE__)

template <class T, class U, class A>
bool operator==(const STDAllocator<T, A>&left, const STDAllocator<U, A>&right)
{
    return &left.m_Allocator == &right.m_Allocator;
}

template <class T, class U, class A>
bool operator!=(const STDAllocator<T, A> &left, const STDAllocator<U, A> &right)
{
    return !(left == right);
}

template<class T> using STDAllocatorRawMem = STDAllocator<T, IMemoryAllocator>; 
#define STD_ALLOCATOR_RAW_MEM(Type, Allocator, Description) STDAllocatorRawMem<Type>(Allocator, Description, __FILE__, __LINE__)

template< class T, typename AllocatorType > 
struct STDDeleter
{
    STDDeleter(AllocatorType &Allocator) : 
        m_Allocator(Allocator)
    {}
        
    void operator()(T *ptr)
    {
        ptr->~T();
        m_Allocator.Free(ptr);
    }

    AllocatorType &m_Allocator;
};
template<class T> using STDDeleterRawMem = STDDeleter<T, IMemoryAllocator>; 

}
