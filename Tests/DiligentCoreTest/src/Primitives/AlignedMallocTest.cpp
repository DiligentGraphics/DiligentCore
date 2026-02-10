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

#if defined(_DEBUG) && defined(_MSC_VER)
#    define USE_CRT_MALLOC_DBG 1
#endif
#include "AlignedMalloc.h"

#include "gtest/gtest.h"

#include <cstring>

using namespace Diligent;

namespace
{

TEST(Primitives_AlignedMalloc, AllocDealloc)
{
    for (size_t alignment = 8; alignment <= 4096; alignment *= 2)
    {
        for (size_t size = alignment; size <= 4096; size *= 2)
        {
            void* ptr = DILIGENT_ALIGNED_MALLOC(size, alignment, __FILE__, __LINE__);
            EXPECT_NE(ptr, nullptr);
            EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignment, 0U);
            std::memset(ptr, 0xCD, size);
            DILIGENT_ALIGNED_FREE(ptr);
        }
    }
}

TEST(Primitives_AlignedMalloc, AllocateAlignedFallback)
{
    for (size_t alignment = 8; alignment <= 4096; alignment *= 2)
    {
        for (size_t size = 1; size <= 4096; size *= 2)
        {
            void* ptr = AllocateAlignedFallback(size, alignment);
            EXPECT_NE(ptr, nullptr);
            EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignment, 0U);
            std::memset(ptr, 0xCD, size);
            FreeAlignedFallback(ptr);
        }
    }
}

} // namespace
