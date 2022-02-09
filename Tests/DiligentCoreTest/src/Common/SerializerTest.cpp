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

#include "Serializer.hpp"
#include "DefaultRawMemoryAllocator.hpp"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

TEST(SerializerTest, SerializerTest)
{
    const char* const RefStr      = "serialized text";
    const char* const RefEmptyStr = "";
    const Uint64      RefU64      = 0x12345678ABCDEF01ull;
    const Uint8       RefU8       = 0x72;
    const Uint32      RefU32      = 0x52830394u;
    const Uint16      RefU16      = 0x4172;

    const Uint32 RefArraySize           = 3;
    const Uint32 RefArray[RefArraySize] = {0x1251, 0x620, 0x8816};
    const Uint32 NumBytes               = 7;
    const Uint8  RefBytes[NumBytes]     = {5, 124, 9, 44, 79, 40, 251};

    auto& RawAllocator{DefaultRawMemoryAllocator::GetAllocator()};

    DynamicLinearAllocator TmpAllocator{RawAllocator};
    const auto             WriteData = [&](auto& Ser) {
        Ser(RefU16);
        Ser(RefStr);
        Ser(RefEmptyStr);
        Ser(RefU64, RefU8);
        Ser(RefU32);
        Ser.SerializeArrayRaw(&TmpAllocator, RefArray, RefArraySize);
        Ser.CopyBytes(RefBytes, sizeof(RefBytes));
    };

    Serializer<SerializerMode::Measure> MSer;
    WriteData(MSer);

    auto Data = MSer.AllocateData(RawAllocator);

    Serializer<SerializerMode::Write> WSer{Data};
    WriteData(WSer);

    EXPECT_TRUE(WSer.IsEnded());

    Serializer<SerializerMode::Read> RSer{Data};

    {
        Uint16 U16 = 0;
        RSer(U16);
        EXPECT_EQ(U16, RefU16);
    }

    {
        const char* Str = nullptr;
        RSer(Str);
        EXPECT_STREQ(Str, RefStr);
    }

    {
        const char* EmptyStr = nullptr;
        RSer(EmptyStr);
        EXPECT_STREQ(EmptyStr, RefEmptyStr);
    }

    {
        Uint64 U64 = 0;
        RSer(U64);
        EXPECT_EQ(U64, RefU64);
    }

    {
        Uint8 U8 = 0;
        RSer(U8);
        EXPECT_EQ(U8, RefU8);
    }

    {
        Uint32 U32 = 0;
        RSer(U32);
        EXPECT_EQ(U32, RefU32);
    }

    {
        Uint32  ArraySize = 0;
        Uint32* pArray    = nullptr;
        RSer.SerializeArrayRaw(&TmpAllocator, pArray, ArraySize);
        EXPECT_EQ(ArraySize, RefArraySize);
        for (Uint32 i = 0; i < RefArraySize; ++i)
            EXPECT_EQ(RefArray[i], pArray[i]);
    }

    {
        Uint8 Bytes[NumBytes] = {};
        RSer.CopyBytes(Bytes, sizeof(Bytes));
        for (Uint32 i = 0; i < NumBytes; ++i)
            EXPECT_EQ(Bytes[i], RefBytes[i]);
    }

    EXPECT_TRUE(RSer.IsEnded());
}

} // namespace
