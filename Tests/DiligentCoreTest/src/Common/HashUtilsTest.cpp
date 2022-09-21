/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include <unordered_map>
#include <unordered_set>
#include <array>
#include <vector>

#include "HashUtils.hpp"
#include "GraphicsTypesOutputInserters.hpp"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

TEST(Common_HashUtils, HashMapStringKey)
{
    {
        const char* Str = "Test String";

        HashMapStringKey Key1{Str};
        EXPECT_TRUE(Key1);
        EXPECT_EQ(Key1.GetStr(), Str);
        EXPECT_STREQ(Key1.GetStr(), Str);

        HashMapStringKey Key2{Str, true};
        EXPECT_NE(Key2.GetStr(), Str);
        EXPECT_STREQ(Key2.GetStr(), Str);

        EXPECT_EQ(Key1, Key1);
        EXPECT_EQ(Key2, Key2);
        EXPECT_EQ(Key1, Key2);

        HashMapStringKey Key3{std::string{Str}};
        EXPECT_NE(Key3.GetStr(), Str);
        EXPECT_STREQ(Key3.GetStr(), Str);

        EXPECT_EQ(Key3, Key1);
        EXPECT_EQ(Key3, Key2);
        EXPECT_EQ(Key3, Key3);
    }

    {
        const char*      Str1 = "Test String 1";
        const char*      Str2 = "Test String 2";
        HashMapStringKey Key1{Str1};
        HashMapStringKey Key2{Str2, true};
        EXPECT_NE(Key1, Key2);

        HashMapStringKey Key3{std::move(Key1)};
        EXPECT_NE(Key1, Key2);
        EXPECT_NE(Key2, Key1);

        HashMapStringKey Key4{std::move(Key2)};
        EXPECT_EQ(Key1, Key2);
        EXPECT_EQ(Key2, Key1);
        EXPECT_NE(Key3, Key4);
    }

    {
        std::unordered_map<HashMapStringKey, int> TestMap;

        const char* Str1 = "String1";
        const char* Str2 = "String2";
        const char* Str3 = "String3";
        const int   Val1 = 1;
        const int   Val2 = 2;

        auto it_ins = TestMap.emplace(HashMapStringKey{Str1, true}, Val1);
        EXPECT_TRUE(it_ins.second);
        EXPECT_NE(it_ins.first->first.GetStr(), Str1);
        EXPECT_STREQ(it_ins.first->first.GetStr(), Str1);

        it_ins = TestMap.emplace(Str2, Val2);
        EXPECT_TRUE(it_ins.second);
        EXPECT_EQ(it_ins.first->first, Str2);

        auto it = TestMap.find(Str1);
        ASSERT_NE(it, TestMap.end());
        EXPECT_EQ(it->second, Val1);
        EXPECT_NE(it->first.GetStr(), Str1);
        EXPECT_STREQ(it->first.GetStr(), Str1);

        it = TestMap.find(Str2);
        ASSERT_NE(it, TestMap.end());
        EXPECT_EQ(it->second, Val2);
        EXPECT_EQ(it->first.GetStr(), Str2);

        it = TestMap.find(Str3);
        EXPECT_EQ(it, TestMap.end());

        it = TestMap.find(HashMapStringKey{std::string{Str3}});
        EXPECT_EQ(it, TestMap.end());
    }

    {
        HashMapStringKey Key1;
        EXPECT_FALSE(Key1);

        HashMapStringKey Key2{"Key2", true};
        Key1 = std::move(Key2);
        EXPECT_TRUE(Key1);
        EXPECT_FALSE(Key2);
        EXPECT_STREQ(Key1.GetStr(), "Key2");

        HashMapStringKey Key3{"Key3", true};
        Key1 = Key3.Clone();
        EXPECT_TRUE(Key1);
        EXPECT_TRUE(Key3);
        EXPECT_NE(Key1.GetStr(), Key3.GetStr());
        EXPECT_STREQ(Key1.GetStr(), "Key3");

        Key1.Clear();
        EXPECT_FALSE(Key1);
        EXPECT_EQ(Key1.GetStr(), nullptr);

        Key2 = HashMapStringKey{"Key2"};
        Key1 = Key2.Clone();
        EXPECT_TRUE(Key1);
        EXPECT_TRUE(Key2);
        EXPECT_EQ(Key1.GetStr(), Key2.GetStr());
    }
}

