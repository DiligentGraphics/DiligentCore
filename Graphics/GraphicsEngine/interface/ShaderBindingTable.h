/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

/// Shader binding table description.
struct ShaderBindingTableDesc DILIGENT_DERIVE(DeviceObjectAttribs)
    
    /// Ray tracing pipeline state object from which shaders will be taken.
    IPipelineState* pPSO                  DEFAULT_INITIALIZER(nullptr);
    
#if DILIGENT_CPP_INTERFACE
    ShaderBindingTableDesc() noexcept {}
#endif
};
typedef struct ShaderBindingTableDesc ShaderBindingTableDesc;


/// Defines shader binding table validation flags, see IShaderBindingTable::Verify().
DILIGENT_TYPED_ENUM(SHADER_BINDING_VALIDATION_FLAGS, Uint8)
{
    /// Checks that all shaders are bound or inactive.
    SHADER_BINDING_VALIDATION_SHADER_ONLY   = 0x1,

    /// Checks that shader record data are initialized.
    SHADER_BINDING_VALIDATION_SHADER_RECORD = 0x2,
        
    /// Checks that all TLAS that used in IShaderBindingTable::BindHitGroup() are alive and
    /// shader binding indices have not changed.
    SHADER_BINDING_VALIDATION_TLAS          = 0x4,

    // Enable all validations.
    SHADER_BINDING_VALIDATION_ALL           = SHADER_BINDING_VALIDATION_SHADER_ONLY   |
                                              SHADER_BINDING_VALIDATION_SHADER_RECORD |
                                              SHADER_BINDING_VALIDATION_TLAS
};
DEFINE_FLAG_ENUM_OPERATORS(SHADER_BINDING_VALIDATION_FLAGS)


#define DILIGENT_INTERFACE_NAME IShaderBindingTable
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IShaderBindingTableInclusiveMethods      \
    IDeviceObjectInclusiveMethods;               \
    IShaderBindingTableMethods ShaderBindingTable

/// Shader binding table interface

