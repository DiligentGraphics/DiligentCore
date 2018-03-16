/*     Copyright 2015-2018 Egor Yusov
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
        D3D11,      ///< D3D11 device
        D3D12,      ///< D3D12 device
        OpenGL,     ///< OpenGL device 
        OpenGLES,   ///< OpenGLES device
        Vulkan      ///< Vulkan device
    };

    /// Texture sampler capabilities
    struct SamplerCaps
    {
        /// Indicates if device supports border texture addressing mode
        Bool bBorderSamplingModeSupported;

        /// Indicates if device supports anisotrpoic filtering
        Bool bAnisotropicFilteringSupported;

        /// Indicates if device supports MIP load bias
        Bool bLODBiasSupported;
        
        /// Initializes the structure members with default values
        SamplerCaps() :
            bBorderSamplingModeSupported( True ),
            bAnisotropicFilteringSupported( True ),
            bLODBiasSupported( True )
        {}
    };

    /// Texture capabilities
    struct TextureCaps
    {
        /// Indicates if device supports 1D textures
        Bool bTexture1DSupported;

        /// Indicates if device supports 1D texture arrays
        Bool bTexture1DArraySupported;

        /// Indicates if device supports 2D multisampled textures
        Bool bTexture2DMSSupported;

        /// Indicates if device supports 2D multisampled texture arrays
        Bool bTexture2DMSArraySupported;

        /// Indicates if device supports texture views
        bool bTextureViewSupported;

        /// Indicates if device supports cubemap arrays
        bool bCubemapArraysSupported;

        /// Initializes the structure members with default values
        TextureCaps():
            bTexture1DSupported( True ),
            bTexture1DArraySupported( True ),
            bTexture2DMSSupported( True ),
            bTexture2DMSArraySupported( True ),
            bTextureViewSupported( True ),
            bCubemapArraysSupported( True )
        {}
    };
    
    /// Device capabilities
    struct DeviceCaps
    {
        /// Device type. See Diligent::DeviceType.
        DeviceType DevType;

        /// Major API revision. For instance, for D3D11.2 this value would be 11,
        /// and for OpenGL4.3 this value would be 4.
        Int32 MajorVersion;

        /// Major API revision. For instance, for D3D11.2 this value would be 2,
        /// and for OpenGL4.3 this value would be 3.
        Int32 MinorVersion;

        /// Indicates if device supports separable programs
        Bool bSeparableProgramSupported;

        /// Indicates if device supports indirect draw commands
        Bool bIndirectRenderingSupported;

        /// Indicates if device supports wireframe fill mode
        Bool bWireframeFillSupported;

        /// Indicates if device supports multithreaded resource creation
        Bool bMultithreadedResourceCreationSupported;

        /// Indicates if device supports compute shaders
        Bool bComputeShadersSupported;

        /// Indicates if device supports geometry shaders
        Bool bGeometryShadersSupported;
        
        /// Indicates if device supports tessellation
        Bool bTessellationSupported;
        
        /// Texture sampling capabilities. See Diligent::SamplerCaps.
        SamplerCaps SamCaps;

        /// Texture capabilities. See Diligent::TextureCaps.
        TextureCaps TexCaps;

        /// Initializes the structure members with default values
        DeviceCaps() :
            DevType( DeviceType::Undefined ),
            MajorVersion( 0 ),
            MinorVersion( 0 ),
            bSeparableProgramSupported( True ),
            bIndirectRenderingSupported( True ),
            bWireframeFillSupported( True ),
            bMultithreadedResourceCreationSupported( False ),
            bComputeShadersSupported(True),
            bGeometryShadersSupported(True),
            bTessellationSupported(True)
        {}
    };
}
