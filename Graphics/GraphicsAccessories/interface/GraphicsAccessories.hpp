/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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

/// \file
/// Defines graphics engine utilities

#include <vector>
#include <cstring>

#include "../../GraphicsEngine/interface/GraphicsTypes.h"
#include "../../GraphicsEngine/interface/Shader.h"
#include "../../GraphicsEngine/interface/Texture.h"
#include "../../GraphicsEngine/interface/Buffer.h"
#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../Archiver/interface/Archiver.h"
#include "../../../Common/interface/BasicMath.hpp"
#include "../../../Platforms/Basic/interface/DebugUtilities.hpp"
#include "../../../Platforms/interface/PlatformMisc.hpp"

namespace Diligent
{

/// Template structure to convert VALUE_TYPE enumeration into C-type
template <VALUE_TYPE ValType>
struct VALUE_TYPE2CType
{};

/// VALUE_TYPE2CType<> template specialization for 8-bit integer value type.

/// Usage example:
///
///     VALUE_TYPE2CType<VT_INT8>::CType MyInt8Var;
template <> struct VALUE_TYPE2CType<VT_INT8>
{
    typedef Int8 CType;
};

/// VALUE_TYPE2CType<> template specialization for 16-bit integer value type.

/// Usage example:
///
///     VALUE_TYPE2CType<VT_INT16>::CType MyInt16Var;
template <> struct VALUE_TYPE2CType<VT_INT16>
{
    typedef Int16 CType;
};

/// VALUE_TYPE2CType<> template specialization for 32-bit integer value type.

/// Usage example:
///
///     VALUE_TYPE2CType<VT_INT32>::CType MyInt32Var;
template <> struct VALUE_TYPE2CType<VT_INT32>
{
    typedef Int32 CType;
};

/// VALUE_TYPE2CType<> template specialization for 8-bit unsigned-integer value type.

/// Usage example:
///
///     VALUE_TYPE2CType<VT_UINT8>::CType MyUint8Var;
template <> struct VALUE_TYPE2CType<VT_UINT8>
{
    typedef Uint8 CType;
};

/// VALUE_TYPE2CType<> template specialization for 16-bit unsigned-integer value type.

/// Usage example:
///
///     VALUE_TYPE2CType<VT_UINT16>::CType MyUint16Var;
template <> struct VALUE_TYPE2CType<VT_UINT16>
{
    typedef Uint16 CType;
};

/// VALUE_TYPE2CType<> template specialization for 32-bit unsigned-integer value type.

/// Usage example:
///
///     VALUE_TYPE2CType<VT_UINT32>::CType MyUint32Var;
template <> struct VALUE_TYPE2CType<VT_UINT32>
{
    typedef Uint32 CType;
};

/// VALUE_TYPE2CType<> template specialization for half-precision 16-bit floating-point value type.

/// Usage example:
///
///     VALUE_TYPE2CType<VT_FLOAT16>::CType MyFloat16Var;
///
/// \note 16-bit floating-point values have no corresponding C++ type and are translated to Uint16
template <> struct VALUE_TYPE2CType<VT_FLOAT16>
{
    typedef Uint16 CType;
};

/// VALUE_TYPE2CType<> template specialization for full-precision 32-bit floating-point value type.

/// Usage example:
///
///     VALUE_TYPE2CType<VT_FLOAT32>::CType MyFloat32Var;
template <> struct VALUE_TYPE2CType<VT_FLOAT32>
{
    typedef Float32 CType;
};

/// VALUE_TYPE2CType<> template specialization for double-precision 64-bit floating-point value type.

/// Usage example:
///
///     VALUE_TYPE2CType<VT_FLOAT64>::CType MyFloat64Var;
template <> struct VALUE_TYPE2CType<VT_FLOAT64>
{
    typedef Float64 CType;
};

// clang-format off
static constexpr Uint32 ValueTypeToSizeMap[] =
{
    0,
    sizeof(VALUE_TYPE2CType<VT_INT8>    :: CType),
    sizeof(VALUE_TYPE2CType<VT_INT16>   :: CType),
    sizeof(VALUE_TYPE2CType<VT_INT32>   :: CType),
    sizeof(VALUE_TYPE2CType<VT_UINT8>   :: CType),
    sizeof(VALUE_TYPE2CType<VT_UINT16>  :: CType),
    sizeof(VALUE_TYPE2CType<VT_UINT32>  :: CType),
    sizeof(VALUE_TYPE2CType<VT_FLOAT16> :: CType),
    sizeof(VALUE_TYPE2CType<VT_FLOAT32> :: CType),
    sizeof(VALUE_TYPE2CType<VT_FLOAT64> :: CType),
};
// clang-format on
static_assert(VT_NUM_TYPES == 10, "Not all value type sizes initialized.");

/// Returns the size of the specified value type
inline Uint32 GetValueSize(VALUE_TYPE Val)
{
    VERIFY_EXPR(Val < _countof(ValueTypeToSizeMap));
    return ValueTypeToSizeMap[Val];
}

/// Returns the string representing the specified value type
const Char* GetValueTypeString(VALUE_TYPE Val);

/// Returns invariant texture format attributes, see TextureFormatAttribs for details.

/// \param [in] Format - Texture format which attributes are requested for.
/// \return Constant reference to the TextureFormatAttribs structure containing
///         format attributes.
const TextureFormatAttribs& GetTextureFormatAttribs(TEXTURE_FORMAT Format);

/// Converts value type to component type.

/// For example:
///  * `VT_UINT8, true,  false -> COMPONENT_TYPE_UNORM`
///  * `VT_UINT8, false, false -> COMPONENT_TYPE_UINT`
///  * `VT_UINT8, true,  true  -> COMPONENT_TYPE_UNORM_SRGB`
///
/// \note Use GetValueSize() to get the component size.
COMPONENT_TYPE ValueTypeToComponentType(VALUE_TYPE ValType, bool IsNormalized, bool IsSRGB);

/// Converts component type and size to value type

/// For example:
///  * `COMPONENT_TYPE_UNORM, 1 -> VT_UINT8`
///  * `COMPONENT_TYPE_FLOAT, 4 -> VT_FLOAT32`
VALUE_TYPE ComponentTypeToValueType(COMPONENT_TYPE CompType, Uint32 Size);

/// Returns texture format for the specified component type, size and number of components

/// For example:
///  * `COMPONENT_TYPE_UNORM, 1, 4 -> TEX_FORMAT_RGBA8_UNORM`
///  * `COMPONENT_TYPE_FLOAT, 4, 1 -> TEX_FORMAT_R32_FLOAT`
///
/// If the format is not found, `TEXTURE_FORMAT_UNKNOWN` is returned.
TEXTURE_FORMAT TextureComponentAttribsToTextureFormat(COMPONENT_TYPE CompType, Uint32 ComponentSize, Uint32 NumComponents);


/// Returns the default format for a specified texture view type

/// The default view is defined as follows:
/// * For a fully qualified texture format, the SRV/RTV/UAV view format is the same as texture format;
///   DSV format, if available, is adjusted accordingly (`R32_FLOAT -> D32_FLOAT`)
/// * For 32-bit typeless formats, default view is `XXXX32_FLOAT` (where `XXXX` are the actual format components)\n
/// * For 16-bit typeless formats, default view is `XXXX16_FLOAT` (where `XXXX` are the actual format components)\n
/// ** `R16_TYPELESS` is special. If `BIND_DEPTH_STENCIL` flag is set, it is translated to `R16_UNORM`/`D16_UNORM`;
///    otherwise it is translated to `R16_FLOAT`.
/// * For 8-bit typeless formats, default view is `XXXX8_UNORM` (where `XXXX` are the actual format components)\n
/// * sRGB is always chosen if it is available (`RGBA8_UNORM_SRGB`, `TEX_FORMAT_BC1_UNORM_SRGB`, etc.)
/// * For combined depth-stencil formats, SRV format references depth component (`R24_UNORM_X8_TYPELESS` for `D24S8` formats, and
///   `R32_FLOAT_X8X24_TYPELESS` for `D32S8X24` formats)
/// * For compressed formats, only SRV format is defined
///
/// \param [in] Format - texture format, for which the view format is requested
/// \param [in] ViewType - texture view type
/// \param [in] BindFlags - texture bind flags
/// \return  texture view type format
TEXTURE_FORMAT GetDefaultTextureViewFormat(TEXTURE_FORMAT TextureFormat, TEXTURE_VIEW_TYPE ViewType, Uint32 BindFlags);

/// Returns the default format for a specified texture view type

/// \param [in] TexDesc - texture description
/// \param [in] ViewType - texture view type
/// \return  texture view type format
inline TEXTURE_FORMAT GetDefaultTextureViewFormat(const TextureDesc& TexDesc, TEXTURE_VIEW_TYPE ViewType)
{
    return GetDefaultTextureViewFormat(TexDesc.Format, ViewType, TexDesc.BindFlags);
}

/// Returns the literal name of a texture view type. For instance,
/// for a shader resource view, "TEXTURE_VIEW_SHADER_RESOURCE" will be returned.

/// \param [in] ViewType - Texture view type.
/// \return Literal name of the texture view type.
const Char* GetTexViewTypeLiteralName(TEXTURE_VIEW_TYPE ViewType);

/// Returns the literal name of a buffer view type. For instance,
/// for an unordered access view, "BUFFER_VIEW_UNORDERED_ACCESS" will be returned.

/// \param [in] ViewType - Buffer view type.
/// \return Literal name of the buffer view type.
const Char* GetBufferViewTypeLiteralName(BUFFER_VIEW_TYPE ViewType);

/// Returns the literal name of a shader type. For instance,
/// for a pixel shader, "SHADER_TYPE_PIXEL" will be returned.

/// \param [in] ShaderType - Shader type.
/// \return Literal name of the shader type.
const Char* GetShaderTypeLiteralName(SHADER_TYPE ShaderType);

/// \param [in] ShaderStages - Shader stages.
/// \return The string representing the shader stages. For example,
///         if ShaderStages == SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL,
///         the following string will be returned:
///         "SHADER_TYPE_VERTEX, SHADER_TYPE_PIXEL"
String GetShaderStagesString(SHADER_TYPE ShaderStages);

/// Returns the literal name of a shader variable type. For instance,
/// for SHADER_RESOURCE_VARIABLE_TYPE_STATIC, if bGetFullName == true, "SHADER_RESOURCE_VARIABLE_TYPE_STATIC" will be returned;
/// if bGetFullName == false, "static" will be returned

/// \param [in] VarType - Variable type.
/// \param [in] bGetFullName - Whether to return string representation of the enum value
/// \return Literal name of the shader variable type.
const Char* GetShaderVariableTypeLiteralName(SHADER_RESOURCE_VARIABLE_TYPE VarType, bool bGetFullName = false);

/// Returns the literal name of a shader resource type. For instance,
/// for `SHADER_RESOURCE_TYPE_CONSTANT_BUFFER`, if `bGetFullName == true`, `"SHADER_RESOURCE_TYPE_CONSTANT_BUFFER"` will be returned;
/// if bGetFullName == false, "constant buffer" will be returned

/// \param [in] ResourceType - Resource type.
/// \param [in] bGetFullName - Whether to return string representation of the enum value
/// \return Literal name of the shader resource type.
const Char* GetShaderResourceTypeLiteralName(SHADER_RESOURCE_TYPE ResourceType, bool bGetFullName = false);

/// Overloaded function that returns the literal name of a texture view type.
/// see GetTexViewTypeLiteralName().
inline const Char* GetViewTypeLiteralName(TEXTURE_VIEW_TYPE TexViewType)
{
    return GetTexViewTypeLiteralName(TexViewType);
}

/// Overloaded function that returns the literal name of a buffer view type.
/// see GetBufferViewTypeLiteralName().
inline const Char* GetViewTypeLiteralName(BUFFER_VIEW_TYPE BuffViewType)
{
    return GetBufferViewTypeLiteralName(BuffViewType);
}

/// Returns the literal name of a filter type. For instance,
/// for FILTER_TYPE_POINT, if bGetFullName == true, "FILTER_TYPE_POINT" will be returned;
/// if bGetFullName == false, "point" will be returned.

/// \param [in] FilterType   - Filter type, see Diligent::FILTER_TYPE.
/// \param [in] bGetFullName - Whether to return string representation of the enum value.
/// \return                    Literal name of the filter type.
const Char* GetFilterTypeLiteralName(FILTER_TYPE FilterType, bool bGetFullName);


/// Returns the literal name of a texture address mode. For instance,
/// for TEXTURE_ADDRESS_WRAP, if bGetFullName == true, "TEXTURE_ADDRESS_WRAP" will be returned;
/// if bGetFullName == false, "wrap" will be returned.

/// \param [in] AddressMode  - Texture address mode, see Diligent::TEXTURE_ADDRESS_MODE.
/// \param [in] bGetFullName - Whether to return string representation of the enum value.
/// \return                    Literal name of the address mode.
const Char* GetTextureAddressModeLiteralName(TEXTURE_ADDRESS_MODE AddressMode, bool bGetFullName);


/// Returns the literal name of a comparison function. For instance,
/// for COMPARISON_FUNC_LESS, if bGetFullName == true, "COMPARISON_FUNC_LESS" will be returned;
/// if bGetFullName == false, "less" will be returned.

/// \param [in] ComparisonFunc - Comparison function, see Diligent::COMPARISON_FUNCTION.
/// \param [in] bGetFullName   - Whether to return string representation of the enum value.
/// \return                      Literal name of the comparison function.
const Char* GetComparisonFunctionLiteralName(COMPARISON_FUNCTION ComparisonFunc, bool bGetFullName);


/// Returns the literal name of a stencil operation.

/// \param [in] StencilOp - Stencil operation, see Diligent::STENCIL_OP.
/// \return                 Literal name of the stencil operation.
const Char* GetStencilOpLiteralName(STENCIL_OP StencilOp);


/// Returns the literal name of a blend factor.

/// \param [in] BlendFactor - Blend factor, see Diligent::BLEND_FACTOR.
/// \return                   Literal name of the blend factor.
const Char* GetBlendFactorLiteralName(BLEND_FACTOR BlendFactor);


/// Returns the literal name of a blend operation.

/// \param [in] BlendOp - Blend operation, see Diligent::BLEND_OPERATION.
/// \return               Literal name of the blend operation.
const Char* GetBlendOperationLiteralName(BLEND_OPERATION BlendOp);


/// Returns the literal name of a fill mode.

/// \param [in] FillMode - Fill mode, see Diligent::FILL_MODE.
/// \return                Literal name of the fill mode.
const Char* GetFillModeLiteralName(FILL_MODE FillMode);

/// Returns the literal name of a cull mode.

/// \param [in] CullMode      - Cull mode, see Diligent::CULL_MODE.
/// \param [in] GetEnumString - Whether to return string representation of the enum value.

/// \return                    Literal name of the cull mode (e.g. "CULL_MODE_BACK" when bGetFullName == true,
///                            or "back" when GetEnumString == false).
const Char* GetCullModeLiteralName(CULL_MODE CullMode, bool GetEnumString = false);

/// Returns the string containing the map type
const Char* GetMapTypeString(MAP_TYPE MapType);

/// Returns the string containing the usage
const Char* GetUsageString(USAGE Usage);

/// Returns the string containing the texture type
const Char* GetResourceDimString(RESOURCE_DIMENSION TexType);

/// Returns the string containing single bind flag
const Char* GetBindFlagString(Uint32 BindFlag);

/// Returns the string containing the bind flags
String GetBindFlagsString(Uint32 BindFlags, const char* Delimiter = "|");

/// Returns the string containing the CPU access flags
String GetCPUAccessFlagsString(Uint32 CpuAccessFlags);

/// Returns the string containing the texture description
String GetTextureDescString(const TextureDesc& Desc);

/// Returns the string containing the buffer format description
String GetBufferFormatString(const BufferFormat& Fmt);

/// Returns the string containing the buffer mode description
const Char* GetBufferModeString(BUFFER_MODE Mode);

/// Returns the string containing the buffer description
String GetBufferDescString(const BufferDesc& Desc);

/// Returns the string containing the shader description
String GetShaderDescString(const ShaderDesc& Desc);

/// Returns the string containing the buffer mode description
const Char* GetResourceStateFlagString(RESOURCE_STATE State);
String      GetResourceStateString(RESOURCE_STATE State);

/// Returns the string containing the command queue type
String GetCommandQueueTypeString(COMMAND_QUEUE_TYPE Type);

/// Returns the string containing the fence type
const Char* GetFenceTypeString(FENCE_TYPE Type);

/// Returns the string containing the shader status (e.g. "SHADER_STATUS_UNINITIALIZED" when GetEnumString is true,
/// or "Uninitialized" when GetEnumString is false).
const Char* GetShaderStatusString(SHADER_STATUS ShaderStatus, bool GetEnumString = false);

/// Returns the string containing the pipeline state status (e.g. "PIPELINE_STATE_STATUS_UNINITIALIZED" when
/// GetEnumString is true, or "Uninitialized" when GetEnumString is false).
const Char* GetPipelineStateStatusString(PIPELINE_STATE_STATUS PipelineStatus, bool GetEnumString = false);


/// Helper template function that converts object description into a string
template <typename TObjectDescType>
String GetObjectDescString(const TObjectDescType&)
{
    return "";
}

inline String GetAttachmentReferenceString(const AttachmentReference& Attachment)
{
    return std::to_string(Attachment.AttachmentIndex) + ", " + GetResourceStateString(Attachment.State);
}

/// Template specialization for texture description
template <>
inline String GetObjectDescString(const TextureDesc& TexDesc)
{
    String Str{"Tex desc: "};
    Str += GetTextureDescString(TexDesc);
    return Str;
}

/// Template specialization for buffer description
template <>
inline String GetObjectDescString(const BufferDesc& BuffDesc)
{
    String Str{"Buff desc: "};
    Str += GetBufferDescString(BuffDesc);
    return Str;
}

/// Returns the string representation of the QUERY_TYPE enum value (e.g. "QUERY_TYPE_OCCLUSION")
const char* GetQueryTypeString(QUERY_TYPE QueryType);

/// Returns the string representation of the SURFACE_TRANSFORM enum value (e.g. "SURFACE_TRANSFORM_ROTATE_90")
const char* GetSurfaceTransformString(SURFACE_TRANSFORM SrfTransform);

/// Returns the string representation of the PIPELINE_TYPE enum value (e.g. "PIPELINE_TYPE_COMPUTE")
const char* GetPipelineTypeString(PIPELINE_TYPE PipelineType);

/// Returns the string representation of the SHADER_COMPILER enum value (e.g. "SHADER_COMPILER_GLSLANG")
const char* GetShaderCompilerTypeString(SHADER_COMPILER Compiler);

/// Returns the string representation of the ARCHIVE_DEVICE_DATA_FLAGS enum value

/// \param [in] Flag - Archive device data flag.
/// \param [in] bGetFullName - Whether to return full name of the enum value
/// \return Literal name of the archive device data flag.
///
/// For example, if bGetFullName == true, "ARCHIVE_DEVICE_DATA_FLAG_D3D11" will be returned;
/// if bGetFullName == false, "D3D11" will be returned.
///
/// \note A single flag must be passed to this function.
const char* GetArchiveDeviceDataFlagString(ARCHIVE_DEVICE_DATA_FLAGS Flag, bool bGetFullName = false);


/// Returns the string representation of the DEVICE_FEATURE_STATE enum value

/// \param [in] State - Device feature state.
/// \param [in] bGetFullName - Whether to return full name of the enum value
/// \return Literal name of the device feature state.
///
/// For example, if bGetFullName == true, "DEVICE_FEATURE_STATE_ENABLED" will be returned;
/// if bGetFullName == false, "Enabled" will be returned.
const char* GetDeviceFeatureStateString(DEVICE_FEATURE_STATE State, bool bGetFullName = false);


/// Returns the render device type string (e.g. "RENDER_DEVICE_TYPE_D3D11" when GetEnumString is true,
/// or "Direct3D11" when GetEnumString is false).
const char* GetRenderDeviceTypeString(RENDER_DEVICE_TYPE DeviceType, bool GetEnumString = false);

/// Returns the render device type short string (e.g. "D3D11" when Capital is true,
/// or "d3d11" when Capital is false).
const char* GetRenderDeviceTypeShortString(RENDER_DEVICE_TYPE DeviceType, bool Capital = false);

const char* GetAdapterTypeString(ADAPTER_TYPE AdapterType, bool bGetEnumString = false);

String GetPipelineResourceFlagsString(PIPELINE_RESOURCE_FLAGS Flags, bool GetFullName = false, const char* DelimiterString = "|");

const char* GetShaderCodeVariableClassString(SHADER_CODE_VARIABLE_CLASS Class);

const char* GetShaderCodeBasicTypeString(SHADER_CODE_BASIC_TYPE Type);

/// Returns the string containing the shader buffer description.
String GetShaderCodeBufferDescString(const ShaderCodeBufferDesc& Desc, size_t GlobalIdent = 0, size_t MemberIdent = 2);

/// Returns the string containing the shader code variable description.
String GetShaderCodeVariableDescString(const ShaderCodeVariableDesc& Desc, size_t GlobalIdent = 0, size_t MemberIdent = 2);

/// Returns the string representation of the input element frequency (e.g. "undefined")
const char* GetInputElementFrequencyString(INPUT_ELEMENT_FREQUENCY Frequency);

/// Returns the string containing the layout element description.
String GetLayoutElementString(const LayoutElement& Element);

/// Returns valid pipeline resource flags for the specified shader resource type
PIPELINE_RESOURCE_FLAGS GetValidPipelineResourceFlags(SHADER_RESOURCE_TYPE ResourceType);

/// Converts shader variable flags to corresponding pipeline resource flags
PIPELINE_RESOURCE_FLAGS ShaderVariableFlagsToPipelineResourceFlags(SHADER_VARIABLE_FLAGS Flags);

/// Returns bind flags for the specified swap chain usage flags
BIND_FLAGS SwapChainUsageFlagsToBindFlags(SWAP_CHAIN_USAGE_FLAGS SwapChainUsage);

ARCHIVE_DEVICE_DATA_FLAGS RenderDeviceTypeToArchiveDataFlag(RENDER_DEVICE_TYPE DevType);
RENDER_DEVICE_TYPE        ArchiveDataFlagToRenderDeviceType(ARCHIVE_DEVICE_DATA_FLAGS Flag);

/// Returns the number of mip levels for the specified texture dimensions
Uint32 ComputeMipLevelsCount(Uint32 Width);

/// Returns the number of mip levels for the specified texture dimensions
Uint32 ComputeMipLevelsCount(Uint32 Width, Uint32 Height);

/// Returns the number of mip levels for the specified texture dimensions
Uint32 ComputeMipLevelsCount(Uint32 Width, Uint32 Height, Uint32 Depth);

/// Checks if the specified filter type is a point filter
inline bool IsComparisonFilter(FILTER_TYPE FilterType)
{
    return FilterType == FILTER_TYPE_COMPARISON_POINT ||
        FilterType == FILTER_TYPE_COMPARISON_LINEAR ||
        FilterType == FILTER_TYPE_COMPARISON_ANISOTROPIC;
}

/// Checks if the specified filter type is an anisotropic filter
inline bool IsAnisotropicFilter(FILTER_TYPE FilterType)
{
    return FilterType == FILTER_TYPE_ANISOTROPIC ||
        FilterType == FILTER_TYPE_COMPARISON_ANISOTROPIC ||
        FilterType == FILTER_TYPE_MINIMUM_ANISOTROPIC ||
        FilterType == FILTER_TYPE_MAXIMUM_ANISOTROPIC;
}

bool VerifyResourceStates(RESOURCE_STATE State, bool IsTexture);

/// Describes the mip level properties
struct MipLevelProperties
{
    /// Logical mip width.
    Uint32 LogicalWidth = 0;

