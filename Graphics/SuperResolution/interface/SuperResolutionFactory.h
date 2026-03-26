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
/// Defines Diligent::ISuperResolutionFactory interface and related structures.

#include "../../../Primitives/interface/Object.h"
#include "../../../Primitives/interface/DebugOutput.h"
#include "../../../Primitives/interface/MemoryAllocator.h"
#include "../../../Primitives/interface/FlagEnum.h"
#include "../../../Graphics/GraphicsEngine/interface/RenderDevice.h"

#include "SuperResolution.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {79A904EC-EB17-4339-86BC-8A37632B0BD1}
static DILIGENT_CONSTEXPR INTERFACE_ID IID_SuperResolutionFactory =
    {0x79a904ec, 0xeb17, 0x4339, {0x86, 0xbc, 0x8a, 0x37, 0x63, 0x2b, 0xb, 0xd1}};

// clang-format off

/// Super resolution upscaler type.
DILIGENT_TYPED_ENUM(SUPER_RESOLUTION_TYPE, Uint8)
{
    /// Spatial upscaling only (single frame, no motion vectors required).
    SUPER_RESOLUTION_TYPE_SPATIAL = 0u,

    /// Temporal upscaling (uses motion vectors and history accumulation).
    SUPER_RESOLUTION_TYPE_TEMPORAL
};

/// Capability flags for spatial super resolution upscaling.
DILIGENT_TYPED_ENUM(SUPER_RESOLUTION_SPATIAL_CAP_FLAGS, Uint32)
{
    /// No special capabilities.
    SUPER_RESOLUTION_SPATIAL_CAP_FLAG_NONE = 0u,

    /// The upscaler is a native hardware-accelerated implementation (e.g. DLSS, DirectSR, MetalFX)
    /// as opposed to a custom software fallback.
    SUPER_RESOLUTION_SPATIAL_CAP_FLAG_NATIVE = 1u << 0,

    /// The upscaler supports the sharpness control parameter.
    /// When set, the Sharpness field in ExecuteSuperResolutionAttribs is used.
    SUPER_RESOLUTION_SPATIAL_CAP_FLAG_SHARPNESS = 1u << 1,

    SUPER_RESOLUTION_SPATIAL_CAP_FLAG_LAST = SUPER_RESOLUTION_SPATIAL_CAP_FLAG_SHARPNESS
};
DEFINE_FLAG_ENUM_OPERATORS(SUPER_RESOLUTION_SPATIAL_CAP_FLAGS)


/// Capability flags for temporal super resolution upscaling.
DILIGENT_TYPED_ENUM(SUPER_RESOLUTION_TEMPORAL_CAP_FLAGS, Uint32)
{
    /// No special capabilities.
    SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_NONE = 0u,

    /// The upscaler is a native hardware-accelerated implementation (e.g. MetalFX, DirectSR)
    /// as opposed to a custom software fallback.
    SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_NATIVE = 1u << 0,

    /// The upscaler supports exposure scale texture input.
    SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_EXPOSURE_SCALE_TEXTURE = 1u << 1,

    /// The upscaler supports ignore history mask texture input.
    /// When set, the backend processes the pIgnoreHistoryMaskTextureSRV field
    /// in ExecuteSuperResolutionAttribs.
    SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_IGNORE_HISTORY_MASK = 1u << 2,

    /// The upscaler supports reactive mask texture input.
    /// When set, the backend processes the pReactiveMaskTextureSRV field
    /// in ExecuteSuperResolutionAttribs.
    SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_REACTIVE_MASK = 1u << 3,

    /// The upscaler supports the sharpness control parameter.
    /// When set, the Sharpness field in ExecuteSuperResolutionAttribs is used.
    SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_SHARPNESS = 1u << 4,

    SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_LAST = SUPER_RESOLUTION_TEMPORAL_CAP_FLAG_SHARPNESS
};
DEFINE_FLAG_ENUM_OPERATORS(SUPER_RESOLUTION_TEMPORAL_CAP_FLAGS)


/// Information about a supported super resolution variant
struct SuperResolutionInfo
{
    /// Human-readable name of the upscaler variant (e.g. "DLSS", "FSR", "MetalFX Spatial", "MetalFX Temporal").
    Char Name[128] DEFAULT_INITIALIZER({});

    /// Unique identifier for this upscaler variant.
    /// Use this identifier when creating the upscaler with ISuperResolutionFactory::CreateSuperResolution().
    INTERFACE_ID VariantId DEFAULT_INITIALIZER({});

    /// Upscaler type. Determines which input textures and parameters are required.
    SUPER_RESOLUTION_TYPE Type DEFAULT_INITIALIZER(SUPER_RESOLUTION_TYPE_SPATIAL);

#if defined(DILIGENT_SHARP_GEN)
    Uint32 CapFlags DEFAULT_INITIALIZER(0);
#else
    union
    {
        /// Capability flags for SUPER_RESOLUTION_TYPE_SPATIAL.
        SUPER_RESOLUTION_SPATIAL_CAP_FLAGS SpatialCapFlags DEFAULT_INITIALIZER(SUPER_RESOLUTION_SPATIAL_CAP_FLAG_NONE);

