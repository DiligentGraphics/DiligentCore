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

// clang-format off

/// \file
/// Definition of the Diligent::IRenderDevice interface and related data structures

#include "../../../Primitives/interface/Object.h"
#include "../../../Platforms/interface/PlatformDefinitions.h"
#include "GraphicsTypes.h"
#include "BlendState.h"
#include "RasterizerState.h"
#include "DepthStencilState.h"
#include "InputLayout.h"
#include "ShaderResourceBinding.h"
#include "ShaderResourceVariable.h"
#include "Shader.h"
#include "Sampler.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

    
/// Sample description

/// This structure is used by GraphicsPipelineDesc to describe multisampling parameters
struct SampleDesc
{
    /// Sample count
    Uint8 Count     DEFAULT_INITIALIZER(1);

    /// Quality
    Uint8 Quality   DEFAULT_INITIALIZER(0);

#if DILIGENT_CPP_INTERFACE
    SampleDesc()noexcept{}

    SampleDesc(Uint8 _Count, Uint8 _Quality) noexcept : 
        Count   {_Count  },
        Quality {_Quality}
    {}
#endif
};


/// Describes shader variable
struct ShaderResourceVariableDesc
{
    /// Shader stages this resources variable applies to. More than one shader stage can be specified.
    SHADER_TYPE                   ShaderStages DEFAULT_INITIALIZER(SHADER_TYPE_UNKNOWN);

    /// Shader variable name
    const Char*                   Name         DEFAULT_INITIALIZER(nullptr);

    /// Shader variable type. See Diligent::SHADER_RESOURCE_VARIABLE_TYPE for a list of allowed types
    SHADER_RESOURCE_VARIABLE_TYPE Type         DEFAULT_INITIALIZER(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

#if DILIGENT_CPP_INTERFACE
    ShaderResourceVariableDesc()noexcept{}

    ShaderResourceVariableDesc(SHADER_TYPE _ShaderStages, const Char* _Name, SHADER_RESOURCE_VARIABLE_TYPE _Type)noexcept : 
        ShaderStages{_ShaderStages},
        Name        {_Name        },
        Type        {_Type        }
    {}
#endif
};


/// Static sampler description
struct StaticSamplerDesc
{
    /// Shader stages that this static sampler applies to. More than one shader stage can be specified.
    SHADER_TYPE ShaderStages         DEFAULT_INITIALIZER(SHADER_TYPE_UNKNOWN);

    /// The name of the sampler itself or the name of the texture variable that 
    /// this static sampler is assigned to if combined texture samplers are used.
    const Char* SamplerOrTextureName DEFAULT_INITIALIZER(nullptr);

    /// Sampler description
    struct SamplerDesc Desc;

#if DILIGENT_CPP_INTERFACE
    StaticSamplerDesc()noexcept{}

    StaticSamplerDesc(SHADER_TYPE        _ShaderStages,
                      const Char*        _SamplerOrTextureName,
                      const SamplerDesc& _Desc)noexcept : 
        ShaderStages        {_ShaderStages        },
        SamplerOrTextureName{_SamplerOrTextureName},
        Desc                {_Desc                }
    {}
#endif
};


/// Pipeline layout description
struct PipelineResourceLayoutDesc
{
    /// Default shader resource variable type. This type will be used if shader
    /// variable description is not found in the Variables array
    /// or if Variables == nullptr
    SHADER_RESOURCE_VARIABLE_TYPE       DefaultVariableType   DEFAULT_INITIALIZER(SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

    /// Number of elements in Variables array            
    Uint32                              NumVariables          DEFAULT_INITIALIZER(0);

    /// Array of shader resource variable descriptions               
    const struct ShaderResourceVariableDesc*   Variables      DEFAULT_INITIALIZER(nullptr);
                                                            
    /// Number of static samplers in StaticSamplers array   
    Uint32                              NumStaticSamplers     DEFAULT_INITIALIZER(0);
                                                            
    /// Array of static sampler descriptions                
    const struct StaticSamplerDesc*            StaticSamplers DEFAULT_INITIALIZER(nullptr);
};


/// Graphics pipeline state description

/// This structure describes the graphics pipeline state and is part of the PipelineStateDesc structure.
struct GraphicsPipelineDesc
{
    /// Vertex shader to be used with the pipeline
    class IShader* pVS DEFAULT_INITIALIZER(nullptr);

    /// Pixel shader to be used with the pipeline
    class IShader* pPS DEFAULT_INITIALIZER(nullptr);

    /// Domain shader to be used with the pipeline
    class IShader* pDS DEFAULT_INITIALIZER(nullptr);

    /// Hull shader to be used with the pipeline
    class IShader* pHS DEFAULT_INITIALIZER(nullptr);

    /// Geometry shader to be used with the pipeline
    class IShader* pGS DEFAULT_INITIALIZER(nullptr);
    
