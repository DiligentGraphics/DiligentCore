/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "TestingEnvironment.hpp"
#include "GraphicsAccessories.hpp"
#include "SerializationAPI.h"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

static Uint32 GetDeviceBits()
{
    Uint32 DeviceBits = 0;
#if D3D11_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_D3D11;
#endif
#if D3D12_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_D3D12;
#endif
#if GL_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_GL;
#endif
#if GLES_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_GLES;
#endif
#if VULKAN_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_VULKAN;
#endif
#if METAL_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_METAL;
#endif
    return DeviceBits;
}

TEST(ArchiveTest, ResourceSignature)
{
    auto* pEnv            = TestingEnvironment::GetInstance();
    auto* pDevice         = pEnv->GetDevice();
    auto* pArchiveFactory = pEnv->GetArchiveFactory();
    auto* pSerialization  = pDevice->GetEngineFactory()->GetSerializationAPI();

    if (!pSerialization || !pArchiveFactory)
        return;

    constexpr char PRSName[] = "PRS archive test";

    TestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    RefCntAutoPtr<IPipelineResourceSignature> pRefPRS;
    RefCntAutoPtr<IDeviceObjectArchive>       pArchive;
    {
        RefCntAutoPtr<IArchiveBuilder> pBuilder;
        pArchiveFactory->CreateArchiveBuilder(&pBuilder);
        ASSERT_NE(pBuilder, nullptr);

        const auto VarType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

        const PipelineResourceDesc Resources[] = //
            {
                {SHADER_TYPE_ALL_GRAPHICS, "g_Tex2D_1", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, VarType},
                {SHADER_TYPE_ALL_GRAPHICS, "g_Tex2D_2", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, VarType},
                {SHADER_TYPE_ALL_GRAPHICS, "ConstBuff_1", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, VarType},
                {SHADER_TYPE_ALL_GRAPHICS, "ConstBuff_2", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, VarType}, //
            };

        PipelineResourceSignatureDesc PRSDesc;
        PRSDesc.Name         = PRSName;
        PRSDesc.Resources    = Resources;
        PRSDesc.NumResources = _countof(Resources);

        const ImmutableSamplerDesc ImmutableSamplers[] = //
            {
                {SHADER_TYPE_ALL_GRAPHICS, "g_Sampler", SamplerDesc{}} //
            };
        PRSDesc.ImmutableSamplers    = ImmutableSamplers;
        PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);

        ResourceSignatureArchiveInfo ArchiveInfo;
        ArchiveInfo.DeviceBits = GetDeviceBits();
        ASSERT_TRUE(pBuilder->ArchivePipelineResourceSignature(PRSDesc, ArchiveInfo));

        pDevice->CreatePipelineResourceSignature(PRSDesc, &pRefPRS);
        ASSERT_NE(pRefPRS, nullptr);

        RefCntAutoPtr<IDataBlob> pBlob;
        pBuilder->SerializeToBlob(&pBlob);
        ASSERT_NE(pBlob, nullptr);

        RefCntAutoPtr<IArchiveSource> pSource;
        pSerialization->CreateArchiveSourceFromBlob(pBlob, &pSource);
        ASSERT_NE(pSource, nullptr);

        pSerialization->CreateDeviceObjectArchive(pSource, &pArchive);
        ASSERT_NE(pArchive, nullptr);
    }

    ResourceSignatureUnpackInfo UnpackInfo;
    UnpackInfo.Name                     = PRSName;
    UnpackInfo.pArchive                 = pArchive;
    UnpackInfo.pDevice                  = pDevice;
    UnpackInfo.SRBAllocationGranularity = 10;

    RefCntAutoPtr<IPipelineResourceSignature> pUnpackedPRS;
    pSerialization->UnpackResourceSignature(UnpackInfo, &pUnpackedPRS);
    ASSERT_NE(pUnpackedPRS, nullptr);

    ASSERT_TRUE(pUnpackedPRS->IsCompatibleWith(pRefPRS)); // AZ TODO: names are ignored
}

} // namespace
