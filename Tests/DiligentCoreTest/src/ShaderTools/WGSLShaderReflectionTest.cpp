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

#include "WGSLShaderReflection.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <random>
#include <vector>

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

constexpr char Marker[] = "/*$DILIGENT_WGSL_REFLECTION=";

std::string MakeMetadata(const std::string& Payload)
{
    return std::string{Marker} + Payload + "*/";
}

std::string Serialize(const WGSLShaderReflection& Reflection)
{
    std::string Text;
    AppendWGSLShaderReflection(Text, Reflection);
    return Text;
}

// Independently specified data and wire text detect field-order and encoding
// mistakes that a serializer/parser round trip alone could hide.
WGSLShaderReflection MakeReferenceReflection()
{
    WGSLShaderReflection Reflection;
    Reflection.Resources[{2, 0}].Name = "A";
    Reflection.Resources[{1, 7}].Name = "*/\n";
    auto& EmptyBuffer                 = Reflection.Resources[{1, 3}];
    EmptyBuffer.UniformBuffer.reset(new ShaderCodeBufferDescX{});

    auto& Resource = Reflection.Resources[{0, 9}];
    Resource.Name  = "CB";
    Resource.UniformBuffer.reset(new ShaderCodeBufferDescX{});
    Resource.UniformBuffer->Size          = 128;
    const ShaderCodeVariableDesc Leaves[] = {
        {"x", "float", SHADER_CODE_BASIC_TYPE_FLOAT, 0},
        {"y", nullptr, SHADER_CODE_VARIABLE_CLASS_VECTOR, SHADER_CODE_BASIC_TYPE_UINT, 1, 4, 16, 2},
    };
    const ShaderCodeVariableDesc Members[] = {
        {"s", "S", 2, Leaves, 0},
        {"m", "float2x3", SHADER_CODE_VARIABLE_CLASS_MATRIX_ROWS, SHADER_CODE_BASIC_TYPE_FLOAT, 2, 3, 32},
    };
    Resource.UniformBuffer->AddVariable(ShaderCodeVariableDesc{"v", "V", 2, Members, 16, 3});
    Resource.UniformBuffer->AddVariable(ShaderCodeVariableDesc{"z", "float3x2", SHADER_CODE_VARIABLE_CLASS_MATRIX_COLUMNS, SHADER_CODE_BASIC_TYPE_FLOAT, 3, 2, 96});
    return Reflection;
}

constexpr char ReferencePayload[] =
    "1\n"
    "0 9 4342 1\n"
    "128 2\n"
    "76 56 5 0 0 0 16 3 2\n"
    "73 53 5 0 0 0 0 0 2\n"
    "78 666c6f6174 1 11 1 1 0 0 0\n"
    "79 - 2 7 1 4 16 2 0\n"
    "6d 666c6f6174327833 3 11 2 3 32 0 0\n"
    "7a 666c6f6174337832 4 11 3 2 96 0 0\n"
    "1 3 - 1\n"
    "0 0\n"
    "1 7 2a2f0a 0\n"
    "2 0 41 0\n";

void ExpectVariableEqual(const ShaderCodeVariableDescX& Actual, const ShaderCodeVariableDescX& Expected)
{
    EXPECT_STREQ(Actual.Name, Expected.Name != nullptr ? Expected.Name : "");
    EXPECT_STREQ(Actual.TypeName, Expected.TypeName != nullptr ? Expected.TypeName : "");
    EXPECT_EQ(Actual.Class, Expected.Class);
    EXPECT_EQ(Actual.BasicType, Expected.BasicType);
    EXPECT_EQ(Actual.NumRows, Expected.NumRows);
    EXPECT_EQ(Actual.NumColumns, Expected.NumColumns);
    EXPECT_EQ(Actual.Offset, Expected.Offset);
    EXPECT_EQ(Actual.ArraySize, Expected.ArraySize);
    ASSERT_EQ(Actual.NumMembers, Expected.NumMembers);
    // The owning descriptors contain vectors of derived types. Traverse their
    // accessors rather than indexing the inherited base-descriptor pointers.
    for (Uint32 i = 0; i < Expected.NumMembers; ++i)
    {
        SCOPED_TRACE(i);
        ExpectVariableEqual(Actual.GetMember(i), Expected.GetMember(i));
    }
}

