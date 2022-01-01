/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include <atomic>

#include "BasicTypes.h"
#include "MemoryAllocator.h"

namespace Diligent
{

class SerializedMemory
{
public:
    SerializedMemory() {}

    explicit SerializedMemory(size_t Size, IMemoryAllocator* pAllocator = nullptr) noexcept;

    SerializedMemory(void* pData, size_t Size, IMemoryAllocator* pAllocator) noexcept;

    SerializedMemory(SerializedMemory&& Other) noexcept :
        m_pAllocator{Other.m_pAllocator},
        m_Ptr{Other.m_Ptr},
        m_Size{Other.m_Size}
    {
        Other.m_pAllocator = nullptr;
        Other.m_Ptr        = nullptr;
        Other.m_Size       = 0;
    }

    SerializedMemory& operator=(SerializedMemory&& Rhs) noexcept;

    SerializedMemory(const SerializedMemory&) = delete;
    SerializedMemory& operator=(const SerializedMemory&) = delete;

    ~SerializedMemory();

    explicit operator bool() const { return m_Ptr != nullptr; }

    bool operator==(const SerializedMemory& Rhs) const;
    bool operator!=(const SerializedMemory& Rhs) const
    {
        return !(*this == Rhs);
    }

    void*  Ptr() const { return m_Ptr; }
    size_t Size() const { return m_Size; }

    size_t CalcHash() const;

    void Free();

    struct Hasher
    {
        size_t operator()(const SerializedMemory& Mem) const
        {
            return Mem.CalcHash();
        }
    };

private:
    IMemoryAllocator* m_pAllocator = nullptr;

    void*  m_Ptr  = nullptr;
    size_t m_Size = 0;

    mutable std::atomic<size_t> m_Hash{0};
};

} // namespace Diligent

namespace std
{
template <>
struct hash<Diligent::SerializedMemory>
{
    size_t operator()(const Diligent::SerializedMemory& Mem) const
    {
        return Mem.CalcHash();
    }
};

} // namespace std
