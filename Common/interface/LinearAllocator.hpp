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

#include <vector>

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

    explicit LinearAllocator(IMemoryAllocator& Allocator) :
        m_pAllocator{&Allocator}
    {}

    LinearAllocator(LinearAllocator&& Other) :
        // clang-format off
        m_pDataStart {Other.m_pDataStart},
        m_pCurrPtr   {Other.m_pCurrPtr  },
        m_pDataEnd   {Other.m_pDataEnd  },
        m_pAllocator {Other.m_pAllocator}
    // clang-format on
    {
        Other.m_pDataStart = nullptr;
        Other.m_pCurrPtr   = nullptr;
        Other.m_pDataEnd   = nullptr;
        Other.m_pAllocator = nullptr;
    }

    ~LinearAllocator()
    {
        Free();
    }

    void Free()
    {
        if (m_pDataStart != nullptr && m_pDataStart != GetDummyMemory() && m_pAllocator != nullptr)
        {
            m_pAllocator->Free(m_pDataStart);
        }

        m_pDataStart = nullptr;
        m_pCurrPtr   = nullptr;
        m_pDataEnd   = nullptr;
        m_pAllocator = nullptr;
    }

    void* Release()
    {
        void* Ptr    = m_pDataStart;
        m_pDataStart = nullptr;
        m_pCurrPtr   = nullptr;
        m_pDataEnd   = nullptr;
        m_pAllocator = nullptr;
        return Ptr;
    }

    void AddSpace(size_t size, size_t align)
    {
        VERIFY(m_pDataStart == nullptr || m_pDataStart == GetDummyMemory(), "Memory has already been allocated");
        AllocateInternal(size, align);
    }

    template <typename T>
    void AddSpace(size_t count = 1)
    {
        AddSpace(sizeof(T) * count, alignof(T));
    }

    void AddSpaceForString(const Char* str)
    {
        VERIFY_EXPR(str != nullptr);
        AddSpace(strlen(str) + 1, 1);
    }

    void AddSpaceForString(const String& str)
    {
        AddSpaceForString(str.c_str());
    }

    void Reserve(size_t size)
    {
        VERIFY(m_pDataStart == nullptr || m_pDataStart == GetDummyMemory(), "Memory has already been allocated");
        VERIFY(m_pCurrPtr == nullptr, "Space has been added to the allocator and will be overriden");
        m_pCurrPtr = m_pDataStart + size;
        Reserve();
    }

    void Reserve()
    {
        VERIFY(m_pDataStart == nullptr || m_pDataStart == GetDummyMemory(), "Memory has already been allocated");
        VERIFY(m_pAllocator != nullptr, "Allocator must not be null");
        // Make sure the data size is at least sizeof(void*)-aligned
        auto DataSize = Align(static_cast<size_t>(m_pCurrPtr - m_pDataStart), sizeof(void*));
        if (DataSize > 0)
        {
            m_pDataStart = reinterpret_cast<uint8_t*>(m_pAllocator->Allocate(DataSize, "Raw memory for linear allocator", __FILE__, __LINE__));
            VERIFY(m_pDataStart == Align(m_pDataStart, sizeof(void*)), "Memory pointer must be at least sizeof(void*)-aligned");

            m_pCurrPtr = m_pDataStart;
            m_pDataEnd = m_pDataStart + DataSize;
        }
    }

    void* Allocate(size_t size, size_t align)
    {
        if (size == 0)
            return nullptr;

        VERIFY(m_pDataStart != nullptr && m_pDataStart != GetDummyMemory(), "Memory has not been allocated");
        return AllocateInternal(size, align);
    }

    template <typename T>
    T* Allocate(size_t count = 1)
    {
        return reinterpret_cast<T*>(Allocate(sizeof(T) * count, alignof(T)));
    }

    template <typename T, typename... Args>
    T* Construct(Args&&... args)
    {
        T* Ptr = Allocate<T>();
        new (Ptr) T{std::forward<Args>(args)...};
        return Ptr;
    }

    template <typename T, typename... Args>
    T* ConstructArray(size_t count, const Args&... args)
    {
        T* Ptr = Allocate<T>(count);
        for (size_t i = 0; i < count; ++i)
        {
            new (Ptr + i) T{args...};
        }
        return Ptr;
    }

    template <typename T>
    T* Copy(const T& Src)
    {
        return Construct<T>(Src);
    }

    template <typename T>
    T* CopyArray(const T* Src, size_t count)
    {
        T* Dst = Allocate<T>(count);
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

        auto* Ptr = reinterpret_cast<Char*>(AllocateInternal(strlen(Str) + 1, 1));
        Char* Dst = Ptr;
        while (*Str != 0 && Dst < reinterpret_cast<Char*>(m_pDataEnd))
        {
            *(Dst++) = *(Str++);
        }
        if (Dst < reinterpret_cast<Char*>(m_pDataEnd))
            *(Dst++) = 0;
        else
            UNEXPECTED("Not enough space reserved for the string");
        VERIFY_EXPR(reinterpret_cast<Char*>(m_pCurrPtr) == Dst);
        return Ptr;
    }

    Char* CopyString(const std::string& Str)
    {
        return CopyString(Str.c_str());
    }

    size_t GetCurrentSize() const
    {
        return static_cast<size_t>(m_pCurrPtr - m_pDataStart);
    }

    size_t GetReservedSize() const
    {
        return static_cast<size_t>(m_pDataEnd - m_pDataStart);
    }