    //D3D12_STREAM_OUTPUT_DESC StreamOutput;
    
    /// Blend state description
    struct BlendStateDesc BlendDesc;

    /// 32-bit sample mask that determines which samples get updated 
    /// in all the active render targets. A sample mask is always applied; 
    /// it is independent of whether multisampling is enabled, and does not 
    /// depend on whether an application uses multisample render targets.
    Uint32 SampleMask DEFAULT_INITIALIZER(0xFFFFFFFF);

    /// Rasterizer state description
    struct RasterizerStateDesc RasterizerDesc;

    /// Depth-stencil state description
    struct DepthStencilStateDesc DepthStencilDesc;

    /// Input layout
    struct InputLayoutDesc InputLayout;
    //D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;

    /// Primitive topology type
    PRIMITIVE_TOPOLOGY PrimitiveTopology DEFAULT_INITIALIZER(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    /// Number of viewports used by this pipeline
    Uint8 NumViewports           DEFAULT_INITIALIZER(1);

    /// Number of render targets in the RTVFormats member
    Uint8 NumRenderTargets       DEFAULT_INITIALIZER(0);

    /// Render target formats
    TEXTURE_FORMAT RTVFormats[8] DEFAULT_INITIALIZER({});

    /// Depth-stencil format
    TEXTURE_FORMAT DSVFormat     DEFAULT_INITIALIZER(TEX_FORMAT_UNKNOWN);

    /// Multisampling parameters
    struct SampleDesc SmplDesc;

    /// Node mask.
    Uint32 NodeMask DEFAULT_INITIALIZER(0);

    //D3D12_CACHED_PIPELINE_STATE CachedPSO;
    //D3D12_PIPELINE_STATE_FLAGS Flags;
};


/// Compute pipeline state description

/// This structure describes the compute pipeline state and is part of the PipelineStateDesc structure.
struct ComputePipelineDesc
{
    /// Compute shader to be used with the pipeline
    class IShader* pCS DEFAULT_INITIALIZER(nullptr);
};

/// Pipeline state description
struct PipelineStateDesc DILIGENT_DERIVE(DeviceObjectAttribs)

    /// Flag indicating if pipeline state is a compute pipeline state
    bool IsComputePipeline          DEFAULT_INITIALIZER(false);

    /// Shader resource binding allocation granularity

    /// This member defines allocation granularity for internal resources required by the shader resource
    /// binding object instances.
    Uint32 SRBAllocationGranularity DEFAULT_INITIALIZER(1);

    /// Defines which command queues this pipeline state can be used with
    Uint64 CommandQueueMask         DEFAULT_INITIALIZER(1);

    /// Pipeline layout description
    struct PipelineResourceLayoutDesc ResourceLayout;

    /// Graphics pipeline state description. This memeber is ignored if IsComputePipeline == True
    struct GraphicsPipelineDesc GraphicsPipeline;

    /// Compute pipeline state description. This memeber is ignored if IsComputePipeline == False
    struct ComputePipelineDesc ComputePipeline;
};

// {06084AE5-6A71-4FE8-84B9-395DD489A28C}
static const struct INTERFACE_ID IID_PipelineState =
    {0x6084ae5, 0x6a71, 0x4fe8, {0x84, 0xb9, 0x39, 0x5d, 0xd4, 0x89, 0xa2, 0x8c}};

#if DILIGENT_CPP_INTERFACE

/// Pipeline state interface
class IPipelineState : public IDeviceObject
{
public:
    /// Returns the blend state description used to create the object
    virtual const PipelineStateDesc& GetDesc()const override = 0;


    /// Binds resources for all shaders in the pipeline state

    /// \param [in] ShaderFlags - Flags that specify shader stages, for which resources will be bound.
    ///                           Any combination of Diligent::SHADER_TYPE may be used.
    /// \param [in] pResourceMapping - Pointer to the resource mapping interface.
    /// \param [in] Flags - Additional flags. See Diligent::BIND_SHADER_RESOURCES_FLAGS.
    virtual void BindStaticResources(Uint32 ShaderFlags, IResourceMapping* pResourceMapping, Uint32 Flags) = 0;


    /// Returns the number of static shader resource variables.

    /// \param [in] ShaderType - Type of the shader.
    /// \remark Only static variables (that can be accessed directly through the PSO) are counted.
    ///         Mutable and dynamic variables are accessed through Shader Resource Binding object.
    virtual Uint32 GetStaticVariableCount(SHADER_TYPE ShaderType) const = 0;


    /// Returns static shader resource variable. If the variable is not found,
    /// returns nullptr.
    
    /// \param [in] ShaderType - Type of the shader to look up the variable. 
    ///                          Must be one of Diligent::SHADER_TYPE.
    /// \param [in] Name - Name of the variable.
    /// \remark The method does not increment the reference counter
    ///         of the returned interface.
    virtual IShaderResourceVariable* GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) = 0;


