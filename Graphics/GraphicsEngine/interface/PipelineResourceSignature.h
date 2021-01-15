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

// clang-format off

/// \file
/// Definition of the Diligent::IRenderDevice interface and related data structures

#include "../../../Primitives/interface/Object.h"
#include "../../../Platforms/interface/PlatformDefinitions.h"
#include "GraphicsTypes.h"
#include "Shader.h"
#include "Sampler.h"
#include "ShaderResourceVariable.h"
#include "ShaderResourceBinding.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)


/// Immutable sampler description.

/// An immutable sampler is compiled into the pipeline state and can't be changed.
/// It is generally more efficient than a regular sampler and should be used
/// whenever possible.
struct ImmutableSamplerDesc
{
    /// Shader stages that this immutable sampler applies to. More than one shader stage can be specified.
    SHADER_TYPE ShaderStages         DEFAULT_INITIALIZER(SHADER_TYPE_UNKNOWN);

    /// The name of the sampler itself or the name of the texture variable that 
    /// this immutable sampler is assigned to if combined texture samplers are used.
    const Char* SamplerOrTextureName DEFAULT_INITIALIZER(nullptr);

    /// Sampler description
    struct SamplerDesc Desc;

#if DILIGENT_CPP_INTERFACE
    ImmutableSamplerDesc()noexcept{}

    ImmutableSamplerDesc(SHADER_TYPE        _ShaderStages,
                         const Char*        _SamplerOrTextureName,
                         const SamplerDesc& _Desc)noexcept : 
        ShaderStages        {_ShaderStages        },
        SamplerOrTextureName{_SamplerOrTextureName},
        Desc                {_Desc                }
    {}
#endif
};
typedef struct ImmutableSamplerDesc ImmutableSamplerDesc;


/// AZ TODO: comment
DILIGENT_TYPED_ENUM(PIPELINE_RESOURCE_FLAGS, Uint8)
{
    PIPELINE_RESOURCE_FLAG_UNKNOWN            = 0x00,
    PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_OFFSETS = 0x01, ///< Vulkan only, for SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_TYPE_BUFFER_UAV, SHADER_RESOURCE_TYPE_BUFFER_SRV
    PIPELINE_RESOURCE_FLAG_COMBINED_IMAGE     = 0x02, ///< For SHADER_RESOURCE_TYPE_TEXTURE_SRV
    PIPELINE_RESOURCE_FLAG_TEXEL_BUFFER       = 0x04, ///< For SHADER_RESOURCE_TYPE_BUFFER_UAV, SHADER_RESOURCE_TYPE_BUFFER_SRV
};
DEFINE_FLAG_ENUM_OPERATORS(PIPELINE_RESOURCE_FLAGS);


/// AZ TODO: comment
struct PipelineResourceDesc
{
    /// AZ TODO: comment
    const char*                    Name          DEFAULT_INITIALIZER(nullptr);

    /// AZ TODO: comment
    SHADER_TYPE                    ShaderStages  DEFAULT_INITIALIZER(SHADER_TYPE_UNKNOWN);

    /// AZ TODO: comment
    Uint32                         ArraySize     DEFAULT_INITIALIZER(1);

    /// AZ TODO: comment
    SHADER_RESOURCE_TYPE           ResourceType  DEFAULT_INITIALIZER(SHADER_RESOURCE_TYPE_UNKNOWN);

    /// AZ TODO: comment
    SHADER_RESOURCE_VARIABLE_TYPE  VarType       DEFAULT_INITIALIZER(SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE);
    
    /// AZ TODO: comment
    PIPELINE_RESOURCE_FLAGS        Flags         DEFAULT_INITIALIZER(PIPELINE_RESOURCE_FLAG_UNKNOWN);
    
    /// AZ TODO: comment
    RESOURCE_DIMENSION             ResourceDim   DEFAULT_INITIALIZER(RESOURCE_DIM_UNDEFINED);

#if DILIGENT_CPP_INTERFACE
    PipelineResourceDesc()noexcept{}

    PipelineResourceDesc(const char*                   _Name,
                         Uint32                        _ArraySize,
                         SHADER_RESOURCE_TYPE          _ResourceType,
                         SHADER_TYPE                   _ShaderStages,
                         SHADER_RESOURCE_VARIABLE_TYPE _VarType,
                         RESOURCE_DIMENSION            _Dim   = RESOURCE_DIM_UNDEFINED,
                         PIPELINE_RESOURCE_FLAGS       _Flags = PIPELINE_RESOURCE_FLAG_UNKNOWN)noexcept : 
        Name        {_Name        },
        ArraySize   {_ArraySize   },
        ResourceType{_ResourceType},
        ShaderStages{_ShaderStages},
        VarType     {_VarType     },
        Flags       {_Flags       },
        ResourceDim {_Dim         }
    {}
#endif
};
typedef struct PipelineResourceDesc PipelineResourceDesc;