private:
    void* AllocateInternal(size_t size, size_t align)
    {
        VERIFY(IsPowerOfTwo(align), "Alignment is not a power of two!");
        if (size == 0)
            return m_pCurrPtr;

        if (m_pCurrPtr == nullptr)
        {
            VERIFY_EXPR(m_pDataStart == nullptr);
            m_pDataStart = m_pCurrPtr = GetDummyMemory();
        }

        m_pCurrPtr = Align(m_pCurrPtr, align);
        auto* ptr  = m_pCurrPtr;

#if DILIGENT_DEBUG
        if (m_pDataStart == GetDummyMemory())
        {
            m_DbgAllocations.emplace_back(size, align, m_pCurrPtr - m_pDataStart);
        }
        else
        {
            VERIFY(m_DbgCurrAllocation < m_DbgAllocations.size(), "Allocation number exceed the number of allocations that were originally reserved.");
            const auto& CurrAllocation = m_DbgAllocations[m_DbgCurrAllocation++];
            VERIFY(CurrAllocation.size == size, "Allocation size (", size, ") does not match the initially requested size (", CurrAllocation.size, ")");
            VERIFY(CurrAllocation.alignment == align, "Allocation alignment (", align, ") does not match initially requested alignment (", CurrAllocation.alignment, ")");

            auto CurrOffset = m_pCurrPtr - m_pDataStart;
            VERIFY(CurrOffset <= CurrAllocation.offset,
                   "Allocation offset exceed the offset that was initially computed. "
                   "This should never happen as long as the allocated memory is sizeof(void*)-aligned.");
        }
#endif

        m_pCurrPtr += size;

        VERIFY(m_pDataEnd == nullptr || m_pCurrPtr <= m_pDataEnd, "Allocation size exceeds the reserved space");

        return ptr;
    }

    static uint8_t* GetDummyMemory()
    {
        // Simulate that allocated memory is only sizeof(void*)-aligned
        auto* DummyMemory = reinterpret_cast<uint8_t*>(sizeof(void*));
        VERIFY_EXPR(DummyMemory != nullptr);
        return DummyMemory;
    }

    uint8_t*          m_pDataStart = nullptr;
    uint8_t*          m_pCurrPtr   = nullptr;
    uint8_t*          m_pDataEnd   = nullptr;
    IMemoryAllocator* m_pAllocator = nullptr;

#if DILIGENT_DEBUG
    size_t m_DbgCurrAllocation = 0;
    struct DbgAllocationInfo
    {
        const size_t    size;
        const size_t    alignment;
        const ptrdiff_t offset;

        DbgAllocationInfo(size_t _size, size_t _alignment, ptrdiff_t _offset) :
            size{_size},
            alignment{_alignment},
            offset{_offset}
        {
        }
    };
    std::vector<DbgAllocationInfo> m_DbgAllocations;
#endif
};

} // namespace Diligent
