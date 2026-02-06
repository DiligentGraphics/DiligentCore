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

#include "MPSCQueue.hpp"

#include "gtest/gtest.h"

#include <thread>
#include <vector>

#include "ThreadSignal.hpp"

using namespace Diligent;

namespace
{

TEST(Common_MPSCQueue, EnqueueDequeue)
{
    MPSCQueue<int> Queue;

    Queue.Enqueue(42);
    Queue.Enqueue(84);
    Queue.Enqueue(126);

    int Value = 0;
    EXPECT_TRUE(Queue.Dequeue(Value));
    EXPECT_EQ(Value, 42);
    EXPECT_TRUE(Queue.Dequeue(Value));
    EXPECT_EQ(Value, 84);
    EXPECT_TRUE(Queue.Dequeue(Value));
    EXPECT_EQ(Value, 126);

    EXPECT_FALSE(Queue.Dequeue(Value));

    Queue.Enqueue(168);
    Queue.Enqueue(210);
    EXPECT_TRUE(Queue.Dequeue(Value));
    EXPECT_EQ(Value, 168);
    EXPECT_TRUE(Queue.Dequeue(Value));
    EXPECT_EQ(Value, 210);

    EXPECT_FALSE(Queue.Dequeue(Value));

    Queue.Enqueue(252);
    Queue.Enqueue(294);
}

TEST(Common_MPSCQueue, EnqueueDequeueMoveOnly)
{
    MPSCQueue<std::unique_ptr<int>> Queue;

    Queue.Enqueue(std::make_unique<int>(42));
    Queue.Enqueue(std::make_unique<int>(84));
    Queue.Enqueue(std::make_unique<int>(126));

    std::unique_ptr<int> Value;
    EXPECT_TRUE(Queue.Dequeue(Value));
    EXPECT_EQ(*Value, 42);
    EXPECT_TRUE(Queue.Dequeue(Value));
    EXPECT_EQ(*Value, 84);
    EXPECT_TRUE(Queue.Dequeue(Value));
    EXPECT_EQ(*Value, 126);

    EXPECT_FALSE(Queue.Dequeue(Value));

    Queue.Enqueue(std::make_unique<int>(168));
    Queue.Enqueue(std::make_unique<int>(210));
    EXPECT_TRUE(Queue.Dequeue(Value));
    EXPECT_EQ(*Value, 168);
    EXPECT_TRUE(Queue.Dequeue(Value));
    EXPECT_EQ(*Value, 210);

    EXPECT_FALSE(Queue.Dequeue(Value));

    Queue.Enqueue(std::make_unique<int>(252));
    Queue.Enqueue(std::make_unique<int>(294));
}


TEST(Common_MPSCQueue, EnqueueDequeueParallel)
{

    struct Data
    {
        Uint32 ThreadId = 0;
        Uint32 Value    = 0;
    };
    MPSCQueue<Data>   Queue;
    std::vector<Data> ProducedData;

    const Uint32 NumProducers        = std::max(std::thread::hardware_concurrency(), 4u);
    const Uint32 NumItemsPerProducer = 10000;
    ProducedData.reserve(NumProducers * NumItemsPerProducer);

    std::vector<std::thread> Producers;
    std::atomic<int>         NumProducersFinished{0};
    for (Uint32 i = 0; i < NumProducers; ++i)
    {
        Producers.emplace_back([&, i]() {
            for (Uint32 j = 0; j < NumItemsPerProducer; ++j)
            {
                Queue.Enqueue(Data{i, j});
            }
            NumProducersFinished.fetch_add(1);
        });
    }

    std::thread Consumer{
        std::thread([&]() {
            while (NumProducersFinished.load() < static_cast<int>(NumProducers))
            {
                Data value{};
                if (Queue.Dequeue(value))
                {
                    ProducedData.push_back(value);
                }
            }

            Data value{};
            while (Queue.Dequeue(value))
            {
                ProducedData.push_back(value);
            }
        })};

    for (std::thread& Producer : Producers)
    {
        Producer.join();
    }
    Consumer.join();

    EXPECT_EQ(ProducedData.size(), NumProducers * NumItemsPerProducer);
    std::vector<Uint32> CurrProducedValue(NumProducers, 0);

    for (const Data& Item : ProducedData)
    {
        EXPECT_EQ(CurrProducedValue[Item.ThreadId], Item.Value);
        CurrProducedValue[Item.ThreadId]++;
    }
}

} // namespace
