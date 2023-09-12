/*
 *  Copyright 2023 Diligent Graphics LLC
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

#include "ObjectsRegistry.hpp"

#include "gtest/gtest.h"

#include <thread>
#include <functional>

#include "ObjectBase.hpp"
#include "ThreadSignal.hpp"

using namespace Diligent;

namespace
{

struct RegistryData
{
    Uint32 Value = ~0u;

    RegistryData() = default;
    RegistryData(Uint32 _Value) :
        Value{_Value}
    {}

    static std::shared_ptr<RegistryData> Create(Uint32 _Value)
    {
        return std::make_shared<RegistryData>(_Value);
    }
};

struct RegistryDataObj : public ObjectBase<IObject>
{
    RegistryDataObj(IReferenceCounters* pRefCounters, Uint32 _Value) :
        ObjectBase<IObject>{pRefCounters},
        Value{_Value}
    {}

    Uint32 Value = ~0u;

    static RefCntAutoPtr<RegistryDataObj> Create(Uint32 _Value)
    {
        return RefCntAutoPtr<RegistryDataObj>{MakeNewRCObj<RegistryDataObj>()(_Value)};
    }
};

template <template <typename T> class StrongPtrType, typename DataType>
void TestObjectRegistryGet()
{
    ObjectsRegistry<int, StrongPtrType<DataType>> Registry;

    {
        int    Key    = 999;
        Uint32 Value  = 123;
        Uint32 Value2 = 456;

        EXPECT_EQ(Registry.Get(Key), nullptr);

        auto pData = Registry.Get(Key, std::bind(DataType::Create, Value));
        ASSERT_EQ(pData, Registry.Get(Key));
        ASSERT_NE(pData, nullptr);
        ASSERT_EQ(pData, Registry.Get(Key));
        EXPECT_EQ(pData->Value, Value);
        auto pData2 = Registry.Get(Key, std::bind(DataType::Create, Value2));
        ASSERT_NE(pData2, nullptr);
        EXPECT_EQ(pData, pData2);
        pData  = {};
        pData2 = {};
        ASSERT_EQ(Registry.Get(Key), nullptr);
    }

    constexpr Uint32                     NumThreads = 16;
    std::vector<std::thread>             Threads(NumThreads);
    std::vector<StrongPtrType<DataType>> Data(NumThreads);

    Threading::Signal StartSignal;
    for (Uint32 i = 0; i < NumThreads; ++i)
    {
        Threads[i] = std::thread(
            [&](Uint32 ThreadId) {
                StartSignal.Wait();
                // Get data with the same key from all threads
                Data[ThreadId] = Registry.Get(1, std::bind(DataType::Create, ThreadId));
            },
            i);
    }
    StartSignal.Trigger(true);

    for (auto& T : Threads)
        T.join();

    for (size_t i = 1; i < Data.size(); ++i)
    {
        // Whatever thread first set the value should be the same for all threads
        EXPECT_EQ(Data[0]->Value, Data[i]->Value);
    }
}

TEST(Common_ObjectsRegistry, Get_SharedPtr)
{
    TestObjectRegistryGet<std::shared_ptr, RegistryData>();
}

TEST(Common_ObjectsRegistry, Get_RefCntAutoPtr)
{
    TestObjectRegistryGet<RefCntAutoPtr, RegistryDataObj>();
}


template <template <typename T> class StrongPtrType, typename DataType>
void TestObjectRegistryCreateDestroyRace()
{
    ObjectsRegistry<int, StrongPtrType<DataType>> Registry{64};

    constexpr Uint32         NumThreads = 16;
    std::vector<std::thread> Threads(NumThreads);

    Threading::Signal StartSignal;
    for (Uint32 i = 0; i < NumThreads; ++i)
    {
        Threads[i] = std::thread(
            [&](Uint32 ThreadId) {
                StartSignal.Wait();
                // Get data with the same key from all threads
                auto pData = Registry.Get(1, std::bind(DataType::Create, ThreadId));
                EXPECT_EQ(pData, Registry.Get(1));
            },
            i);
    }
    StartSignal.Trigger(true);

    for (auto& T : Threads)
        T.join();
}

TEST(Common_ObjectsRegistry, CreateDestroyRace_SharedPtr)
{
    TestObjectRegistryCreateDestroyRace<std::shared_ptr, RegistryData>();
}

TEST(Common_ObjectsRegistry, CreateDestroyRace_RefCntAutoPtr)
{
    TestObjectRegistryCreateDestroyRace<RefCntAutoPtr, RegistryDataObj>();
}


template <template <typename T> class StrongPtrType, typename DataType>
void TestObjectRegistryExceptions()
{
    ObjectsRegistry<int, StrongPtrType<DataType>> Registry{128};

    constexpr Uint32         NumThreads = 15; // Use odd number
    std::vector<std::thread> Threads(NumThreads);

    std::vector<std::vector<StrongPtrType<DataType>>> ThreadsData(NumThreads);

    Threading::Signal StartSignal;
    for (Uint32 i = 0; i < NumThreads; ++i)
    {
        ThreadsData[i].resize(128);

        Threads[i] = std::thread(
            [&](Uint32 ThreadId) {
                StartSignal.Wait();

                auto& Data = ThreadsData[ThreadId];
                for (Uint32 i = 0; i < Data.size(); ++i)
                {
                    try
                    {
                        // Set elements with the same keys from all threads
                        Data[i] = Registry.Get(i,
                                               [&]() //
                                               {
                                                   switch ((i * NumThreads + ThreadId) % 3)
                                                   {
                                                       case 0: return DataType::Create(i);
                                                       case 1: throw std::runtime_error("test error");
                                                       case 2: return StrongPtrType<DataType>{};
                                                       default:
                                                           UNEXPECTED("Unexpected value");
                                                           return StrongPtrType<DataType>{};
                                                   }
                                               });
                    }
                    catch (...)
                    {
                    }
                }
            },
            i);
    }
    StartSignal.Trigger(true);

    for (auto& T : Threads)
        T.join();

    for (auto& Data : ThreadsData)
    {
        for (Uint32 i = 0; i < Data.size(); ++i)
        {
            if (Data[i])
            {
                auto Value = Data[i]->Value;
                EXPECT_TRUE(Value == i);
            }
        }
    }
}


TEST(Common_ObjectsRegistry, Exceptions_SharedPtr)
{
    TestObjectRegistryExceptions<std::shared_ptr, RegistryData>();
}

TEST(Common_ObjectsRegistry, Exceptions_RefCntAutoPtr)
{
    TestObjectRegistryExceptions<RefCntAutoPtr, RegistryDataObj>();
}

} // namespace
