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
/// Definition of the ray tracing interfaces: Diligent::IBottomLevelAS, Diligent::ITopLevelAS, Diligent::IShaderBindingTable and related data structures

#include "../../../Primitives/interface/Object.h"
#include "../../../Primitives/interface/FlagEnum.h"
#include "GraphicsTypes.h"
#include "Constants.h"
#include "Buffer.h"
#include "PipelineState.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {E56F5755-FE5E-496C-BFA7-BCD535360FF7}
static const INTERFACE_ID IID_BottomLevelAS =
    {0xe56f5755, 0xfe5e, 0x496c, {0xbf, 0xa7, 0xbc, 0xd5, 0x35, 0x36, 0xf, 0xf7}};

// {16561861-294B-4804-96FA-1717333F769A}
static const INTERFACE_ID IID_TopLevelAS =
    {0x16561861, 0x294b, 0x4804, {0x96, 0xfa, 0x17, 0x17, 0x33, 0x3f, 0x76, 0x9a}};

// {1EE12101-7010-4825-AA8E-AC6BB9858BD6}
static const INTERFACE_ID IID_ShaderBindingTable =
    {0x1ee12101, 0x7010, 0x4825, {0xaa, 0x8e, 0xac, 0x6b, 0xb9, 0x85, 0x8b, 0xd6}};

// clang-format off

/// Defines bottom level acceleration structure triangles description.

/// AZ TODO
struct BLASTriangleDesc
{
    /// The geometry name.
    /// Name used only to map BLASBuildTriangleData to this geometry.
    const char*               GeometryName          DEFAULT_INITIALIZER(nullptr);

    /// The maximum vertex count for this geometry.
    /// Current number of vertices defined in BLASBuildTriangleData::VertexCount.
    Uint32                    MaxVertexCount        DEFAULT_INITIALIZER(0);

    /// The vertices value type of this geometry.
    /// Float, Int16 are supported.
    VALUE_TYPE                VertexValueType       DEFAULT_INITIALIZER(VT_UNDEFINED);

    /// The number of components in vertex.
    /// 2 and 3 are supported.
    Uint8                     VertexComponentCount  DEFAULT_INITIALIZER(0);

    /// The maximum index count for this geometry.
    /// Current number of indices defined in BLASBuildTriangleData::IndexCount.
    /// Must be 0 if IndexType is VT_UNDEFINED and greater than zero otherwise.
    Uint32                    MaxIndexCount         DEFAULT_INITIALIZER(0);

    /// The indices type of this geometry.
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
    /// The geometry name.
    /// Name used only to map BLASBuildBoundingBoxData to this geometry.
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

    /// Indicates that this acceleration structure should minimize the size of the scratch memory and the final result build, potentially at the expense of build time or trace performance.
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

    /// Number of triangle geometries.
    Uint32                     TriangleCount    DEFAULT_INITIALIZER(0);

    /// Array of AABB geometry descriptions.
    const BLASBoundingBoxDesc* pBoxes           DEFAULT_INITIALIZER(nullptr);

    /// Number of AABB geometries;
    Uint32                     BoxCount         DEFAULT_INITIALIZER(0);
    
    /// AZ TODO
    RAYTRACING_BUILD_AS_FLAGS  Flags            DEFAULT_INITIALIZER(RAYTRACING_BUILD_AS_NONE);
    
    /// Defines which command queues this BLAS can be used with
    Uint64                     CommandQueueMask DEFAULT_INITIALIZER(1);

#if DILIGENT_CPP_INTERFACE
    /// AZ TODO
    BottomLevelASDesc() noexcept {}
#endif
};
typedef BottomLevelASDesc BottomLevelASDesc;

struct ScratchBufferSizes
{
    Uint32 Build  DEFAULT_INITIALIZER(0);
    Uint32 Update DEFAULT_INITIALIZER(0);
    
#if DILIGENT_CPP_INTERFACE
    /// AZ TODO
    ScratchBufferSizes() noexcept {}
#endif
};
typedef ScratchBufferSizes ScratchBufferSizes;

#define DILIGENT_INTERFACE_NAME IBottomLevelAS
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IBottomLevelASInclusiveMethods     \
    IDeviceObjectInclusiveMethods;         \
    IBottomLevelASMethods IBottomLevelAS

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
};
DILIGENT_END_INTERFACE