void ExpectReflectionEqual(const WGSLShaderReflection& Actual, const WGSLShaderReflection& Expected)
{
    ASSERT_EQ(Actual.Resources.size(), Expected.Resources.size());
    for (const auto& Entry : Expected.Resources)
    {
        SCOPED_TRACE(::testing::Message{} << "Group " << Entry.first.Group << ", binding " << Entry.first.Binding);
        const auto* Resource = Actual.Find(Entry.first.Group, Entry.first.Binding);
        ASSERT_NE(Resource, nullptr);
        EXPECT_EQ(Resource->Name, Entry.second.Name);
        ASSERT_EQ(bool{Resource->UniformBuffer}, bool{Entry.second.UniformBuffer});
        if (Resource->UniformBuffer)
        {
            const auto& Buffer    = *Resource->UniformBuffer;
            const auto& RefBuffer = *Entry.second.UniformBuffer;
            EXPECT_EQ(Buffer.Size, RefBuffer.Size);
            ASSERT_EQ(Buffer.NumVariables, RefBuffer.NumVariables);
            for (Uint32 i = 0; i < RefBuffer.NumVariables; ++i)
            {
                SCOPED_TRACE(i);
                ExpectVariableEqual(Buffer.GetVariable(i), RefBuffer.GetVariable(i));
            }
        }
    }
}

void CheckRoundTrip(const WGSLShaderReflection& Expected)
{
    const std::string    Text = Serialize(Expected);
    WGSLShaderReflection Actual;
    ASSERT_TRUE(ParseWGSLShaderReflection(Text, Actual));
    ExpectReflectionEqual(Actual, Expected);
    EXPECT_EQ(Serialize(Actual), Text);
}

void ExpectRejected(const std::string& Text)
{
    const auto  Expected = MakeReferenceReflection();
    auto        Actual   = MakeReferenceReflection();
    const auto* Resource = Actual.Find(0, 9);
    const auto* Buffer   = Resource->UniformBuffer.get();
    EXPECT_FALSE(ParseWGSLShaderReflection(Text, Actual));
    // Every failure must preserve both the contents and ownership of the result.
    ASSERT_EQ(Actual.Find(0, 9), Resource);
    EXPECT_EQ(Actual.Find(0, 9)->UniformBuffer.get(), Buffer);
    ExpectReflectionEqual(Actual, Expected);
}

using Tokens = std::array<std::string, 16>;

Tokens MakeValidTokens()
{
    // Version; resource group/binding/name/flag; buffer size/count;
    // variable name/type/class/basic type/rows/columns/offset/array size/member count.
    return {{"1", "0", "1", "52", "1", "64", "1", "56", "54", "1", "11", "1", "1", "0", "0", "0"}};
}

std::string JoinTokens(const Tokens& Fields)
{
    std::string Payload;
    for (const auto& Field : Fields)
        Payload += Field + ' ';
    return MakeMetadata(Payload);
}

TEST(WGSLShaderReflection, EmptyReflectionDoesNotChangeSource)
{
    const WGSLShaderReflection Empty;
    for (const std::string& Source : {std::string{}, std::string{"@compute fn main() {}"}, std::string{"a\0b", 3}})
    {
        std::string Text = Source;
        AppendWGSLShaderReflection(Text, Empty);
        EXPECT_EQ(Text, Source);
    }
}

