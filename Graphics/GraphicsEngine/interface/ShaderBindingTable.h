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
/// Definition of the Diligent::IShaderBindingTable interface and related data structures

#include "../../../Primitives/interface/Object.h"
#include "../../../Primitives/interface/FlagEnum.h"
#include "GraphicsTypes.h"
#include "Constants.h"
#include "Buffer.h"
#include "PipelineState.h"
#include "TopLevelAS.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {1EE12101-7010-4825-AA8E-AC6BB9858BD6}
static const INTERFACE_ID IID_ShaderBindingTable =
    {0x1ee12101, 0x7010, 0x4825, {0xaa, 0x8e, 0xac, 0x6b, 0xb9, 0x85, 0x8b, 0xd6}};

// clang-format off

/// AZ TODO
struct ShaderBindingTableDesc DILIGENT_DERIVE(DeviceObjectAttribs)
    
    /// AZ TODO
    IPipelineState* pPSO                  DEFAULT_INITIALIZER(nullptr);

    // Size of the additional data passed to the shader, maximum size is 4064 bytes.
    Uint32          ShaderRecordSize      DEFAULT_INITIALIZER(0);
    
    /// AZ TODO
    Uint32          HitShadersPerInstance DEFAULT_INITIALIZER(1);
    
#if DILIGENT_CPP_INTERFACE
    /// AZ TODO
    ShaderBindingTableDesc() noexcept {}
#endif
};
typedef struct ShaderBindingTableDesc ShaderBindingTableDesc;

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
                                          const char* ShaderGroupName,
                                          const void* Data             DEFAULT_INITIALIZER(nullptr),
                                          Uint32      DataSize         DEFAULT_INITIALIZER(0)) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindMissShader)(THIS_
                                        const char* ShaderGroupName,
                                        Uint32      MissIndex,
                                        const void* Data             DEFAULT_INITIALIZER(nullptr),
                                        Uint32      DataSize         DEFAULT_INITIALIZER(0)) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindHitGroup)(THIS_
                                      ITopLevelAS* pTLAS,
                                      const char*  InstanceName,
                                      const char*  GeometryName,
                                      Uint32       RayOffsetInHitGroupIndex,
                                      const char*  ShaderGroupName,
                                      const void*  Data             DEFAULT_INITIALIZER(nullptr),
                                      Uint32       DataSize         DEFAULT_INITIALIZER(0)) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindHitGroups)(THIS_
                                       ITopLevelAS* pTLAS,
                                       const char*  InstanceName,
                                       Uint32       RayOffsetInHitGroupIndex,
                                       const char*  ShaderGroupName,
                                       const void*  Data             DEFAULT_INITIALIZER(nullptr),
                                       Uint32       DataSize         DEFAULT_INITIALIZER(0)) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindCallableShader)(THIS_
                                            const char* ShaderGroupName,
                                            Uint32      CallableIndex,
                                            const void* Data            DEFAULT_INITIALIZER(nullptr),
                                            Uint32      DataSize        DEFAULT_INITIALIZER(0)) PURE;
    
    /// AZ TODO
    VIRTUAL void METHOD(BindAll)(THIS_
                                 const BindAllAttribs REF Attribs) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

// clang-format off

#    define IShaderBindingTable_Verify(This)                   CALL_IFACE_METHOD(ShaderBindingTable, Verify,             This)
#    define IShaderBindingTable_Reset(This, ...)               CALL_IFACE_METHOD(ShaderBindingTable, Reset,              This, __VA_ARGS__)
#    define IShaderBindingTable_ResetHitGroups(This, ...)      CALL_IFACE_METHOD(ShaderBindingTable, ResetHitGroups,     This, __VA_ARGS__)
#    define IShaderBindingTable_BindRayGenShader(This, ...)    CALL_IFACE_METHOD(ShaderBindingTable, BindRayGenShader,   This, __VA_ARGS__)
#    define IShaderBindingTable_BindMissShader(This, ...)      CALL_IFACE_METHOD(ShaderBindingTable, BindMissShader,     This, __VA_ARGS__)
#    define IShaderBindingTable_BindHitGroup(This, ...)        CALL_IFACE_METHOD(ShaderBindingTable, BindHitGroup,       This, __VA_ARGS__)
#    define IShaderBindingTable_BindHitGroups(This, ...)       CALL_IFACE_METHOD(ShaderBindingTable, BindHitGroups,      This, __VA_ARGS__)
#    define IShaderBindingTable_BindCallableShader(This, ...)  CALL_IFACE_METHOD(ShaderBindingTable, BindCallableShader, This, __VA_ARGS__)
#    define IShaderBindingTable_BindAll(This, ...)             CALL_IFACE_METHOD(ShaderBindingTable, BindAll,            This, __VA_ARGS__)

// clang-format on

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
