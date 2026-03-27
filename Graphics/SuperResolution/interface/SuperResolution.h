/*
 *  Copyright 2026 Diligent Graphics LLC
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
/// Defines Diligent::ISuperResolution interface and related data structures

#include "../../../Primitives/interface/Object.h"
#include "../../GraphicsEngine/interface/GraphicsTypes.h"
#include "../../GraphicsEngine/interface/TextureView.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
static DILIGENT_CONSTEXPR INTERFACE_ID IID_SuperResolution =
    {0xa1b2c3d4, 0xe5f6, 0x7890, {0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90}};

// clang-format off

/// Super resolution flags.
DILIGENT_TYPED_ENUM(SUPER_RESOLUTION_FLAGS, Uint32)
{
    SUPER_RESOLUTION_FLAG_NONE = 0u,

    /// When set, the upscaler automatically calculates exposure for each frame.
    /// The exposure texture in ExecuteSuperResolutionAttribs is ignored.
    SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE = 1u << 0,

    /// When set, enables the sharpening pass in the upscaler.
    /// The Sharpness field in ExecuteSuperResolutionAttribs controls the amount.
    SUPER_RESOLUTION_FLAG_ENABLE_SHARPENING = 1u << 1,

    SUPER_RESOLUTION_FLAG_LAST = SUPER_RESOLUTION_FLAG_ENABLE_SHARPENING
};
DEFINE_FLAG_ENUM_OPERATORS(SUPER_RESOLUTION_FLAGS)


/// This structure describes the super resolution upscaler object and is part of the creation
/// parameters given to ISuperResolutionFactory::CreateSuperResolution().
struct SuperResolutionDesc
{
    /// Object name.
    const Char* Name DEFAULT_INITIALIZER(nullptr);

    /// Unique identifier of the super resolution variant to create.
    ///
    /// Must match one of the VariantIds reported by ISuperResolutionFactory::EnumerateVariants().
    INTERFACE_ID VariantId DEFAULT_INITIALIZER({});

    /// Input (render) width. Must be greater than zero and not exceed OutputWidth.
    ///
    /// Use ISuperResolutionFactory::GetSourceSettings() to obtain the
    /// optimal input resolution for a given output resolution and optimization type.
    Uint32 InputWidth             DEFAULT_INITIALIZER(0);

    /// Input (render) height. Must be greater than zero and not exceed OutputHeight
    ///
    /// Use ISuperResolutionFactory::GetSourceSettings() to obtain the
    /// optimal input resolution for a given output resolution and optimization type.
    Uint32 InputHeight            DEFAULT_INITIALIZER(0);

    /// Target (output) texture width.
    Uint32 OutputWidth            DEFAULT_INITIALIZER(0);

    /// Target (output) texture height.
    Uint32 OutputHeight           DEFAULT_INITIALIZER(0);

    /// Output texture format.
    TEXTURE_FORMAT OutputFormat   DEFAULT_INITIALIZER(TEX_FORMAT_RGBA16_FLOAT);

    /// Color input texture format.
    TEXTURE_FORMAT ColorFormat    DEFAULT_INITIALIZER(TEX_FORMAT_RGBA16_FLOAT);

    /// Depth input texture format.
    /// Required for temporal upscaling.
    TEXTURE_FORMAT DepthFormat    DEFAULT_INITIALIZER(TEX_FORMAT_UNKNOWN);

    /// Motion vectors texture format.
    ///
    /// Required for temporal upscaling.
    TEXTURE_FORMAT MotionFormat   DEFAULT_INITIALIZER(TEX_FORMAT_UNKNOWN);

    /// Reactive mask texture format.
    ///
    /// Optional. Used for temporal upscaling to guide the denoiser for areas with inaccurate motion information (e.g., alpha-blended objects).
    TEXTURE_FORMAT ReactiveMaskFormat DEFAULT_INITIALIZER(TEX_FORMAT_UNKNOWN);

    /// Exposure scale texture format.
    ///
    /// Optional. When auto-exposure is disabled, specifies the format of the 1x1 exposure
    /// texture provided in ExecuteSuperResolutionAttribs::pExposureTextureSRV.
    TEXTURE_FORMAT ExposureFormat DEFAULT_INITIALIZER(TEX_FORMAT_UNKNOWN);

    /// Engine creation flags controlling the super resolution upscaler behavior.
    /// See SUPER_RESOLUTION_FLAGS.
    SUPER_RESOLUTION_FLAGS Flags DEFAULT_INITIALIZER(SUPER_RESOLUTION_FLAG_NONE);
};
typedef struct SuperResolutionDesc SuperResolutionDesc;


/// Super resolution execute attributes

/// This structure is used by ISuperResolution::Execute().
struct ExecuteSuperResolutionAttribs
{
    /// Device context to execute the super resolution on.
    IDeviceContext* pContext            DEFAULT_INITIALIZER(nullptr);

    /// Low-resolution color texture (shader resource view).
    ///
    /// This is the input image to be upscaled.
    ITextureView* pColorTextureSRV      DEFAULT_INITIALIZER(nullptr);

    /// Depth buffer of the low-resolution render (shader resource view).
    ///
    /// Required for temporal upscaling (SUPER_RESOLUTION_TYPE_TEMPORAL).
    ITextureView* pDepthTextureSRV      DEFAULT_INITIALIZER(nullptr);

    /// Motion vectors texture (shader resource view).
    ///
    /// Required for temporal upscaling (SUPER_RESOLUTION_TYPE_TEMPORAL).
    /// Expected to contain per-pixel 2D motion vectors in pixel space.
    ITextureView* pMotionVectorsSRV     DEFAULT_INITIALIZER(nullptr);

    /// Output (upscaled) texture (unordered access view or render target view).
    ///
    /// Must match SuperResolutionDesc::OutputWidth x SuperResolutionDesc::OutputWidthOutputHeight.
    ITextureView* pOutputTextureView     DEFAULT_INITIALIZER(nullptr);

    /// Exposure texture (shader resource view).
    ///
    /// Optional. A 1x1 R16_FLOAT texture containing the exposure value.
    /// The upscaler reads the R channel and uses it to multiply the input color.
    /// Ignored when SuperResolutionDesc::Flags includes SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE.
    ITextureView* pExposureTextureSRV   DEFAULT_INITIALIZER(nullptr);

    /// Reactive mask texture (shader resource view).
    ///
    /// Optional. Per-pixel mask in [0, 1] range guiding temporal history usage:
    ///   0.0 - normal temporal behavior
    ///   1.0 - ignore temporal history (use current frame only)
    /// Useful for alpha-blended objects or areas with inaccurate motion vectors.
    /// Only used when SuperResolutionDesc::ReactiveMaskFormat != TEX_FORMAT_UNKNOWN.
    ITextureView* pReactiveMaskTextureSRV DEFAULT_INITIALIZER(nullptr);

    /// Ignore history mask texture (shader resource view).
    ///
    /// Optional. Binary per-pixel mask where non-zero values indicate regions
    /// where temporal history should be completely discarded.
    /// Unlike the reactive mask which provides proportional control,
    /// this is a binary decision (discard or keep).
    /// Format must be TEX_FORMAT_R8_UINT.
    ITextureView* pIgnoreHistoryMaskTextureSRV DEFAULT_INITIALIZER(nullptr);

    /// Resource state transition mode.
    ///
    /// Specifies whether the upscaler should perform resource state transitions for the input and output textures.
    RESOURCE_STATE_TRANSITION_MODE StateTransitionMode DEFAULT_INITIALIZER(RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    /// Jitter offset X applied to the projection matrix (in pixels).
    ///
    /// Used for temporal upscaling.
    float JitterX                       DEFAULT_INITIALIZER(0.0f);

    /// Jitter offset Y applied to the projection matrix (in pixels).
    ///
    /// Used for temporal upscaling. 
    float JitterY                       DEFAULT_INITIALIZER(0.0f);

    /// Pre-exposure value.
    ///
    /// If the input color texture is pre-multiplied by a fixed value,
    /// set this to that value so the upscaler can divide by it.
    /// Default is 1.0 (no pre-exposure adjustment).
    float PreExposure                   DEFAULT_INITIALIZER(1.0f);

    /// Motion vector scale X.
    ///
    /// Multiplier applied to the X component of motion vectors.
    /// Use this to convert motion vectors from their native space to pixel space.
    /// Default is 1.0 (motion vectors are already in pixel space).
    float MotionVectorScaleX            DEFAULT_INITIALIZER(1.0f);

    /// Motion vector scale Y.
    ///
    /// Multiplier applied to the Y component of motion vectors.
    /// Use this to convert motion vectors from their native space to pixel space.
    /// Default is 1.0 (motion vectors are already in pixel space).
    float MotionVectorScaleY            DEFAULT_INITIALIZER(1.0f);

    /// Exposure scale value (scalar).
    ///
    /// A multiplier applied to the exposure. This is separate from PreExposure
    /// and the exposure texture. Used by DirectSR-style upscalers.
    /// Default is 1.0 (no additional scaling).
    float ExposureScale                 DEFAULT_INITIALIZER(1.0f);

    /// Sharpness control.
    ///
    /// Controls the amount of sharpening applied during upscaling.
    /// Range is typically [0.0, 1.0], where 0.0 means no sharpening
    /// and 1.0 means maximum sharpening.
    /// Only used when the upscaler supports sharpness (see SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_SHARPNESS).
    /// Default is 0.0 (no sharpening).
    float Sharpness                     DEFAULT_INITIALIZER(0.0f);

    /// Camera near plane distance.
    ///
    /// Used by some upscalers for depth reconstruction.
    /// Default is 0.0 (not provided).
    float CameraNear                    DEFAULT_INITIALIZER(0.0f);

    /// Camera far plane distance.
    ///
    /// Used by some upscalers for depth reconstruction.
    /// Default is 0.0 (not provided).
    float CameraFar                     DEFAULT_INITIALIZER(0.0f);

    /// Camera vertical field of view angle, in radians.
    ///
    /// Used by some upscalers for depth reconstruction.
    /// Default is 0.0 (not provided).
    float CameraFovAngleVert            DEFAULT_INITIALIZER(0.0f);

    /// Time elapsed since the previous frame, in seconds.
    ///
    /// Used by some upscalers to adjust temporal accumulation behavior.
    /// Default is 0.0.
    float TimeDeltaInSeconds            DEFAULT_INITIALIZER(0.0f);

    /// Set to true to reset temporal history (e.g., on camera cut).
    ///
    /// Default is False.
    Bool  ResetHistory                  DEFAULT_INITIALIZER(False);
};
typedef struct ExecuteSuperResolutionAttribs ExecuteSuperResolutionAttribs;


#define DILIGENT_INTERFACE_NAME ISuperResolution
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define ISuperResolutionInclusiveMethods   \
    IObjectInclusiveMethods;               \
    ISuperResolutionMethods SuperResolution

/// Super resolution upscaler interface.
///
/// The super resolution object encapsulates a hardware-accelerated or software-based super resolution
/// effect (e.g., MetalFX on Metal, DirectSR on D3D12).
/// It is created via ISuperResolutionFactory::CreateSuperResolution().
DILIGENT_BEGIN_INTERFACE(ISuperResolution, IObject)
{
    /// Returns the super resolution description used to create the object.
    VIRTUAL const SuperResolutionDesc REF METHOD(GetDesc)(THIS) CONST PURE;

    /// Returns the optimal jitter offset for the given frame index.
    
    /// \param [in]  Index   - Frame index. The sequence wraps automatically.
    /// \param [out] JitterX - Jitter offset X in pixel space, typically in [-0.5, 0.5] range.
    /// \param [out] JitterY - Jitter offset Y in pixel space, typically in [-0.5, 0.5] range.
    ///
    /// For temporal upscaling, the upscaler provides a recommended jitter pattern
    /// (e.g. Halton sequence) that should be applied to the projection matrix each frame.
    /// For spatial upscaling, both values are set to zero.
    VIRTUAL void METHOD(GetJitterOffset)(THIS_
                                         Uint32 Index,
                                         float REF JitterX,
                                         float REF JitterY) CONST PURE;


    /// Executes the super resolution upscaler.

    /// \param [in] Attribs - Upscale operation attributes, see Diligent::ExecuteSuperResolutionAttribs.
    ///
    /// The command must be called outside of a render pass.
    /// All input textures must be in the appropriate states or
    /// StateTransitionMode should be set to RESOURCE_STATE_TRANSITION_MODE_TRANSITION.
    ///
    /// \remarks Supported contexts: graphics.
    VIRTUAL void METHOD(Execute)(THIS_
                                 const ExecuteSuperResolutionAttribs REF Attribs) PURE;
};
DILIGENT_END_INTERFACE

// clang-format on

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

// clang-format off
#    define ISuperResolution_GetDesc(This)              CALL_IFACE_METHOD(SuperResolution, GetDesc, This)
#    define ISuperResolution_GetJitterOffset(This, ...) CALL_IFACE_METHOD(SuperResolution, GetJitterOffset, This, __VA_ARGS__)
#    define ISuperResolution_Execute(This, ...)         CALL_IFACE_METHOD(SuperResolution, Execute, This, __VA_ARGS__)

// clang-format on

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