TEST(WGSLShaderReflection, MissingOrEmptyMetadataClearsOutput)
{
    const std::string Inputs[] = {
        "",
        "@compute fn main() {}",
        "/*$SHADER_SOURCE_LANGUAGE=1*/",
        "/*$DILIGENT_WGSL_REFLECTION",
        MakeMetadata("1"),
        MakeMetadata("1 \r\n\t\v\f"),
    };
    for (const auto& Text : Inputs)
    {
        SCOPED_TRACE(Text);
        auto Reflection = MakeReferenceReflection();
        ASSERT_TRUE(ParseWGSLShaderReflection(Text, Reflection));
        EXPECT_TRUE(Reflection.Resources.empty());
    }
}

TEST(WGSLShaderReflection, ExactSerialization)
{
    const auto        Reflection = MakeReferenceReflection();
    const std::string Prefix     = "// Existing source with an unrelated */\n@compute fn main() {}";
    std::string       Text       = Prefix;
    AppendWGSLShaderReflection(Text, Reflection);
    EXPECT_EQ(Text, Prefix + '\n' + MakeMetadata(ReferencePayload) + '\n');

    // Same binding index in another group and duplicate names are independent resources.
    WGSLShaderReflection RepeatedNames;
    RepeatedNames.Resources[{2, 7}].Name = "A";
    RepeatedNames.Resources[{1, 7}].Name = "A";
    RepeatedNames.Resources[{1, 2}].Name = "A";
    EXPECT_EQ(Serialize(RepeatedNames), "\n" + MakeMetadata("1\n1 2 41 0\n1 7 41 0\n2 7 41 0\n") + '\n');
    CheckRoundTrip(RepeatedNames);
}

TEST(WGSLShaderReflection, ParseKnownMetadataAndOwnStrings)
{
    std::string          Text = "// Earlier comment */\n" + MakeMetadata(ReferencePayload) + "\n/*$SHADER_SOURCE_LANGUAGE=1*/";
    WGSLShaderReflection Reflection;
    ASSERT_TRUE(ParseWGSLShaderReflection(Text, Reflection));
    Text.assign(Text.size(), 'x');
    Text.clear();
    Text.shrink_to_fit();
    ExpectReflectionEqual(Reflection, MakeReferenceReflection());
}

TEST(WGSLShaderReflection, SuccessfulParseReplacesPreviousReflection)
{
    auto Reflection = MakeReferenceReflection();
    ASSERT_TRUE(ParseWGSLShaderReflection(MakeMetadata("1 10 20 41 0"), Reflection));
    ASSERT_EQ(Reflection.Resources.size(), 1u);
    const auto* Resource = Reflection.Find(10, 20);
    ASSERT_NE(Resource, nullptr);
    EXPECT_EQ(Resource->Name, "A");
    EXPECT_EQ(Resource->UniformBuffer, nullptr);
    EXPECT_EQ(Reflection.Find(0, 9), nullptr);
}

TEST(WGSLShaderReflection, NestedLayoutsRoundTrip)
{
    CheckRoundTrip(MakeReferenceReflection());
}

TEST(WGSLShaderReflection, StringsRoundTrip)
{
    std::string AllBytes;
    for (unsigned Ch = 1; Ch <= 255; ++Ch)
        AllBytes += static_cast<char>(Ch);
    const std::string Names[] = {"", "-", "*/ /*\n\r\t\v\f", Marker, "\xc3\xa9\xf0\x9f\x98\x80", AllBytes, std::string(16384, 'x')};
    for (size_t i = 0; i < std::size(Names); ++i)
    {
        SCOPED_TRACE(i);
        WGSLShaderReflection Reflection;
        auto&                Resource = Reflection.Resources[{0, 0}];
        Resource.Name                 = Names[i];
        Resource.UniformBuffer.reset(new ShaderCodeBufferDescX{});
        ShaderCodeVariableDescX Var;
        Var.SetName(Names[i]);
        Var.SetTypeName(Names[i]);
        Resource.UniformBuffer->AddVariable(std::move(Var));
        CheckRoundTrip(Reflection);
        const std::string Text = Serialize(Reflection);
        EXPECT_EQ(Text.find("*/"), Text.size() - 3);
        EXPECT_EQ(Text.find(Marker, 2), std::string::npos);
    }

    WGSLShaderReflection NullNames;
    auto&                Buffer = NullNames.Resources[{0, 0}].UniformBuffer;
    Buffer.reset(new ShaderCodeBufferDescX{});
    Buffer->AddVariable(ShaderCodeVariableDescX{});
    Buffer->GetVariable(0).Name     = nullptr;
    Buffer->GetVariable(0).TypeName = nullptr;
    EXPECT_EQ(Serialize(NullNames), "\n" + MakeMetadata("1\n0 0 - 1\n0 1\n- - 0 0 0 0 0 0 0\n") + '\n');
    CheckRoundTrip(NullNames);
}