/// AZ TODO
DILIGENT_TYPED_ENUM(SHADER_BINDING_MODE, Uint8)
{
    /// Each geometry in each instance can have unique shader.
    SHADER_BINDING_MODE_PER_GEOMETRY = 0,

    /// Each instance can have unique shader. In this mode SBT buffer will use less memory.
    SHADER_BINDING_MODE_PER_INSTANCE,

    // User must specify TLASBuildInstanceData::InstanceContributionToHitGroupIndex and use only IShaderBindingTable::BindAll()
    SHADER_BINDING_USER_DEFINED,
};

/// AZ TODO
struct TopLevelASDesc DILIGENT_DERIVE(DeviceObjectAttribs)

    // Here we allocate space for instances.
    // Instances can be dynamicaly updated.
    Uint32                    MaxInstanceCount DEFAULT_INITIALIZER(0);

    /// AZ TODO
    RAYTRACING_BUILD_AS_FLAGS Flags            DEFAULT_INITIALIZER(RAYTRACING_BUILD_AS_NONE);

    // binding mode used for instanceOffset calculation.
    SHADER_BINDING_MODE       BindingMode      DEFAULT_INITIALIZER(SHADER_BINDING_MODE_PER_GEOMETRY);
    
    /// Defines which command queues this BLAS can be used with
    Uint64                    CommandQueueMask DEFAULT_INITIALIZER(1);
    
#if DILIGENT_CPP_INTERFACE
    /// AZ TODO
    TopLevelASDesc() noexcept {}
#endif
};
typedef struct TopLevelASDesc TopLevelASDesc;


/// AZ TODO
struct TLASInstanceDesc
{
    /// AZ TODO
    Uint32          InstanceContributionToHitGroupIndex DEFAULT_INITIALIZER(0);

    /// AZ TODO
    IBottomLevelAS* pBLAS                               DEFAULT_INITIALIZER(nullptr);
    
#if DILIGENT_CPP_INTERFACE
    /// AZ TODO
    TLASInstanceDesc() noexcept {}
#endif
};
typedef struct TLASInstanceDesc TLASInstanceDesc;


#undef DILIGENT_INTERFACE_NAME
#define DILIGENT_INTERFACE_NAME ITopLevelAS
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define ITopLevelASInclusiveMethods        \
    IDeviceObjectInclusiveMethods;         \
    ITopLevelASMethods TopLevelAS

/// AZ TODO
DILIGENT_BEGIN_INTERFACE(ITopLevelAS, IDeviceObject)
{
#if DILIGENT_CPP_INTERFACE
    /// Returns the top level AS description used to create the object
    virtual const TopLevelASDesc& DILIGENT_CALL_TYPE GetDesc() const override = 0;
#endif
    
    /// AZ TODO
    VIRTUAL TLASInstanceDesc METHOD(GetInstanceDesc)(THIS_
                                                     const char* Name) CONST PURE;
    
    /// AZ TODO
    VIRTUAL ScratchBufferSizes METHOD(GetScratchBufferSizes)(THIS) CONST PURE;
};
DILIGENT_END_INTERFACE


/// AZ TODO
struct ShaderBindingTableDesc DILIGENT_DERIVE(DeviceObjectAttribs)
    
    /// AZ TODO
    IPipelineState* pPSO                  DEFAULT_INITIALIZER(nullptr);

    // size of additional data that passed to a shader, maximum size is 4064
    Uint32          ShaderRecordSize      DEFAULT_INITIALIZER(0);
    
    /// AZ TODO
    Uint32          HitShadersPerInstance DEFAULT_INITIALIZER(1);
    
#if DILIGENT_CPP_INTERFACE
    /// AZ TODO
    ShaderBindingTableDesc() noexcept {}
#endif
};
typedef struct ShaderBindingCreateInfo ShaderBindingCreateInfo;

/// AZ TODO
struct BindAllAttribs
{
    /// AZ TODO
    Uint32        RayGenShader        DEFAULT_INITIALIZER(~0u);
    const void*   RayGenSRData        DEFAULT_INITIALIZER(nullptr);  // optional, can be null
    Uint32        RayGenSRDataSize    DEFAULT_INITIALIZER(0);
    
    /// AZ TODO
    const Uint32* MissShaders         DEFAULT_INITIALIZER(nullptr);
    Uint32        MissShaderCount     DEFAULT_INITIALIZER(0);
    const void*   MissSRData          DEFAULT_INITIALIZER(nullptr);  // optional, can be null
    Uint32        MissSRDataSize      DEFAULT_INITIALIZER(0);        // stride will be calculated as (MissSRDataSize / MissShaderCount)
    
