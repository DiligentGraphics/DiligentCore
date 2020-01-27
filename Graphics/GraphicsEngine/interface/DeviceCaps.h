/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

#pragma once

// clang-format off

/// \file
/// Definition of the device capabilities

#include "GraphicsTypes.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

/// Device type
enum RENDER_DEVICE_TYPE
{
    RENDER_DEVICE_TYPE_UNDEFINED = 0,  ///< Undefined device
    RENDER_DEVICE_TYPE_D3D11,          ///< D3D11 device
    RENDER_DEVICE_TYPE_D3D12,          ///< D3D12 device
    RENDER_DEVICE_TYPE_GL,             ///< OpenGL device 
    RENDER_DEVICE_TYPE_GLES,           ///< OpenGLES device
    RENDER_DEVICE_TYPE_VULKAN,         ///< Vulkan device
    RENDER_DEVICE_TYPE_METAL           ///< Metal device (not yet implemented)
};

/// Texture sampler capabilities
struct SamplerCaps
{
    /// Indicates if device supports border texture addressing mode
    Bool BorderSamplingModeSupported   DEFAULT_INITIALIZER(False);

    /// Indicates if device supports anisotrpoic filtering
    Bool AnisotropicFilteringSupported DEFAULT_INITIALIZER(False);

    /// Indicates if device supports MIP load bias
    Bool LODBiasSupported              DEFAULT_INITIALIZER(False);
};
typedef struct SamplerCaps SamplerCaps;

/// Texture capabilities
struct TextureCaps
{
    /// Maximum dimension (width) of a 1D texture, or 0 if 1D textures are not supported.
    Uint32 MaxTexture1DDimension   DEFAULT_INITIALIZER(0);

    /// Maximum number of slices in a 1D texture array, or 0 if 1D texture arrays are not supported.
    Uint32 MaxTexture1DArraySlices DEFAULT_INITIALIZER(0);

    /// Maximum dimension (width or height) of a 2D texture.
    Uint32 MaxTexture2DDimension   DEFAULT_INITIALIZER(0);

    /// Maximum number of slices in a 2D texture array, or 0 if 2D texture arrays are not supported.
    Uint32 MaxTexture2DArraySlices DEFAULT_INITIALIZER(0);

    /// Maximum dimension (width, height, or depth) of a 3D texture, or 0 if 3D textures are not supported.
    Uint32 MaxTexture3DDimension   DEFAULT_INITIALIZER(0);

    /// Maximum dimension (width or height) of a cubemap face, or 0 if cubemap textures are not supported.
    Uint32 MaxTextureCubeDimension DEFAULT_INITIALIZER(0);

    /// Indicates if device supports 2D multisampled textures
    Bool Texture2DMSSupported      DEFAULT_INITIALIZER(False);

    /// Indicates if device supports 2D multisampled texture arrays
    Bool Texture2DMSArraySupported DEFAULT_INITIALIZER(False);

    /// Indicates if device supports texture views
    Bool TextureViewSupported      DEFAULT_INITIALIZER(False);

    /// Indicates if device supports cubemap arrays
    Bool CubemapArraysSupported    DEFAULT_INITIALIZER(False);
};
typedef struct TextureCaps TextureCaps;
    
/// Describes supported device features
struct DeviceFeatures
{
    /// Indicates if device supports separable programs
    Bool SeparablePrograms             DEFAULT_INITIALIZER(False);

    /// Indicates if device supports indirect draw commands
    Bool IndirectRendering             DEFAULT_INITIALIZER(False);

    /// Indicates if device supports wireframe fill mode
    Bool WireframeFill                 DEFAULT_INITIALIZER(False);

    /// Indicates if device supports multithreaded resource creation
    Bool MultithreadedResourceCreation DEFAULT_INITIALIZER(False);

    /// Indicates if device supports compute shaders
    Bool ComputeShaders                DEFAULT_INITIALIZER(False);

    /// Indicates if device supports geometry shaders
    Bool GeometryShaders               DEFAULT_INITIALIZER(False);
        
    /// Indicates if device supports tessellation
    Bool Tessellation                  DEFAULT_INITIALIZER(False);
        
    /// Indicates if device supports bindless resources
    Bool BindlessResources             DEFAULT_INITIALIZER(False);

    /// Indicates if device supports occlusion queries (see Diligent::QUERY_TYPE_OCCLUSION).
    Bool OcclusionQueries              DEFAULT_INITIALIZER(False);

    /// Indicates if device supports binary occlusion queries (see Diligent::QUERY_TYPE_BINARY_OCCLUSION).
    Bool BinaryOcclusionQueries        DEFAULT_INITIALIZER(False);

    /// Indicates if device supports timestamp queries (see Diligent::QUERY_TYPE_TIMESTAMP).
    Bool TimestampQueries              DEFAULT_INITIALIZER(False);

    /// Indicates if device supports pipeline statistics queries (see Diligent::QUERY_TYPE_PIPELINE_STATISTICS).
    Bool PipelineStatisticsQueries     DEFAULT_INITIALIZER(False);