    /// Returns static shader resource variable by its index.

    /// \param [in] ShaderType - Type of the shader to look up the variable. 
    ///                          Must be one of Diligent::SHADER_TYPE.
    /// \param [in] Index - Shader variable index. The index must be between
    ///                     0 and the total number of variables returned by 
    ///                     GetStaticVariableCount().
    /// \remark Only static shader resource variables can be accessed through this method.
    ///         Mutable and dynamic variables are accessed through Shader Resource 
    ///         Binding object
    virtual IShaderResourceVariable* GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) = 0;


    /// Creates a shader resource binding object

    /// \param [out] ppShaderResourceBinding - memory location where pointer to the new shader resource
    ///                                        binding object is written.
    /// \param [in] InitStaticResources      - if set to true, the method will initialize static resources in
    ///                                        the created object, which has the exact same effect as calling 
    ///                                        IShaderResourceBinding::InitializeStaticResources().
    virtual void CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding, bool InitStaticResources = false) = 0;


    /// Checks if this pipeline state object is compatible with another PSO

    /// If two pipeline state objects are compatible, they can use shader resource binding
    /// objects interchangebly, i.e. SRBs created by one PSO can be committed
    /// when another PSO is bound.
    /// \param [in] pPSO - Pointer to the pipeline state object to check compatibility with
    /// \return     true if this PSO is compatbile with pPSO. false otherwise.
    /// \remarks    The function only checks that shader resource layouts are compatible, but 
    ///             does not check if resource types match. For instance, if a pixel shader in one PSO
    ///             uses a texture at slot 0, and a pixel shader in another PSO uses texture array at slot 0,
    ///             the pipelines will be compatible. However, if you try to use SRB object from the first pipeline
    ///             to commit resources for the second pipeline, a runtime error will occur.\n
    ///             The function only checks compatibility of shader resource layouts. It does not take
    ///             into account vertex shader input layout, number of outputs, etc.
    virtual bool IsCompatibleWith(const IPipelineState* pPSO)const = 0;
};

#else

struct IPipelineState;

struct IPipelineStateMethods
{
    void                           (*BindStaticResources)        (struct IPipelineState*, Uint32 ShaderFlags, class IResourceMapping* pResourceMapping, Uint32 Flags);
    Uint32                         (*GetStaticVariableCount)     (struct IPipelineState*, SHADER_TYPE ShaderType);
    class IShaderResourceVariable* (*GetStaticVariableByName)    (struct IPipelineState*, SHADER_TYPE ShaderType, const Char* Name);
    class IShaderResourceVariable* (*GetStaticVariableByIndex)   (struct IPipelineState*, SHADER_TYPE ShaderType, Uint32 Index);
    void                           (*CreateShaderResourceBinding)(struct IPipelineState*, class IShaderResourceBinding** ppShaderResourceBinding, bool InitStaticResources);
    bool                           (*IsCompatibleWith)           (struct IPipelineState*, const class IPipelineState* pPSO);
};

// clang-format on

struct IPipelineStateVtbl
{
    struct IObjectMethods        Object;
    struct IDeviceObjectMethods  DeviceObject;
    struct IPipelineStateMethods PipelineState;
};

struct IPipelineState
{
    struct IPipelineStateVtbl* pVtbl;
};

// clang-format off

#    define IPipelineState_GetDesc(This) (const struct PipelineStateDesc*)IDeviceObject_GetDesc(This)

#    define IPipelineState_BindStaticResources(This, ...)         (This)->pPipelineStateVtbl->BindStaticResources        ((struct IPipelineState*)(This), __VA_ARGS__)
#    define IPipelineState_GetStaticVariableCount(This, ...)      (This)->pPipelineStateVtbl->GetStaticVariableCount     ((struct IPipelineState*)(This), __VA_ARGS__)
#    define IPipelineState_GetStaticVariableByName(This, ...)     (This)->pPipelineStateVtbl->GetStaticVariableByName    ((struct IPipelineState*)(This), __VA_ARGS__)
#    define IPipelineState_GetStaticVariableByIndex(This, ...)    (This)->pPipelineStateVtbl->GetStaticVariableByIndex   ((struct IPipelineState*)(This), __VA_ARGS__)
#    define IPipelineState_CreateShaderResourceBinding(This, ...) (This)->pPipelineStateVtbl->CreateShaderResourceBinding((struct IPipelineState*)(This), __VA_ARGS__)
#    define IPipelineState_IsCompatibleWith(This, ...)            (This)->pPipelineStateVtbl->IsCompatibleWith           ((struct IPipelineState*)(This), __VA_ARGS__)

// clang-format on

#endif

DILIGENT_END_NAMESPACE
