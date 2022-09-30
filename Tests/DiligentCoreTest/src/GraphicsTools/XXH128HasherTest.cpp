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

#include "XXH128Hasher.hpp"
#include "gtest/gtest.h"
#include <memory>
#include <unordered_set>

using namespace Diligent;

namespace
{

template <typename CharType, size_t Size>
void TestStr(const CharType (&RefStr)[Size])
{
    auto CStr1 = std::make_unique<CharType[]>(Size);
    memcpy(CStr1.get(), RefStr, sizeof(RefStr));

    auto CStr2 = std::make_unique<CharType[]>(Size);
    memcpy(CStr2.get(), RefStr, sizeof(RefStr));

    XXH128State Hasher1;
    Hasher1.Update(CStr1.get());
    XXH128State Hasher2;
    Hasher2.Update(CStr2.get());
    EXPECT_EQ(Hasher1.Digest(), Hasher2.Digest());

    std::basic_string<CharType> StdStr1{RefStr};
    std::basic_string<CharType> StdStr2{RefStr};
    Hasher1.Update(StdStr1);
    Hasher2.Update(StdStr1);

    EXPECT_EQ(Hasher1.Digest(), Hasher2.Digest());
};

TEST(XXH128HasherTest, String)
{
    TestStr("01234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    TestStr(L"01234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
}





template <typename Type>
class XXH128HasherTestHelper
{
public:
    XXH128HasherTestHelper(const char* StructName) :
        m_StructName{StructName}
    {
    }

    void Add(const char* Msg)
    {
        XXH128State Hasher;
        Hasher.Update(m_Desc);
        EXPECT_TRUE(m_Hashes.insert(Hasher.Digest()).second) << Msg;
    }

    template <typename MemberType>
    void Add(MemberType& Member, const char* MemberName, MemberType Value)
    {
        Member = Value;
        std::stringstream ss;
        ss << m_StructName << '.' << MemberName << '=' << Value;
        Add(ss.str().c_str());
    }

    template <typename MemberType>
    typename std::enable_if<std::is_enum<MemberType>::value, void>::type
    AddRange(MemberType& Member, const char* MemberName, MemberType StartValue, MemberType EndValue)
    {
        for (typename std::underlying_type<MemberType>::type i = StartValue; i < EndValue; ++i)
        {
            Add(Member, MemberName, static_cast<MemberType>(i));
        }
    }

    template <typename MemberType>
    typename std::enable_if<std::is_floating_point<MemberType>::value || std::is_integral<MemberType>::value, void>::type
    AddRange(MemberType& Member, const char* MemberName, MemberType StartValue, MemberType EndValue, MemberType Step = MemberType{1})
    {
        for (auto i = StartValue; i <= EndValue; i += Step)
        {
            Add(Member, MemberName, static_cast<MemberType>(i));
            if (i == EndValue)
                break;
        }
    }

    void AddBool(bool& Member, const char* MemberName)
    {
        Add(Member, MemberName, !Member);
    }

    template <typename MemberType>
    void AddFlags(MemberType& Member, const char* MemberName, MemberType StartValue, MemberType EndValue)
    {
        for (Uint64 i = StartValue; i <= EndValue; i *= 2)
        {
            Add(Member, MemberName, static_cast<MemberType>(i));
            if (i == EndValue)
                break;
        }
    }

    template <typename MemberType>
    void AddStrings(MemberType& Member, const char* MemberName, const std::vector<const char*>& Strings)
    {
        for (const auto* Str : Strings)
        {
            Add(Member, MemberName, Str);
        }
    }

    Type& Get()
    {
        return m_Desc;
    }
    operator Type&()
    {
        return Get();
    }

private:
    const char* const m_StructName;

    Type m_Desc;
    bool m_DefaultOccured = false;

    std::unordered_set<XXH128Hash> m_Hashes;
};
#define DEFINE_HELPER(Type) \
    XXH128HasherTestHelper<Type> Helper { #Type }
#define TEST_VALUE(Member, ...)   Helper.Add(Helper.Get().Member, #Member, __VA_ARGS__)
#define TEST_RANGE(Member, ...)   Helper.AddRange(Helper.Get().Member, #Member, __VA_ARGS__)
#define TEST_BOOL(Member, ...)    Helper.AddBool(Helper.Get().Member, #Member)
#define TEST_FLAGS(Member, ...)   Helper.AddFlags(Helper.Get().Member, #Member, __VA_ARGS__)
#define TEST_STRINGS(Member, ...) Helper.AddStrings(Helper.Get().Member, #Member, {__VA_ARGS__})



TEST(XXH128HasherTest, ShaderCreateInfo)
{
    ASSERT_SIZEOF64(ShaderCreateInfo, 152, "Did you add new members to ShaderCreateInfo? Please update the tests.");
    DEFINE_HELPER(ShaderCreateInfo);

    TEST_STRINGS(Source, "Source1", "Source2", "Source3");
    TEST_RANGE(SourceLength, size_t{1}, size_t{5});

    Helper.Get().Source       = nullptr;
    constexpr uint32_t Data[] = {1, 2, 3, 4};
    Helper.Get().ByteCode     = Data;

    TEST_RANGE(ByteCodeSize, size_t{1}, size_t{8});

    TEST_STRINGS(EntryPoint, "Entry1", "Entry2", "Entry3");

    constexpr ShaderMacro Macros[] = {
        {"Macro1", "Def1"},
        {"Macro2", "Def2"},
        {"Macro3", "Def3"},
        {},
    };
    TEST_VALUE(Macros, Macros);
    TEST_BOOL(UseCombinedTextureSamplers);

    TEST_STRINGS(CombinedSamplerSuffix, "_sampler1", "_sampler2", "_sampler3");

    TEST_FLAGS(Desc.ShaderType, static_cast<SHADER_TYPE>(1), SHADER_TYPE_LAST);
    TEST_RANGE(SourceLanguage, static_cast<SHADER_SOURCE_LANGUAGE>(1), SHADER_SOURCE_LANGUAGE_COUNT);
    TEST_RANGE(ShaderCompiler, static_cast<SHADER_COMPILER>(1), SHADER_COMPILER_COUNT);

    TEST_RANGE(HLSLVersion.Minor, 1u, 10u);
    TEST_RANGE(HLSLVersion.Major, 1u, 10u);
    TEST_RANGE(GLSLVersion.Minor, 1u, 10u);
    TEST_RANGE(GLSLVersion.Major, 1u, 10u);
    TEST_RANGE(GLESSLVersion.Minor, 1u, 10u);
    TEST_RANGE(GLESSLVersion.Major, 1u, 10u);
    TEST_RANGE(MSLVersion.Minor, 1u, 10u);
    TEST_RANGE(MSLVersion.Major, 1u, 10u);

    TEST_FLAGS(CompileFlags, static_cast<SHADER_COMPILE_FLAGS>(1), SHADER_COMPILE_FLAG_LAST);
}

} // namespace