    /// Indicates if device supports depth bias clamping
    Bool DepthBiasClamp                DEFAULT_INITIALIZER(False);

    /// Indicates if device supports depth clamping
    Bool DepthClamp                    DEFAULT_INITIALIZER(False);

    /// Indicates if device supports depth clamping
    Bool IndependentBlend              DEFAULT_INITIALIZER(False);

    /// Indicates if device supports dual-source blend
    Bool DualSourceBlend               DEFAULT_INITIALIZER(False);

    /// Indicates if device supports multiviewport
    Bool MultiViewport                 DEFAULT_INITIALIZER(False);

    /// Indicates if device supports all BC-compressed formats
    Bool TextureCompressionBC          DEFAULT_INITIALIZER(False);

    /// Indicates if device supports writes to UAVs as well as atomic operations in vertex,
    /// tessellation, and geometry shader stages.
    Bool VertexPipelineUAVWritesAndAtomics DEFAULT_INITIALIZER(False);

    /// Indicates if device supports writes to UAVs as well as atomic operations in pixel
    /// shader stage.
    Bool PixelUAVWritesAndAtomics          DEFAULT_INITIALIZER(False);

    /// Specifies whether all the extended UAV texture formats are available in shader code.
    Bool TextureUAVExtendedFormats         DEFAULT_INITIALIZER(False);
};
typedef struct DeviceFeatures DeviceFeatures;

/// Device capabilities
struct DeviceCaps
{
    /// Device type. See Diligent::DeviceType.
    enum RENDER_DEVICE_TYPE DevType DEFAULT_INITIALIZER(RENDER_DEVICE_TYPE_UNDEFINED);

    /// Major revision of the graphics API supported by the graphics adapter.
    /// Note that this value indicates the maximum supported feature level, so,
    /// for example, if the device type is D3D11, this value will be 10 when 
    /// the maximum supported Direct3D feature level of the graphics adapter is 10.0.
    Int32 MajorVersion DEFAULT_INITIALIZER(0);

    /// Minor revision of the graphics API supported by the graphics adapter.
    /// Similar to MajorVersion, this value indicates the maximum supported feature level.
    Int32 MinorVersion DEFAULT_INITIALIZER(0);

    /// Adapter type. See Diligent::ADAPTER_TYPE.
    ADAPTER_TYPE AdaterType DEFAULT_INITIALIZER(ADAPTER_TYPE_UNKNOWN);

    /// Texture sampling capabilities. See Diligent::SamplerCaps.
    SamplerCaps SamCaps;

    /// Texture capabilities. See Diligent::TextureCaps.
    TextureCaps TexCaps;

    /// Device features. See Diligent::DeviceFeatures.
    DeviceFeatures Features;

#if DILIGENT_CPP_INTERFACE
    bool IsGLDevice()const
    {
        return DevType == RENDER_DEVICE_TYPE_GL || DevType == RENDER_DEVICE_TYPE_GLES;
    }
    bool IsD3DDevice()const
    {
        return DevType == RENDER_DEVICE_TYPE_D3D11 || DevType == RENDER_DEVICE_TYPE_D3D12;
    }
    bool IsVulkanDevice()const
    {
        return DevType == RENDER_DEVICE_TYPE_VULKAN;
    }

    struct NDCAttribs
    {
        const float MinZ;          // Minimum z value of normalized device coordinate space
        const float ZtoDepthScale; // NDC z to depth scale
        const float YtoVScale;     // Scale to transform NDC y coordinate to texture V coordinate

        float GetZtoDepthBias() const
        {
            // Returns ZtoDepthBias such that given NDC z coordinate, depth value can be
            // computed as follows:
            // d = z * ZtoDepthScale + ZtoDepthBias
            return -MinZ * ZtoDepthScale;
        }
    };

    const NDCAttribs& GetNDCAttribs()const
    {
        if (IsVulkanDevice())
        {
            // Note that Vulkan itself does not invert Y coordinate when transforming
            // normalized device Y to window space. However, we use negative viewport
            // height which achieves the same effect as in D3D, thererfore we need to
            // invert y (see comments in DeviceContextVkImpl::CommitViewports() for details)
            static constexpr const NDCAttribs NDCAttribsVk {0.0f, 1.0f, -0.5f};
            return NDCAttribsVk;
        }
        else if (IsD3DDevice())
        {
            static constexpr const NDCAttribs NDCAttribsD3D {0.0f, 1.0f, -0.5f};
            return NDCAttribsD3D;
        }
        else if (IsGLDevice())
        {
            static constexpr const NDCAttribs NDCAttribsGL {-1.0f, 0.5f, 0.5f};
            return NDCAttribsGL;
        }
        else
        {
            static constexpr const NDCAttribs NDCAttribsDefault {0.0f, 1.0f, 0.5f};
            return NDCAttribsDefault;
        }
    }
#endif
};
typedef struct DeviceCaps DeviceCaps;

DILIGENT_END_NAMESPACE // namespace Diligent
