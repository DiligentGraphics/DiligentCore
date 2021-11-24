/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "SerializedMemory.hpp"
#include "EngineMemory.h"
#include "HashUtils.hpp"

namespace Diligent
{

SerializedMemory::SerializedMemory(void* pData, size_t Size, IMemoryAllocator* pAllocator) noexcept :
    m_pAllocator{pAllocator},
    m_Ptr{pData},
    m_Size{Size}
{
    VERIFY_EXPR((m_Ptr != nullptr) == (m_Size > 0));
    VERIFY_EXPR((m_Ptr != nullptr) == (m_pAllocator != nullptr));
}

SerializedMemory::SerializedMemory(size_t Size, IMemoryAllocator* pAllocator) noexcept :
    m_pAllocator{pAllocator != nullptr ? pAllocator : &GetRawAllocator()},
    m_Ptr{ALLOCATE_RAW(*m_pAllocator, "Serialized memory", Size)},
    m_Size{Size}
{
}

SerializedMemory::~SerializedMemory()
{
    Free();
}

void SerializedMemory::Free()
{
    if (m_Ptr)
    {
        VERIFY_EXPR(m_pAllocator != nullptr);
        m_pAllocator->Free(m_Ptr);
    }

    m_pAllocator = nullptr;
    m_Ptr        = nullptr;
    m_Size       = 0;
}

SerializedMemory& SerializedMemory::operator=(SerializedMemory&& Rhs) noexcept
{
    Free();

    m_pAllocator = Rhs.m_pAllocator;
    m_Ptr        = Rhs.m_Ptr;
    m_Size       = Rhs.m_Size;

    Rhs.m_pAllocator = nullptr;
    Rhs.m_Ptr        = nullptr;
    Rhs.m_Size       = 0;
    return *this;
}

size_t SerializedMemory::CalcHash() const
{
    if (m_Ptr == nullptr || m_Size == 0)
        return 0;

    if (m_Hash.load() != 0)
        return m_Hash;

    size_t Hash = 0;
    HashCombine(Hash, m_Size);

    const auto* BytePtr = static_cast<const Uint8*>(m_Ptr);
    for (size_t i = 0; i < m_Size; ++i)
        HashCombine(Hash, BytePtr[i]);

    m_Hash.store(Hash);

    return Hash;
}

bool SerializedMemory::operator==(const SerializedMemory& Rhs) const
{
    if (m_Size != Rhs.m_Size)
        return false;

    return std::memcmp(m_Ptr, Rhs.m_Ptr, m_Size) == 0;
}

} // namespace Diligent