        /// Capability flags for SUPER_RESOLUTION_TYPE_TEMPORAL.
        SUPER_RESOLUTION_TEMPORAL_CAP_FLAGS TemporalCapFlags;
    };
#endif

#if DILIGENT_CPP_INTERFACE
    constexpr Uint32 SpatialOrTemporalCapFlags() const
    {
#    if defined(DILIGENT_SHARP_GEN)
        return CapFlags;
#    else
        return SpatialCapFlags;
#    endif
    }

    /// Comparison operator tests if two structures are equivalent

    /// \param [in] RHS - reference to the structure to perform comparison with
    /// \return
    /// - True if all members of the two structures are equal.
    /// - False otherwise.
    bool operator==(const SuperResolutionInfo& RHS) const noexcept
    {
        return (VariantId == RHS.VariantId &&
                Type      == RHS.Type      &&
                SpatialOrTemporalCapFlags() == RHS.SpatialOrTemporalCapFlags() &&
                memcmp(Name, RHS.Name, sizeof(Name)) == 0);
    }
#endif
};
typedef struct SuperResolutionInfo SuperResolutionInfo;


/// Optimal source (input) settings returned by ISuperResolutionFactory::GetSourceSettings().
struct SuperResolutionSourceSettings
{
    /// Recommended input width for the given output resolution and optimization type.
    Uint32 OptimalInputWidth DEFAULT_INITIALIZER(0);

    /// Recommended input height for the given output resolution and optimization type.
    Uint32 OptimalInputHeight DEFAULT_INITIALIZER(0);
};
typedef struct SuperResolutionSourceSettings SuperResolutionSourceSettings;


/// Super resolution optimization type.
/// Defines the quality/performance trade-off for super resolution upscaling.
DILIGENT_TYPED_ENUM(SUPER_RESOLUTION_OPTIMIZATION_TYPE, Uint8)
{
    /// Maximum quality, lowest performance.
    SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_QUALITY = 0u,

    /// Favor quality over performance.
    SUPER_RESOLUTION_OPTIMIZATION_TYPE_HIGH_QUALITY,

    /// Balanced quality/performance trade-off.
    SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED,

    /// Favor performance over quality.
    SUPER_RESOLUTION_OPTIMIZATION_TYPE_HIGH_PERFORMANCE,

    /// Maximum performance, lowest quality.
    SUPER_RESOLUTION_OPTIMIZATION_TYPE_MAX_PERFORMANCE,

    SUPER_RESOLUTION_OPTIMIZATION_TYPE_COUNT
};


/// Attributes for querying the optimal source (input) settings for super resolution upscaling.
///
/// This structure is used by ISuperResolutionFactory::GetSourceSettings().
struct SuperResolutionSourceSettingsAttribs
{
    /// Unique identifier of the super resolution variant to create.
    ///
    /// Must match one of the VariantIds reported by ISuperResolutionFactory::EnumerateVariants().
    INTERFACE_ID VariantId DEFAULT_INITIALIZER({});

    /// Target (output) texture width. Must be greater than zero.
    Uint32 OutputWidth     DEFAULT_INITIALIZER(0);

    /// Target (output) texture height. Must be greater than zero.
    Uint32 OutputHeight    DEFAULT_INITIALIZER(0);

    /// Output texture format.
    ///
    /// Some backends (e.g. DirectSR) may return different optimal input resolutions
    /// depending on the output format. When set to TEX_FORMAT_UNKNOWN, the backend will use a reasonable default.
    TEXTURE_FORMAT OutputFormat DEFAULT_INITIALIZER(TEX_FORMAT_UNKNOWN);

    /// Flags controlling the super resolution behavior.
    ///
    /// These flags affect the optimal source resolution returned by the backend.
    /// Must match the flags that will be used when creating the upscaler.
    SUPER_RESOLUTION_FLAGS Flags DEFAULT_INITIALIZER(SUPER_RESOLUTION_FLAG_NONE);

    /// Optimization type controlling the quality/performance trade-off.
    SUPER_RESOLUTION_OPTIMIZATION_TYPE OptimizationType DEFAULT_INITIALIZER(SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED);
};
typedef struct SuperResolutionSourceSettingsAttribs SuperResolutionSourceSettingsAttribs;


#define DILIGENT_INTERFACE_NAME ISuperResolutionFactory
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define ISuperResolutionFactoryInclusiveMethods \
    IObjectInclusiveMethods;                    \
    ISuperResolutionFactoryMethods SuperResolutionFactory

// clang-format off

