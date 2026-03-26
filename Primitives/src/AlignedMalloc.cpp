/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include "AlignedMalloc.h"

#include <algorithm>
#include <cstdint>
#include <assert.h>

namespace Diligent
{

void* AllocateAlignedFallback(size_t Size, size_t Alignment)
{
    assert((Alignment & (Alignment - 1)) == 0 && "Alignment must be a power of two");

    constexpr size_t PointerSize = sizeof(void*);

    // Make sure the alignment is at least the size of a pointer.
    Alignment = std::max(Alignment, PointerSize);

    void* Pointer = malloc(Size + Alignment + PointerSize);
    if (Pointer == nullptr)
        return nullptr;

    uintptr_t RawAddress     = reinterpret_cast<uintptr_t>(Pointer) + PointerSize;
    uintptr_t AlignmentU     = static_cast<uintptr_t>(Alignment);
    uintptr_t AlignedAddress = (RawAddress + AlignmentU - 1) & ~(AlignmentU - 1);
    void*     AlignedPointer = reinterpret_cast<void*>(AlignedAddress);

    void** StoredPointer = reinterpret_cast<void**>(AlignedPointer) - 1;
    *StoredPointer       = Pointer;

    return AlignedPointer;
}

void FreeAlignedFallback(void* Ptr)
{
    if (Ptr != nullptr)
    {
        void* OriginalPointer = *(reinterpret_cast<void**>(Ptr) - 1);
        free(OriginalPointer);
    }
}

} // namespace Diligent
