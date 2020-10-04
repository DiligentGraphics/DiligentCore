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
/// Definition of the Diligent::ITopLevelAS interface and related data structures

#include "../../../Primitives/interface/Object.h"
#include "../../../Primitives/interface/FlagEnum.h"
#include "GraphicsTypes.h"
#include "Constants.h"
#include "Buffer.h"
#include "BottomLevelAS.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {16561861-294B-4804-96FA-1717333F769A}
static const INTERFACE_ID IID_TopLevelAS =
    {0x16561861, 0x294b, 0x4804, {0x96, 0xfa, 0x17, 0x17, 0x33, 0x3f, 0x76, 0x9a}};

// clang-format off

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
    Uint32          contributionToHitGroupIndex DEFAULT_INITIALIZER(0);

    /// AZ TODO
    IBottomLevelAS* pBLAS                       DEFAULT_INITIALIZER(nullptr);
    
#if DILIGENT_CPP_INTERFACE
    /// AZ TODO
    TLASInstanceDesc() noexcept {}
#endif
};
typedef struct TLASInstanceDesc TLASInstanceDesc;


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

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

// clang-format off

#    define ITopLevelAS_GetInstanceDesc(This, ...)           CALL_IFACE_METHOD(TopLevelAS, GetInstanceDesc,          This, __VA_ARGS__)

// clang-format on

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
