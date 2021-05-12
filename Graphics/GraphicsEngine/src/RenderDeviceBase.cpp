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

#include "RenderDeviceBase.hpp"

namespace Diligent
{

DeviceFeatures EnableDeviceFeatures(const DeviceFeatures& SupportedFeatures,
                                    const DeviceFeatures& RequestedFeatures) noexcept(false)
{
    auto GetFeatureState = [](DEVICE_FEATURE_STATE RequestedState, DEVICE_FEATURE_STATE SupportedState, const char* FeatureName) //
    {
        switch (RequestedState)
        {
            case DEVICE_FEATURE_STATE_DISABLED:
                return SupportedState == DEVICE_FEATURE_STATE_ENABLED ?
                    DEVICE_FEATURE_STATE_ENABLED : // the feature is supported by default and can not be disabled
                    DEVICE_FEATURE_STATE_DISABLED;

            case DEVICE_FEATURE_STATE_ENABLED:
            {
                if (SupportedState != DEVICE_FEATURE_STATE_DISABLED)
                    return DEVICE_FEATURE_STATE_ENABLED;
                else
                    LOG_ERROR_AND_THROW(FeatureName, " not supported by this device");
            }

            case DEVICE_FEATURE_STATE_OPTIONAL:
                return SupportedState != DEVICE_FEATURE_STATE_DISABLED ?
                    DEVICE_FEATURE_STATE_ENABLED :
                    DEVICE_FEATURE_STATE_DISABLED;

            default:
                UNEXPECTED("Unexpected feature state");
                return DEVICE_FEATURE_STATE_DISABLED;
        }
    };

    if (SupportedFeatures.SeparablePrograms == DEVICE_FEATURE_STATE_ENABLED &&
        RequestedFeatures.SeparablePrograms == DEVICE_FEATURE_STATE_DISABLED)
    {
        LOG_INFO_MESSAGE("Can not disable SeparablePrograms");
    }

    DeviceFeatures EnabledFeatures;
#define ENABLE_FEATURE(Feature, FeatureName) \
    EnabledFeatures.Feature = GetFeatureState(RequestedFeatures.Feature, SupportedFeatures.Feature, FeatureName)

    // clang-format off
    ENABLE_FEATURE(SeparablePrograms,                 "Separable programs are");
    ENABLE_FEATURE(ShaderResourceQueries,             "Shader resource queries are");
    ENABLE_FEATURE(IndirectRendering,                 "Indirect rendering is");
    ENABLE_FEATURE(WireframeFill,                     "Wireframe fill is");
    ENABLE_FEATURE(MultithreadedResourceCreation,     "Multithreaded resource creation is");
    ENABLE_FEATURE(ComputeShaders,                    "Compute shaders are");
    ENABLE_FEATURE(GeometryShaders,                   "Geometry shaders are");
    ENABLE_FEATURE(Tessellation,                      "Tessellation is");
    ENABLE_FEATURE(MeshShaders,                       "Mesh shaders are");
    ENABLE_FEATURE(RayTracing,                        "Ray tracing is");
    ENABLE_FEATURE(BindlessResources,                 "Bindless resources are");
    ENABLE_FEATURE(OcclusionQueries,                  "Occlusion queries are");
    ENABLE_FEATURE(BinaryOcclusionQueries,            "Binary occlusion queries are");
    ENABLE_FEATURE(TimestampQueries,                  "Timestamp queries are");
    ENABLE_FEATURE(PipelineStatisticsQueries,         "Pipeline atatistics queries are");
    ENABLE_FEATURE(DurationQueries,                   "Duration queries are");
    ENABLE_FEATURE(DepthBiasClamp,                    "Depth bias clamp is");
    ENABLE_FEATURE(DepthClamp,                        "Depth clamp is");
    ENABLE_FEATURE(IndependentBlend,                  "Independent blend is");
    ENABLE_FEATURE(DualSourceBlend,                   "Dual-source blend is");
    ENABLE_FEATURE(MultiViewport,                     "Multiviewport is");
    ENABLE_FEATURE(TextureCompressionBC,              "BC texture compression is");
    ENABLE_FEATURE(VertexPipelineUAVWritesAndAtomics, "Vertex pipeline UAV writes and atomics are");
    ENABLE_FEATURE(PixelUAVWritesAndAtomics,          "Pixel UAV writes and atomics are");
    ENABLE_FEATURE(TextureUAVExtendedFormats,         "Texture UAV extended formats are");
    ENABLE_FEATURE(ShaderFloat16,                     "16-bit float shader operations are");
    ENABLE_FEATURE(ResourceBuffer16BitAccess,         "16-bit resoure buffer access is");
    ENABLE_FEATURE(UniformBuffer16BitAccess,          "16-bit uniform buffer access is");
    ENABLE_FEATURE(ShaderInputOutput16,               "16-bit shader inputs/outputs are");
    ENABLE_FEATURE(ShaderInt8,                        "8-bit int shader operations are");
    ENABLE_FEATURE(ResourceBuffer8BitAccess,          "8-bit resoure buffer access is");
    ENABLE_FEATURE(UniformBuffer8BitAccess,           "8-bit uniform buffer access is");
    ENABLE_FEATURE(ShaderResourceRuntimeArray,        "Shader resource runtime array is");
    ENABLE_FEATURE(WaveOp,                            "Wave operations are");
    ENABLE_FEATURE(InstanceDataStepRate,              "Instance data step rate is");
    ENABLE_FEATURE(NativeFence,                       "Native fence is");
    ENABLE_FEATURE(TileShaders,                       "Tile shaders are");
    // clang-format on
#undef ENABLE_FEATURE

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(Diligent::DeviceFeatures) == 37, "Did you add a new feature to DeviceFeatures? Please handle its satus here (if necessary).");
#endif
    return EnabledFeatures;
}

} // namespace Diligent