    /// Logical mip height.
    Uint32 LogicalHeight = 0;

    /// Storage mip width.

    /// \note   For compressed formats, storage width is rounded
    ///         up to the block size. For example, for a texture
    ///         mip with logical width 10 and BC1 format (with 4x4
    ///         pixel block size), the storage width will be 12.
    Uint32 StorageWidth = 0;

    /// Storage mip height.

    /// \note   For compressed formats, storage height is rounded
    ///         up to the block size. For example, for a texture
    ///         mip with logical height 10 and BC1 format (with 4x4
    ///         pixel block size), the storage height will be 12.
    Uint32 StorageHeight = 0;

    /// Mip level depth.

    /// \note that logical and storage depths are always the same.
    Uint32 Depth = 1;

    /// Row size in bytes.

    /// \note   For compressed formats, row size defines
    ///         the size of one row of compressed blocks.
    Uint64 RowSize = 0;

    /// Depth slice size in bytes.
    Uint64 DepthSliceSize = 0;

    /// Total mip level data size in bytes.
    Uint64 MipSize = 0;
};

/// Returns mip level properties for the specified texture description and mip level
MipLevelProperties GetMipLevelProperties(const TextureDesc& TexDesc, Uint32 MipLevel);

ADAPTER_VENDOR VendorIdToAdapterVendor(Uint32 VendorId);
Uint32         AdapterVendorToVendorId(ADAPTER_VENDOR Vendor);


inline Int32 GetShaderTypeIndex(SHADER_TYPE Type)
{
    if (Type == SHADER_TYPE_UNKNOWN)
        return -1;

    VERIFY(Type > SHADER_TYPE_UNKNOWN && Type <= SHADER_TYPE_LAST, "Value ", Uint32{Type}, " is not a valid SHADER_TYPE enum value");
    VERIFY(((Uint32{Type} & (Uint32{Type} - 1)) == 0), "Only single shader stage should be provided");

    return PlatformMisc::GetLSB(Type);
}

inline Int32 GetFirstShaderStageIndex(SHADER_TYPE Stages)
{
    if (Stages == SHADER_TYPE_UNKNOWN)
        return -1;

    VERIFY(Stages > SHADER_TYPE_UNKNOWN && Stages < SHADER_TYPE_LAST * 2, "Value ", Uint32{Stages}, " is not a valid SHADER_TYPE enum value");

    return PlatformMisc::GetLSB(Stages);
}

inline Int32 ExtractFirstShaderStageIndex(SHADER_TYPE& Stages)
{
    if (Stages == SHADER_TYPE_UNKNOWN)
        return -1;

    VERIFY(Stages > SHADER_TYPE_UNKNOWN && Stages < SHADER_TYPE_LAST * 2, "Value ", Uint32{Stages}, " is not a valid SHADER_TYPE enum value");

    const Uint32 StageIndex = PlatformMisc::GetLSB(Stages);
    Stages &= ~static_cast<SHADER_TYPE>(1u << StageIndex);
    return StageIndex;
}


static_assert(SHADER_TYPE_LAST == 0x4000, "Please add the new shader type index below");

static constexpr Int32 VSInd   = 0;
static constexpr Int32 PSInd   = 1;
static constexpr Int32 GSInd   = 2;
static constexpr Int32 HSInd   = 3;
static constexpr Int32 DSInd   = 4;
static constexpr Int32 CSInd   = 5;
static constexpr Int32 ASInd   = 6;
static constexpr Int32 MSInd   = 7;
static constexpr Int32 RGSInd  = 8;
static constexpr Int32 RMSInd  = 9;
static constexpr Int32 RCHSInd = 10;
static constexpr Int32 RAHSInd = 11;
static constexpr Int32 RISInd  = 12;
static constexpr Int32 RCSInd  = 13;
static constexpr Int32 TLSInd  = 14;

static constexpr Int32 LastShaderInd = TLSInd;

// clang-format off
static_assert(SHADER_TYPE_VERTEX           == (1 << VSInd),   "VSInd is not consistent with SHADER_TYPE_VERTEX");
static_assert(SHADER_TYPE_PIXEL            == (1 << PSInd),   "PSInd is not consistent with SHADER_TYPE_PIXEL");
static_assert(SHADER_TYPE_GEOMETRY         == (1 << GSInd),   "GSInd is not consistent with SHADER_TYPE_GEOMETRY");
static_assert(SHADER_TYPE_HULL             == (1 << HSInd),   "HSInd is not consistent with SHADER_TYPE_HULL");
static_assert(SHADER_TYPE_DOMAIN           == (1 << DSInd),   "DSInd is not consistent with SHADER_TYPE_DOMAIN");
static_assert(SHADER_TYPE_COMPUTE          == (1 << CSInd),   "CSInd is not consistent with SHADER_TYPE_COMPUTE");
static_assert(SHADER_TYPE_AMPLIFICATION    == (1 << ASInd),   "ASInd is not consistent with SHADER_TYPE_AMPLIFICATION");
static_assert(SHADER_TYPE_MESH             == (1 << MSInd),   "MSInd is not consistent with SHADER_TYPE_MESH");
static_assert(SHADER_TYPE_RAY_GEN          == (1 << RGSInd),  "RGSInd is not consistent with SHADER_TYPE_RAY_GEN");
static_assert(SHADER_TYPE_RAY_MISS         == (1 << RMSInd),  "RMSInd is not consistent with SHADER_TYPE_RAY_MISS");
static_assert(SHADER_TYPE_RAY_CLOSEST_HIT  == (1 << RCHSInd), "RCHSInd is not consistent with SHADER_TYPE_RAY_CLOSEST_HIT");
static_assert(SHADER_TYPE_RAY_ANY_HIT      == (1 << RAHSInd), "RAHSInd is not consistent with SHADER_TYPE_RAY_ANY_HIT");
static_assert(SHADER_TYPE_RAY_INTERSECTION == (1 << RISInd),  "RISInd is not consistent with SHADER_TYPE_RAY_INTERSECTION");
static_assert(SHADER_TYPE_CALLABLE         == (1 << RCSInd),  "RCSInd is not consistent with SHADER_TYPE_CALLABLE");
static_assert(SHADER_TYPE_TILE             == (1 << TLSInd),  "TLSInd is not consistent with SHADER_TYPE_TILE");

static_assert(SHADER_TYPE_LAST == (1 << LastShaderInd), "LastShaderInd is not consistent with SHADER_TYPE_LAST");
// clang-format on

inline SHADER_TYPE GetShaderTypeFromIndex(Int32 Index)
{
    VERIFY(Index >= 0 && Index <= LastShaderInd, "Shader type index is out of range");
    return static_cast<SHADER_TYPE>(1 << Index);
}


bool          IsConsistentShaderType(SHADER_TYPE ShaderType, PIPELINE_TYPE PipelineType);
Int32         GetShaderTypePipelineIndex(SHADER_TYPE ShaderType, PIPELINE_TYPE PipelineType);
SHADER_TYPE   GetShaderTypeFromPipelineIndex(Int32 Index, PIPELINE_TYPE PipelineType);
PIPELINE_TYPE PipelineTypeFromShaderStages(SHADER_TYPE ShaderStages);

/// Returns an offset from the beginning of the buffer backing a staging texture
/// to the specified location within the given subresource.
///
/// \param [in] TexDesc     - Staging texture description.
/// \param [in] ArraySlice  - Array slice.
/// \param [in] MipLevel    - Mip level.
/// \param [in] Alignment   - Subresource alignment. The alignment is applied
///                           to whole subresources only, but not to the row/depth strides.
///                           In other words, there may be padding between subresources, but
///                           texels in every subresource are assumed to be tightly packed.
/// \param [in] LocationX   - X location within the subresource.
/// \param [in] LocationY   - Y location within the subresource.
/// \param [in] LocationZ   - Z location within the subresource.
///
/// \return     Offset from the beginning of the buffer to the given location.
///
/// \remarks
///     Alignment is applied to the subresource sizes, such that the beginning of data
///     of every subresource starts at an offset aligned by 'Alignment'. The alignment
///     is not applied to the row/depth strides and texels in all subresources are assumed
///     to be tightly packed.
///
///                 Subres 0
///                  stride
///           |<-------------->|
///           |________________|       Subres 1
///           |                |        stride
///           |                |     |<------->|
///           |                |     |_________|
///           |    Subres 0    |     |         |
///           |                |     | Subres 1|
///           |                |     |         |                     _
///           |________________|     |_________|         ...        |_|
///           A                      A                              A
///           |                      |                              |
///         Buffer start            Subres 1 offset,               Subres N offset,
///                              aligned by 'Alignment'         aligned by 'Alignment'
///
Uint64 GetStagingTextureLocationOffset(const TextureDesc& TexDesc,
                                       Uint32             ArraySlice,
                                       Uint32             MipLevel,
                                       Uint32             Alignment,
                                       Uint32             LocationX,
                                       Uint32             LocationY,
                                       Uint32             LocationZ);

/// Returns an offset from the beginning of the buffer backing a staging texture
/// to the given subresource.
/// Texels within subresources are assumed to be tightly packed. There is no padding
/// except between whole subresources.
inline Uint64 GetStagingTextureSubresourceOffset(const TextureDesc& TexDesc,
                                                 Uint32             ArraySlice,
                                                 Uint32             MipLevel,
                                                 Uint32             Alignment)
{
    return GetStagingTextureLocationOffset(TexDesc, ArraySlice, MipLevel, Alignment, 0, 0, 0);
}

/// Returns the total memory size required to store the staging texture data.
inline Uint64 GetStagingTextureDataSize(const TextureDesc& TexDesc,
                                        Uint32             Alignment = 4)
{
    return GetStagingTextureSubresourceOffset(TexDesc, TexDesc.GetArraySize(), 0, Alignment);
}

/// Information required to perform a copy operation between a buffer and a texture
struct BufferToTextureCopyInfo
{
    /// Texture region row size, in bytes. For compressed formats,
    /// this is the size of one row of compressed blocks.
    Uint64 RowSize = 0;