TEST(WGSLShaderReflection, AllHexDigitInputs)
{
    const std::string Digits = "0123456789abcdef";
    for (unsigned Ch = 0; Ch <= 255; ++Ch)
    {
        for (size_t Position = 0; Position < 2; ++Position)
        {
            SCOPED_TRACE(::testing::Message{} << "Byte " << Ch << ", nibble " << Position);
            auto Fields         = MakeValidTokens();
            Fields[3][Position] = static_cast<char>(Ch);
            WGSLShaderReflection Reflection;
            const bool           IsHex = Digits.find(static_cast<char>(Ch)) != std::string::npos;
            ASSERT_EQ(ParseWGSLShaderReflection(JoinTokens(Fields), Reflection), IsHex);
            if (IsHex)
            {
                const auto* Resource = Reflection.Find(0, 1);
                ASSERT_NE(Resource, nullptr);
                const unsigned Value = Position == 0 ? static_cast<unsigned>(Digits.find(static_cast<char>(Ch))) * 16 + 2 : 0x50 + static_cast<unsigned>(Digits.find(static_cast<char>(Ch)));
                EXPECT_EQ(Resource->Name, std::string(1, static_cast<char>(Value)));
            }
        }
    }
}

TEST(WGSLShaderReflection, Uint32Boundaries)
{
    std::vector<Uint32> Values{0, 1, 0x7fffffffu, 0x80000000u, 0xfffffffeu, 0xffffffffu};
    for (Uint64 Power = 10; Power <= 1000000000; Power *= 10)
    {
        Values.push_back(static_cast<Uint32>(Power - 1));
        Values.push_back(static_cast<Uint32>(Power));
        Values.push_back(static_cast<Uint32>(Power + 1));
    }
    for (Uint32 Value : Values)
    {
        SCOPED_TRACE(Value);
        auto Fields = MakeValidTokens();
        for (size_t Field : {1u, 2u, 5u, 13u, 14u})
            Fields[Field] = std::to_string(Value);
        WGSLShaderReflection Reflection;
        ASSERT_TRUE(ParseWGSLShaderReflection(JoinTokens(Fields), Reflection));
        const auto* Resource = Reflection.Find(Value, Value);
        ASSERT_NE(Resource, nullptr);
        ASSERT_NE(Resource->UniformBuffer, nullptr);
        EXPECT_EQ(Resource->UniformBuffer->Size, Value);
        const auto& Var = Resource->UniformBuffer->GetVariable(0);
        EXPECT_EQ(Var.Offset, Value);
        EXPECT_EQ(Var.ArraySize, Value);
        // Compare against independent decimal formatting as well as re-parsing.
        const std::string Number = std::to_string(Value);
        EXPECT_EQ(Serialize(Reflection), "\n" + MakeMetadata("1\n" + Number + ' ' + Number + " 52 1\n" + Number + " 1\n56 54 1 11 1 1 " + Number + ' ' + Number + " 0\n") + '\n');
        CheckRoundTrip(Reflection);
    }
}