/// AZ TODO: comment
struct PipelineResourceSignatureDesc DILIGENT_DERIVE(DeviceObjectAttribs)

    /// AZ TODO: comment
    const PipelineResourceDesc*  Resources  DEFAULT_INITIALIZER(nullptr);
    
    /// AZ TODO: comment
    Uint32  NumResources  DEFAULT_INITIALIZER(0);
    
    /// AZ TODO: comment
    const ImmutableSamplerDesc*  ImmutableSamplers  DEFAULT_INITIALIZER(nullptr);
    
    /// AZ TODO: comment
    Uint32  NumImmutableSamplers  DEFAULT_INITIALIZER(0);
    
    /// AZ TODO: comment
    Uint8  BindingIndex DEFAULT_INITIALIZER(0);
    
    /// AZ TODO: comment
    Uint16 BindingOffsets [SHADER_RESOURCE_TYPE_LAST + 1]  DEFAULT_INITIALIZER({});

    /// Shader resource binding allocation granularity

    /// This member defines allocation granularity for internal resources required by the shader resource
    /// binding object instances.
    Uint32 SRBAllocationGranularity DEFAULT_INITIALIZER(1);
};
typedef struct PipelineResourceSignatureDesc PipelineResourceSignatureDesc;


// {DCE499A5-F812-4C93-B108-D684A0B56118}
static const INTERFACE_ID IID_PipelineResourceSignature = 
    {0xdce499a5, 0xf812, 0x4c93, {0xb1, 0x8, 0xd6, 0x84, 0xa0, 0xb5, 0x61, 0x18}};

#define DILIGENT_INTERFACE_NAME IPipelineResourceSignature
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IPipelineResourceSignatureInclusiveMethods \
    IDeviceObjectInclusiveMethods;     \
    IPipelineResourceSignatureMethods PipelineResourceSignature

// clang-format off

/// Pipeline state interface
DILIGENT_BEGIN_INTERFACE(IPipelineResourceSignature, IDeviceObject)
{
#if DILIGENT_CPP_INTERFACE
    /// AZ TODO: comment
    virtual const PipelineResourceSignatureDesc& METHOD(GetDesc)() const override = 0;
#endif
    
    /// Creates a shader resource binding object

    /// \param [out] ppShaderResourceBinding - memory location where pointer to the new shader resource
    ///                                        binding object is written.
    /// \param [in] InitStaticResources      - if set to true, the method will initialize static resources in
    ///                                        the created object, which has the exact same effect as calling 
    ///                                        IShaderResourceBinding::InitializeStaticResources().
    VIRTUAL void METHOD(CreateShaderResourceBinding)(THIS_
                                                     IShaderResourceBinding** ppShaderResourceBinding,
                                                     bool                     InitStaticResources DEFAULT_VALUE(false)) PURE;
    

    /// Returns static shader resource variable. If the variable is not found,
    /// returns nullptr.
    
    /// \param [in] ShaderType - Type of the shader to look up the variable. 
    ///                          Must be one of Diligent::SHADER_TYPE.
    /// \param [in] Name - Name of the variable.
    /// \remark The method does not increment the reference counter
    ///         of the returned interface.
    VIRTUAL IShaderResourceVariable* METHOD(GetStaticVariableByName)(THIS_
                                                                     SHADER_TYPE ShaderType,
                                                                     const Char* Name) PURE;
    

    /// Returns static shader resource variable by its index.

    /// \param [in] ShaderType - Type of the shader to look up the variable. 
    ///                          Must be one of Diligent::SHADER_TYPE.
    /// \param [in] Index - Shader variable index. The index must be between
    ///                     0 and the total number of variables returned by 
    ///                     GetStaticVariableCount().
    /// \remark Only static shader resource variables can be accessed through this method.
    ///         Mutable and dynamic variables are accessed through Shader Resource 
    ///         Binding object
    VIRTUAL IShaderResourceVariable* METHOD(GetStaticVariableByIndex)(THIS_
                                                                      SHADER_TYPE ShaderType,
                                                                      Uint32      Index) PURE;
    

    /// Returns the number of static shader resource variables.

    /// \param [in] ShaderType - Type of the shader.
    /// \remark Only static variables (that can be accessed directly through the PSO) are counted.
    ///         Mutable and dynamic variables are accessed through Shader Resource Binding object.
    VIRTUAL Uint32 METHOD(GetStaticVariableCount)(THIS_
                                                  SHADER_TYPE ShaderType) CONST PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

// clang-format off

#    define IPipelineResourceSignature_GetDesc(This) (const struct PipelineResourceSignatureDesc*)IDeviceObject_GetDesc(This)

#    define IPipelineResourceSignature_CreateShaderResourceBinding(This, ...)  CALL_IFACE_METHOD(PipelineResourceSignature, CreateShaderResourceBinding, This, __VA_ARGS__)
#    define IPipelineResourceSignature_GetStaticVariableByName(This, ...)      CALL_IFACE_METHOD(PipelineResourceSignature, GetStaticVariableByName,     This, __VA_ARGS__)
#    define IPipelineResourceSignature_GetStaticVariableByIndex(This, ...)     CALL_IFACE_METHOD(PipelineResourceSignature, GetStaticVariableByIndex,    This, __VA_ARGS__)
#    define IPipelineResourceSignature_GetStaticVariableCount(This, ...)       CALL_IFACE_METHOD(PipelineResourceSignature, GetStaticVariableCount,      This, __VA_ARGS__)

// clang-format on

#endif

DILIGENT_END_NAMESPACE
