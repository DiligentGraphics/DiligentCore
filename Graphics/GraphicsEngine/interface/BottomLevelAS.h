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

/// \file
/// Definition of the Diligent::IBottomLevelAS interface and related data structures

#include "../../../Primitives/interface/Object.h"
#include "../../../Primitives/interface/FlagEnum.h"
#include "GraphicsTypes.h"
#include "Constants.h"
#include "Buffer.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {E56F5755-FE5E-496C-BFA7-BCD535360FF7}
static const INTERFACE_ID IID_BottomLevelAS =
    {0xe56f5755, 0xfe5e, 0x496c, {0xbf, 0xa7, 0xbc, 0xd5, 0x35, 0x36, 0xf, 0xf7}};

// clang-format off

/// Defines bottom level acceleration structure triangles description.

/// AZ TODO
struct BLASTriangleDesc
{
    /// Geometry name.
    /// The name is used to map BLASBuildTriangleData to this geometry.
    const char*               GeometryName          DEFAULT_INITIALIZER(nullptr);

    /// The maximum vertex count for this geometry.
    /// Current number of vertices is defined in BLASBuildTriangleData::VertexCount.
    Uint32                    MaxVertexCount        DEFAULT_INITIALIZER(0);

    /// The type of vertices in this geometry.
    /// Float, Int16 are supported.
    VALUE_TYPE                VertexValueType       DEFAULT_INITIALIZER(VT_UNDEFINED);

    /// The number of components in vertex.
    /// 2 and 3 are supported.
    Uint8                     VertexComponentCount  DEFAULT_INITIALIZER(0);

    /// The maximum index count for this geometry.
    /// The current number of indices is defined in BLASBuildTriangleData::IndexCount.
    /// It must be 0 if IndexType is VT_UNDEFINED and greater than zero otherwise.
    Uint32                    MaxIndexCount         DEFAULT_INITIALIZER(0);

    /// Index type of this geometry.
    /// Must be VT_UINT16, VT_UINT32 or VT_UNDEFINED.
    VALUE_TYPE                IndexType             DEFAULT_INITIALIZER(VT_UNDEFINED);

    /// AZ TODO
    Bool                      AllowsTransforms      DEFAULT_INITIALIZER(False);
    
#if DILIGENT_CPP_INTERFACE
    /// AZ TODO
    BLASTriangleDesc() noexcept {}
#endif
};
typedef struct BLASTriangleDesc BLASTriangleDesc;


/// Defines bottom level acceleration structure axis aligned bounding boxes description.

/// AZ TODO
struct BLASBoundingBoxDesc
{
    /// Geometry name.
    /// The name is used to map BLASBuildBoundingBoxData to this geometry.
    const char*               GeometryName  DEFAULT_INITIALIZER(nullptr);
    
    /// The maximum AABBs count.
    /// Current number of AABBs defined in BLASBuildBoundingBoxData::BoxCount. 
    Uint32                    MaxBoxCount   DEFAULT_INITIALIZER(0);
    
#if DILIGENT_CPP_INTERFACE
    /// AZ TODO
    BLASBoundingBoxDesc() noexcept {}
#endif
};
typedef struct BLASBoundingBoxDesc BLASBoundingBoxDesc;


/// AZ TODO

/// AZ TODO
DILIGENT_TYPED_ENUM(RAYTRACING_BUILD_AS_FLAGS, Uint8)
{
    /// AZ TODO
    RAYTRACING_BUILD_AS_NONE              = 0,
        
    /// AZ TODO
    RAYTRACING_BUILD_AS_ALLOW_UPDATE      = 0x01,

    /// Indicates that the specified acceleration structure can act as the source for a copy acceleration structure command
    /// with mode of COPY_AS_MODE_COMPACT to produce a compacted acceleration structure.
    RAYTRACING_BUILD_AS_ALLOW_COMPACTION  = 0x02,

    /// Indicates that the given acceleration structure build should prioritize trace performance over build time.
    RAYTRACING_BUILD_AS_PREFER_FAST_TRACE = 0x04,

    /// Indicates that the given acceleration structure build should prioritize build time over trace performance.
    RAYTRACING_BUILD_AS_PREFER_FAST_BUILD = 0x08,

    /// Indicates that this acceleration structure should minimize the size of the scratch memory and the final
    /// result build, potentially at the expense of build time or trace performance.
    RAYTRACING_BUILD_AS_LOW_MEMORY        = 0x10,

    RAYTRACING_BUILD_AS_FLAGS_LAST        = 0x10
};
DEFINE_FLAG_ENUM_OPERATORS(RAYTRACING_BUILD_AS_FLAGS)