TEST(Common_HashUtils, ComputeHashRaw)
{
    {
        std::array<Uint8, 16> Data{};
        for (Uint8 i = 0; i < Data.size(); ++i)
            Data[i] = 1u + i * 3u;

        std::unordered_set<size_t> Hashes;
        for (size_t start = 0; start < Data.size() - 1; ++start)
        {
            for (size_t size = 1; size <= Data.size() - start; ++size)
            {
                auto Hash = ComputeHashRaw(&Data[start], size);
                EXPECT_NE(Hash, size_t{0});
                auto inserted = Hashes.insert(Hash).second;
                EXPECT_TRUE(inserted) << Hash;
            }
        }
    }

    {
        std::array<Uint8, 16> RefData = {1, 3, 5, 7, 11, 13, 21, 35, 2, 4, 8, 10, 22, 40, 60, 82};
        for (size_t size = 1; size <= RefData.size(); ++size)
        {
            auto RefHash = ComputeHashRaw(RefData.data(), size);
            for (size_t offset = 0; offset < RefData.size() - size; ++offset)
            {
                std::array<Uint8, RefData.size()> Data{};
                std::copy(RefData.begin(), RefData.begin() + size, Data.begin() + offset);
                auto Hash = ComputeHashRaw(&Data[offset], size);
                EXPECT_EQ(RefHash, Hash) << offset << " " << size;
            }
        }
    }
}


template <typename Type>
class Common_HashUtilsHelper
{
public:
    Common_HashUtilsHelper(const char* StructName) :
        m_StructName{StructName}
    {
        EXPECT_TRUE(m_Hashes.insert(m_Hasher(m_Desc)).second);
        EXPECT_TRUE(m_Descs.insert(m_Desc).second);
    }

