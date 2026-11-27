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

#include "../../../../Graphics/GraphicsEngine/include/PipelineResourceSignatureBase.hpp"
#include "CommonlyUsedStates.h"
#include "TestingEnvironment.hpp"
#include "StringTools.hpp"

#include "gtest/gtest.h"

#include <array>
#include <limits>

using namespace Diligent;

namespace
{

TEST(PipelineResourceSignatureBaseTest, ImmutableSamplersDoNotConsumeResourceCacheOffsets)
{
    // One sampler must not push the resource count past the largest accepted total.
    // Validation only inspects the descriptors; it does not allocate the resource array.
    const PipelineResourceDesc Resource{SHADER_TYPE_PIXEL, "Texture", std::numeric_limits<Uint32>::max() - 1,
                                        SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE};

    const ImmutableSamplerDesc Sampler{SHADER_TYPE_PIXEL, "Sampler", Sam_LinearWrap};

    PipelineResourceSignatureDesc Desc;
    Desc.NumResources         = 1;
    Desc.Resources            = &Resource;
    Desc.NumImmutableSamplers = 1;
    Desc.ImmutableSamplers    = &Sampler;
    EXPECT_NO_THROW(ValidatePipelineResourceSignatureDesc(Desc, nullptr, RENDER_DEVICE_TYPE_D3D12));
}

TEST(PipelineResourceSignatureBaseTest, CountValidation)
{
    PipelineResourceDesc Resources[] = {
        {SHADER_TYPE_PIXEL, "Resource0", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "Resource1", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    };
    PipelineResourceSignatureDesc Desc;
    Desc.Resources    = Resources;
    Desc.NumResources = _countof(Resources);

    EXPECT_NO_THROW(ValidatePipelineResourceSignatureDesc(Desc, nullptr, RENDER_DEVICE_TYPE_D3D12));

    const auto ExpectFailure = [&Desc](const char* Message) {
        Testing::TestingEnvironment::ErrorScope Errors{Message};
        EXPECT_THROW(ValidatePipelineResourceSignatureDesc(Desc, nullptr, RENDER_DEVICE_TYPE_D3D12), std::runtime_error);
    };

    Resources[0].ArraySize = std::numeric_limits<Uint32>::max() - 1;
    ExpectFailure("Total resource count, including array elements, exceeds the maximum representable value");
    Resources[0].ArraySize = 1u << 31;
    Resources[1].ArraySize = 1u << 31;
    ExpectFailure("Total resource count, including array elements, exceeds the maximum representable value");

    Desc.NumResources = MAX_RESOURCES_IN_SIGNATURE + 1;
    ExpectFailure("Desc.NumResources");
    Desc.NumResources = 1;
    Desc.Resources    = nullptr;
    ExpectFailure("Desc.Resources is null");
    Desc.Resources         = Resources;
    Resources[0].ArraySize = 0;
    ExpectFailure("ArraySize must not be 0");

    Resources[0].Flags     = PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS;
    Resources[0].ArraySize = MAX_INLINE_CONSTANTS;
    EXPECT_NO_THROW(ValidatePipelineResourceSignatureDesc(Desc, nullptr, RENDER_DEVICE_TYPE_D3D12));
    Resources[0].ArraySize = std::numeric_limits<Uint32>::max();
    ExpectFailure("for inline constants");

    Desc.NumResources      = 2;
    Resources[0].ArraySize = MAX_INLINE_CONSTANTS;
    Resources[1].Flags     = PIPELINE_RESOURCE_FLAG_INLINE_CONSTANTS;
    Resources[1].ArraySize = 1;
    ExpectFailure("Total number of inline constant values");
    Resources[0].ArraySize -= 1;
    EXPECT_NO_THROW(ValidatePipelineResourceSignatureDesc(Desc, nullptr, RENDER_DEVICE_TYPE_D3D12));
}

void ExpectInvalidShaderStages(const PipelineResourceSignatureDesc& Desc,
                               SHADER_TYPE                          ShaderStages = SHADER_TYPE_UNKNOWN,
                               const char*                          Error        = "must belong to a single pipeline type")
{
    Testing::TestingEnvironment::ErrorScope Errors{Error};
    EXPECT_THROW(ValidatePipelineResourceSignatureDesc(Desc, nullptr, RENDER_DEVICE_TYPE_D3D12, ShaderStages), std::runtime_error);
}

TEST(PipelineResourceSignatureBaseTest, CompatibleShaderStages)
{
    PipelineResourceDesc          Resource{SHADER_TYPE_PIXEL, "Resource", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE};
    ImmutableSamplerDesc          Sampler{SHADER_TYPE_PIXEL, "Sampler", Sam_LinearWrap};
    PipelineResourceSignatureDesc Desc;
    Desc.Resources         = &Resource;
    Desc.ImmutableSamplers = &Sampler;
    EXPECT_NO_THROW(ValidatePipelineResourceSignatureDesc(Desc, nullptr, RENDER_DEVICE_TYPE_D3D12));

    // Check every nonempty subset, including pixel-only stages shared by graphics and mesh pipelines.
    const SHADER_TYPE PipelineStages[] = {SHADER_TYPE_ALL_GRAPHICS, SHADER_TYPE_ALL_MESH,
                                          SHADER_TYPE_COMPUTE, SHADER_TYPE_ALL_RAY_TRACING, SHADER_TYPE_TILE};
    for (SHADER_TYPE Stages : PipelineStages)
    {
        for (Uint32 Subset = Stages; Subset != 0; Subset = (Subset - 1) & Stages)
        {
            SCOPED_TRACE(::testing::Message() << "Stages: " << Subset);
            Resource.ShaderStages = static_cast<SHADER_TYPE>(Subset);
            Sampler.ShaderStages  = Resource.ShaderStages;

            Desc.NumResources         = 1;
            Desc.NumImmutableSamplers = 0;
            EXPECT_NO_THROW(ValidatePipelineResourceSignatureDesc(Desc, nullptr, RENDER_DEVICE_TYPE_D3D12));

            Desc.NumResources         = 0;
            Desc.NumImmutableSamplers = 1;
            EXPECT_NO_THROW(ValidatePipelineResourceSignatureDesc(Desc, nullptr, RENDER_DEVICE_TYPE_D3D12));

            Desc.NumImmutableSamplers = 0;
            EXPECT_NO_THROW(ValidatePipelineResourceSignatureDesc(Desc, nullptr, RENDER_DEVICE_TYPE_D3D12, Resource.ShaderStages));
        }
    }
}

TEST(PipelineResourceSignatureBaseTest, IncompatibleShaderStages)
{
    PipelineResourceDesc Resources[] = {
        {SHADER_TYPE_PIXEL, "Resource0", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "Resource1", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    };
    ImmutableSamplerDesc Samplers[] = {
        {SHADER_TYPE_PIXEL, "Sampler0", Sam_LinearWrap},
        {SHADER_TYPE_PIXEL, "Sampler1", Sam_LinearWrap},
    };
    PipelineResourceSignatureDesc Desc;
    Desc.Resources         = Resources;
    Desc.ImmutableSamplers = Samplers;

    // Each pair mixes two incompatible pipeline families. Use distinct names so that
    // validation must inspect the combined stages rather than duplicate declarations.
    const SHADER_TYPE PipelineStages[] = {SHADER_TYPE_VERTEX, SHADER_TYPE_MESH,
                                          SHADER_TYPE_COMPUTE, SHADER_TYPE_RAY_GEN, SHADER_TYPE_TILE};
    for (size_t i = 0; i < _countof(PipelineStages); ++i)
    {
        for (size_t j = i + 1; j < _countof(PipelineStages); ++j)
        {
            const SHADER_TYPE Stages = PipelineStages[i] | PipelineStages[j];
            SCOPED_TRACE(::testing::Message() << "Stages: " << Uint32{Stages});

            Desc.NumResources         = 1;
            Desc.NumImmutableSamplers = 0;
            Resources[0].ShaderStages = Stages;
            ExpectInvalidShaderStages(Desc);

            Desc.NumResources         = 2;
            Resources[0].ShaderStages = PipelineStages[i];
            Resources[1].ShaderStages = PipelineStages[j];
            ExpectInvalidShaderStages(Desc);

            Desc.NumResources = 1;
            ExpectInvalidShaderStages(Desc, PipelineStages[j]);

            Desc.NumResources         = 0;
            Desc.NumImmutableSamplers = 1;
            Samplers[0].ShaderStages  = Stages;
            ExpectInvalidShaderStages(Desc);

            Desc.NumImmutableSamplers = 2;
            Samplers[0].ShaderStages  = PipelineStages[i];
            Samplers[1].ShaderStages  = PipelineStages[j];
            ExpectInvalidShaderStages(Desc);
        }
    }

    Desc.NumResources         = 1;
    Desc.NumImmutableSamplers = 0;
    Resources[0].ShaderStages = SHADER_TYPE_ALL;
    ExpectInvalidShaderStages(Desc);

    // A standalone all-stage sampler must fail before pooled variable-data allocation.
    Desc.NumResources             = 0;
    Desc.NumImmutableSamplers     = 1;
    Desc.SRBAllocationGranularity = 4;
    Samplers[0].ShaderStages      = SHADER_TYPE_ALL;
    ExpectInvalidShaderStages(Desc);
}

TEST(PipelineResourceSignatureBaseTest, InvalidShaderStageBits)
{
    PipelineResourceDesc          Resource{SHADER_TYPE_PIXEL, "Resource", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE};
    ImmutableSamplerDesc          Sampler{SHADER_TYPE_PIXEL, "Sampler", Sam_LinearWrap};
    PipelineResourceSignatureDesc Desc;
    Desc.Resources         = &Resource;
    Desc.ImmutableSamplers = &Sampler;

    const Uint32 InvalidMasks[] = {Uint32{SHADER_TYPE_LAST} << 1, Uint32{1} << 31,
                                   (Uint32{SHADER_TYPE_LAST} << 1) | SHADER_TYPE_PIXEL,
                                   (Uint32{1} << 31) | SHADER_TYPE_PIXEL};
    for (Uint32 Mask : InvalidMasks)
    {
        SCOPED_TRACE(::testing::Message() << "Stages: " << Mask);
        const auto ResourceError = FormatString("Desc.Resources[0].ShaderStages (", Mask, ") contains invalid shader stage bits.");
        const auto SamplerError  = FormatString("Desc.ImmutableSamplers[0].ShaderStages (", Mask, ") contains invalid shader stage bits.");

        Resource.ShaderStages = static_cast<SHADER_TYPE>(Mask);
        Sampler.ShaderStages  = Resource.ShaderStages;

        Desc.NumResources         = 1;
        Desc.NumImmutableSamplers = 0;
        ExpectInvalidShaderStages(Desc, SHADER_TYPE_UNKNOWN, ResourceError.c_str());

        // Reject invalid bits before input-attachment diagnostics format the stage names.
        Resource.ResourceType = SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT;
        ExpectInvalidShaderStages(Desc, SHADER_TYPE_UNKNOWN, ResourceError.c_str());
        Resource.ResourceType = SHADER_RESOURCE_TYPE_TEXTURE_SRV;

        Desc.NumResources         = 0;
        Desc.NumImmutableSamplers = 1;
        ExpectInvalidShaderStages(Desc, SHADER_TYPE_UNKNOWN, SamplerError.c_str());

        // A known resource or PSO pipeline must not hide undefined sampler visibility bits.
        Resource.ShaderStages = SHADER_TYPE_PIXEL;
        Desc.NumResources     = 1;
        ExpectInvalidShaderStages(Desc, SHADER_TYPE_UNKNOWN, SamplerError.c_str());

        Desc.NumResources = 0;
        ExpectInvalidShaderStages(Desc, SHADER_TYPE_PIXEL, SamplerError.c_str());
    }
}

TEST(PipelineResourceSignatureBaseTest, ImmutableSamplerStageVisibility)
{
    PipelineResourceDesc          Resource{SHADER_TYPE_PIXEL, "Resource", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE};
    const ImmutableSamplerDesc    Sampler{SHADER_TYPE_ALL, "Sampler", Sam_LinearWrap};
    PipelineResourceSignatureDesc Desc;
    Desc.Resources                = &Resource;
    Desc.ImmutableSamplers        = &Sampler;
    Desc.NumImmutableSamplers     = 1;
    Desc.SRBAllocationGranularity = 4;

    const SHADER_TYPE PipelineStages[] = {SHADER_TYPE_ALL_GRAPHICS, SHADER_TYPE_ALL_MESH,
                                          SHADER_TYPE_COMPUTE, SHADER_TYPE_ALL_RAY_TRACING, SHADER_TYPE_TILE};
    for (SHADER_TYPE Stages : PipelineStages)
    {
        SCOPED_TRACE(::testing::Message() << "Stages: " << Uint32{Stages});
        // Broad immutable visibility does not widen a pipeline type established by resources.
        Resource.ShaderStages = Stages;
        Desc.NumResources     = 1;
        EXPECT_NO_THROW(ValidatePipelineResourceSignatureDesc(Desc, nullptr, RENDER_DEVICE_TYPE_D3D12));

        // Implicit signatures also retain the pipeline type supplied by their PSO,
        // including when no ordinary resources are reflected by the shaders.
        Desc.NumResources = 0;
        EXPECT_NO_THROW(ValidatePipelineResourceSignatureDesc(Desc, nullptr, RENDER_DEVICE_TYPE_D3D12, Stages));
    }
}

TEST(PipelineResourceSignatureBaseTest, Compatibility)
{
    {
        PipelineResourceSignatureDesc NullDesc1;
        PipelineResourceSignatureDesc NullDesc2;
        EXPECT_TRUE(PipelineResourceSignaturesCompatible(NullDesc1, NullDesc2));
        EXPECT_EQ(CalculatePipelineResourceSignatureDescHash(NullDesc1), CalculatePipelineResourceSignatureDescHash(NullDesc2));

        NullDesc2.BindingIndex = 1;
        EXPECT_FALSE(PipelineResourceSignaturesCompatible(NullDesc1, NullDesc2));
        EXPECT_NE(CalculatePipelineResourceSignatureDescHash(NullDesc1), CalculatePipelineResourceSignatureDescHash(NullDesc2));
    }

    const PipelineResourceDesc RefRes[] = //
        {
            {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "Buff", 2u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS | PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY},
            {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "Tex", 4u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER | PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY} //
        };

    const ImmutableSamplerDesc RefSam[] = //
        {
            {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "SamA", Sam_LinearMirror},
            {SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "SamB", Sam_LinearWrap} //
        };

    PipelineResourceSignatureDesc RefDesc;
    RefDesc.Name                 = "Ref PRS Desc";
    RefDesc.NumResources         = _countof(RefRes);
    RefDesc.Resources            = RefRes;
    RefDesc.NumImmutableSamplers = _countof(RefSam);
    RefDesc.ImmutableSamplers    = RefSam;

    const auto RefHash = CalculatePipelineResourceSignatureDescHash(RefDesc);

    PipelineResourceSignatureDesc       TestDesc;
    std::array<PipelineResourceDesc, 2> TestRes = {};
    std::array<ImmutableSamplerDesc, 2> TestSam = {};

    auto Test = [&](const auto& ModifyTestDesc) //
    {
        TestDesc = {};

        TestDesc.Name = "Test PRS Desc";

        TestRes[0]      = RefRes[0];
        TestRes[1]      = RefRes[1];
        TestRes[0].Name = "OtherBuff";
        TestRes[1].Name = "OtherTex";
        VERIFY_EXPR(TestRes[0] != RefRes[0]);
        VERIFY_EXPR(TestRes[1] != RefRes[1]);

        TestSam[0]                      = RefSam[0];
        TestSam[1]                      = RefSam[1];
        TestSam[0].SamplerOrTextureName = "OtherSamA";
        TestSam[1].SamplerOrTextureName = "OtherSamB";
        VERIFY_EXPR(TestSam[0] != RefSam[0]);
        VERIFY_EXPR(TestSam[1] != RefSam[1]);

        TestDesc.NumResources         = StaticCast<Uint32>(TestRes.size());
        TestDesc.Resources            = TestRes.data();
        TestDesc.NumImmutableSamplers = StaticCast<Uint32>(TestSam.size());
        TestDesc.ImmutableSamplers    = TestSam.data();
        VERIFY_EXPR(TestDesc.Resources != RefDesc.Resources);
        VERIFY_EXPR(TestDesc.ImmutableSamplers != RefDesc.ImmutableSamplers);

        EXPECT_TRUE(PipelineResourceSignaturesCompatible(TestDesc, RefDesc));
        EXPECT_EQ(RefHash, CalculatePipelineResourceSignatureDescHash(TestDesc));

        ModifyTestDesc();

        EXPECT_FALSE(PipelineResourceSignaturesCompatible(TestDesc, RefDesc));
        EXPECT_NE(RefHash, CalculatePipelineResourceSignatureDescHash(TestDesc));
    };

    Test([&TestDesc]() { TestDesc.NumResources = 1; });
    Test([&TestDesc]() { TestDesc.NumImmutableSamplers = 1; });
    Test([&TestDesc]() { TestDesc.BindingIndex = 1; });
    for (SHADER_TYPE ShaderType = static_cast<SHADER_TYPE>(1u); ShaderType <= SHADER_TYPE_LAST; ShaderType = static_cast<SHADER_TYPE>(ShaderType * 2))
    {
        Test([&TestRes, ShaderType]() { TestRes[0].ShaderStages = ShaderType; });
        Test([&TestRes, ShaderType]() { TestRes[1].ShaderStages = ShaderType; });
        Test([&TestSam, ShaderType]() { TestSam[0].ShaderStages = ShaderType; });
        Test([&TestSam, ShaderType]() { TestSam[1].ShaderStages = ShaderType; });
    }

    for (Uint32 ArrSize = 0; ArrSize < 64; ++ArrSize)
    {
        Test([&TestRes, ArrSize]() { TestRes[0].ArraySize = (TestRes[0].ArraySize != ArrSize) ? ArrSize : 128; });
        Test([&TestRes, ArrSize]() { TestRes[1].ArraySize = (TestRes[1].ArraySize != ArrSize) ? ArrSize : 128; });
    }

    {
        size_t Count = 0;
        for (SHADER_RESOURCE_TYPE ResType = SHADER_RESOURCE_TYPE_UNKNOWN;
             ResType <= SHADER_RESOURCE_TYPE_LAST;
             ResType = static_cast<SHADER_RESOURCE_TYPE>(ResType + 1))
        {
            if (TestRes[0].ResourceType != ResType)
            {
                Test([&TestRes, ResType]() { TestRes[0].ResourceType = ResType; });
                ++Count;
            }

            if (TestRes[1].ResourceType != ResType)
            {
                Test([&TestRes, ResType]() { TestRes[1].ResourceType = ResType; });
                ++Count;
            }
        }
        EXPECT_EQ(Count, static_cast<size_t>(SHADER_RESOURCE_TYPE_LAST) * 2);
    }


    {
        size_t Count = 0;
        for (SHADER_RESOURCE_VARIABLE_TYPE VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(0);
             VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES;
             VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType + 1))
        {
            if (TestRes[0].VarType != VarType)
            {
                Test([&TestRes, VarType]() { TestRes[0].VarType = VarType; });
                ++Count;
            }

            if (TestRes[1].VarType != VarType)
            {
                Test([&TestRes, VarType]() { TestRes[1].VarType = VarType; });
                ++Count;
            }
        }
        EXPECT_EQ(Count, (static_cast<size_t>(SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES) - 1) * 2);
    }

    for (PIPELINE_RESOURCE_FLAGS ResFlags = static_cast<PIPELINE_RESOURCE_FLAGS>(1u);
         ResFlags <= PIPELINE_RESOURCE_FLAG_LAST;
         ResFlags = static_cast<PIPELINE_RESOURCE_FLAGS>(ResFlags * 2))
    {
        Test([&TestRes, ResFlags]() { TestRes[0].Flags = ResFlags; });
        Test([&TestRes, ResFlags]() { TestRes[1].Flags = ResFlags; });
    }
}

} // namespace