TEST(WGSLShaderReflection, AllUint8Dimensions)
{
    for (unsigned Value = 0; Value <= 255; ++Value)
    {
        SCOPED_TRACE(Value);
        auto Fields = MakeValidTokens();
        Fields[11]  = std::to_string(Value);
        Fields[12]  = std::to_string(255 - Value);
        WGSLShaderReflection Reflection;
        ASSERT_TRUE(ParseWGSLShaderReflection(JoinTokens(Fields), Reflection));
        const auto& Var = Reflection.Find(0, 1)->UniformBuffer->GetVariable(0);
        EXPECT_EQ(Var.NumRows, Value);
        EXPECT_EQ(Var.NumColumns, 255 - Value);
        CheckRoundTrip(Reflection);
    }
}

TEST(WGSLShaderReflection, AllVariableClassesAndBasicTypes)
{
    for (unsigned Class = 0; Class < SHADER_CODE_VARIABLE_CLASS_COUNT; ++Class)
    {
        for (unsigned Type = 0; Type < SHADER_CODE_BASIC_TYPE_COUNT; ++Type)
        {
            SCOPED_TRACE(::testing::Message{} << "Class " << Class << ", type " << Type);
            auto Fields = MakeValidTokens();
            Fields[9]   = std::to_string(Class);
            Fields[10]  = std::to_string(Type);
            WGSLShaderReflection Reflection;
            ASSERT_TRUE(ParseWGSLShaderReflection(JoinTokens(Fields), Reflection));
            const auto& Var = Reflection.Find(0, 1)->UniformBuffer->GetVariable(0);
            EXPECT_EQ(Var.Class, Class);
            EXPECT_EQ(Var.BasicType, Type);
            CheckRoundTrip(Reflection);
        }
    }
}

TEST(WGSLShaderReflection, WhitespaceAndNumericSpelling)
{
    for (char Separator : {' ', '\t', '\n', '\r', '\v', '\f'})
    {
        SCOPED_TRACE(static_cast<unsigned>(Separator));
        std::string Payload = ReferencePayload;
        std::replace(Payload.begin(), Payload.end(), ' ', Separator);
        std::replace(Payload.begin(), Payload.end(), '\n', Separator);
        WGSLShaderReflection Reflection;
        ASSERT_TRUE(ParseWGSLShaderReflection(MakeMetadata(std::string(3, Separator) + Payload + std::string(3, Separator)), Reflection));
        ExpectReflectionEqual(Reflection, MakeReferenceReflection());
        EXPECT_EQ(Serialize(Reflection), "\n" + MakeMetadata(ReferencePayload) + '\n');
    }
    auto Fields = MakeValidTokens();
    for (size_t Field : {0u, 1u, 2u, 4u, 5u, 6u, 9u, 10u, 11u, 12u, 13u, 14u, 15u})
        Fields[Field] = "+000" + Fields[Field];
    WGSLShaderReflection Reflection;
    ASSERT_TRUE(ParseWGSLShaderReflection(JoinTokens(Fields), Reflection));
    WGSLShaderReflection Expected;
    ASSERT_TRUE(ParseWGSLShaderReflection(JoinTokens(MakeValidTokens()), Expected));
    ExpectReflectionEqual(Reflection, Expected);
}

TEST(WGSLShaderReflection, RejectMalformedEnvelope)
{
    const std::string Inputs[] = {
        Marker,
        std::string{Marker} + "1",
        std::string{Marker} + "1*",
        MakeMetadata(""),
        MakeMetadata(" \t\n"),
        MakeMetadata("0"),
        MakeMetadata("2"),
        MakeMetadata("4294967295"),
        MakeMetadata("1x"),
        MakeMetadata("1") + MakeMetadata("1"),
        MakeMetadata("1") + Marker,
        MakeMetadata(std::string{"1 "} + Marker + "1"),
        MakeMetadata("1 junk"),
    };
    for (const auto& Text : Inputs)
    {
        SCOPED_TRACE(Text);
        ExpectRejected(Text);
    }
}

