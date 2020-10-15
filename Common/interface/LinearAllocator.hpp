/*
 *  Copyright 2019-2020 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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
/// Defines Diligent::LinearAllocator class

#include "../../Primitives/interface/BasicTypes.h"
#include "../../Primitives/interface/MemoryAllocator.h"
#include "../../Platforms/Basic/interface/DebugUtilities.hpp"
#include "Align.hpp"

namespace Diligent
{

/// Implementation of a linear allocator on a fixed-size memory page
class LinearAllocator
{
public:
    // clang-format off
    LinearAllocator           (const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;
    LinearAllocator& operator=(LinearAllocator&&)      = delete;
    // clang-format on

    LinearAllocator(IMemoryAllocator& Allocator) :
        m_pAllocator{&Allocator}
    {}

    LinearAllocator(LinearAllocator&& Other) :
        // clang-format off
        m_pBuffer     {Other.m_pBuffer     },
        m_pCurrPtr    {Other.m_pCurrPtr    },
        m_RequiredSize{Other.m_RequiredSize},
        m_pAllocator  {Other.m_pAllocator  }
    // clang-format on
    {
        Other.m_pBuffer      = nullptr;
        Other.m_pCurrPtr     = nullptr;
        Other.m_RequiredSize = 0;
        Other.m_pAllocator   = nullptr;
    }

    ~LinearAllocator()
    {
        Free();
    }

    void Free()
    {
        if (m_pBuffer != nullptr && m_pAllocator != nullptr)
        {
            m_pAllocator->Free(m_pBuffer);
        }

        m_pBuffer      = nullptr;
        m_pCurrPtr     = nullptr;
        m_RequiredSize = 0;
        m_pAllocator   = nullptr;
    }

    void* Release()
    {
        void* Ptr      = m_pBuffer;
        m_pBuffer      = nullptr;
        m_pCurrPtr     = nullptr;
        m_RequiredSize = 0;
        return Ptr;
    }

    void AddRequiredSize(size_t size, size_t align)
    {
        VERIFY(m_pBuffer == nullptr, "Memory already allocated");
        if (size > 0)
        {
            m_RequiredSize = Align(m_RequiredSize, align) + size;

            // Reserve additional space for pointer alignment
            m_RequiredSize += (align > sizeof(void*) ? align : 0);
        }
    }

    template <typename T>
    void AddRequiredSize(size_t count)
    {
        AddRequiredSize(sizeof(T) * count, alignof(T));
    }

    void Reserve(size_t size)
    {
        VERIFY(m_RequiredSize == 0, "Required size will be overrided by input argument");
        m_RequiredSize = size;
        Reserve();
    }

    void Reserve()
    {
        VERIFY(m_pBuffer == nullptr, "Memory already allocated");
        VERIFY(m_pAllocator != nullptr, "Allocator must not be null");
        if (m_RequiredSize > 0)
        {
            m_pBuffer  = reinterpret_cast<uint8_t*>(m_pAllocator->Allocate(m_RequiredSize, "Memory for linear allocator", __FILE__, __LINE__));
            m_pCurrPtr = m_pBuffer;
        }
    }

    void* Allocate(size_t size, size_t align)
    {
        if (size == 0)
            return nullptr;

        uint8_t* Ptr = reinterpret_cast<uint8_t*>(Align(reinterpret_cast<size_t>(m_pCurrPtr), align));
        m_pCurrPtr   = Ptr + size;
        VERIFY(m_pCurrPtr <= m_pBuffer + m_RequiredSize, "Not enough space in the buffer");
        return Ptr;
    }

    template <typename T>
    T* Allocate(size_t count)
    {
        return reinterpret_cast<T*>(Allocate(sizeof(T) * count, alignof(T)));
    }

    template <typename T, typename... Args>
    T* Construct(Args&&... args)
    {
        T* Ptr = Allocate<T>(1);
        new (Ptr) T{std::forward<Args>(args)...};
        return Ptr;
    }

    template <typename T, typename... Args>
    T* ConstructArray(size_t count, Args&&... args)
    {
        T* Ptr = Allocate<T>(count);
        for (size_t i = 0; i < count; ++i)
        {
            new (Ptr + i) T{std::forward<Args>(args)...};
        }
        return Ptr;
    }

    template <typename T>
    T* CopyArray(const T* Src, size_t count)
    {
        T* Dst     = reinterpret_cast<T*>(Align(reinterpret_cast<size_t>(m_pCurrPtr), alignof(T)));
        m_pCurrPtr = reinterpret_cast<uint8_t*>(Dst + count);
        VERIFY(m_pCurrPtr <= m_pBuffer + m_RequiredSize, "Not enough space in the buffer");

        for (size_t i = 0; i < count; ++i)
        {
            new (Dst + i) T{Src[i]};
        }
        return Dst;
    }

    Char* CopyString(const char* Str)
    {
        if (Str == nullptr)
            return nullptr;

        Char* Ptr = reinterpret_cast<Char*>(m_pCurrPtr);
        Char* Dst = Ptr;
        while (*Str != 0 && Dst < reinterpret_cast<Char*>(m_pBuffer + m_RequiredSize))
        {
            *(Dst++) = *(Str++);
        }
        if (Dst < reinterpret_cast<Char*>(m_pBuffer + m_RequiredSize))
            *(Dst++) = 0;
        else
            UNEXPECTED("Not enough space reserved in the string pool");
        m_pCurrPtr = reinterpret_cast<uint8_t*>(Dst);
        return Ptr;
    }

private:
    uint8_t*          m_pBuffer      = nullptr;
    uint8_t*          m_pCurrPtr     = nullptr;
    size_t            m_RequiredSize = 0;
    IMemoryAllocator* m_pAllocator   = nullptr;
};

} // namespace Diligent
