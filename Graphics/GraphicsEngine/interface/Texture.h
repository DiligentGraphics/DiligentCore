/*     Copyright 2015-2019 Egor Yusov
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
/// Definition of the Diligent::ITexture interface and related data structures

#include "DeviceObject.h"

namespace Diligent
{

class IDeviceContext;

// {A64B0E60-1B5E-4CFD-B880-663A1ADCBE98}
static constexpr INTERFACE_ID IID_Texture =
{ 0xa64b0e60, 0x1b5e, 0x4cfd, { 0xb8, 0x80, 0x66, 0x3a, 0x1a, 0xdc, 0xbe, 0x98 } };

/// Defines optimized depth-stencil clear value.
struct DepthStencilClearValue
{
    /// Depth clear value
    Float32 Depth   = 1.f;
    /// Stencil clear value
    Uint8 Stencil   = 0;

    // We have to explicitly define constructors because otherwise Apple's clang fails to compile the following legitimate code:
    //     DepthStencilClearValue{1, 0}

    DepthStencilClearValue()noexcept{}

    DepthStencilClearValue(Float32 _Depth,
                           Uint8   _Stencil)noexcept : 
        Depth   (_Depth),
        Stencil (_Stencil)
    {}
};

/// Defines optimized clear value.
struct OptimizedClearValue
{
    /// Format
    TEXTURE_FORMAT Format = TEX_FORMAT_UNKNOWN;

    /// Render target clear value
    Float32 Color[4]     = {0, 0, 0, 0};

    /// Depth stencil clear value
    DepthStencilClearValue DepthStencil;

    bool operator == (const OptimizedClearValue& rhs)const
    {
        return Format == rhs.Format &&
               Color[0] == rhs.Color[0] &&
               Color[1] == rhs.Color[1] &&
               Color[2] == rhs.Color[2] &&
               Color[3] == rhs.Color[3] &&
               DepthStencil.Depth   == rhs.DepthStencil.Depth &&
               DepthStencil.Stencil == rhs.DepthStencil.Stencil;
    }
};

/// Texture description
struct TextureDesc : DeviceObjectAttribs
{
    /// Texture type. See Diligent::RESOURCE_DIMENSION for details.
    RESOURCE_DIMENSION Type = RESOURCE_DIM_UNDEFINED;

    /// Texture width, in pixels.
    Uint32 Width            = 0;

    /// Texture height, in pixels.
    Uint32 Height           = 0;

    union
    {
        /// For a 1D array or 2D array, number of array slices
        Uint32 ArraySize    = 1;

        /// For a 3D texture, number of depth slices
        Uint32 Depth;
    };

    /// Texture format, see Diligent::TEXTURE_FORMAT.
    TEXTURE_FORMAT Format   = TEX_FORMAT_UNKNOWN;

    /// Number of Mip levels in the texture. Multisampled textures can only have 1 Mip level.
    /// Specify 0 to generate full mipmap chain.
    Uint32 MipLevels        = 1;

    /// Number of samples.\n
    /// Only 2D textures or 2D texture arrays can be multisampled.
    Uint32 SampleCount      = 1;

    /// Texture usage. See Diligent::USAGE for details.
    USAGE Usage             = USAGE_DEFAULT;

    /// Bind flags, see Diligent::BIND_FLAGS for details. \n
    /// The following bind flags are allowed:
    /// Diligent::BIND_SHADER_RESOURCE, Diligent::BIND_RENDER_TARGET, Diligent::BIND_DEPTH_STENCIL,
    /// Diligent::and BIND_UNORDERED_ACCESS. \n
    /// Multisampled textures cannot have Diligent::BIND_UNORDERED_ACCESS flag set
    BIND_FLAGS BindFlags    = BIND_NONE;

    /// CPU access flags or 0 if no CPU access is allowed, 
    /// see Diligent::CPU_ACCESS_FLAGS for details.
    CPU_ACCESS_FLAGS CPUAccessFlags = CPU_ACCESS_NONE;
    
    /// Miscellaneous flags, see Diligent::MISC_TEXTURE_FLAGS for details.
    MISC_TEXTURE_FLAGS MiscFlags    = MISC_TEXTURE_FLAG_NONE;
    
    /// Optimized clear value
    OptimizedClearValue ClearValue;

    /// Defines which command queues this texture can be used with
    Uint64 CommandQueueMask         = 1;

    TextureDesc()noexcept{}

    TextureDesc(RESOURCE_DIMENSION  _Type, 
                Uint32              _Width,
                Uint32              _Height,
                Uint32              _ArraySizeOrDepth,
                TEXTURE_FORMAT      _Format,
                Uint32              _MipLevels        = TextureDesc{}.MipLevels,
                Uint32              _SampleCount      = TextureDesc{}.SampleCount,
                USAGE               _Usage            = TextureDesc{}.Usage,
                BIND_FLAGS          _BindFlags        = TextureDesc{}.BindFlags,
                CPU_ACCESS_FLAGS    _CPUAccessFlags   = TextureDesc{}.CPUAccessFlags,
                MISC_TEXTURE_FLAGS  _MiscFlags        = TextureDesc{}.MiscFlags,
                OptimizedClearValue _ClearValue       = TextureDesc{}.ClearValue,
                Uint64              _CommandQueueMask = TextureDesc{}.CommandQueueMask) : 
        Type             (_Type), 
        Width            (_Width),
        Height           (_Height),
        ArraySize        (_ArraySizeOrDepth),
        Format           (_Format),
        MipLevels        (_MipLevels),
        SampleCount      (_SampleCount),
        Usage            (_Usage),
        BindFlags        (_BindFlags),
        CPUAccessFlags   (_CPUAccessFlags),
        MiscFlags        (_MiscFlags),
        ClearValue       (_ClearValue),
        CommandQueueMask (_CommandQueueMask)
    {}

    /// Tests if two structures are equivalent

    /// \param [in] RHS - reference to the structure to perform comparison with
    /// \return 
    /// - True if all members of the two structures except for the Name are equal.
    /// - False otherwise.
    /// The operator ignores DeviceObjectAttribs::Name field as it does not affect 
    /// the texture description state.
    bool operator ==(const TextureDesc& RHS)const
    {
                // Name is primarily used for debug purposes and does not affect the state.
                // It is ignored in comparison operation.
        return  // strcmp(Name, RHS.Name) == 0          &&
                Type             == RHS.Type           &&
                Width            == RHS.Width          &&
                Height           == RHS.Height         &&
                ArraySize        == RHS.ArraySize      &&
                Format           == RHS.Format         &&
                MipLevels        == RHS.MipLevels      &&
                SampleCount      == RHS.SampleCount    &&
                Usage            == RHS.Usage          &&
                BindFlags        == RHS.BindFlags      &&
                CPUAccessFlags   == RHS.CPUAccessFlags &&
                MiscFlags        == RHS.MiscFlags      &&
                ClearValue       == RHS.ClearValue     &&
                CommandQueueMask == RHS.CommandQueueMask;
    }
};

/// Describes data for one subresource
struct TextureSubResData
{
    /// Pointer to the subresource data in CPU memory.
    /// If provided, pSrcBuffer must be null
    const void* pData           = nullptr;

    /// Pointer to the GPU buffer that contains subresource data.
    /// If provided, pData must be null
    class IBuffer* pSrcBuffer   = nullptr;

    /// When updating data from the buffer (pSrcBuffer is not null),
    /// offset from the beginning of the buffer to the data start
    Uint32 SrcOffset            = 0;

    /// For 2D and 3D textures, row stride in bytes
    Uint32 Stride               = 0;

    /// For 3D textures, depth slice stride in bytes
    /// \note On OpenGL, this must be a mutliple of Stride
    Uint32 DepthStride          = 0;

    /// Initializes the structure members with default values

    /// Default values:
    /// Member          | Default value
    /// ----------------|--------------
    /// pData           | nullptr
    /// SrcOffset       | 0
    /// Stride          | 0
    /// DepthStride     | 0
    TextureSubResData()noexcept{}
    
    /// Initializes the structure members to perform copy from the CPU memory
    TextureSubResData(void *_pData, Uint32 _Stride, Uint32 _DepthStride = 0)noexcept :
        pData       (_pData),
        pSrcBuffer  (nullptr),
        SrcOffset   (0),
        Stride      (_Stride),
        DepthStride (_DepthStride)
    {}

    /// Initializes the structure members to perform copy from the GPU buffer
    TextureSubResData(IBuffer *_pBuffer, Uint32 _SrcOffset, Uint32 _Stride, Uint32 _DepthStride = 0)noexcept :
        pData       (nullptr),
        pSrcBuffer  (_pBuffer),
        SrcOffset   (_SrcOffset),
        Stride      (_Stride),
        DepthStride (_DepthStride)
    {}
};

/// Describes the initial data to store in the texture
struct TextureData
{
    /// Pointer to the array of the TextureSubResData elements containing
    /// information about each subresource.
    TextureSubResData* pSubResources = nullptr;

    /// Number of elements in pSubResources array.
    /// NumSubresources must exactly match the number
    /// of subresources in the texture. Otherwise an error
    /// occurs.
    Uint32 NumSubresources           = 0;
};

struct MappedTextureSubresource
{
    PVoid pData        = nullptr;
    Uint32 Stride      = 0;
    Uint32 DepthStride = 0;
};

/// Texture inteface
class ITexture : public IDeviceObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface ) = 0;

    /// Returns the texture description used to create the object
    virtual const TextureDesc& GetDesc()const = 0;
    
    /// Creates a new texture view

    /// \param [in] ViewDesc - View description. See Diligent::TextureViewDesc for details.
    /// \param [out] ppView - Address of the memory location where the pointer to the view interface will be written to.
    /// 
    /// \remarks To create a shader resource view addressing the entire texture, set only TextureViewDesc::ViewType 
    ///          member of the ViewDesc parameter to Diligent::TEXTURE_VIEW_SHADER_RESOURCE and leave all other 
    ///          members in their default values. Using the same method, you can create render target or depth stencil
    ///          view addressing the largest mip level.\n
    ///          If texture view format is Diligent::TEX_FORMAT_UNKNOWN, the view format will match the texture format.\n
    ///          If texture view type is Diligent::TEXTURE_VIEW_UNDEFINED, the type will match the texture type.\n
    ///          If the number of mip levels is 0, and the view type is shader resource, the view will address all mip levels.
    ///          For other view types it will address one mip level.\n
    ///          If the number of slices is 0, all slices from FirstArraySlice or FirstDepthSlice will be referenced by the view.
    ///          For non-array textures, the only allowed values for the number of slices are 0 and 1.\n
    ///          Texture view will contain strong reference to the texture, so the texture will not be destroyed
    ///          until all views are released.\n
    ///          The function calls AddRef() for the created interface, so it must be released by
    ///          a call to Release() when it is no longer needed.
    virtual void CreateView(const struct TextureViewDesc &ViewDesc, class ITextureView **ppView) = 0;

    /// Returns the pointer to the default view.
    
    /// \param [in] ViewType - Type of the requested view. See Diligent::TEXTURE_VIEW_TYPE.
    /// \return Pointer to the interface
    ///
    /// \note The function does not increase the reference counter for the returned interface, so
    ///       Release() must *NOT* be called.
    virtual ITextureView* GetDefaultView( TEXTURE_VIEW_TYPE ViewType ) = 0;


    /// Returns native texture handle specific to the underlying graphics API

    /// \return pointer to ID3D11Resource interface, for D3D11 implementation\n
    ///         pointer to ID3D12Resource interface, for D3D12 implementation\n
    ///         GL buffer handle, for GL implementation
    virtual void* GetNativeHandle() = 0;

    /// Sets the usage state for all texture subresources.

    /// \note This method does not perform state transition, but
    ///       resets the internal texture state to the given value.
    ///       This method should be used after the application finished
    ///       manually managing the texture state and wants to hand over
    ///       state management back to the engine.
    virtual void SetState(RESOURCE_STATE State) = 0;

    /// Returns the internal texture state
    virtual RESOURCE_STATE GetState() const = 0;
};

}
