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

#include "Atomics.hpp"

#include "gtest/gtest.h"

#include <thread>
#include <algorithm>

#include "ThreadSignal.hpp"

using namespace Diligent;

namespace
{

static const int kNumThreads = static_cast<int>(std::max(4u, std::thread::hardware_concurrency()));

TEST(Platforms_Atomics, AtomicMax)
{
    std::atomic<int>         Val{0};
    std::vector<std::thread> Threads;

    Threading::Signal Start;
    for (int i = 0; i < kNumThreads; ++i)
    {
        Threads.emplace_back([&, i] {
            Start.Wait(true, kNumThreads);
            for (int j = 0; j < 10000; ++j)
            {
                AtomicMax(Val, i * 10000 + j);
            }
        });
    }
    Start.Trigger(true, kNumThreads);
    for (auto& Thread : Threads)
        Thread.join();
    EXPECT_EQ(Val.load(), kNumThreads * 10000 - 1);
}


TEST(Platforms_Atomics, AtomicMin)
{
    std::atomic<int>         Val{1 << 30};
    std::vector<std::thread> Threads;

    Threading::Signal Start;
    for (int i = 0; i < kNumThreads; ++i)
    {
        Threads.emplace_back([&, i] {
            Start.Wait(true, kNumThreads);
            for (int j = 0; j < 10000; ++j)
            {
                AtomicMin(Val, i * 10000 + j);
            }
        });
    }
    Start.Trigger(true, kNumThreads);
    for (auto& Thread : Threads)
        Thread.join();
    EXPECT_EQ(Val.load(), 0);
}

} // namespace
