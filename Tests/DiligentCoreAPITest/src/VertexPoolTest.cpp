/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

#include "VertexPool.h"

#include <vector>
#include <algorithm>
#include <thread>

#include "GPUTestingEnvironment.hpp"
#include "FastRand.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

TEST(VertexPoolTest, Create)
{
    auto* const pEnv     = GPUTestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto* const pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    constexpr VertexPoolElementDesc Elements[] =
        {
            VertexPoolElementDesc{16},
            VertexPoolElementDesc{24, BIND_SHADER_RESOURCE, USAGE_DEFAULT, BUFFER_MODE_STRUCTURED, CPU_ACCESS_NONE},
        };
    VertexPoolCreateInfo CI;
    CI.Desc.Name        = "Test vertex pool";
    CI.Desc.pElements   = Elements;
    CI.Desc.NumElements = _countof(Elements);
    CI.Desc.VertexCount = 1024;

    RefCntAutoPtr<IVertexPool> pVtxPool;
    CreateVertexPool(pDevice, CI, &pVtxPool);
    EXPECT_NE(pVtxPool, nullptr);

    auto* pBuffer0 = pVtxPool->Update(0, pDevice, pContext);
    EXPECT_NE(pBuffer0, nullptr);
    EXPECT_EQ(pBuffer0, pVtxPool->GetBuffer(0));

    auto* pBuffer1 = pVtxPool->Update(1, pDevice, pContext);
    EXPECT_NE(pBuffer1, nullptr);
    EXPECT_EQ(pBuffer1, pVtxPool->GetBuffer(1));

    RefCntAutoPtr<IVertexPoolAllocation> pAlloc0;
    pVtxPool->Allocate(256, &pAlloc0);
    ASSERT_NE(pAlloc0, nullptr);
    EXPECT_EQ(pAlloc0->GetStartVertex(), 0u);
    EXPECT_EQ(pAlloc0->GetVertexCount(), 256u);

    RefCntAutoPtr<IVertexPoolAllocation> pAlloc1;
    pVtxPool->Allocate(1024, &pAlloc1);
    ASSERT_NE(pAlloc1, nullptr);
    EXPECT_EQ(pAlloc1->GetStartVertex(), 256u);
    EXPECT_EQ(pAlloc1->GetVertexCount(), 1024u);

    // Release allocator first
    pVtxPool.Release();
    pAlloc0.Release();
    pAlloc1.Release();
}

TEST(VertexPoolTest, Allocate)
{
    auto* pEnv     = GPUTestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pContext = pEnv->GetDeviceContext();

    GPUTestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    constexpr VertexPoolElementDesc Elements[] =
        {
            VertexPoolElementDesc{16},
            VertexPoolElementDesc{24, BIND_SHADER_RESOURCE, USAGE_DEFAULT, BUFFER_MODE_STRUCTURED, CPU_ACCESS_NONE},
        };
    VertexPoolCreateInfo CI;
    CI.Desc.Name        = "Test vertex pool";
    CI.Desc.pElements   = Elements;
    CI.Desc.NumElements = _countof(Elements);
    CI.Desc.VertexCount = 128;

    RefCntAutoPtr<IVertexPool> pVtxPool;
    CreateVertexPool(pDevice, CI, &pVtxPool);
    EXPECT_NE(pVtxPool, nullptr);

#ifdef DILIGENT_DEBUG
    constexpr size_t NumIterations = 8;
#else
    constexpr size_t NumIterations = 32;
#endif
    for (size_t i = 0; i < NumIterations; ++i)
    {
        const size_t NumThreads = std::max(4u, std::thread::hardware_concurrency());

        const size_t NumAllocations = NumIterations * 8;

        std::vector<std::vector<RefCntAutoPtr<IVertexPoolAllocation>>> pAllocations(NumThreads);
        for (auto& Allocs : pAllocations)
            Allocs.resize(NumAllocations);

        {
            std::vector<std::thread> Threads(NumThreads);
            for (size_t t = 0; t < Threads.size(); ++t)
            {
                Threads[t] = std::thread{
                    [&](size_t thread_id) //
                    {
                        FastRandInt rnd{static_cast<unsigned int>(thread_id), 4, 64};

                        auto& Allocs = pAllocations[thread_id];
                        for (auto& Alloc : Allocs)
                        {
                            Uint32 size = static_cast<Uint32>(rnd());
                            pVtxPool->Allocate(size, &Alloc);
                            ASSERT_TRUE(Alloc);
                            EXPECT_EQ(Alloc->GetVertexCount(), size);
                        }
                    },
                    t //
                };
            }

            for (auto& Thread : Threads)
                Thread.join();
        }

        auto* pBuffer0 = pVtxPool->Update(0, pDevice, pContext);
        EXPECT_NE(pBuffer0, nullptr);
        EXPECT_EQ(pBuffer0, pVtxPool->GetBuffer(0));

        auto* pBuffer1 = pVtxPool->Update(1, pDevice, pContext);
        EXPECT_NE(pBuffer1, nullptr);
        EXPECT_EQ(pBuffer1, pVtxPool->GetBuffer(1));

        {
            std::vector<std::thread> Threads(NumThreads);
            for (size_t t = 0; t < Threads.size(); ++t)
            {
                Threads[t] = std::thread{
                    [&](size_t thread_id) //
                    {
                        auto& Allocs = pAllocations[thread_id];
                        for (auto& Alloc : Allocs)
                            Alloc.Release();
                    },
                    t //
                };
            }

            for (auto& Thread : Threads)
                Thread.join();
        }
    }
}

} // namespace