    /// Row stride, in bytes. The stride is computed by
    /// aligning the RowSize, and is thus always >= RowSize.
    Uint64 RowStride = 0;

    /// Row stride in texels.
    Uint32 RowStrideInTexels = 0;

    /// The number of rows in the region. For compressed formats,
    /// this is the number of compressed-block rows.
    Uint32 RowCount = 0;

    /// Depth stride (RowStride * RowCount)
    Uint64 DepthStride = 0;

    /// Total memory size required to store the pixels in the region.
    Uint64 MemorySize = 0;

    /// Texture region
    Box Region;
};
BufferToTextureCopyInfo GetBufferToTextureCopyInfo(TEXTURE_FORMAT Format,
                                                   const Box&     Region,
                                                   Uint32         RowStrideAlignment);


/// Copies texture subresource data on the CPU.

/// \param [in] SrcSubres      - Source subresource data.
/// \param [in] NumRows        - The number of rows in the subresource.
/// \param [in] NumDepthSlices - The number of depth slices in the subresource.
/// \param [in] RowSize        - Subresource data row size, in bytes.
/// \param [in] pDstData       - Pointer to the destination subresource data.
/// \param [in] DstRowStride   - Destination subresource row stride, in bytes.
/// \param [in] DstDepthStride - Destination subresource depth stride, in bytes.
void CopyTextureSubresource(const TextureSubResData& SrcSubres,
                            Uint32                   NumRows,
                            Uint32                   NumDepthSlices,
                            Uint64                   RowSize,
                            void*                    pDstData,
                            Uint64                   DstRowStride,
                            Uint64                   DstDepthStride);


inline String GetShaderResourcePrintName(const char* Name, Uint32 ArraySize, Uint32 ArrayIndex)
{
    VERIFY(ArrayIndex < ArraySize, "Array index is out of range");
    String PrintName = Name;
    if (ArraySize > 1)
    {
        PrintName.push_back('[');
        PrintName.append(std::to_string(ArrayIndex));
        PrintName.push_back(']');
    }
    return PrintName;
}

template <typename DescType>
String GetShaderResourcePrintName(const DescType& ResDesc, Uint32 ArrayIndex = 0)
{
    return GetShaderResourcePrintName(ResDesc.Name, ResDesc.ArraySize, ArrayIndex);
}

/// Converts UNORM format to a corresponding SRGB format

/// For example:
///   * `RGBA8_UNORM -> RGBA8_UNORM_SRGB`
///   * `BC3_UNORM   -> BC3_UNORM_SRGB`
TEXTURE_FORMAT UnormFormatToSRGB(TEXTURE_FORMAT Fmt);

/// Converts SRGB format to a corresponding UNORM format

/// For example:
///   * `RGBA8_UNORM_SRGB -> RGBA8_UNORM`
///   * `BC3_UNORM_SRGB   -> BC3_UNORM`
TEXTURE_FORMAT SRGBFormatToUnorm(TEXTURE_FORMAT Fmt);

/// Converts block-compressed format to a corresponding uncompressed format

/// For example:
///   * `BC1_UNORM -> RGBA8_UNORM`
///   * `BC4_UNORM -> R8_UNORM`
TEXTURE_FORMAT BCFormatToUncompressed(TEXTURE_FORMAT Fmt);

/// Converts typeless format to a corresponding UNORM format

/// For example:
///   * `RGBA8_TYPELESS -> RGBA8_UNORM`
///   * `BC1_TYPELESS   -> BC1_UNORM`
///
/// If the format is not typeless, or cannot be converted to UNORM, it is returned as is.
TEXTURE_FORMAT TypelessFormatToUnorm(TEXTURE_FORMAT Fmt);

/// Converts typeless format to a corresponding SRGB format

/// For example:
///   * `RGBA8_TYPELESS -> RGBA8_UNORM_SRGB`
///   * `BC1_TYPELESS   -> BC1_UNORM_SRGB`
///
/// If the format is not typeless, or cannot be converted to SRGB, it is returned as is.
TEXTURE_FORMAT TypelessFormatToSRGB(TEXTURE_FORMAT Fmt);

/// Checks if the format is an SRGB format
bool IsSRGBFormat(TEXTURE_FORMAT Fmt);

String GetPipelineShadingRateFlagsString(PIPELINE_SHADING_RATE_FLAGS Flags);

/// Converts texture component mapping to a string

/// For example:
///  * `{R, G, B, A} -> "rgba"`
///  * `{R, G, B, 1} -> "rgb1"`
String GetTextureComponentMappingString(const TextureComponentMapping& Mapping);

/// Converts texture component mapping string to the mapping

/// For example:
///  * `"rgba" -> {R, G, B, A}`
///  * `"rgb1" -> {R, G, B, 1}`
bool TextureComponentMappingFromString(const String& MappingStr, TextureComponentMapping& Mapping);


/// Returns the sparse texture properties assuming the standard tile shapes
SparseTextureProperties GetStandardSparseTextureProperties(const TextureDesc& TexDesc);

/// Returns the number of sparse memory tiles in the given box region
inline uint3 GetNumSparseTilesInBox(const Box& Region, const Uint32 TileSize[3])
{
    return uint3 // clang-format off
        {
            (Region.Width()  + TileSize[0] - 1) / TileSize[0],
            (Region.Height() + TileSize[1] - 1) / TileSize[1],
            (Region.Depth()  + TileSize[2] - 1) / TileSize[2]
        }; // clang-format on
}

/// Returns the number of sparse memory tiles in the given texture mip level
inline uint3 GetNumSparseTilesInMipLevel(const TextureDesc& Desc,
                                         const Uint32       TileSize[3],
                                         Uint32             MipLevel)
{
    // Texture dimensions may not be multiples of the tile size
    const MipLevelProperties MipProps = GetMipLevelProperties(Desc, MipLevel);
    return GetNumSparseTilesInBox(Box{0, MipProps.StorageWidth, 0, MipProps.StorageHeight, 0, MipProps.Depth}, TileSize);
}

/// Returns true if the Mapping defines an identity texture component swizzle
bool IsIdentityComponentMapping(const TextureComponentMapping& Mapping);

/// Resolves LAYOUT_ELEMENT_AUTO_OFFSET and LAYOUT_ELEMENT_AUTO_STRIDE values in the input layout,
/// and returns an array of buffer strides for each used input buffer slot.
std::vector<Uint32> ResolveInputLayoutAutoOffsetsAndStrides(LayoutElement* pLayoutElements, Uint32 NumElements);

inline void WriteShaderMatrix(void* pDst, const float4x4& Mat, bool Transpose)
{
    if (!Transpose)
    {
        std::memcpy(pDst, &Mat, sizeof(float4x4));
    }
    else
    {
        const float4x4 TransposedMat = Mat.Transpose();
        std::memcpy(pDst, &TransposedMat, sizeof(float4x4));
    }
}

inline void WriteShaderMatrices(void* pDst, const float4x4* pMat, size_t NumMatrices, bool Transpose)
{
    if (!Transpose)
    {
        std::memcpy(pDst, pMat, sizeof(float4x4) * NumMatrices);
    }
    else
    {
        for (size_t i = 0; i < NumMatrices; ++i)
        {
            const float4x4 TransposedMat = pMat[i].Transpose();
            std::memcpy(static_cast<float4x4*>(pDst) + i, &TransposedMat, sizeof(float4x4));
        }
    }
}

template <typename CreateInfoType, typename HandlerType>
typename std::enable_if<std::is_same<typename std::decay<CreateInfoType>::type, GraphicsPipelineStateCreateInfo>::value>::type
ProcessPipelineStateCreateInfoShaders(CreateInfoType&& CI, HandlerType&& Handler)
{
    Handler(CI.pVS);
    Handler(CI.pPS);
    Handler(CI.pDS);
    Handler(CI.pHS);
    Handler(CI.pGS);
    Handler(CI.pAS);
    Handler(CI.pMS);
}

template <typename CreateInfoType, typename HandlerType>
typename std::enable_if<std::is_same<typename std::decay<CreateInfoType>::type, ComputePipelineStateCreateInfo>::value>::type
ProcessPipelineStateCreateInfoShaders(CreateInfoType&& CI, HandlerType&& Handler)
{
    Handler(CI.pCS);
}

template <typename CreateInfoType, typename HandlerType>
typename std::enable_if<std::is_same<typename std::decay<CreateInfoType>::type, TilePipelineStateCreateInfo>::value>::type
ProcessPipelineStateCreateInfoShaders(CreateInfoType&& CI, HandlerType&& Handler)
{
    Handler(CI.pTS);
}

template <typename CreateInfoType, typename HandlerType>
typename std::enable_if<std::is_same<typename std::decay<CreateInfoType>::type, RayTracingPipelineStateCreateInfo>::value>::type
ProcessPipelineStateCreateInfoShaders(CreateInfoType&& CI, HandlerType&& Handler)
{
    for (Uint32 i = 0; i < CI.GeneralShaderCount; ++i)
    {
        Handler(CI.pGeneralShaders[i].pShader);
    }

    for (Uint32 i = 0; i < CI.TriangleHitShaderCount; ++i)
    {
        Handler(CI.pTriangleHitShaders[i].pClosestHitShader);
        Handler(CI.pTriangleHitShaders[i].pAnyHitShader);
    }

    for (Uint32 i = 0; i < CI.ProceduralHitShaderCount; ++i)
    {
        Handler(CI.pProceduralHitShaders[i].pIntersectionShader);
        Handler(CI.pProceduralHitShaders[i].pClosestHitShader);
        Handler(CI.pProceduralHitShaders[i].pAnyHitShader);
    }
}

template <typename CreateInfoType>
SHADER_STATUS GetPipelineStateCreateInfoShadersStatus(const CreateInfoType& CI, bool WaitForCompletion = false)
{
    SHADER_STATUS OverallStatus = SHADER_STATUS_READY;
    ProcessPipelineStateCreateInfoShaders(CI, [&OverallStatus, WaitForCompletion](IShader* pShader) {
        if (pShader == nullptr)
            return;

        SHADER_STATUS ShaderStatus = pShader->GetStatus(WaitForCompletion);
        switch (ShaderStatus)
        {
            case SHADER_STATUS_UNINITIALIZED:
                UNEXPECTED("Shader status must not be uninitialized");
                break;

            case SHADER_STATUS_COMPILING:
                OverallStatus = (OverallStatus == SHADER_STATUS_READY) ? SHADER_STATUS_COMPILING : OverallStatus;
                break;

            case SHADER_STATUS_READY:
                // Do nothing
                break;

            case SHADER_STATUS_FAILED:
                OverallStatus = SHADER_STATUS_FAILED;
                break;

            default:
                UNEXPECTED("Unexpected shader status");
        }
    });
    return OverallStatus;
}

size_t ComputeRenderTargetFormatsHash(Uint32 NumRenderTargets, const TEXTURE_FORMAT RTVFormats[], TEXTURE_FORMAT DSVFormat);


/// Returns the string containing the device features
///
/// \param Features   - device features.
/// \param NumColumns - the number of columns in the output.
/// \param Indent     - indentation of the first column.
/// \param Spacing    - spacing between columns.
/// \param Flags      - flags to control which features to include in the output.
/// 				    If (1<<State) & Flags is true, the feature will be included.
/// \return             string containing the device features.
template <typename FeaturesType>
std::string GetDeviceFeaturesString(const FeaturesType& Features,
                                    size_t              NumColumns,
                                    int                 Indent  = 4,
                                    int                 Spacing = 4,
                                    Uint32              Flags   = ~0u)
{
    VERIFY_EXPR(NumColumns > 0);

    std::vector<std::string> FeatureStrings;
    std::vector<size_t>      ColWidth(NumColumns);
    FeaturesType::Enumerate(Features,
                            [&](const char* Name, DEVICE_FEATURE_STATE State) {
                                if ((Flags & (1u << State)) != 0u)
                                {
                                    std::string FeatureStateStr{Name};
                                    FeatureStateStr += ": ";
                                    FeatureStateStr += GetDeviceFeatureStateString(State);

                                    size_t col    = FeatureStrings.size() % NumColumns;
                                    ColWidth[col] = std::max(ColWidth[col], FeatureStateStr.length());

                                    FeatureStrings.emplace_back(std::move(FeatureStateStr));
                                }
                                return true;
                            });

    std::stringstream ss;
    for (size_t i = 0; i < FeatureStrings.size();)
    {
        for (size_t col = 0; col < NumColumns && i < FeatureStrings.size(); ++col, ++i)
        {
            if (col == 0 && i > 0)
                ss << std::endl;
            ss << std::setw(col == 0 ? Indent : Spacing) << std::left << ' ';
            if (col + 1 < NumColumns && i + 1 < FeatureStrings.size())
                ss << std::setw(static_cast<int>(ColWidth[col])) << std::left;
            ss << FeatureStrings[i];
        }
    }

    return ss.str();
}

} // namespace Diligent
