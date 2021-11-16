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

SerializedMemory::~SerializedMemory()
{
    if (Ptr)
    {
        auto& RawMemAllocator = GetRawAllocator();
        RawMemAllocator.Free(Ptr);
    }
}

SerializedMemory& SerializedMemory::operator=(SerializedMemory&& Rhs)
{
    if (Ptr)
    {
        auto& RawMemAllocator = GetRawAllocator();
        RawMemAllocator.Free(Ptr);
    }

    Ptr  = Rhs.Ptr;
    Size = Rhs.Size;

    Rhs.Ptr  = nullptr;
    Rhs.Size = 0;
    return *this;
}

size_t SerializedMemory::CalcHash() const
{
    if (Ptr == nullptr || Size == 0)
        return 0;

    size_t Hash = 0;
    if (reinterpret_cast<size_t>(Ptr) % 4 == 0 && Size % 4 == 0)
    {
        const auto* UintPtr = static_cast<const Uint32*>(Ptr);
        for (size_t i = 0, Count = Size / 4; i < Count; ++i)
            HashCombine(Hash, UintPtr[i]);
    }
    else
    {
        const auto* BytePtr = static_cast<const Uint8*>(Ptr);
        for (size_t i = 0; i < Size; ++i)
            HashCombine(Hash, BytePtr[i]);
    }
    return Hash;
}

bool SerializedMemory::operator==(const SerializedMemory& Rhs) const
{
    if (Size != Rhs.Size)
        return false;

    return std::memcmp(Ptr, Rhs.Ptr, Size) == 0;
}

} // namespace Diligent