    /// AZ TODO
    const Uint32* CallableShaders     DEFAULT_INITIALIZER(nullptr);
    Uint32        CallableShaderCount DEFAULT_INITIALIZER(0);
    const void*   CallableSRData      DEFAULT_INITIALIZER(nullptr);  // optional, can be null
    Uint32        CallableSRDataSize  DEFAULT_INITIALIZER(0);        // stride will be calculated as (CallableSRDataSize / CallableShaderCount)
    
    /// AZ TODO
    const Uint32* HitGroups           DEFAULT_INITIALIZER(nullptr);  // optional, can be null
    Uint32        HitGroupCount       DEFAULT_INITIALIZER(0);
    const void*   HitSRData           DEFAULT_INITIALIZER(nullptr);  // optional, can be null
    Uint32        HitSRDataSize       DEFAULT_INITIALIZER(0);        // stride will be calculated as (HitSRDataSize / HitGroupCount)
    
#if DILIGENT_CPP_INTERFACE
    /// AZ TODO
    BindAllAttribs() noexcept {}
#endif
};
typedef struct BindAllAttribs BindAllAttribs;

#undef DILIGENT_INTERFACE_NAME
#define DILIGENT_INTERFACE_NAME IShaderBindingTable
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IShaderBindingTableInclusiveMethods      \
    IDeviceObjectInclusiveMethods;               \
    IShaderBindingTableMethods ShaderBindingTable

/// AZ TODO
DILIGENT_BEGIN_INTERFACE(IShaderBindingTable, IDeviceObject)
{
#if DILIGENT_CPP_INTERFACE
    /// Returns the shader binding table description used to create the object
    virtual const ShaderBindingTableDesc& DILIGENT_CALL_TYPE GetDesc() const override = 0;
#endif
    
    /// AZ TODO
    VIRTUAL void METHOD(Verify)(THIS) CONST PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(Reset)(THIS_
                               const ShaderBindingTableDesc REF Desc) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(ResetHitGroups)(THIS_
                                        Uint32 HitShadersPerInstance) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindRayGenShader)(THIS_
                                          const char* ShaderGroupName) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindRayGenShader)(THIS_
                                          const char* ShaderGroupName,
                                          const void* Data,
                                          Uint32      DataSize) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindMissShader)(THIS_
                                        const char* ShaderGroupName,
                                        Uint32      MissIndex) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindMissShader)(THIS_
                                        const char* ShaderGroupName,
                                        Uint32      MissIndex,
                                        const void* Data,
                                        Uint32      DataSize) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindHitGroup)(THIS_
                                      ITopLevelAS* pTLAS,
                                      const char*  InstanceName,
                                      const char*  GeometryName,
                                      Uint32       RayOffsetInHitGroupIndex,
                                      const char*  ShaderGroupName) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindHitGroup)(THIS_
                                      ITopLevelAS* pTLAS,
                                      const char*  InstanceName,
                                      const char*  GeometryName,
                                      Uint32       RayOffsetInHitGroupIndex,
                                      const char*  ShaderGroupName,
                                      const void*  Data,
                                      Uint32       DataSize) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindHitGroup)(THIS_
                                      ITopLevelAS* pTLAS,
                                      const char*  InstanceName,
                                      Uint32       RayOffsetInHitGroupIndex,
                                      const char*  ShaderGroupName) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindHitGroup)(THIS_
                                      ITopLevelAS* pTLAS,
                                      const char*  InstanceName,
                                      Uint32       RayOffsetInHitGroupIndex,
                                      const char*  ShaderGroupName,
                                      const void*  Data,
                                      Uint32       DataSize) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindCallableShader)(THIS_
                                            Uint32      Index,
                                            const char* ShaderName) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindCallableShader)(THIS_
                                            Uint32      Index,
                                            const char* ShaderName,
                                            const void* Data,
                                            Uint32      DataSize) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindAll)(THIS_
                                 const BindAllAttribs REF Attribs) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

// clang-format off

#    define IBottomLevelAS_GetGeometryIndex(This, ...)        CALL_IFACE_METHOD(BottomLevelAS, GetGeometryIndex,          This, __VA_ARGS__)

#    define ITopLevelAS_GetInstanceDesc(This, ...)           CALL_IFACE_METHOD(TopLevelAS, GetInstanceDesc,          This, __VA_ARGS__)

#    define IShaderBindingTable_(This, ...)          CALL_IFACE_METHOD(ShaderBindingTable, SetPipelineState,          This, __VA_ARGS__)

// clang-format on

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
