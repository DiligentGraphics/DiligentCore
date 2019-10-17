/*     Copyright 2019 Diligent Graphics LLC
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

/// \file
/// Definition of the device capabilities

#include "GraphicsTypes.h"

namespace Diligent
{
    /// Device type
    enum class DeviceType : Int32
    {
        Undefined = 0,  ///< Undefined device
        D3D11,          ///< D3D11 device
        D3D12,          ///< D3D12 device
        OpenGL,         ///< OpenGL device 
        OpenGLES,       ///< OpenGLES device
        Vulkan,         ///< Vulkan device
        Metal           ///< Metal device (not yet implemented)
    };

    /// Texture sampler capabilities
    struct SamplerCaps
    {
        /// Indicates if device supports border texture addressing mode
        Bool bBorderSamplingModeSupported   = True;

        /// Indicates if device supports anisotrpoic filtering
        Bool bAnisotropicFilteringSupported = True;

        /// Indicates if device supports MIP load bias
        Bool bLODBiasSupported              = True;
    };

    /// Texture capabilities
    struct TextureCaps
    {
        /// Indicates if device supports 1D textures
        Bool bTexture1DSupported        = True;

        /// Indicates if device supports 1D texture arrays
        Bool bTexture1DArraySupported   = True;

        /// Indicates if device supports 2D multisampled textures
        Bool bTexture2DMSSupported      = True;

        /// Indicates if device supports 2D multisampled texture arrays
        Bool bTexture2DMSArraySupported = True;

        /// Indicates if device supports texture views
        Bool bTextureViewSupported      = True;

        /// Indicates if device supports cubemap arrays
        Bool bCubemapArraysSupported    = True;
    };
    
    /// Device capabilities
    struct DeviceCaps
    {
        /// Device type. See Diligent::DeviceType.
        DeviceType DevType = DeviceType::Undefined;

        /// Major revision of the graphics API supported by the graphics adapter.
        /// Note that this value indicates the maximum supported feature level, so,
        /// for example, if the device type is D3D11, this value will be 10 when 
        /// the maximum supported Direct3D feature level of the graphics adapter is 10.0.
        Int32 MajorVersion = 0;

        /// Minor revision of the graphics API supported by the graphics adapter.
        /// Similar to MajorVersion, this value indicates the maximum supported feature level.
        Int32 MinorVersion = 0;

        /// Indicates if device supports separable programs
        Bool bSeparableProgramSupported = True;

        /// Indicates if device supports indirect draw commands
        Bool bIndirectRenderingSupported = True;

        /// Indicates if device supports wireframe fill mode
        Bool bWireframeFillSupported = True;

        /// Indicates if device supports multithreaded resource creation
        Bool bMultithreadedResourceCreationSupported = False;

        /// Indicates if device supports compute shaders
        Bool bComputeShadersSupported = True;

        /// Indicates if device supports geometry shaders
        Bool bGeometryShadersSupported = True;
        
        /// Indicates if device supports tessellation
        Bool bTessellationSupported = True;
        
        /// Indicates if device supports bindless resources
        Bool bBindlessSupported = False;

        /// Texture sampling capabilities. See Diligent::SamplerCaps.
        SamplerCaps SamCaps;

        /// Texture capabilities. See Diligent::TextureCaps.
        TextureCaps TexCaps;

        bool IsGLDevice()const
        {
            return DevType == DeviceType::OpenGL || DevType == DeviceType::OpenGLES;
        }
        bool IsD3DDevice()const
        {
            return DevType == DeviceType::D3D11 || DevType == DeviceType::D3D12;
        }
        bool IsVulkanDevice()const
        {
            return DevType == DeviceType::Vulkan;
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
    };
}
