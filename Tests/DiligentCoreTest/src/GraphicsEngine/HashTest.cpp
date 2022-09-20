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

#include "RenderDeviceBase.hpp"
#include "GraphicsTypesOutputInserters.hpp"
#include "PlatformMisc.hpp"

#include <unordered_set>

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

template <typename Type>
class HashTestHelper
{
public:
    HashTestHelper(const char* StructName) :
        m_StructName{StructName}
    {
        EXPECT_TRUE(m_Hashes.insert(m_Hasher(m_Desc)).second);
        EXPECT_TRUE(m_Descs.insert(m_Desc).second);
    }

    template <typename MemberType>
    void Add(MemberType& Member, const char* MemberName, MemberType Value)
    {
        Member = Value;
        if (m_Desc == Type{})
        {
            EXPECT_FALSE(m_DefaultOccured);
            m_DefaultOccured = true;
            return;
        }

        EXPECT_TRUE(m_Hashes.insert(m_Hasher(m_Desc)).second) << m_StructName << '.' << MemberName << '=' << Value;
        EXPECT_TRUE(m_Descs.insert(m_Desc).second) << m_StructName << '.' << MemberName << '=' << Value;

        EXPECT_FALSE(m_Desc == m_LastDesc) << m_StructName << '.' << MemberName << '=' << Value;
        EXPECT_TRUE(m_Desc != m_LastDesc) << m_StructName << '.' << MemberName << '=' << Value;
        m_LastDesc = m_Desc;
        EXPECT_TRUE(m_Desc == m_LastDesc) << m_StructName << '.' << MemberName << '=' << Value;
        EXPECT_FALSE(m_Desc != m_LastDesc) << m_StructName << '.' << MemberName << '=' << Value;
    }

    template <typename MemberType>
    typename std::enable_if<std::is_enum<MemberType>::value, void>::type
    AddRange(MemberType& Member, const char* MemberName, MemberType StartValue, MemberType EndValue)
    {
        Restart();
        for (typename std::underlying_type<MemberType>::type i = StartValue; i < EndValue; ++i)
        {
            Add(Member, MemberName, static_cast<MemberType>(i));
        }
    }

