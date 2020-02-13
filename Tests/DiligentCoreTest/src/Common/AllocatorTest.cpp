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

#include "DefaultRawMemoryAllocator.hpp"
#include "FixedBlockMemoryAllocator.hpp"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

TEST(Common_FixedBlockMemoryAllocator, AllocDealloc)
{
    constexpr Uint32 AllocSize             = 32;
    constexpr Uint32 NumAllocationsPerPage = 16;

    FixedBlockMemoryAllocator TestAllocator(DefaultRawMemoryAllocator::GetAllocator(), AllocSize, NumAllocationsPerPage);

    void* Allocations[NumAllocationsPerPage][2] = {};
    for (int p = 0; p < 2; ++p)
    {
        for (int a = 1; a < NumAllocationsPerPage; ++a)
        {
            for (int i = 0; i < a; ++i)
                Allocations[i][p] = TestAllocator.Allocate(AllocSize, "Fixed block allocator test", __FILE__, __LINE__);

            for (int i = a - 1; i >= 0; --i)
                TestAllocator.Free(Allocations[i][p]);

            for (int i = 0; i < a; ++i)
            {
                auto* NewAlloc = TestAllocator.Allocate(AllocSize, "Fixed block allocator test", __FILE__, __LINE__);
                EXPECT_EQ(Allocations[i][p], NewAlloc);
            }

            for (int i = a - 1; i >= 0; --i)
                TestAllocator.Free(Allocations[i][p]);
        }
        for (int i = 0; i < NumAllocationsPerPage; ++i)
            Allocations[i][p] = TestAllocator.Allocate(AllocSize, "Fixed block allocator test", __FILE__, __LINE__);
    }

    for (int p = 0; p < 2; ++p)
        for (int i = 0; i < NumAllocationsPerPage; ++i)
            TestAllocator.Free(Allocations[i][p]);

    for (int p = 0; p < 2; ++p)
        for (int i = 0; i < NumAllocationsPerPage; ++i)
            Allocations[i][p] = TestAllocator.Allocate(AllocSize, "Fixed block allocator test", __FILE__, __LINE__);

    for (int p = 0; p < 2; ++p)
        for (int s = 0; s < 5; ++s)
            for (int i = s; i < NumAllocationsPerPage; i += 5)
                TestAllocator.Free(Allocations[i][p]);
}

TEST(Common_FixedBlockMemoryAllocator, SmallObject)
{
    constexpr Uint32 AllocSize             = 4;
    constexpr Uint32 NumAllocationsPerPage = 1;

    FixedBlockMemoryAllocator TestAllocator(DefaultRawMemoryAllocator::GetAllocator(), AllocSize, NumAllocationsPerPage);

    {
        void* pRawMem0 = TestAllocator.Allocate(AllocSize, "Small object allocation test", __FILE__, __LINE__);
        TestAllocator.Free(pRawMem0);
    }

    {
        void* pRawMem0 = TestAllocator.Allocate(AllocSize, "Small object allocation test", __FILE__, __LINE__);
        void* pRawMem1 = TestAllocator.Allocate(AllocSize, "Small object allocation test", __FILE__, __LINE__);
        TestAllocator.Free(pRawMem0);
        TestAllocator.Free(pRawMem1);
    }
}

TEST(Common_FixedBlockMemoryAllocator, UnalignedSize)
{
    constexpr Uint32 AllocSize             = 10;
    constexpr Uint32 NumAllocationsPerPage = 1;

    FixedBlockMemoryAllocator TestAllocator(DefaultRawMemoryAllocator::GetAllocator(), AllocSize, NumAllocationsPerPage);

    {
        void* pRawMem0 = TestAllocator.Allocate(AllocSize, "Unaligned-size object allocation test", __FILE__, __LINE__);
        TestAllocator.Free(pRawMem0);
    }

    {
        void* pRawMem0 = TestAllocator.Allocate(AllocSize, "Unaligned-size object allocation test", __FILE__, __LINE__);
        void* pRawMem1 = TestAllocator.Allocate(AllocSize, "Unaligned-size object allocation test", __FILE__, __LINE__);
        TestAllocator.Free(pRawMem0);
        TestAllocator.Free(pRawMem1);
    }
}

} // namespace