TEST(WGSLShaderReflection, RejectInvalidNumberInEveryField)
{
    const std::string BadNumbers[] = {"", "+", "-", "-0", "-1", "++1", "+-1", "0x1", "1.0", "1e0", "1x", "4294967296", "18446744073709551616", std::string(128, '9'), std::string{"1\0", 2}};
    for (size_t Field : {0u, 1u, 2u, 4u, 5u, 6u, 9u, 10u, 11u, 12u, 13u, 14u, 15u})
    {
        for (const auto& Bad : BadNumbers)
        {
            SCOPED_TRACE(::testing::Message{} << "Field " << Field << ", value " << Bad);
            auto Fields   = MakeValidTokens();
            Fields[Field] = Bad;
            ExpectRejected(JoinTokens(Fields));
        }
    }
}

TEST(WGSLShaderReflection, RejectOutOfRangeFields)
{
    const std::pair<size_t, unsigned> InvalidValues[] = {
        {4, 2},
        {4, 255},
        {9, SHADER_CODE_VARIABLE_CLASS_COUNT},
        {9, 255},
        {9, 256},
        {10, SHADER_CODE_BASIC_TYPE_COUNT},
        {10, 255},
        {10, 256},
        {11, 256},
        {11, 65535},
        {12, 256},
        {12, 65535},
    };
    for (const auto& Entry : InvalidValues)
    {
        SCOPED_TRACE(::testing::Message{} << "Field " << Entry.first << ", value " << Entry.second);
        auto Fields         = MakeValidTokens();
        Fields[Entry.first] = std::to_string(Entry.second);
        ExpectRejected(JoinTokens(Fields));
    }
}

TEST(WGSLShaderReflection, RejectInvalidStringInEveryField)
{
    const std::string BadStrings[] = {"", "0", "123", "zz", "4g", "g4", "AF", "--", "-0", "00", "410042", "41-42", std::string{"41\0ff", 5}};
    for (size_t Field : {3u, 7u, 8u})
    {
        for (const auto& Bad : BadStrings)
        {
            SCOPED_TRACE(::testing::Message{} << "Field " << Field << ", value " << Bad);
            auto Fields   = MakeValidTokens();
            Fields[Field] = Bad;
            ExpectRejected(JoinTokens(Fields));
        }
    }
}

TEST(WGSLShaderReflection, RejectTruncatedInputAtEveryByte)
{
    const std::string Text = MakeMetadata(ReferencePayload);
    for (size_t Length = sizeof(Marker) - 1; Length < Text.size(); ++Length)
    {
        SCOPED_TRACE(Length);
        ExpectRejected(Text.substr(0, Length));
    }

    // Keep the closing delimiter, but truncate inside a required resource,
    // buffer, variable, or nested member record. This exercises the reader's
    // bounds independently of the initial check for a closing comment.
    const std::string    Payload = "1\n0 0 41 1\n64 1\n42 54 5 0 0 0 0 0 1\n43 - 1 11 1 1 0 0 0";
    WGSLShaderReflection Complete;
    ASSERT_TRUE(ParseWGSLShaderReflection(MakeMetadata(Payload), Complete));
    for (size_t Length = 3; Length < Payload.size(); ++Length)
    {
        SCOPED_TRACE(Length);
        ExpectRejected(MakeMetadata(Payload.substr(0, Length)));
    }
}

TEST(WGSLShaderReflection, RejectMismatchedCounts)
{
    for (const char* Count : {"0", "2", "4294967295"})
    {
        auto Fields = MakeValidTokens();
        Fields[6]   = Count;
        ExpectRejected(JoinTokens(Fields));
    }
    for (const char* Count : {"1", "2", "4294967295"})
    {
        auto Fields = MakeValidTokens();
        Fields[15]  = Count;
        ExpectRejected(JoinTokens(Fields));
    }
    ExpectRejected(MakeMetadata("1\n0 0 41 1\n64 0\n42 - 1 11 1 1 0 0 0\n"));
}