    template <typename MemberType>
    typename std::enable_if<std::is_floating_point<MemberType>::value || std::is_integral<MemberType>::value, void>::type
    AddRange(MemberType& Member, const char* MemberName, MemberType StartValue, MemberType EndValue, MemberType Step)
    {
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
#define TEST_RANGE(Member, ...) Helper.AddRange(Helper.Get().Member, #Member, __VA_ARGS__)
#define TEST_BOOL(Member, ...)  Helper.AddBool(Helper.Get().Member, #Member)
#define TEST_FLAGS(Member, ...) Helper.AddFlags(Helper.Get().Member, #Member, __VA_ARGS__)

TEST(HasherTest, SamplerDesc)
{
    ASSERT_SIZEOF64(SamplerDesc, 56, "Did you add new members to SamplerDesc? Please update the tests.");
    HashTestHelper<SamplerDesc> Helper{"SamplerDesc"};

    TEST_RANGE(MinFilter, FILTER_TYPE_UNKNOWN, FILTER_TYPE_NUM_FILTERS);
    TEST_RANGE(MagFilter, FILTER_TYPE_UNKNOWN, FILTER_TYPE_NUM_FILTERS);
    TEST_RANGE(MipFilter, FILTER_TYPE_UNKNOWN, FILTER_TYPE_NUM_FILTERS);

    TEST_RANGE(AddressU, TEXTURE_ADDRESS_UNKNOWN, TEXTURE_ADDRESS_NUM_MODES);
    TEST_RANGE(AddressV, TEXTURE_ADDRESS_UNKNOWN, TEXTURE_ADDRESS_NUM_MODES);
    TEST_RANGE(AddressW, TEXTURE_ADDRESS_UNKNOWN, TEXTURE_ADDRESS_NUM_MODES);

    TEST_FLAGS(Flags, static_cast<SAMPLER_FLAGS>(1), SAMPLER_FLAG_LAST);
    TEST_BOOL(UnnormalizedCoords);
    TEST_RANGE(MipLODBias, -10.f, +10.f, 0.25f);

    TEST_RANGE(MaxAnisotropy, 0u, 16u, 1u);
    TEST_RANGE(ComparisonFunc, COMPARISON_FUNC_UNKNOWN, COMPARISON_FUNC_NUM_FUNCTIONS);
    TEST_RANGE(BorderColor[0], 1.f, 10.f, 0.25f);
    TEST_RANGE(BorderColor[1], 1.f, 10.f, 0.25f);
    TEST_RANGE(BorderColor[2], 1.f, 10.f, 0.25f);
    TEST_RANGE(BorderColor[3], 1.f, 10.f, 0.25f);
    TEST_RANGE(MinLOD, -10.f, +10.f, 0.25f);
    TEST_RANGE(MaxLOD, -10.f, +10.f, 0.25f);
}


TEST(HasherTest, StencilOpDesc)
{
    ASSERT_SIZEOF(StencilOpDesc, 4, "Did you add new members to StencilOpDesc? Please update the tests.");
    HashTestHelper<StencilOpDesc> Helper{"StencilOpDesc"};

    TEST_RANGE(StencilFailOp, STENCIL_OP_UNDEFINED, STENCIL_OP_NUM_OPS);
    TEST_RANGE(StencilDepthFailOp, STENCIL_OP_UNDEFINED, STENCIL_OP_NUM_OPS);
    TEST_RANGE(StencilPassOp, STENCIL_OP_UNDEFINED, STENCIL_OP_NUM_OPS);
    TEST_RANGE(StencilFunc, COMPARISON_FUNC_UNKNOWN, COMPARISON_FUNC_NUM_FUNCTIONS);
}


TEST(HasherTest, DepthStencilStateDesc)
{
    ASSERT_SIZEOF(DepthStencilStateDesc, 14, "Did you add new members to StencilOpDesc? Please update the tests.");
    HashTestHelper<DepthStencilStateDesc> Helper{"DepthStencilStateDesc"};

    TEST_BOOL(DepthEnable);
    TEST_BOOL(DepthWriteEnable);
    TEST_RANGE(DepthFunc, COMPARISON_FUNC_UNKNOWN, COMPARISON_FUNC_NUM_FUNCTIONS);
    TEST_BOOL(StencilEnable);
    TEST_RANGE(StencilReadMask, Uint8{0u}, Uint8{255u}, Uint8{1u});
    TEST_RANGE(StencilWriteMask, Uint8{0u}, Uint8{255u}, Uint8{1u});
}


TEST(HasherTest, RasterizerStateDesc)
{
    ASSERT_SIZEOF(RasterizerStateDesc, 20, "Did you add new members to RasterizerStateDesc? Please update the tests.");
    HashTestHelper<RasterizerStateDesc> Helper{"RasterizerStateDesc"};

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


TEST(HasherTest, BlendStateDesc)
{
    ASSERT_SIZEOF(BlendStateDesc, 82, "Did you add new members to RasterizerStateDesc? Please update the tests.");
    HashTestHelper<BlendStateDesc> Helper{"BlendStateDesc"};

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


TEST(HasherTest, TextureViewDesc)
{
    ASSERT_SIZEOF64(TextureViewDesc, 32, "Did you add new members to TextureViewDesc? Please update the tests.");
    HashTestHelper<TextureViewDesc> Helper{"TextureViewDesc"};

    TEST_RANGE(ViewType, TEXTURE_VIEW_UNDEFINED, TEXTURE_VIEW_NUM_VIEWS);
    TEST_RANGE(TextureDim, RESOURCE_DIM_UNDEFINED, RESOURCE_DIM_NUM_DIMENSIONS);
    TEST_RANGE(Format, TEX_FORMAT_UNKNOWN, TEX_FORMAT_NUM_FORMATS);
    TEST_RANGE(MostDetailedMip, 0u, 32u, 1u);
    TEST_RANGE(NumMipLevels, 0u, 32u, 1u);
    TEST_RANGE(FirstArraySlice, 0u, 32u, 1u);
    TEST_RANGE(NumArraySlices, 0u, 2048u, 1u);
    TEST_FLAGS(AccessFlags, static_cast<UAV_ACCESS_FLAG>(1u), UAV_ACCESS_FLAG_LAST);
    TEST_FLAGS(Flags, static_cast<TEXTURE_VIEW_FLAGS>(1u), TEXTURE_VIEW_FLAG_LAST);
}

} // namespace
