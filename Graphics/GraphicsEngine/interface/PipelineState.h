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

namespace Diligent
{
    
/// Sample description

/// This structure is used by GraphicsPipelineDesc to describe multisampling parameters
struct SampleDesc
{
    /// Sample count
    Uint8 Count     = 1;

    /// Quality
    Uint8 Quality   = 0;

    SampleDesc()noexcept{}

    SampleDesc(Uint8 _Count, Uint8 _Quality) noexcept : 
        Count   {_Count  },
        Quality {_Quality}
    {}
};


/// Describes shader variable
struct ShaderResourceVariableDesc
{
    /// Shader stages this resources variable applies to. More than one shader stage can be specified.
    SHADER_TYPE                   ShaderStages = SHADER_TYPE_UNKNOWN;

    /// Shader variable name
    const Char*                   Name         = nullptr;

    /// Shader variable type. See Diligent::SHADER_RESOURCE_VARIABLE_TYPE for a list of allowed types
    SHADER_RESOURCE_VARIABLE_TYPE Type         = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    ShaderResourceVariableDesc()noexcept{}

    ShaderResourceVariableDesc(SHADER_TYPE _ShaderStages, const Char* _Name, SHADER_RESOURCE_VARIABLE_TYPE _Type)noexcept : 
        ShaderStages{_ShaderStages},
        Name        {_Name        },
        Type        {_Type        }
    {}
};


/// Static sampler description
struct StaticSamplerDesc
{
    /// Shader stages that this static sampler applies to. More than one shader stage can be specified.
    SHADER_TYPE ShaderStages         = SHADER_TYPE_UNKNOWN;

    /// The name of the sampler itself or the name of the texture variable that 
    /// this static sampler is assigned to if combined texture samplers are used.
    const Char* SamplerOrTextureName = nullptr;

    /// Sampler description
    SamplerDesc Desc;

    StaticSamplerDesc()noexcept{}

    StaticSamplerDesc(SHADER_TYPE        _ShaderStages,
                      const Char*        _SamplerOrTextureName,
                      const SamplerDesc& _Desc)noexcept : 
        ShaderStages        {_ShaderStages        },
        SamplerOrTextureName{_SamplerOrTextureName},
        Desc                {_Desc                }
    {}
};


/// Pipeline layout description
struct PipelineResourceLayoutDesc
{
    /// Default shader resource variable type. This type will be used if shader
    /// variable description is not found in the Variables array
    /// or if Variables == nullptr
    SHADER_RESOURCE_VARIABLE_TYPE       DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    /// Number of elements in Variables array            
    Uint32                              NumVariables        = 0;

    /// Array of shader resource variable descriptions               
    const ShaderResourceVariableDesc*   Variables           = nullptr;
                                                            
    /// Number of static samplers in StaticSamplers array   
    Uint32                              NumStaticSamplers   = 0;
                                                            
    /// Array of static sampler descriptions                
    const StaticSamplerDesc*            StaticSamplers      = nullptr;
};


/// Graphics pipeline state description

/// This structure describes the graphics pipeline state and is part of the PipelineStateDesc structure.
struct GraphicsPipelineDesc
{
    /// Vertex shader to be used with the pipeline
    IShader* pVS = nullptr;

    /// Pixel shader to be used with the pipeline
    IShader* pPS = nullptr;

    /// Domain shader to be used with the pipeline
    IShader* pDS = nullptr;

    /// Hull shader to be used with the pipeline
    IShader* pHS = nullptr;

    /// Geometry shader to be used with the pipeline
    IShader* pGS = nullptr;
    
    //D3D12_STREAM_OUTPUT_DESC StreamOutput;
    
    /// Blend state description
    BlendStateDesc BlendDesc;

    /// 32-bit sample mask that determines which samples get updated 
    /// in all the active render targets. A sample mask is always applied; 
    /// it is independent of whether multisampling is enabled, and does not 
    /// depend on whether an application uses multisample render targets.
    Uint32 SampleMask = 0xFFFFFFFF;

    /// Rasterizer state description
    RasterizerStateDesc RasterizerDesc;

    /// Depth-stencil state description
    DepthStencilStateDesc DepthStencilDesc;

    /// Input layout
    InputLayoutDesc InputLayout;
    //D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;

    /// Primitive topology type
    PRIMITIVE_TOPOLOGY PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    /// Number of viewports used by this pipeline
    Uint8 NumViewports = 1;

    /// Number of render targets in the RTVFormats member
    Uint8 NumRenderTargets = 0;

    /// Render target formats
    TEXTURE_FORMAT RTVFormats[8] = {};

    /// Depth-stencil format
    TEXTURE_FORMAT DSVFormat = TEX_FORMAT_UNKNOWN;

    /// Multisampling parameters
    SampleDesc SmplDesc;

    /// Node mask.
    Uint32 NodeMask = 0;

    //D3D12_CACHED_PIPELINE_STATE CachedPSO;
    //D3D12_PIPELINE_STATE_FLAGS Flags;
};


/// Compute pipeline state description

/// This structure describes the compute pipeline state and is part of the PipelineStateDesc structure.
struct ComputePipelineDesc
{
    /// Compute shader to be used with the pipeline
    IShader* pCS = nullptr;
};

/// Pipeline state description
struct PipelineStateDesc : DeviceObjectAttribs
{
    /// Flag indicating if pipeline state is a compute pipeline state
    bool IsComputePipeline          = false;

    /// Shader resource binding allocation granularity

    /// This member defines allocation granularity for internal resources required by the shader resource
    /// binding object instances.
    Uint32 SRBAllocationGranularity = 1;

    /// Defines which command queues this pipeline state can be used with
    Uint64 CommandQueueMask         = 1;

    /// Pipeline layout description
    PipelineResourceLayoutDesc ResourceLayout;

    /// Graphics pipeline state description. This memeber is ignored if IsComputePipeline == True
    GraphicsPipelineDesc GraphicsPipeline;

    /// Compute pipeline state description. This memeber is ignored if IsComputePipeline == False
    ComputePipelineDesc ComputePipeline;
};

// {06084AE5-6A71-4FE8-84B9-395DD489A28C}
static constexpr INTERFACE_ID IID_PipelineState =
    {0x6084ae5, 0x6a71, 0x4fe8, {0x84, 0xb9, 0x39, 0x5d, 0xd4, 0x89, 0xa2, 0x8c}};

/// Pipeline state interface
class IPipelineState : public IDeviceObject
{
public:
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface( const INTERFACE_ID& IID, IObject** ppInterface )override = 0;


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

}