/// SuperResolution factory interface
///
/// The factory is created per render device using CreateSuperResolutionFactory().
/// It enumerates available super resolution backends, queries optimal settings,
/// and creates upscaler instances for the device it was created with.
DILIGENT_BEGIN_INTERFACE(ISuperResolutionFactory, IObject)
{
    /// Enumerates the supported super resolution variants.

    /// \param [in, out] NumVariants - Number of super resolution variants. If `Variants` is null, this
    ///                                parameter is used to return the number of supported variants.
    ///                                If `Variants` is not null, this parameter should contain the maximum number
    ///                                of elements to be written to `Variants` array. It is overwritten with the actual
    ///                                number of variants written to the array.
    /// \param [out]     Variants    - Array to receive the supported super resolution variants.
    ///                                Each variant is described by SuperResolutionInfo structure.
    VIRTUAL void METHOD(EnumerateVariants)(THIS_
                                           Uint32 REF           NumVariants,
                                           SuperResolutionInfo* Variants) PURE;


    /// Returns the optimal source (input) settings for super resolution upscaling.

    /// \param [in]  Attribs  - Attributes, see Diligent::SuperResolutionSourceSettingsAttribs for details.
    /// \param [out] Settings - On success, receives the optimal source settings, 
    ///                         see Diligent::SuperResolutionSourceSettings for details.
    ///
    /// \remarks    On backends that don't support hardware upscaling, Settings will be zero-initialized.
    ///             Use this method to determine the optimal render resolution before creating
    ///             the upscaler object.
    VIRTUAL void METHOD(GetSourceSettings)(THIS_
                                           const SuperResolutionSourceSettingsAttribs REF Attribs,
                                           SuperResolutionSourceSettings              REF Settings) CONST PURE;


    /// Creates a new upscaler object.

    /// \param [in]  Desc       - Super resolution upscaler description, see Diligent::SuperResolutionDesc for details.
    /// \param [out] ppUpscaler - Address of the memory location where a pointer to the
    ///                           super resolution upscaler interface will be written.
    ///                           The function calls AddRef(), so that the new object will have
    ///                           one reference.
    ///
    /// \remarks    On backends that don't support hardware upscaling, the method will
    ///             return nullptr.
    VIRTUAL void METHOD(CreateSuperResolution)(THIS_
                                               const SuperResolutionDesc REF Desc,
                                               ISuperResolution**            ppUpscaler) PURE;

    /// Sets a user-provided debug message callback.

    /// \param [in]     MessageCallback - Debug message callback function to use instead of the default one.
    VIRTUAL void METHOD(SetMessageCallback)(THIS_
                                            DebugMessageCallbackType MessageCallback) CONST PURE;

    /// Sets whether to break program execution on assertion failure.

    /// \param [in]     BreakOnError - Whether to break on assertion failure.
    VIRTUAL void METHOD(SetBreakOnError)(THIS_
                                         bool BreakOnError) CONST PURE;

    /// Sets the memory allocator to be used by the SuperResolution.
    
    /// \param [in] pAllocator - Pointer to the memory allocator.
    ///
    /// The allocator is a global setting that applies to the entire execution unit
    /// (executable or shared library that contains the SuperResolution implementation).
    ///
    /// The allocator should be set before any other factory method is called and
    /// should not be changed afterwards.
    /// The allocator object must remain valid until all objects created by the factory
    /// are destroyed.
    VIRTUAL void METHOD(SetMemoryAllocator)(THIS_
                                            IMemoryAllocator* pAllocator) CONST PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

#    define ISuperResolutionFactory_EnumerateVariants(This, ...)     CALL_IFACE_METHOD(SuperResolutionFactory, EnumerateVariants,     This, __VA_ARGS__)
#    define ISuperResolutionFactory_GetSourceSettings(This, ...)     CALL_IFACE_METHOD(SuperResolutionFactory, GetSourceSettings,     This, __VA_ARGS__)
#    define ISuperResolutionFactory_CreateSuperResolution(This, ...) CALL_IFACE_METHOD(SuperResolutionFactory, CreateSuperResolution, This, __VA_ARGS__)
#    define ISuperResolutionFactory_SetMessageCallback(This, ...)    CALL_IFACE_METHOD(SuperResolutionFactory, SetMessageCallback,    This, __VA_ARGS__)
#    define ISuperResolutionFactory_SetBreakOnError(This, ...)       CALL_IFACE_METHOD(SuperResolutionFactory, SetBreakOnError,       This, __VA_ARGS__)
#    define ISuperResolutionFactory_SetMemoryAllocator(This, ...)    CALL_IFACE_METHOD(SuperResolutionFactory, SetMemoryAllocator,    This, __VA_ARGS__)

#endif

/// Creates a super resolution factory for the specified render device.

/// \param [in]  pDevice    - Render device to create the factory for.
/// \param [out] ppFactory  - Address of the memory location where a pointer to the
///                           super resolution factory interface will be written.
void DILIGENT_GLOBAL_FUNCTION(CreateSuperResolutionFactory)(IRenderDevice*            pDevice,
                                                            ISuperResolutionFactory** ppFactory);

DILIGENT_END_NAMESPACE // namespace Diligent