TEST(WGSLShaderReflection, RejectDuplicateBindings)
{
    ExpectRejected(MakeMetadata("1\n0 0 41 0\n0 0 42 0\n"));
    ExpectRejected(MakeMetadata("1\n0 0 41 1\n0 0\n0 0 41 0\n"));
    ExpectRejected(MakeMetadata("1\n0 0 41 0\n1 0 42 0\n0 0 43 0\n"));
}

TEST(WGSLShaderReflection, NestingLimit)
{
    for (unsigned Depth : {1u, 63u, 64u, 65u})
    {
        SCOPED_TRACE(Depth);
        std::string Payload = "1\n0 0 41 1\n16 1\n";
        for (unsigned Level = 0; Level < Depth; ++Level)
            Payload += Level + 1 == Depth ? "78 - 1 11 1 1 0 0 0\n" : "73 53 5 0 0 0 0 0 1\n";
        if (Depth > 64)
        {
            ExpectRejected(MakeMetadata(Payload));
            continue;
        }
        WGSLShaderReflection Reflection;
        ASSERT_TRUE(ParseWGSLShaderReflection(MakeMetadata(Payload), Reflection));
        const ShaderCodeVariableDescX* Var = &Reflection.Find(0, 0)->UniformBuffer->GetVariable(0);
        for (unsigned Level = 1; Level < Depth; ++Level)
        {
            ASSERT_EQ(Var->NumMembers, 1u);
            Var = &Var->GetMember(0);
        }
        EXPECT_EQ(Var->NumMembers, 0u);
        EXPECT_EQ(Serialize(Reflection), "\n" + MakeMetadata(Payload) + '\n');
    }
}

std::string MakeRandomName(std::mt19937& Random)
{
    std::string Name(Random() % 65, '\0');
    for (char& Ch : Name)
        Ch = static_cast<char>(1 + Random() % 255);
    return Name;
}

ShaderCodeVariableDescX MakeRandomVariable(std::mt19937& Random, unsigned Depth = 0)
{
    ShaderCodeVariableDescX Var;
    Var.SetName(MakeRandomName(Random));
    Var.SetTypeName(MakeRandomName(Random));
    Var.Class      = static_cast<SHADER_CODE_VARIABLE_CLASS>(Random() % SHADER_CODE_VARIABLE_CLASS_COUNT);
    Var.BasicType  = static_cast<SHADER_CODE_BASIC_TYPE>(Random() % SHADER_CODE_BASIC_TYPE_COUNT);
    Var.NumRows    = static_cast<Uint8>(Random());
    Var.NumColumns = static_cast<Uint8>(Random());
    Var.Offset     = Random();
    Var.ArraySize  = Random();
    if (Depth < 4 && Random() % 3 == 0)
    {
        Var.Class        = SHADER_CODE_VARIABLE_CLASS_STRUCT;
        const auto Count = Random() % 4;
        for (unsigned i = 0; i < Count; ++i)
            Var.AddMember(MakeRandomVariable(Random, Depth + 1));
    }
    return Var;
}

TEST(WGSLShaderReflection, GeneratedLayoutsRoundTrip)
{
    std::mt19937 Random{0x5747534c};
    for (unsigned Case = 0; Case < 100; ++Case)
    {
        SCOPED_TRACE(Case);
        WGSLShaderReflection Reflection;
        const unsigned       NumResources = Case == 99 ? 128 : Random() % 17;
        for (unsigned i = 0; i < NumResources; ++i)
        {
            auto& Resource = Reflection.Resources[{static_cast<Uint32>(Random() % 4), i}];
            Resource.Name  = MakeRandomName(Random);
            if (Random() % 2 != 0)
            {
                Resource.UniformBuffer.reset(new ShaderCodeBufferDescX{});
                Resource.UniformBuffer->Size = Random();
                const auto NumVariables      = Random() % 17;
                for (unsigned j = 0; j < NumVariables; ++j)
                    Resource.UniformBuffer->AddVariable(MakeRandomVariable(Random));
            }
        }
        CheckRoundTrip(Reflection);
    }
}

} // namespace