/// Defines the methods to manipulate a SBT object
DILIGENT_BEGIN_INTERFACE(IShaderBindingTable, IDeviceObject)
{
#if DILIGENT_CPP_INTERFACE
    /// Returns the shader binding table description used to create the object
    virtual const ShaderBindingTableDesc& DILIGENT_CALL_TYPE GetDesc() const override = 0;
#endif
    
    /// Check that all shaders are bound, instances and geometries are not changed, shader record data are initialized.
    
    /// \param [in] Flags - Flags used for validation.
    /// \return     True if SBT content is valid.
    /// 
    /// \note Access to the SBT must be externally synchronized.
    ///       This method implemented only for development build and has no effect in release build.
    VIRTUAL Bool METHOD(Verify)(THIS_
                                SHADER_BINDING_VALIDATION_FLAGS Flags) CONST PURE;
    

    /// Reset SBT with the new pipeline state. This is more effecient than creating a new SBT.
    
    /// \note Access to the SBT must be externally synchronized.
    VIRTUAL void METHOD(Reset)(THIS_
                               IPipelineState* pPSO) PURE;
    

    /// When TLAS or BLAS was rebuilt or updated, hit group shader bindings may have become invalid,
    /// you can reset only hit groups and keep ray-gen, miss and callable shader bindings intact.
    
    /// \note Access to the SBT must be externally synchronized.
    VIRTUAL void METHOD(ResetHitGroups)(THIS) PURE;
    

    /// Bind ray-generation shader.
    
    /// \param [in] pShaderGroupName - Ray-generation shader name that was specified in RayTracingGeneralShaderGroup::Name.
    /// \param [in] pData            - Shader record data, can be null.
    /// \param [in] DataSize         - Shader record data size, should be equal to RayTracingPipelineDesc::ShaderRecordSize.
    /// 
    /// \note Access to the SBT must be externally synchronized.
    VIRTUAL void METHOD(BindRayGenShader)(THIS_
                                          const char* pShaderGroupName,
                                          const void* pData            DEFAULT_INITIALIZER(nullptr),
                                          Uint32      DataSize         DEFAULT_INITIALIZER(0)) PURE;
    

    /// Bind ray-miss shader.
    
    /// \param [in] pShaderGroupName - Ray-miss shader name that was specified in RayTracingGeneralShaderGroup::Name,
    ///                                can be null to make shader inactive.
    /// \param [in] MissIndex        - Miss shader offset in shader binding table, use the same value as in the shader:
    ///                                'MissShaderIndex' argument in TraceRay() in HLSL, 'missIndex' in traceRay() in GLSL.
    /// \param [in] pData            - Shader record data, can be null.
    /// \param [in] DataSize         - Shader record data size, should be equal to RayTracingPipelineDesc::ShaderRecordSize.
    /// 
    /// \note Access to the SBT must be externally synchronized.
    VIRTUAL void METHOD(BindMissShader)(THIS_
                                        const char* pShaderGroupName,
                                        Uint32      MissIndex,
                                        const void* pData            DEFAULT_INITIALIZER(nullptr),
                                        Uint32      DataSize         DEFAULT_INITIALIZER(0)) PURE;
    

    /// Bind hit group for the specified geometry in instance.
    
    /// \param [in] pTLAS                    - Top-level AS, used to calculate offset for instance.
    /// \param [in] pInstanceName            - Instance name, see TLASBuildInstanceData::InstanceName.
    /// \param [in] pGeometryName            - Geometry name, see BLASBuildTriangleData::GeometryName and BLASBuildBoundingBoxData::GeometryName.
    /// \param [in] RayOffsetInHitGroupIndex - Ray offset in shader binding table, use the same value as in the shader:
    ///                                        'RayContributionToHitGroupIndex' argument in TraceRay() in HLSL, 'sbtRecordOffset' argument in traceRay() in GLSL.
    ///                                        Must be less than HitShadersPerInstance.
    /// \param [in] pShaderGroupName         - Hit group name that was specified in RayTracingTriangleHitShaderGroup::Name and RayTracingProceduralHitShaderGroup::Name,
    ///                                        can be null to make the shader inactive.
    /// \param [in] pData                    - Shader record data, can be null.
    /// \param [in] DataSize                 - Shader record data size, should be equal to RayTracingPipelineDesc::ShaderRecordSize.
    /// 
    /// \note Access to the SBT must be externally synchronized.
    ///       Access to the TLAS must be externally synchronized.
    ///       Access to the BLAS that was used in TLAS instance with name pInstanceName must be externally synchronized.
    VIRTUAL void METHOD(BindHitGroup)(THIS_
                                      ITopLevelAS* pTLAS,
                                      const char*  pInstanceName,
                                      const char*  pGeometryName,
                                      Uint32       RayOffsetInHitGroupIndex,
                                      const char*  pShaderGroupName,
                                      const void*  pData            DEFAULT_INITIALIZER(nullptr),
                                      Uint32       DataSize         DEFAULT_INITIALIZER(0)) PURE;
    

    /// Bind hit group to specified location.
    
    /// \param [in] BindingIndex     - location of the hit group. 
    /// \param [in] pShaderGroupName - hit group name that specified in RayTracingTriangleHitShaderGroup::Name and RayTracingProceduralHitShaderGroup::Name,
    ///                                can be null to make shader inactive.
    /// \param [in] pData            - shader record data, can be null.
    /// \param [in] DataSize         - shader record data size, should equal to RayTracingPipelineDesc::ShaderRecordSize.
    /// 
    /// \note Access to the SBT must be externally synchronized.
    /// 
    /// \remarks Use IBottomLevelAS::GetGeometryIndex(), ITopLevelAS::GetBuildInfo(), ITopLevelAS::GetInstanceDesc().ContributionToHitGroupIndex to calculate binding index.
    VIRTUAL void METHOD(BindHitGroupByIndex)(THIS_
                                             Uint32      BindingIndex,
                                             const char* pShaderGroupName,
                                             const void* pData            DEFAULT_INITIALIZER(nullptr),
                                             Uint32      DataSize         DEFAULT_INITIALIZER(0)) PURE;


    /// Bind hit group for each geometries in specified instance.
    
    /// \param [in] pTLAS                    - Top-level AS, used to calculate offset for instance.
    /// \param [in] pInstanceName            - Instance name, see TLASBuildInstanceData::InstanceName.
    /// \param [in] RayOffsetInHitGroupIndex - Ray offset in shader binding table, use the same value as in the shader:
    ///                                        'RayContributionToHitGroupIndex' argument in TraceRay() in HLSL, 'sbtRecordOffset' argument in traceRay() in GLSL.
    ///                                        Must be less than HitShadersPerInstance.
    /// \param [in] pShaderGroupName         - Hit group name that was specified in RayTracingTriangleHitShaderGroup::Name and RayTracingProceduralHitShaderGroup::Name,
    ///                                        can be null to make shader inactive.
    /// \param [in] pData                    - Shader record data, can be null.
    /// \param [in] DataSize                 - Shader record data size, should equal to RayTracingPipelineDesc::ShaderRecordSize.
    /// 
    /// \note Access to the SBT must be externally synchronized.
    ///       Access to the TLAS must be externally synchronized.
    VIRTUAL void METHOD(BindHitGroups)(THIS_
                                       ITopLevelAS* pTLAS,
                                       const char*  pInstanceName,
                                       Uint32       RayOffsetInHitGroupIndex,
                                       const char*  pShaderGroupName,
                                       const void*  pData            DEFAULT_INITIALIZER(nullptr),
                                       Uint32       DataSize         DEFAULT_INITIALIZER(0)) PURE;
    
    
    /// Bind hit group for each instances in top-level AS.
    
    /// \param [in] pTLAS                    - Top-level AS, used to calculate offset for instance.
    /// \param [in] RayOffsetInHitGroupIndex - Ray offset in shader binding table, use the same value as in the shader:
    ///                                        'RayContributionToHitGroupIndex' argument in TraceRay() in HLSL, 'sbtRecordOffset' argument in traceRay() in GLSL.
    ///                                        Must be less than HitShadersPerInstance.
    /// \param [in] pShaderGroupName         - Hit group name that was specified in RayTracingTriangleHitShaderGroup::Name and RayTracingProceduralHitShaderGroup::Name,
    ///                                        can be null to make shader inactive.
    /// \param [in] pData                    - Shader record data, can be null.
    /// \param [in] DataSize                 - Shader record data size, should equal to RayTracingPipelineDesc::ShaderRecordSize.
    /// 
    /// \note Access to the SBT must be externally synchronized.
    ///       Access to the TLAS must be externally synchronized.
    VIRTUAL void METHOD(BindHitGroupForAll)(THIS_
                                            ITopLevelAS* pTLAS,
                                            Uint32       RayOffsetInHitGroupIndex,
                                            const char*  pShaderGroupName,
                                            const void*  pData            DEFAULT_INITIALIZER(nullptr),
                                            Uint32       DataSize         DEFAULT_INITIALIZER(0)) PURE;


    /// Bind callable shader.
    
    /// \param [in] pShaderGroupName - Callable shader name that specified in RayTracingGeneralShaderGroup::Name,
    ///                                can be null to make shader inactive.
    /// \param [in] CallableIndex    - Callable shader offset in shader binding table, use the same value as in the shader:
    ///                                'ShaderIndex' argument in CallShader() in HLSL, 'callable' argument in executeCallable() in GLSL.
    /// \param [in] pData            - Shader record data, can be null.
    /// \param [in] DataSize         - Shader record data size, should equal to RayTracingPipelineDesc::ShaderRecordSize.
    /// 
    /// \note Access to the SBT must be externally synchronized.
    VIRTUAL void METHOD(BindCallableShader)(THIS_
                                            const char* pShaderGroupName,
                                            Uint32      CallableIndex,
                                            const void* pData           DEFAULT_INITIALIZER(nullptr),
                                            Uint32      DataSize        DEFAULT_INITIALIZER(0)) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

// clang-format off

#    define IShaderBindingTable_Verify(This, ...)              CALL_IFACE_METHOD(ShaderBindingTable, Verify,              This, __VA_ARGS__)
#    define IShaderBindingTable_Reset(This, ...)               CALL_IFACE_METHOD(ShaderBindingTable, Reset,               This, __VA_ARGS__)
#    define IShaderBindingTable_ResetHitGroups(This)           CALL_IFACE_METHOD(ShaderBindingTable, ResetHitGroups,      This)
#    define IShaderBindingTable_BindRayGenShader(This, ...)    CALL_IFACE_METHOD(ShaderBindingTable, BindRayGenShader,    This, __VA_ARGS__)
#    define IShaderBindingTable_BindMissShader(This, ...)      CALL_IFACE_METHOD(ShaderBindingTable, BindMissShader,      This, __VA_ARGS__)
#    define IShaderBindingTable_BindHitGroupByIndex(This, ...) CALL_IFACE_METHOD(ShaderBindingTable, BindHitGroupByIndex, This, __VA_ARGS__)
#    define IShaderBindingTable_BindHitGroup(This, ...)        CALL_IFACE_METHOD(ShaderBindingTable, BindHitGroup,        This, __VA_ARGS__)
#    define IShaderBindingTable_BindHitGroups(This, ...)       CALL_IFACE_METHOD(ShaderBindingTable, BindHitGroups,       This, __VA_ARGS__)
#    define IShaderBindingTable_BindHitGroupForAll(This, ...)  CALL_IFACE_METHOD(ShaderBindingTable, BindHitGroupForAll,  This, __VA_ARGS__)
#    define IShaderBindingTable_BindCallableShader(This, ...)  CALL_IFACE_METHOD(ShaderBindingTable, BindCallableShader,  This, __VA_ARGS__)

// clang-format on

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
