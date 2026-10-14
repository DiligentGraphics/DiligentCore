/*
 *  Copyright 2023-2026 Diligent Graphics LLC
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

#include <webgpu/webgpu.h>

#include "../../../../Graphics/GraphicsEngineWebGPU/include/WebGPUTypeConversions.hpp"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

TEST(WebGPUTypeConversionsTest, TextureFormatRoundTrip)
{
    // Include formats on both sides of the former RGBA16Snorm table boundary.
    constexpr TEXTURE_FORMAT Formats[] = {
        TEX_FORMAT_R8_UNORM,
        TEX_FORMAT_R16_UNORM,
        TEX_FORMAT_RG16_SNORM,
        TEX_FORMAT_RGBA16_SNORM,
        TEX_FORMAT_RGBA16_FLOAT,
        TEX_FORMAT_RGBA32_FLOAT,
        TEX_FORMAT_D16_UNORM,
        TEX_FORMAT_D24_UNORM_S8_UINT,
        TEX_FORMAT_D32_FLOAT_S8X24_UINT,
        TEX_FORMAT_BC1_UNORM,
        TEX_FORMAT_BC7_UNORM_SRGB,
        TEX_FORMAT_ETC2_RGBA8_UNORM_SRGB,
    };
    for (TEXTURE_FORMAT Format : Formats)
    {
        const WGPUTextureFormat WebGPUFormat = TextureFormatToWGPUFormat(Format);
        ASSERT_NE(WebGPUFormat, WGPUTextureFormat_Undefined);
        EXPECT_EQ(WGPUFormatToTextureFormat(WebGPUFormat), Format);
    }
}

TEST(WebGPUTypeConversionsTest, UnsupportedTextureFormats)
{
    EXPECT_EQ(WGPUFormatToTextureFormat(WGPUTextureFormat_Undefined), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(WGPUFormatToTextureFormat(WGPUTextureFormat_ASTC4x4Unorm), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(WGPUFormatToTextureFormat(WGPUTextureFormat_ASTC12x12UnormSrgb), TEX_FORMAT_UNKNOWN);
    EXPECT_EQ(WGPUFormatToTextureFormat(WGPUTextureFormat_Force32), TEX_FORMAT_UNKNOWN);
}

} // namespace
