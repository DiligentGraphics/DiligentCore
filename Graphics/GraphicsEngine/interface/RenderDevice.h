/*     Copyright 2015-2017 Egor Yusov
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
/// Definition of the Diligent::IRenderDevice interface and related data structures

#include "GraphicsTypes.h"
#include "Object.h"
#include "DeviceCaps.h"
#include "Constants.h"
#include "Buffer.h"
#include "InputLayout.h"
#include "Shader.h"
#include "Texture.h"
#include "Sampler.h"
#include "ResourceMapping.h"
#include "TextureView.h"
#include "BufferView.h"
#include "PipelineState.h"

#include "DepthStencilState.h"
#include "RasterizerState.h"
#include "BlendState.h"

namespace Diligent
{

// {F0E9B607-AE33-4B2B-B1AF-A8B2C3104022}
static constexpr INTERFACE_ID IID_RenderDevice =
{ 0xf0e9b607, 0xae33, 0x4b2b, { 0xb1, 0xaf, 0xa8, 0xb2, 0xc3, 0x10, 0x40, 0x22 } };

/// Render device interface
class IRenderDevice : public IObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface( const INTERFACE_ID &IID, IObject **ppInterface ) = 0;

    /// Creates a new buffer object

    /// \param [in] BuffDesc   - Buffer description, see Diligent::BufferDesc for details.
    /// \param [in] BuffData   - Reference to a Diligent::BufferData structure that describes
    ///                          the initialization data. To allocate space only, provide default value 
    ///                          BufferData(). Static buffers (USAGE_STATIC) must be initialized
    ///                          at creation time.
    /// \param [out] ppBuffer  - Address of the memory location where the pointer to the
    ///                          buffer interface will be stored. The function calls AddRef(),
    ///                          so that the new buffer will contain one refernce and must be
    ///                          released by a call to Release().
    ///
    /// \remarks
    /// Size of a uniform buffer (BIND_UNIFORM_BUFFER) must be multiple of 16.\n
    /// Stride of a formatted buffer will be computed automatically from the format if
    /// ElementByteStride member of buffer description is set to default value (0).
    virtual void CreateBuffer(const BufferDesc& BuffDesc, 
                              const BufferData& BuffData, 
                              IBuffer **ppBuffer) = 0;

    /// Creates a new shader object

    /// \param [in] CreationAttribs - Shader creation attributes, see 
    ///                               Diligent::ShaderCreationAttribs for details.
    /// \param [out] ppShader - Address of the memory location where the pointer to the
    ///                         shader interface will be stored. 
    ///                         The function calls AddRef(), so that the new object will contain 
    ///                         one refernce.
    virtual void CreateShader(const ShaderCreationAttribs &CreationAttribs, 
                              IShader **ppShader) = 0;
    
    /// Creates a new texture object

    /// \param [in] TexDesc - Texture description, see Diligent::TextureDesc for details.
    /// \param [in] Data    -  Reference to a Diligent::TextureData structure that describes
    ///                        the initialization data. To allocate space only, provide default value 
    ///                        TextureData(). Static textures (USAGE_STATIC) must be initialized
    ///                        at creation time.
    /// \param [out] ppTexture - Address of the memory location where the pointer to the
    ///                          texture interface will be stored. 
    ///                          The function calls AddRef(), so that the new object will contain 
    ///                          one refernce.
    /// \remarks 
    /// To create all mip levels, set the TexDesc.MipLevels to zero.\n
    /// Multisampled resources cannot be initialzed with data when they are created. \n
    /// If initial data is provided, number of subresources must exactly match the number 
    /// of subresources in the texture (which is the number of mip levels times the number of array slices.
    /// For a 3D texture, this is just the number of mip levels).
    /// For example, for a 15 x 6 x 2 2D texture array, the following array of subresources should be
    /// provided: \n
    /// 15x6, 7x3, 3x1, 1x1, 15x6, 7x3, 3x1, 1x1.\n
    /// For a 15 x 6 x 4 3D texture, the following array of subresources should be provided:\n
    /// 15x6x4, 7x3x2, 3x1x1, 1x1x1
    virtual void CreateTexture(const TextureDesc& TexDesc, 
                               const TextureData &Data, 
                               ITexture **ppTexture) = 0;

    /// Creates a new sampler object

    /// \param [in] SamDesc - Sampler description, see Diligent::SamplerDesc for details.
    /// \param [out] ppSampler - Address of the memory location where the pointer to the
    ///                          sampler interface will be stored. 
    ///                          The function calls AddRef(), so that the new object will contain 
    ///                          one refernce.
    /// \remark If an application attempts to create a sampler interface with the same attributes 
    ///         as an existing interface, the same interface will be returned.
    /// \note   In D3D11, 4096 unique sampler state objects can be created on a device at a time.        
    virtual void CreateSampler(const SamplerDesc& SamDesc, 
                               ISampler **ppSampler) = 0;

    /// Creates a new resource mapping

    /// \param [in] MappingDesc - Resource mapping description, see Diligent::ResourceMappingDesc for details.
    /// \param [out] ppMapping - Address of the memory location where the pointer to the
    ///                          resource mapping interface will be stored. 
    ///                          The function calls AddRef(), so that the new object will contain 
    ///                          one refernce.
    virtual void CreateResourceMapping( const ResourceMappingDesc &MappingDesc, 
                                        IResourceMapping **ppMapping ) = 0;

    /// Creates a new pipeline state object

    /// \param [in] PipelineDesc - Pipeline state description, see Diligent::PipelineStateDesc for details.
    /// \param [out] ppPipelineState - Address of the memory location where the pointer to the
    ///                                pipeline state interface will be stored. 
    ///                                The function calls AddRef(), so that the new object will contain 
    ///                                one refernce.
    virtual void CreatePipelineState( const PipelineStateDesc &PipelineDesc, 
                                      IPipelineState **ppPipelineState ) = 0;


    /// Gets the device capabilities, see Diligent::DeviceCaps for details
    virtual const DeviceCaps& GetDeviceCaps()const = 0;

    /// Returns the basic texture format information.

    /// See Diligent::TextureFormatInfo for details on the provided information.
    /// \param [in] TexFormat - Texture format for which to provide the information
    /// \return Const reference to the TextureFormatInfo structure containing the
    ///         texture format description.
    virtual const TextureFormatInfo &GetTextureFormatInfo( TEXTURE_FORMAT TexFormat ) = 0;


    /// Returns the extended texture format information.

    /// See Diligent::TextureFormatInfoExt for details on the provided information.
    /// \param [in] TexFormat - Texture format for which to provide the information
    /// \return Const reference to the TextureFormatInfoExt structure containing the
    ///         extended texture format description.
    /// \remark The first time this method is called for a particular format, it may be
    ///         considerably slower than GetTextureFormatInfo(). If you do not require
    ///         extended information, call GetTextureFormatInfo() instead.
    virtual const TextureFormatInfoExt &GetTextureFormatInfoExt( TEXTURE_FORMAT TexFormat ) = 0;
};

}
