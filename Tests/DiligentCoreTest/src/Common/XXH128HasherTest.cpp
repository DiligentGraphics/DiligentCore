/*
 *  Copyright 2019-2026 Diligent Graphics LLC
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

#include "XXH128Hasher.hpp"
#include "xxhash.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
#    include <crtdbg.h>
#endif

using namespace Diligent;

namespace
{

XXH128Hash ReferenceHash(const void* Data, size_t Size)
{
    const XXH128_hash_t Hash = XXH3_128bits(Data, Size);
    return {Hash.low64, Hash.high64};
}

// Build the documented string encoding independently of XXH128State:
// a native-endian Uint64 byte count, followed by exactly that many bytes.
// The all-ones count represents a null pointer and has no payload.
void AppendStringFrame(std::vector<Uint8>& Bytes, const void* Data, size_t ByteSize)
{
    const Uint64 Length = Data != nullptr ? static_cast<Uint64>(ByteSize) : ~Uint64{0};
    const auto*  Prefix = reinterpret_cast<const Uint8*>(&Length);
    Bytes.insert(Bytes.end(), Prefix, Prefix + sizeof(Length));
    if (Data != nullptr && ByteSize != 0)
    {
        const auto* Payload = static_cast<const Uint8*>(Data);
        Bytes.insert(Bytes.end(), Payload, Payload + ByteSize);
    }
}

XXH128Hash ReferenceStringHash(const void* Data, size_t ByteSize)
{
    std::vector<Uint8> Bytes;
    AppendStringFrame(Bytes, Data, ByteSize);
    return ReferenceHash(Bytes.data(), Bytes.size());
}

TEST(XXH128HasherTest, KnownAnswers)
{
    // Seed-zero vectors and buffer generator from xxHash/tests/sanity_test.c and sanity_test_vectors.h.
    std::array<Uint8, 1024> Data{};
    Uint64                  Generator = 0x9E3779B1u;
    for (auto& Byte : Data)
    {
        Byte = static_cast<Uint8>(Generator >> 56u);
        Generator *= 0x9E3779B185EBCA8Dull;
    }
    const struct
    {
        size_t     Size;
        XXH128Hash Hash;
    } Cases[] = {
        {0, {0x6001C324468D497Full, 0x99AA06D3014798D8ull}},
        {1, {0xC44BDFF4074EECDBull, 0xA6CD5E9392000F6Aull}},
        {16, {0x562980258A998629ull, 0xC68C368ECF8A9C05ull}},
        {128, {0xEBB15E34A7FB5AB1ull, 0x39992220E045260Aull}},
        {240, {0x5C9AAE94C8EBE5A0ull, 0xAA4202DAA2769DC8ull}},
        {241, {0xC5A639ECD2030E5Eull, 0x99A80ECF0ECFC647ull}},
        {1024, {0xDD85C9B5C1109C5Cull, 0x0D30D24071C64C57ull}},
    };
    for (const auto& Test : Cases)
    {
        SCOPED_TRACE(Test.Size);
        XXH128State Hasher;
        if (Test.Size != 0)
            Hasher.UpdateRaw(Data.data(), Test.Size);
        EXPECT_EQ(Hasher.Digest(), Test.Hash);
    }
}

TEST(XXH128HasherTest, RawBytesAndChunking)
{
    std::array<Uint8, 2050> Data{};
    for (size_t i = 0; i < Data.size(); ++i)
        Data[i] = static_cast<Uint8>(i * 37u + 11u);

    // Exercise short inputs, xxHash block boundaries, long inputs, and unaligned data.
    const size_t Sizes[] = {1, 3, 4, 8, 9, 16, 17, 31, 32, 63, 64, 65, 127, 128, 129, 239, 240, 241, 255, 256, 257, 511, 512, 513, 1024, 2048};
    for (size_t Offset : {size_t{0}, size_t{1}})
    {
        for (size_t Size : Sizes)
        {
            SCOPED_TRACE(::testing::Message() << "offset=" << Offset << ", size=" << Size);
            const auto* Bytes    = Data.data() + Offset;
            const auto  Expected = ReferenceHash(Bytes, Size);
            XXH128State Whole;
            EXPECT_EQ(&Whole.UpdateRaw(Bytes, Size), &Whole);
            EXPECT_EQ(Whole.Digest(), Expected);
            for (size_t ChunkSize : {1u, 3u, 17u, 64u, 127u, 1024u})
            {
                SCOPED_TRACE(ChunkSize);
                XXH128State Chunked;
                for (size_t Pos = 0; Pos < Size; Pos += ChunkSize)
                    Chunked.UpdateRaw(Bytes + Pos, std::min(ChunkSize, Size - Pos));
                EXPECT_EQ(Chunked.Digest(), Expected);
            }
        }
    }
}

template <typename CharType, size_t Size>
void TestString(const CharType (&Text)[Size])
{
    auto Copy1 = std::make_unique<CharType[]>(Size);
    auto Copy2 = std::make_unique<CharType[]>(Size);
    memcpy(Copy1.get(), Text, sizeof(Text));
    memcpy(Copy2.get(), Text, sizeof(Text));
    const auto Expected = ReferenceStringHash(Text, (Size - 1) * sizeof(CharType));

    XXH128State Pointer1;
    XXH128State Pointer2;
    Pointer1.Update(Copy1.get());
    Pointer2.Update(Copy2.get());
    EXPECT_EQ(Pointer1.Digest(), Expected);
    EXPECT_EQ(Pointer2.Digest(), Expected);

    std::basic_string<CharType> String1{Text};
    std::basic_string<CharType> String2{Text};
    XXH128State                 Std1;
    XXH128State                 Std2;
    Std1.Update(String1);
    Std2.Update(String2);
    EXPECT_EQ(Std1.Digest(), Expected);
    EXPECT_EQ(Std2.Digest(), Expected);

    String2.back() = static_cast<CharType>(String2.back() + 1);
    XXH128State Changed;
    Changed.Update(String2);
    EXPECT_FALSE(Changed.Digest() == Expected);

    const CharType Binary[] = {static_cast<CharType>('A'), 0, static_cast<CharType>('B'), static_cast<CharType>('C')};
    for (size_t Length : {size_t{1}, size_t{2}, size_t{3}, size_t{4}})
    {
        XXH128State ExplicitLength;
        EXPECT_EQ(&ExplicitLength.UpdateStr(Binary, Length), &ExplicitLength);
        EXPECT_EQ(ExplicitLength.Digest(), ReferenceStringHash(Binary, Length * sizeof(CharType)));
        XXH128State EmbeddedNull;
        EmbeddedNull.Update(std::basic_string<CharType>{Binary, Length});
        EXPECT_EQ(EmbeddedNull.Digest(), ExplicitLength.Digest());
    }
    XXH128State NullTerminated;
    NullTerminated.Update(Binary);
    EXPECT_EQ(NullTerminated.Digest(), ReferenceStringHash(Binary, sizeof(CharType)));

    // An explicit zero length retains the API's automatic C-string length.
    XXH128State AutomaticLength;
    AutomaticLength.UpdateStr(Binary, 0);
    EXPECT_EQ(AutomaticLength.Digest(), NullTerminated.Digest());
}

TEST(XXH128HasherTest, String)
{
    TestString("01234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
}

TEST(XXH128HasherTest, WideString)
{
    TestString(L"AB");
    TestString(L"A\u4E2DZ");
}

TEST(XXH128HasherTest, EmptyInput)
{
    XXH128State Hasher;
    EXPECT_EQ(Hasher.Digest(), ReferenceHash(nullptr, 0));
    for (int Pass = 0; Pass < 2; ++Pass)
    {
        const auto Before = Hasher.Digest();
        EXPECT_EQ(&Hasher.UpdateRaw(nullptr, 0), &Hasher);
        EXPECT_EQ(&Hasher.UpdateRaw("", 0), &Hasher);
        EXPECT_EQ(Hasher.Digest(), Before);
        Hasher.UpdateRaw("prefix", 6);
    }
}

TEST(XXH128HasherTest, EmptyAndNullStrings)
{
    XXH128State EmptyChar;
    XXH128State EmptyWide;
    XXH128State EmptyString;
    XXH128State EmptyWideString;
    EmptyChar.Update("");
    EmptyWide.Update(L"");
    EmptyString.Update(std::string{});
    EmptyWideString.Update(std::wstring{});
    const Uint64 EmptyLength   = 0;
    const auto   EmptyExpected = ReferenceHash(&EmptyLength, sizeof(EmptyLength));
    EXPECT_EQ(EmptyChar.Digest(), EmptyExpected);
    EXPECT_EQ(EmptyWide.Digest(), EmptyExpected);
    EXPECT_EQ(EmptyString.Digest(), EmptyExpected);
    EXPECT_EQ(EmptyWideString.Digest(), EmptyExpected);
    EXPECT_FALSE(EmptyExpected == ReferenceHash(nullptr, 0));

    XXH128State NullChar;
    XXH128State NullWide;
    EXPECT_EQ(&NullChar.Update(static_cast<const char*>(nullptr)), &NullChar);
    EXPECT_EQ(&NullWide.UpdateStr(static_cast<const wchar_t*>(nullptr)), &NullWide);
    const Uint64 NullMarker   = ~Uint64{0};
    const auto   NullExpected = ReferenceHash(&NullMarker, sizeof(NullMarker));
    EXPECT_EQ(NullChar.Digest(), NullExpected);
    EXPECT_EQ(NullWide.Digest(), NullExpected);
    EXPECT_FALSE(NullExpected == EmptyExpected);
    EXPECT_FALSE(NullExpected == ReferenceHash(nullptr, 0));
}

template <typename CharType>
void TestStringFieldBoundaries(const CharType* A,
                               const CharType* AB,
                               const CharType* BC,
                               const CharType* C,
                               const CharType* ABC,
                               const CharType* Empty)
{
    // These distinct field sequences must remain distinct even when their raw
    // payloads concatenate to the same bytes or contain only empty/null fields.
    const std::vector<std::vector<const CharType*>> Fields = {
        {A, BC},
        {AB, C},
        {ABC},
        {},
        {Empty},
        {Empty, Empty},
        {nullptr},
        {nullptr, nullptr},
        {nullptr, Empty},
        {Empty, nullptr},
        {A},
        {Empty, A},
        {A, Empty},
        {nullptr, A},
        {A, nullptr},
        {Empty, A, nullptr},
        {nullptr, A, Empty},
    };

    std::vector<XXH128Hash> Hashes;
    for (size_t i = 0; i < Fields.size(); ++i)
    {
        SCOPED_TRACE(i);
        XXH128State        Hasher;
        std::vector<Uint8> Bytes;
        for (const CharType* Field : Fields[i])
        {
            Hasher.Update(Field);
            const size_t ByteSize = Field != nullptr ? std::char_traits<CharType>::length(Field) * sizeof(CharType) : 0;
            AppendStringFrame(Bytes, Field, ByteSize);
        }

        const auto Hash = Hasher.Digest();
        EXPECT_EQ(Hash, ReferenceHash(Bytes.empty() ? nullptr : Bytes.data(), Bytes.size()));
        for (size_t j = 0; j < Hashes.size(); ++j)
        {
            SCOPED_TRACE(j);
            EXPECT_FALSE(Hash == Hashes[j]);
        }
        Hashes.push_back(Hash);
    }

    XXH128State Pair;
    Pair.Update(A, BC);
    EXPECT_EQ(Pair.Digest(), Hashes[0]);

    // Framed strings are fields, while UpdateRaw remains a byte stream.
    XXH128State Raw;
    Raw.UpdateRaw(ABC, std::char_traits<CharType>::length(ABC) * sizeof(CharType));
    EXPECT_FALSE(Raw.Digest() == Hashes[2]);
}

TEST(XXH128HasherTest, StringFieldBoundaries)
{
    TestStringFieldBoundaries("a", "ab", "bc", "c", "abc", "");
}

TEST(XXH128HasherTest, WideStringFieldBoundaries)
{
    TestStringFieldBoundaries(L"a", L"ab", L"bc", L"c", L"abc", L"");
}

TEST(XXH128HasherTest, ScalarsAndVariadicUpdate)
{
    enum class TestEnum : Uint16
    {
        Value = 0x1234
    };
    const Int32        Integer = -321;
    const float        Float   = 1.25f;
    const bool         Bool    = true;
    const TestEnum     Enum    = TestEnum::Value;
    const std::string  Text{"A\0B", 3};
    std::vector<Uint8> Bytes;
    auto               Append = [&Bytes](const auto& Value) {
        const auto* First = reinterpret_cast<const Uint8*>(&Value);
        Bytes.insert(Bytes.end(), First, First + sizeof(Value));
    };
    Append(Integer);
    Append(Float);
    Append(Bool);
    Append(Enum);
    AppendStringFrame(Bytes, Text.data(), Text.size());
    const auto Expected = ReferenceHash(Bytes.data(), Bytes.size());

    XXH128State Variadic;
    EXPECT_EQ(&Variadic.Update(Integer, Float, Bool, Enum, Text), &Variadic);
    EXPECT_EQ(Variadic.Digest(), Expected);
    XXH128State Callable;
    EXPECT_EQ(&Callable(Integer, Float, Bool, Enum, Text), &Callable);
    EXPECT_EQ(Callable.Digest(), Expected);
    XXH128State Chained;
    Chained.Update(Integer).Update(Float).Update(Bool).Update(Enum).Update(Text);
    EXPECT_EQ(Chained.Digest(), Expected);
}

TEST(XXH128HasherTest, DigestDoesNotFinalize)
{
    XXH128State Hasher;
    Hasher.UpdateRaw("abc", 3);
    EXPECT_EQ(Hasher.Digest(), ReferenceHash("abc", 3));
    EXPECT_EQ(Hasher.Digest(), ReferenceHash("abc", 3));
    Hasher.UpdateRaw("def", 3);
    EXPECT_EQ(Hasher.Digest(), ReferenceHash("abcdef", 6));
}

TEST(XXH128HasherTest, MoveConstructionAndAssignment)
{
    XXH128State Source;
    Source.UpdateRaw("abc", 3);
    XXH128State Moved{std::move(Source)};
    EXPECT_EQ(Moved.Digest(), ReferenceHash("abc", 3));
    Moved.UpdateRaw("def", 3);

    XXH128State Destination;
    Destination.Update("discard this state");
    EXPECT_EQ(&(Destination = std::move(Moved)), &Destination);
    EXPECT_EQ(Destination.Digest(), ReferenceHash("abcdef", 6));
    Destination.UpdateRaw("ghi", 3);
    EXPECT_EQ(Destination.Digest(), ReferenceHash("abcdefghi", 9));

    // A moved-from object can be assigned a new state and used again.
    Source = XXH128State{};
    Source.UpdateRaw("reused", 6);
    EXPECT_EQ(Source.Digest(), ReferenceHash("reused", 6));
}

TEST(XXH128HasherTest, SelfMoveAssignment)
{
    XXH128State Hasher;
    Hasher.UpdateRaw("abc", 3);
    auto Assign = [](XXH128State& To, XXH128State& From) {
        To = std::move(From);
    };
    Assign(Hasher, Hasher);
    EXPECT_EQ(Hasher.Digest(), ReferenceHash("abc", 3));
}

#if defined(_MSC_VER) && defined(_DEBUG)
TEST(XXH128HasherTest, MoveAssignmentReleasesState)
{
    // Keep assertions outside the measured scope so GoogleTest allocations do not
    // affect the heap snapshots. The portable move tests can also run under leak sanitizers.
    _CrtMemState Before{};
    _CrtMemState After{};
    _CrtMemCheckpoint(&Before);
    {
        XXH128State Source;
        XXH128State Destination;
        Destination = std::move(Source);
    }
    _CrtMemCheckpoint(&After);
    EXPECT_EQ(After.lCounts[_NORMAL_BLOCK], Before.lCounts[_NORMAL_BLOCK]);
    EXPECT_EQ(After.lSizes[_NORMAL_BLOCK], Before.lSizes[_NORMAL_BLOCK]);
}
#endif

TEST(XXH128HasherTest, Struct)
{
    ShaderVersion Version{1, 2};
    XXH128State   Hasher;
    Hasher(Version);
    const Uint32 Fields[] = {Version.Minor, Version.Major};
    EXPECT_EQ(Hasher.Digest(), ReferenceHash(Fields, sizeof(Fields)));
}

TEST(XXH128HasherTest, HashValue)
{
    const XXH128Hash Hash{0x0123456789ABCDEFull, 0xFEDCBA9876543210ull};
    EXPECT_TRUE(Hash == (XXH128Hash{Hash.LowPart, Hash.HighPart}));
    EXPECT_FALSE(Hash == (XXH128Hash{Hash.LowPart ^ 1, Hash.HighPart}));
    EXPECT_FALSE(Hash == (XXH128Hash{Hash.LowPart, Hash.HighPart ^ 1}));
    EXPECT_EQ(std::hash<XXH128Hash>{}(Hash), std::hash<XXH128Hash>{}(XXH128Hash{Hash.LowPart, Hash.HighPart}));
    EXPECT_EQ(Hash.ToString(), "FEDCBA98765432100123456789ABCDEF");
    EXPECT_EQ(XXH128Hash{}.ToString(), std::string(32, '0'));
}

} // namespace
