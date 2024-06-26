/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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

#include <string>

namespace
{

namespace WGSL
{

// clang-format off
const std::string FillTextureCS{
    R"(
@group(0) @binding(0) var g_tex2DUAV : texture_storage_2d<rgba8unorm, write>;

@compute @workgroup_size(16, 16, 1)
fn main(@builtin(global_invocation_id) DTid: vec3<u32>)
{
    let Dimensions = vec2u(textureDimensions(g_tex2DUAV).xy);
	if (DTid.x >= Dimensions.x || DTid.y >= Dimensions.y) {
        return;
    }
    let Color = vec4f(vec2f(vec2u(DTid.xy % 256u)) / 256.0, 0.0, 1.0);
    textureStore(g_tex2DUAV, DTid.xy, Color);
}
)"
};

// clang-format on

} // namespace WGSL

} // namespace
