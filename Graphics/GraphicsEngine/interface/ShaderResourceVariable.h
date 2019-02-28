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
/// Definition of the Diligent::IShaderResourceVariable interface and related data structures

#include "../../../Primitives/interface/BasicTypes.h"
#include "../../../Primitives/interface/Object.h"
#include "DeviceObject.h"

namespace Diligent
{

// {0D57DF3F-977D-4C8F-B64C-6675814BC80C}
static constexpr INTERFACE_ID IID_ShaderResourceVariable =
{ 0xd57df3f, 0x977d, 0x4c8f, { 0xb6, 0x4c, 0x66, 0x75, 0x81, 0x4b, 0xc8, 0xc } };


/// Describes the type of the shader resource variable
enum SHADER_RESOURCE_VARIABLE_TYPE : Uint8
{
    /// Shader resource bound to the variable is the same for all SRB instances.
    /// It must be set *once* directly through Pipeline State object.
    SHADER_RESOURCE_VARIABLE_TYPE_STATIC = 0, 

    /// Shader resource bound to the variable is specific to the shader resource binding 
    /// instance (see Diligent::IShaderResourceBinding). It must be set *once* through 
    /// Diligent::IShaderResourceBinding interface. It cannot be set through Diligent::IPipelineState
    /// interface and cannot be change once bound.
    SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE,

    /// Shader variable binding is dynamic. It can be set multiple times for every instance of shader resource 
    /// binding (see Diligent::IShaderResourceBinding). It cannot be set through Diligent::IPipelineState interface.
    SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC,

    /// Total number of shader variable types
    SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES
};


static_assert(SHADER_RESOURCE_VARIABLE_TYPE_STATIC == 0 && SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE == 1 && SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC == 2 && SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES == 3, "BIND_SHADER_RESOURCES_UPDATE_* flags rely on shader variable SHADER_RESOURCE_VARIABLE_TYPE_* values being 0,1,2");

/// Shader resource binding flags
enum BIND_SHADER_RESOURCES_FLAGS : Uint32
{
    /// Indicates that static shader variable bindings are to be updated.
    BIND_SHADER_RESOURCES_UPDATE_STATIC  = (0x01 << SHADER_RESOURCE_VARIABLE_TYPE_STATIC),

    /// Indicates that mutable shader variable bindings are to be updated.
    BIND_SHADER_RESOURCES_UPDATE_MUTABLE = (0x01 << SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE),

    /// Indicates that dynamic shader variable bindings are to be updated.
    BIND_SHADER_RESOURCES_UPDATE_DYNAMIC = (0x01 << SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC),

    /// Indicates that all shader variable types (static, mutable and dynamic) are to be updated.
    /// \note If none of BIND_SHADER_RESOURCES_UPDATE_STATIC, BIND_SHADER_RESOURCES_UPDATE_MUTABLE,
    ///       and BIND_SHADER_RESOURCES_UPDATE_DYNAMIC flags are set, all variable types are updated
    ///       as if BIND_SHADER_RESOURCES_UPDATE_ALL was specified.
    BIND_SHADER_RESOURCES_UPDATE_ALL = (BIND_SHADER_RESOURCES_UPDATE_STATIC | BIND_SHADER_RESOURCES_UPDATE_MUTABLE | BIND_SHADER_RESOURCES_UPDATE_DYNAMIC),

    /// If this flag is specified, all existing bindings will be preserved and 
    /// only unresolved ones will be updated.
    /// If this flag is not specified, every shader variable will be
    /// updated if the mapping contains corresponding resource.
    BIND_SHADER_RESOURCES_KEEP_EXISTING = 0x08,

    /// If this flag is specified, all shader bindings are expected
    /// to be resolved after the call. If this is not the case, debug message 
    /// will be displayed.
    /// \note Only these variables are verified that are being updated by setting
    ///       BIND_SHADER_RESOURCES_UPDATE_STATIC, BIND_SHADER_RESOURCES_UPDATE_MUTABLE, and
    ///       BIND_SHADER_RESOURCES_UPDATE_DYNAMIC flags.
    BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED = 0x10
};


/// Shader resource variable
class IShaderResourceVariable : public IObject
{
public:
    /// Binds resource to the variable

    /// \remark The method performs run-time correctness checks.
    ///         For instance, shader resource view cannot
    ///         be assigned to a constant buffer variable.
    virtual void Set(IDeviceObject* pObject) = 0;

    /// Binds resource array to the variable

    /// \param [in] ppObjects    - pointer to the array of objects
    /// \param [in] FirstElement - first array element to set
    /// \param [in] NumElements  - number of objects in ppObjects array
    ///
    /// \remark The method performs run-time correctness checks.
    ///         For instance, shader resource view cannot
    ///         be assigned to a constant buffer variable.
    virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements) = 0;

    /// Returns the shader resource variable type
    virtual SHADER_RESOURCE_VARIABLE_TYPE GetType()const = 0;

    /// Returns array size. For non-array variables returns one.
    virtual Uint32 GetArraySize()const = 0;

    /// Returns the variable name
    virtual const Char* GetName()const = 0;

    /// Returns the variable index that can be used to access the variable.
    virtual Uint32 GetIndex()const = 0;
};

}
