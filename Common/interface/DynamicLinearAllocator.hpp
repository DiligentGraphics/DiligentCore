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
/// Defines Diligent::DynamicLinearAllocator class

#include <vector>

#include "../../Primitives/interface/BasicTypes.h"
#include "../../Primitives/interface/MemoryAllocator.h"
#include "../../Platforms/Basic/interface/DebugUtilities.hpp"
#include "Definitions.hpp"
#include "Align.hpp"

namespace Diligent
{

/// Implementation of a linear allocator on a fixed memory pages
class DynamicLinearAllocator
{
public:
    // clang-format off
    DynamicLinearAllocator           (const DynamicLinearAllocator&) = delete;
    DynamicLinearAllocator           (DynamicLinearAllocator&&)      = delete;
    DynamicLinearAllocator& operator=(const DynamicLinearAllocator&) = delete;
    DynamicLinearAllocator& operator=(DynamicLinearAllocator&&)      = delete;
    // clang-format on

    explicit DynamicLinearAllocator(IMemoryAllocator& Allocator, Uint32 BlockSize = 4 << 10) :
        m_pAllocator{&Allocator},
        m_BlockSize{BlockSize}
    {}

    ~DynamicLinearAllocator()
    {
        Free();
    }

    void Free()
    {
        for (auto& block : m_Blocks)
        {
            m_pAllocator->Free(block.Page);
        }
        m_Blocks.clear();

        m_pAllocator = nullptr;
    }

    void Discard()
    {
        for (auto& block : m_Blocks)
        {
            block.Size = 0;
        }
    }

    NDDISCARD void* Allocate(size_t size, size_t align)
    {
        if (size == 0)
            return nullptr;

        for (auto& block : m_Blocks)
        {
            size_t offset = Align(reinterpret_cast<size_t>(block.Page) + block.Size, align) - reinterpret_cast<size_t>(block.Page);

            if (size <= (block.Capacity - offset))
            {
                block.Size = offset + size;
                return block.Page + offset;
            }
        }

        // create new block
        size_t BlockSize = m_BlockSize;
        BlockSize        = size * 2 < BlockSize ? BlockSize : size * 2;
        m_Blocks.emplace_back(m_pAllocator->Allocate(BlockSize, "dynamic linear allocator page", __FILE__, __LINE__), 0, BlockSize);

        auto&  block  = m_Blocks.back();
        size_t offset = Align(reinterpret_cast<size_t>(block.Page), align) - reinterpret_cast<size_t>(block.Page);
        block.Size    = offset + size;
        return block.Page + offset;
    }

    template <typename T>
    NDDISCARD T* Allocate(size_t count = 1)
    {
        return reinterpret_cast<T*>(Allocate(sizeof(T) * count, alignof(T)));
    }

    template <typename T, typename... Args>
    NDDISCARD T* Construct(Args&&... args)
    {
        T* Ptr = Allocate<T>(1);
        new (Ptr) T{std::forward<Args>(args)...};
        return Ptr;
    }

    template <typename T, typename... Args>
    NDDISCARD T* ConstructArray(size_t count, const Args&... args)
    {
        T* Ptr = Allocate<T>(count);
        for (size_t i = 0; i < count; ++i)
        {
            new (Ptr + i) T{args...};
        }
        return Ptr;
    }

    template <typename T>
    NDDISCARD T* CopyArray(const T* Src, size_t count)
    {
        T* Dst = Allocate<T>(count);
        for (size_t i = 0; i < count; ++i)
        {
            new (Dst + i) T{Src[i]};
        }
        return Dst;
    }

    NDDISCARD Char* CopyString(const Char* Str)
    {
        if (Str == nullptr)
            return nullptr;

        size_t len = strlen(Str) + 1;
        Char*  Dst = Allocate<Char>(len + 1);
        std::memcpy(Dst, Str, sizeof(Char) * len);
        Dst[len] = 0;
        return Dst;
    }

    NDDISCARD wchar_t* CopyWString(const char* Str)
    {
        if (Str == nullptr)
            return nullptr;

        size_t len = strlen(Str) + 1;
        auto*  Dst = Allocate<wchar_t>(len + 1);
        for (size_t i = 0; i < len; ++i)
        {
            Dst[i] = static_cast<wchar_t>(Str[i]);
        }
        Dst[len] = 0;
        return Dst;
    }

    NDDISCARD Char* CopyString(const String& Str)
    {
        size_t len = Str.length() + 1;
        Char*  Dst = Allocate<Char>(len + 1);
        std::memcpy(Dst, Str.c_str(), sizeof(Char) * len);
        Dst[len] = 0;
        return Dst;
    }

private:
    struct Block
    {
        uint8_t* Page     = nullptr;
        size_t   Size     = 0;
        size_t   Capacity = 0;

        Block(void* _Page, size_t _Size, size_t _Capacity) :
            Page{static_cast<uint8_t*>(_Page)}, Size{_Size}, Capacity{_Capacity} {}
    };

    std::vector<Block> m_Blocks;
    Uint32             m_BlockSize  = 4 << 10;
    IMemoryAllocator*  m_pAllocator = nullptr;
};

} // namespace Diligent