/// AZ TODO

// Here we allocate space for geometry data.
// Geometry can be dynamically updated.
struct BottomLevelASDesc DILIGENT_DERIVE(DeviceObjectAttribs)

    /// Array of triangle geometry descriptions.
    const BLASTriangleDesc*    pTriangles       DEFAULT_INITIALIZER(nullptr);

    /// The number of triangle geometries in pTriangles array.
    Uint32                     TriangleCount    DEFAULT_INITIALIZER(0);

    /// Array of AABB geometry descriptions.
    const BLASBoundingBoxDesc* pBoxes           DEFAULT_INITIALIZER(nullptr);

    /// The number of AABB geometries in pBoxes array.
    Uint32                     BoxCount         DEFAULT_INITIALIZER(0);
    
    /// Ray tracing build flags, see Diligent::RAYTRACING_BUILD_AS_FLAGS.
    RAYTRACING_BUILD_AS_FLAGS  Flags            DEFAULT_INITIALIZER(RAYTRACING_BUILD_AS_NONE);
    
    /// Defines which command queues this BLAS can be used with
    Uint64                     CommandQueueMask DEFAULT_INITIALIZER(1);

#if DILIGENT_CPP_INTERFACE
    /// AZ TODO
    BottomLevelASDesc() noexcept {}
#endif
};
typedef struct BottomLevelASDesc BottomLevelASDesc;

struct ScratchBufferSizes
{
    Uint32 Build  DEFAULT_INITIALIZER(0);
    Uint32 Update DEFAULT_INITIALIZER(0);
    
#if DILIGENT_CPP_INTERFACE
    /// AZ TODO
    ScratchBufferSizes() noexcept {}
#endif
};
typedef struct ScratchBufferSizes ScratchBufferSizes;

#define DILIGENT_INTERFACE_NAME IBottomLevelAS
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IBottomLevelASInclusiveMethods     \
    IDeviceObjectInclusiveMethods;         \
    IBottomLevelASMethods BottomLevelAS

/// AZ TODO
DILIGENT_BEGIN_INTERFACE(IBottomLevelAS, IDeviceObject)
{
#if DILIGENT_CPP_INTERFACE
    /// Returns the bottom level AS description used to create the object
    virtual const BottomLevelASDesc& DILIGENT_CALL_TYPE GetDesc() const override = 0;
#endif

    /// AZ TODO
    VIRTUAL Uint32 METHOD(GetGeometryIndex)(THIS_
                                            const char* Name) CONST PURE;

    /// AZ TODO
    VIRTUAL ScratchBufferSizes METHOD(GetScratchBufferSizes)(THIS) CONST PURE;

    /// Returns native acceleration structure handle specific to the underlying graphics API

    /// \return pointer to ID3D12Resource interface, for D3D12 implementation\n
    ///         VkAccelerationStructureKHR handle, for Vulkan implementation
    VIRTUAL void* METHOD(GetNativeHandle)(THIS) PURE;

    /// Sets the acceleration structure usage state.

    /// \note This method does not perform state transition, but
    ///       resets the internal acceleration structure state to the given value.
    ///       This method should be used after the application finished
    ///       manually managing the acceleration structure state and wants to hand over
    ///       state management back to the engine.
    VIRTUAL void METHOD(SetState)(THIS_
                                  RESOURCE_STATE State) PURE;

    /// Returns the internal acceleration structure state
    VIRTUAL RESOURCE_STATE METHOD(GetState)(THIS) CONST PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

// clang-format off

#    define IBottomLevelAS_GetGeometryIndex(This, ...) CALL_IFACE_METHOD(BottomLevelAS, GetGeometryIndex,      This, __VA_ARGS__)
#    define IBottomLevelAS_GetScratchBufferSizes(This) CALL_IFACE_METHOD(BottomLevelAS, GetScratchBufferSizes, This)
#    define IBottomLevelAS_GetNativeHandle(This)       CALL_IFACE_METHOD(BottomLevelAS, GetNativeHandle,       This)
#    define IBottomLevelAS_SetState(This, ...)         CALL_IFACE_METHOD(BottomLevelAS, SetState,              This, __VA_ARGS__)
#    define IBottomLevelAS_GetState(This)              CALL_IFACE_METHOD(BottomLevelAS, GetState,              This)

// clang-format on

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