    void Add(const char* Msg)
    {
        if (m_Desc == Type{})
        {
            EXPECT_FALSE(m_DefaultOccured);
            m_DefaultOccured = true;
            return;
        }

        EXPECT_TRUE(m_Hashes.insert(m_Hasher(m_Desc)).second) << Msg;
        EXPECT_TRUE(m_Descs.insert(m_Desc).second) << Msg;

        EXPECT_FALSE(m_Desc == m_LastDesc) << Msg;
        EXPECT_TRUE(m_Desc != m_LastDesc) << Msg;
        m_LastDesc = m_Desc;
        EXPECT_TRUE(m_Desc == m_LastDesc) << Msg;
        EXPECT_FALSE(m_Desc != m_LastDesc) << Msg;
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
    AddRange(MemberType& Member, const char* MemberName, MemberType StartValue, MemberType EndValue, bool NoRestart = false)
    {
        if (!NoRestart)
            Restart();
        for (typename std::underlying_type<MemberType>::type i = StartValue; i < EndValue; ++i)
        {
            Add(Member, MemberName, static_cast<MemberType>(i));
        }
    }

    template <typename MemberType>
    typename std::enable_if<std::is_floating_point<MemberType>::value || std::is_integral<MemberType>::value, void>::type
    AddRange(MemberType& Member, const char* MemberName, MemberType StartValue, MemberType EndValue, MemberType Step = MemberType{1}, bool NoRestart = false)
    {
        if (!NoRestart)
            Restart();
        for (auto i = StartValue; i <= EndValue; i += Step)
        {
            Add(Member, MemberName, static_cast<MemberType>(i));
            if (i == EndValue)
                break;
        }
    }

    void AddBool(bool& Member, const char* MemberName)
    {
        Restart();
        Add(Member, MemberName, false);
        Add(Member, MemberName, true);
    }

    template <typename MemberType>
    void AddFlags(MemberType& Member, const char* MemberName, MemberType StartValue, MemberType EndValue)
    {
        Restart();
        for (Uint64 i = StartValue; i <= EndValue; i *= 2)
        {
            Add(Member, MemberName, static_cast<MemberType>(i));
            if (i == EndValue)
                break;
        }
    }

    template <typename MemberType>
    void AddStrings(MemberType& Member, const char* MemberName, const std::vector<const char*>& Strings, bool NoRestart = false)
    {
        if (!NoRestart)
            Restart();
        for (const auto* Str : Strings)
        {
            Add(Member, MemberName, Str);
        }
    }

    void Clear()
    {
        m_Hashes.clear();
        m_Descs.clear();
        Restart();
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
    void Restart()
    {
        m_Desc = m_LastDesc = {};
        m_DefaultOccured    = false;
    }

private:
    const char* const m_StructName;

    Type m_Desc;
    Type m_LastDesc;
    bool m_DefaultOccured = false;

    std::hash<Type>            m_Hasher;
    std::unordered_set<size_t> m_Hashes;
    std::unordered_set<Type>   m_Descs;
};
#define DEFINE_HELPER(Type) \
    Common_HashUtilsHelper<Type> Helper { #Type }
#define TEST_VALUE(Member, ...)   Helper.Add(Helper.Get().Member, #Member, __VA_ARGS__)
#define TEST_RANGE(Member, ...)   Helper.AddRange(Helper.Get().Member, #Member, __VA_ARGS__)
#define TEST_BOOL(Member, ...)    Helper.AddBool(Helper.Get().Member, #Member)
#define TEST_FLAGS(Member, ...)   Helper.AddFlags(Helper.Get().Member, #Member, __VA_ARGS__)
#define TEST_STRINGS(Member, ...) Helper.AddStrings(Helper.Get().Member, #Member, {__VA_ARGS__})

TEST(Common_HashUtils, SamplerDescHasher)
{
    ASSERT_SIZEOF64(SamplerDesc, 56, "Did you add new members to SamplerDesc? Please update the tests.");
    DEFINE_HELPER(SamplerDesc);

    TEST_RANGE(MinFilter, FILTER_TYPE_UNKNOWN, FILTER_TYPE_NUM_FILTERS);
    TEST_RANGE(MagFilter, FILTER_TYPE_UNKNOWN, FILTER_TYPE_NUM_FILTERS);
    TEST_RANGE(MipFilter, FILTER_TYPE_UNKNOWN, FILTER_TYPE_NUM_FILTERS);

    TEST_RANGE(AddressU, TEXTURE_ADDRESS_UNKNOWN, TEXTURE_ADDRESS_NUM_MODES);
    TEST_RANGE(AddressV, TEXTURE_ADDRESS_UNKNOWN, TEXTURE_ADDRESS_NUM_MODES);
    TEST_RANGE(AddressW, TEXTURE_ADDRESS_UNKNOWN, TEXTURE_ADDRESS_NUM_MODES);

    TEST_FLAGS(Flags, static_cast<SAMPLER_FLAGS>(1), SAMPLER_FLAG_LAST);
    TEST_BOOL(UnnormalizedCoords);
    TEST_RANGE(MipLODBias, -10.f, +10.f, 0.25f);

    TEST_RANGE(MaxAnisotropy, 0u, 16u);
    TEST_RANGE(ComparisonFunc, COMPARISON_FUNC_UNKNOWN, COMPARISON_FUNC_NUM_FUNCTIONS);
    TEST_RANGE(BorderColor[0], 1.f, 10.f, 0.25f);
    TEST_RANGE(BorderColor[1], 1.f, 10.f, 0.25f);
    TEST_RANGE(BorderColor[2], 1.f, 10.f, 0.25f);
    TEST_RANGE(BorderColor[3], 1.f, 10.f, 0.25f);
    TEST_RANGE(MinLOD, -10.f, +10.f, 0.25f);
    TEST_RANGE(MaxLOD, -10.f, +10.f, 0.25f);
}


TEST(Common_HashUtils, StencilOpDescHasher)
{
    ASSERT_SIZEOF(StencilOpDesc, 4, "Did you add new members to StencilOpDesc? Please update the tests.");
    DEFINE_HELPER(StencilOpDesc);

    TEST_RANGE(StencilFailOp, STENCIL_OP_UNDEFINED, STENCIL_OP_NUM_OPS);
    TEST_RANGE(StencilDepthFailOp, STENCIL_OP_UNDEFINED, STENCIL_OP_NUM_OPS);
    TEST_RANGE(StencilPassOp, STENCIL_OP_UNDEFINED, STENCIL_OP_NUM_OPS);
    TEST_RANGE(StencilFunc, COMPARISON_FUNC_UNKNOWN, COMPARISON_FUNC_NUM_FUNCTIONS);
}


TEST(Common_HashUtils, DepthStencilStateDescHasher)
{
    ASSERT_SIZEOF(DepthStencilStateDesc, 14, "Did you add new members to StencilOpDesc? Please update the tests.");
    DEFINE_HELPER(DepthStencilStateDesc);

    TEST_BOOL(DepthEnable);
    TEST_BOOL(DepthWriteEnable);
    TEST_RANGE(DepthFunc, COMPARISON_FUNC_UNKNOWN, COMPARISON_FUNC_NUM_FUNCTIONS);
    TEST_BOOL(StencilEnable);
    TEST_RANGE(StencilReadMask, Uint8{0u}, Uint8{255u});
    TEST_RANGE(StencilWriteMask, Uint8{0u}, Uint8{255u});
}


TEST(Common_HashUtils, RasterizerStateDescHasher)
{
    ASSERT_SIZEOF(RasterizerStateDesc, 20, "Did you add new members to RasterizerStateDesc? Please update the tests.");
    DEFINE_HELPER(RasterizerStateDesc);

    TEST_RANGE(FillMode, FILL_MODE_UNDEFINED, FILL_MODE_NUM_MODES);
    TEST_RANGE(CullMode, CULL_MODE_UNDEFINED, CULL_MODE_NUM_MODES);
    TEST_BOOL(FrontCounterClockwise);
    TEST_BOOL(DepthClipEnable);
    TEST_BOOL(ScissorEnable);
    TEST_BOOL(AntialiasedLineEnable);
    TEST_RANGE(DepthBias, -32, +32, 1);
    TEST_RANGE(DepthBiasClamp, -32.f, +32.f, 0.25f);
    TEST_RANGE(SlopeScaledDepthBias, -16.f, +16.f, 0.125f);
}


TEST(Common_HashUtils, BlendStateDescHasher)
{
    ASSERT_SIZEOF(BlendStateDesc, 82, "Did you add new members to RasterizerStateDesc? Please update the tests.");
    DEFINE_HELPER(BlendStateDesc);

    TEST_BOOL(AlphaToCoverageEnable);
    TEST_BOOL(IndependentBlendEnable);

    for (Uint32 rt = 0; rt < DILIGENT_MAX_RENDER_TARGETS; ++rt)
    {
        TEST_BOOL(RenderTargets[rt].BlendEnable);
        TEST_BOOL(RenderTargets[rt].LogicOperationEnable);
        TEST_RANGE(RenderTargets[rt].SrcBlend, BLEND_FACTOR_UNDEFINED, BLEND_FACTOR_NUM_FACTORS);
        TEST_RANGE(RenderTargets[rt].DestBlend, BLEND_FACTOR_UNDEFINED, BLEND_FACTOR_NUM_FACTORS);
        TEST_RANGE(RenderTargets[rt].BlendOp, BLEND_OPERATION_UNDEFINED, BLEND_OPERATION_NUM_OPERATIONS);
        TEST_RANGE(RenderTargets[rt].SrcBlendAlpha, BLEND_FACTOR_UNDEFINED, BLEND_FACTOR_NUM_FACTORS);
        TEST_RANGE(RenderTargets[rt].DestBlendAlpha, BLEND_FACTOR_UNDEFINED, BLEND_FACTOR_NUM_FACTORS);
        TEST_RANGE(RenderTargets[rt].BlendOpAlpha, BLEND_OPERATION_UNDEFINED, BLEND_OPERATION_NUM_OPERATIONS);
        TEST_RANGE(RenderTargets[rt].LogicOp, LOGIC_OP_CLEAR, LOGIC_OP_NUM_OPERATIONS);
        TEST_RANGE(RenderTargets[rt].RenderTargetWriteMask, COLOR_MASK_NONE, static_cast<COLOR_MASK>(COLOR_MASK_ALL + 1));
    }
}


TEST(Common_HashUtils, TextureViewDescHasher)
{
    ASSERT_SIZEOF64(TextureViewDesc, 32, "Did you add new members to TextureViewDesc? Please update the tests.");
    DEFINE_HELPER(TextureViewDesc);

    TEST_RANGE(ViewType, TEXTURE_VIEW_UNDEFINED, TEXTURE_VIEW_NUM_VIEWS);
    TEST_RANGE(TextureDim, RESOURCE_DIM_UNDEFINED, RESOURCE_DIM_NUM_DIMENSIONS);
    TEST_RANGE(Format, TEX_FORMAT_UNKNOWN, TEX_FORMAT_NUM_FORMATS);
    TEST_RANGE(MostDetailedMip, 0u, 32u);
    TEST_RANGE(NumMipLevels, 0u, 32u);
    TEST_RANGE(FirstArraySlice, 0u, 32u);
    TEST_RANGE(NumArraySlices, 0u, 2048u);
    TEST_FLAGS(AccessFlags, static_cast<UAV_ACCESS_FLAG>(1u), UAV_ACCESS_FLAG_LAST);
    TEST_FLAGS(Flags, static_cast<TEXTURE_VIEW_FLAGS>(1u), TEXTURE_VIEW_FLAG_LAST);
}


TEST(Common_HashUtils, SampleDescHasher)
{
    ASSERT_SIZEOF(SampleDesc, 2, "Did you add new members to SampleDesc? Please update the tests.");
    DEFINE_HELPER(SampleDesc);

    TEST_RANGE(Count, Uint8{0u}, Uint8{255u});
    TEST_RANGE(Quality, Uint8{0u}, Uint8{255u});
}


TEST(Common_HashUtils, ShaderResourceVariableDescHasher)
{
    ASSERT_SIZEOF64(ShaderResourceVariableDesc, 16, "Did you add new members to ShaderResourceVariableDesc? Please update the tests.");
    DEFINE_HELPER(ShaderResourceVariableDesc);

    TEST_STRINGS(Name, "Name1", "Name2", "Name3");
    TEST_FLAGS(ShaderStages, static_cast<SHADER_TYPE>(1), SHADER_TYPE_LAST);
    TEST_RANGE(Type, static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(0), SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES);
    TEST_FLAGS(Flags, static_cast<SHADER_VARIABLE_FLAGS>(1), SHADER_VARIABLE_FLAG_LAST);
}


TEST(Common_HashUtils, ImmutableSamplerDescHasher)
{
    ASSERT_SIZEOF64(ImmutableSamplerDesc, 16 + sizeof(SamplerDesc), "Did you add new members to ImmutableSamplerDesc? Please update the tests.");
    DEFINE_HELPER(ImmutableSamplerDesc);

    TEST_FLAGS(ShaderStages, static_cast<SHADER_TYPE>(1), SHADER_TYPE_LAST);
    TEST_STRINGS(SamplerOrTextureName, "Name1", "Name2", "Name3");
}


TEST(Common_HashUtils, PipelineResourceDescHasher)
{
    ASSERT_SIZEOF64(PipelineResourceDesc, 24, "Did you add new members to PipelineResourceDesc? Please update the tests.");
    DEFINE_HELPER(PipelineResourceDesc);

    TEST_STRINGS(Name, "Name1", "Name2", "Name3");
    TEST_FLAGS(ShaderStages, static_cast<SHADER_TYPE>(1), SHADER_TYPE_LAST);
    TEST_RANGE(ArraySize, 0u, 2048u);
    TEST_RANGE(ResourceType, SHADER_RESOURCE_TYPE_UNKNOWN, static_cast<SHADER_RESOURCE_TYPE>(SHADER_RESOURCE_TYPE_LAST + 1));
    TEST_RANGE(VarType, static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(0), SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES);
    TEST_FLAGS(Flags, static_cast<PIPELINE_RESOURCE_FLAGS>(1), PIPELINE_RESOURCE_FLAG_LAST);
}


TEST(Common_HashUtils, PipelineResourceLayoutDescHasher)
{
    ASSERT_SIZEOF64(PipelineResourceLayoutDesc, 40, "Did you add new members to PipelineResourceLayoutDesc? Please update the tests.");
    DEFINE_HELPER(PipelineResourceLayoutDesc);

    TEST_RANGE(DefaultVariableType, static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(0), SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES);
    TEST_FLAGS(DefaultVariableMergeStages, static_cast<SHADER_TYPE>(1), SHADER_TYPE_LAST);

    constexpr ShaderResourceVariableDesc Vars[] =
        {
            {SHADER_TYPE_VERTEX, "Var1", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_VARIABLE_FLAG_NO_DYNAMIC_BUFFERS},
            {SHADER_TYPE_PIXEL, "Var2", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, SHADER_VARIABLE_FLAG_GENERAL_INPUT_ATTACHMENT},
        };
    Helper.Get().Variables = Vars;
    TEST_VALUE(NumVariables, 1u);
    TEST_VALUE(NumVariables, 2u);

    constexpr ImmutableSamplerDesc ImtblSamplers[] = //
        {
            {SHADER_TYPE_VERTEX, "Sam1", SamplerDesc{}},
            {SHADER_TYPE_PIXEL, "Sam2", SamplerDesc{}} //
        };
    Helper.Get().ImmutableSamplers = ImtblSamplers;
    TEST_VALUE(NumImmutableSamplers, 1u);
    TEST_VALUE(NumImmutableSamplers, 2u);
}


TEST(Common_HashUtils, RenderPassAttachmentDescHasher)
{
    ASSERT_SIZEOF(RenderPassAttachmentDesc, 16, "Did you add new members to RenderPassAttachmentDesc? Please update the tests.");
    DEFINE_HELPER(RenderPassAttachmentDesc);

    TEST_RANGE(Format, TEX_FORMAT_UNKNOWN, TEX_FORMAT_NUM_FORMATS);
    TEST_RANGE(SampleCount, Uint8{1u}, Uint8{32u});
    TEST_RANGE(LoadOp, static_cast<ATTACHMENT_LOAD_OP>(0), ATTACHMENT_LOAD_OP_COUNT);
    TEST_RANGE(StoreOp, static_cast<ATTACHMENT_STORE_OP>(0), ATTACHMENT_STORE_OP_COUNT);
    TEST_RANGE(StencilLoadOp, static_cast<ATTACHMENT_LOAD_OP>(0), ATTACHMENT_LOAD_OP_COUNT);
    TEST_RANGE(StencilStoreOp, static_cast<ATTACHMENT_STORE_OP>(0), ATTACHMENT_STORE_OP_COUNT);
    TEST_FLAGS(InitialState, static_cast<RESOURCE_STATE>(1), RESOURCE_STATE_MAX_BIT);
    TEST_FLAGS(FinalState, static_cast<RESOURCE_STATE>(1), RESOURCE_STATE_MAX_BIT);
}


TEST(Common_HashUtils, AttachmentReferenceHasher)
{
    ASSERT_SIZEOF(AttachmentReference, 8, "Did you add new members to AttachmentReference? Please update the tests.");
    DEFINE_HELPER(AttachmentReference);

    TEST_RANGE(AttachmentIndex, 0u, 8u);
    TEST_FLAGS(State, static_cast<RESOURCE_STATE>(1), RESOURCE_STATE_MAX_BIT);
}


TEST(Common_HashUtils, ShadingRateAttachmentHasher)
{
    ASSERT_SIZEOF(ShadingRateAttachment, 16, "Did you add new members to ShadingRateAttachment? Please update the tests.");
    DEFINE_HELPER(ShadingRateAttachment);

    TEST_VALUE(Attachment, AttachmentReference{1, RESOURCE_STATE_RENDER_TARGET});
    TEST_VALUE(Attachment, AttachmentReference{2, RESOURCE_STATE_UNORDERED_ACCESS});

    TEST_RANGE(TileSize[0], 1u, 32u);
    TEST_RANGE(TileSize[1], 1u, 32u);
}


TEST(Common_HashUtils, SubpassDescHasher)
{
    ASSERT_SIZEOF64(SubpassDesc, 72, "Did you add new members to SubpassDesc? Please update the tests.");
    DEFINE_HELPER(SubpassDesc);

    constexpr AttachmentReference Inputs[] =
        {
            {1, RESOURCE_STATE_INPUT_ATTACHMENT},
            {3, RESOURCE_STATE_INPUT_ATTACHMENT},
            {5, RESOURCE_STATE_INPUT_ATTACHMENT},
        };
    Helper.Get().pInputAttachments = Inputs;
    TEST_VALUE(InputAttachmentCount, 1u);
    TEST_VALUE(InputAttachmentCount, 2u);
    TEST_VALUE(InputAttachmentCount, 3u);

    constexpr AttachmentReference RenderTargets[] =
        {
            {2, RESOURCE_STATE_RENDER_TARGET},
            {4, RESOURCE_STATE_UNORDERED_ACCESS},
            {6, RESOURCE_STATE_COMMON},
        };
    Helper.Get().pRenderTargetAttachments = RenderTargets;
    TEST_VALUE(RenderTargetAttachmentCount, 1u);
    TEST_VALUE(RenderTargetAttachmentCount, 2u);
    TEST_VALUE(RenderTargetAttachmentCount, 3u);

    constexpr AttachmentReference ResolveTargets[] =
        {
            {7, RESOURCE_STATE_RENDER_TARGET},
            {8, RESOURCE_STATE_UNORDERED_ACCESS},
            {9, RESOURCE_STATE_COMMON},
        };
    Helper.Get().pResolveAttachments = ResolveTargets;
    TEST_VALUE(RenderTargetAttachmentCount, 1u);
    TEST_VALUE(RenderTargetAttachmentCount, 2u);
    TEST_VALUE(RenderTargetAttachmentCount, 3u);

    constexpr AttachmentReference DepthStencil{10, RESOURCE_STATE_DEPTH_WRITE};
    TEST_VALUE(pDepthStencilAttachment, &DepthStencil);

    constexpr Uint32 Preserves[]      = {3, 4, 7};
    Helper.Get().pPreserveAttachments = Preserves;
    TEST_VALUE(PreserveAttachmentCount, 1u);
    TEST_VALUE(PreserveAttachmentCount, 2u);
    TEST_VALUE(PreserveAttachmentCount, 3u);

    constexpr ShadingRateAttachment SRA{{5, RESOURCE_STATE_SHADING_RATE}, 32, 64};
    TEST_VALUE(pShadingRateAttachment, &SRA);
}


TEST(Common_HashUtils, SubpassDependencyDescHasher)
{
    ASSERT_SIZEOF64(SubpassDependencyDesc, 24, "Did you add new members to SubpassDependencyDesc? Please update the tests.");
    DEFINE_HELPER(SubpassDependencyDesc);

    TEST_RANGE(SrcSubpass, 1u, 32u);
    TEST_RANGE(DstSubpass, 1u, 32u);
    TEST_FLAGS(SrcStageMask, static_cast<PIPELINE_STAGE_FLAGS>(1), PIPELINE_STAGE_FLAG_DEFAULT);
    TEST_FLAGS(DstStageMask, static_cast<PIPELINE_STAGE_FLAGS>(1), PIPELINE_STAGE_FLAG_DEFAULT);
    TEST_FLAGS(SrcAccessMask, static_cast<ACCESS_FLAGS>(1), ACCESS_FLAG_DEFAULT);
    TEST_FLAGS(DstAccessMask, static_cast<ACCESS_FLAGS>(1), ACCESS_FLAG_DEFAULT);
}


TEST(Common_HashUtils, RenderPassDescHasher)
{
    ASSERT_SIZEOF64(RenderPassDesc, 56, "Did you add new members to RenderPassDesc? Please update the tests.");
    DEFINE_HELPER(RenderPassDesc);

    RenderPassAttachmentDesc Attachments[3];
    Helper.Get().pAttachments = Attachments;
    TEST_VALUE(AttachmentCount, 1u);
    TEST_VALUE(AttachmentCount, 2u);
    TEST_VALUE(AttachmentCount, 3u);

    SubpassDesc Subpasses[3];
    Helper.Get().pSubpasses = Subpasses;
    TEST_VALUE(SubpassCount, 1u);
    TEST_VALUE(SubpassCount, 2u);
    TEST_VALUE(SubpassCount, 3u);

    SubpassDependencyDesc Deps[3];
    Helper.Get().pDependencies = Deps;
    TEST_VALUE(DependencyCount, 1u);
    TEST_VALUE(DependencyCount, 2u);
    TEST_VALUE(DependencyCount, 3u);
}


TEST(Common_HashUtils, LayoutElementHasher)
{
    ASSERT_SIZEOF64(LayoutElement, 40, "Did you add new members to LayoutElement? Please update the tests.");
    DEFINE_HELPER(LayoutElement);

    TEST_STRINGS(HLSLSemantic, "ATTRIB1", "ATTRIB2", "ATTRIB3");
    TEST_RANGE(InputIndex, 1u, 32u);
    TEST_RANGE(BufferSlot, 1u, 32u);
    TEST_RANGE(NumComponents, 1u, 8u);
    TEST_RANGE(ValueType, VT_UNDEFINED, VT_NUM_TYPES);
    TEST_BOOL(IsNormalized);
    TEST_RANGE(RelativeOffset, 0u, 1024u, 32u);
    TEST_RANGE(Stride, 16u, 1024u, 32u);
    TEST_RANGE(Frequency, INPUT_ELEMENT_FREQUENCY_UNDEFINED, INPUT_ELEMENT_FREQUENCY_NUM_FREQUENCIES);
    TEST_RANGE(InstanceDataStepRate, 0u, 64u);
}


TEST(Common_HashUtils, InputLayoutDescHasher)
{
    ASSERT_SIZEOF64(InputLayoutDesc, 16, "Did you add new members to InputLayoutDesc? Please update the tests.");
    DEFINE_HELPER(InputLayoutDesc);

    constexpr LayoutElement LayoutElems[] =
        {
            LayoutElement{0, 0, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{1, 0, 4, VT_UINT32, False, INPUT_ELEMENT_FREQUENCY_PER_VERTEX},
            LayoutElement{2, 1, 3, VT_UINT16, False, INPUT_ELEMENT_FREQUENCY_PER_VERTEX},
            LayoutElement{3, 3, 3, VT_UINT8, True, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{4, 5, 1, VT_INT8, True, INPUT_ELEMENT_FREQUENCY_PER_VERTEX},
        };
    Helper.Get().LayoutElements = LayoutElems;
    TEST_RANGE(NumElements, 0u, Uint32{_countof(LayoutElems)}, 1u, true);
}


TEST(Common_HashUtils, GraphicsPipelineDescHasher)
{
    DEFINE_HELPER(GraphicsPipelineDesc);

    TEST_FLAGS(SampleMask, 1u, 0xFFFFFFFFu);

    Helper.Get().BlendDesc.AlphaToCoverageEnable = True;
    Helper.Add("BlendDesc");

    Helper.Get().RasterizerDesc.ScissorEnable = True;
    Helper.Add("RasterizerDesc");

    Helper.Get().DepthStencilDesc.StencilEnable = True;
    Helper.Add("DepthStencilDesc");

    constexpr LayoutElement LayoutElems[] = {LayoutElement{0, 0, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}};
    Helper.Get().InputLayout              = {LayoutElems, 1};
    Helper.Add("InputLayout");

    TEST_RANGE(PrimitiveTopology, PRIMITIVE_TOPOLOGY_UNDEFINED, PRIMITIVE_TOPOLOGY_NUM_TOPOLOGIES);
    TEST_RANGE(NumRenderTargets, Uint8{0u}, Uint8{8u});
    TEST_RANGE(NumViewports, Uint8{1u}, Uint8{32u});
    TEST_RANGE(SubpassIndex, Uint8{1u}, Uint8{8u});
    TEST_FLAGS(ShadingRateFlags, static_cast<PIPELINE_SHADING_RATE_FLAGS>(1), PIPELINE_SHADING_RATE_FLAG_LAST);

    for (Uint8 i = 1; i < MAX_RENDER_TARGETS; ++i)
    {
        Helper.Get().NumRenderTargets = i;
        TEST_RANGE(RTVFormats[i - 1], TEX_FORMAT_UNKNOWN, TEX_FORMAT_NUM_FORMATS, true);
    }

    TEST_RANGE(DSVFormat, TEX_FORMAT_UNKNOWN, TEX_FORMAT_NUM_FORMATS);

    Helper.Get().SmplDesc.Count = 4;
    Helper.Add("SmplDesc");

    //IRenderPass* pRenderPass DEFAULT_INITIALIZER(nullptr);

    TEST_RANGE(NodeMask, 0u, 64u);
}


TEST(Common_HashUtils, RayTracingPipelineDescHasher)
{
    DEFINE_HELPER(RayTracingPipelineDesc);

    TEST_RANGE(ShaderRecordSize, Uint16{32u}, Uint16{48000u}, Uint16{1024u});
    TEST_RANGE(MaxRecursionDepth, Uint8{0u}, Uint8{32u});
}


TEST(Common_HashUtils, PipelineStateDescHasher)
{
    DEFINE_HELPER(PipelineStateDesc);

    TEST_RANGE(PipelineType, static_cast<PIPELINE_TYPE>(0), PIPELINE_TYPE_COUNT);
    TEST_RANGE(SRBAllocationGranularity, 0u, 64u);
    TEST_FLAGS(ImmediateContextMask, Uint64{1u}, Uint64{1u} << Uint64{63u});

    Helper.Get().ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
    Helper.Add("ResourceLayout");
}


TEST(Common_HashUtils, PipelineResourceSignatureDescHasher)
{
    ASSERT_SIZEOF64(PipelineResourceSignatureDesc, 56, "Did you add new members to PipelineResourceSignatureDesc? Please update the tests.");
    DEFINE_HELPER(PipelineResourceSignatureDesc);

    constexpr PipelineResourceDesc Resources[] = //
        {
            {SHADER_TYPE_VERTEX, "Res1", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC, PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS},
            {SHADER_TYPE_PIXEL, "Res2", 2, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER} //
        };

    constexpr ImmutableSamplerDesc ImtblSamplers[] = //
        {
            {SHADER_TYPE_VERTEX, "Sam1", SamplerDesc{}},
            {SHADER_TYPE_PIXEL, "Sam2", SamplerDesc{}} //
        };

    Helper.Get().Resources = Resources;
    TEST_VALUE(NumResources, 1u);
    TEST_VALUE(NumResources, 2u);

    Helper.Get().ImmutableSamplers = ImtblSamplers;
    TEST_VALUE(NumImmutableSamplers, 1u);
    TEST_VALUE(NumImmutableSamplers, 2u);

    TEST_RANGE(BindingIndex, Uint8{0u}, Uint8{8u});
    TEST_BOOL(UseCombinedTextureSamplers);

    Helper.Get().UseCombinedTextureSamplers = true;
    Helper.AddStrings(Helper.Get().CombinedSamplerSuffix, "CombinedSamplerSuffix", {"_Sampler", "_sam", "_Samp"}, true);
}

} // namespace
