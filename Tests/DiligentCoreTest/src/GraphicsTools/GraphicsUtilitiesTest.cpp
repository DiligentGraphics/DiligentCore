/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "GraphicsUtilities.h"
#include "GraphicsAccessories.hpp"

#include <unordered_set>

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

TEST(GraphicsUtilitiesTest, GetNativeTextureFormat_GetTextureFormatFromNative)
{
    static_assert(TEX_FORMAT_NUM_FORMATS == 106, "The test below may need to be updated to include new formats");

    const std::unordered_set<TEXTURE_FORMAT> SkipFormatsD3D{
        TEX_FORMAT_ETC2_RGB8_UNORM,
        TEX_FORMAT_ETC2_RGB8_UNORM_SRGB,
        TEX_FORMAT_ETC2_RGB8A1_UNORM,
        TEX_FORMAT_ETC2_RGB8A1_UNORM_SRGB,
        TEX_FORMAT_ETC2_RGBA8_UNORM,
        TEX_FORMAT_ETC2_RGBA8_UNORM_SRGB,
    };

    const std::unordered_set<TEXTURE_FORMAT> SkipFormatsGL{
        TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS,
        TEX_FORMAT_X32_TYPELESS_G8X24_UINT,
        TEX_FORMAT_R24_UNORM_X8_TYPELESS,
        TEX_FORMAT_X24_TYPELESS_G8_UINT,
        TEX_FORMAT_A8_UNORM,
        TEX_FORMAT_R1_UNORM,
        TEX_FORMAT_RG8_B8G8_UNORM,
        TEX_FORMAT_G8R8_G8B8_UNORM,
        TEX_FORMAT_B5G6R5_UNORM,
        TEX_FORMAT_B5G5R5A1_UNORM,
        TEX_FORMAT_BGRA8_UNORM,
        TEX_FORMAT_BGRX8_UNORM,
        TEX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
        TEX_FORMAT_BGRA8_UNORM_SRGB,
        TEX_FORMAT_BGRX8_UNORM_SRGB,
    };

    const std::unordered_set<TEXTURE_FORMAT> SkipFormatsVk{
        TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS,
        TEX_FORMAT_X32_TYPELESS_G8X24_UINT,
        TEX_FORMAT_R24_UNORM_X8_TYPELESS,
        TEX_FORMAT_X24_TYPELESS_G8_UINT,
        TEX_FORMAT_A8_UNORM,
        TEX_FORMAT_R1_UNORM,
        TEX_FORMAT_RG8_B8G8_UNORM,
        TEX_FORMAT_G8R8_G8B8_UNORM,
        TEX_FORMAT_BGRX8_UNORM,
        TEX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
        TEX_FORMAT_BGRX8_UNORM_SRGB,
    };

    const std::unordered_set<TEXTURE_FORMAT> SkipFormatsMtl{
        TEX_FORMAT_RGB32_FLOAT,
        TEX_FORMAT_RGB32_UINT,
        TEX_FORMAT_RGB32_SINT,
        TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS,
        TEX_FORMAT_X32_TYPELESS_G8X24_UINT,
        TEX_FORMAT_R24_UNORM_X8_TYPELESS,
        TEX_FORMAT_X24_TYPELESS_G8_UINT,
        TEX_FORMAT_A8_UNORM,
        TEX_FORMAT_R1_UNORM,
        TEX_FORMAT_RG8_B8G8_UNORM,
        TEX_FORMAT_G8R8_G8B8_UNORM,
        TEX_FORMAT_B5G6R5_UNORM,
        TEX_FORMAT_B5G5R5A1_UNORM,
        TEX_FORMAT_BGRX8_UNORM,
        TEX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
        TEX_FORMAT_BGRX8_UNORM_SRGB,
    };

    const std::unordered_set<TEXTURE_FORMAT> SkipFormatsWebGPU{
        TEX_FORMAT_RGB32_FLOAT,
        TEX_FORMAT_RGB32_UINT,
        TEX_FORMAT_RGB32_SINT,
        TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS,
        TEX_FORMAT_X32_TYPELESS_G8X24_UINT,
        TEX_FORMAT_R24_UNORM_X8_TYPELESS,
        TEX_FORMAT_X24_TYPELESS_G8_UINT,
        TEX_FORMAT_A8_UNORM,
        TEX_FORMAT_R1_UNORM,
        TEX_FORMAT_RG8_B8G8_UNORM,
        TEX_FORMAT_G8R8_G8B8_UNORM,
        TEX_FORMAT_B5G6R5_UNORM,
        TEX_FORMAT_B5G5R5A1_UNORM,
        TEX_FORMAT_BGRX8_UNORM,
        TEX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
        TEX_FORMAT_BGRX8_UNORM_SRGB,
    };

    for (int i = 1; i < TEX_FORMAT_NUM_FORMATS; ++i)
    {
        TEXTURE_FORMAT              Fmt        = static_cast<TEXTURE_FORMAT>(i);
        const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(Fmt);
        if (FmtAttribs.IsTypeless)
            continue;

        auto Test = [Fmt, &FmtAttribs](RENDER_DEVICE_TYPE DevType) {
            int64_t NativeFmt = GetNativeTextureFormat(Fmt, DevType);
            EXPECT_NE(NativeFmt, 0);
            TEXTURE_FORMAT FmtFromNative = GetTextureFormatFromNative(NativeFmt, DevType);
            EXPECT_EQ(Fmt, FmtFromNative) << "DevType: " << GetRenderDeviceTypeString(DevType) << ", Fmt: " << FmtAttribs.Name;
        };

#if D3D11_SUPPORTED
        if (SkipFormatsD3D.find(Fmt) == SkipFormatsD3D.end())
        {
            Test(RENDER_DEVICE_TYPE_D3D11);
        }
#endif

#if D3D12_SUPPORTED
        if (SkipFormatsD3D.find(Fmt) == SkipFormatsD3D.end())
        {
            Test(RENDER_DEVICE_TYPE_D3D12);
        }
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        if (SkipFormatsGL.find(Fmt) == SkipFormatsGL.end())
        {
            Test(RENDER_DEVICE_TYPE_GL);
        }
#endif

#if VULKAN_SUPPORTED
        if (SkipFormatsVk.find(Fmt) == SkipFormatsVk.end())
        {
            Test(RENDER_DEVICE_TYPE_VULKAN);
        }
#endif

#if METAL_SUPPORTED
        if (SkipFormatsMtl.find(Fmt) == SkipFormatsMtl.end())
        {
            Test(RENDER_DEVICE_TYPE_METAL);
        }
#endif

#if WEBGPU_SUPPORTED
        if (SkipFormatsWebGPU.find(Fmt) == SkipFormatsWebGPU.end())
        {
            Test(RENDER_DEVICE_TYPE_WEBGPU);
        }
#endif
    }
}

} // namespace
