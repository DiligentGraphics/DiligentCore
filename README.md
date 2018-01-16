# DiligentCore

This module implements key engine functionality. It provides API implementations with Direct3D11, Direct3D12, 
OpenGL and OpenGLES as well as basic platform-specific utilities.

# Build Status

| Platform                   | Status        |
| -------------------------- | ------------- |
| Win32/Universal Windows    | [![Build Status](https://ci.appveyor.com/api/projects/status/github/DiligentGraphics/DiligentCore?svg=true)](https://ci.appveyor.com/project/DiligentGraphics/diligentcore) |
| Linux                      | [![Build Status](https://travis-ci.org/DiligentGraphics/DiligentCore.svg?branch=master)](https://travis-ci.org/DiligentGraphics/DiligentCore)      |


# Repository structure

 The repository contains the following projects:

 | Project                                                          | Description       |
 |------------------------------------------------------------------|-------------------|
 | [Primitives](https://github.com/DiligentGraphics/DiligentCore/tree/master/Primitives) 										| Definitions of basic types (Int32, Int16, Uint32, etc.) and interfaces (IObject, IReferenceCounters, etc.) |
 | [Common](https://github.com/DiligentGraphics/DiligentCore/tree/master/Common)												| Common functionality such as file wrapper, logging, debug utilities, etc. |
 | [Graphics/GraphicsAccessories](https://github.com/DiligentGraphics/DiligentCore/tree/master/Graphics/GraphicsAccessories)	| Basic graphics accessories used by all implementations  |
 | [Graphics/GraphicsEngine](https://github.com/DiligentGraphics/DiligentCore/tree/master/Graphics/GraphicsEngine)	            | Platform-independent base functionality |
 | [Graphics/GraphicsEngineD3DBase](https://github.com/DiligentGraphics/DiligentCore/tree/master/Graphics/GraphicsEngineD3DBase)| Base functionality for D3D11/D3D12 implementations |
 | [Graphics/GraphicsEngineD3D11](https://github.com/DiligentGraphics/DiligentCore/tree/master/Graphics/GraphicsEngineD3D11)     | Engine implementation with Direct3D11 |
 | [Graphics/GraphicsEngineD3D12](https://github.com/DiligentGraphics/DiligentCore/tree/master/Graphics/GraphicsEngineD3D12)     | Engine implementation with Direct3D12 |
 | [Graphics/GraphicsEngineOpenGL](https://github.com/DiligentGraphics/DiligentCore/tree/master/Graphics/GraphicsEngineOpenGL)   | Engine implementation with OpenGL/GLES |
 | [Graphics/GraphicsTools](https://github.com/DiligentGraphics/DiligentCore/tree/master/Graphics/GraphicsTools)                 | Graphics utilities build on top of core interfaces (definitions of commonly used states, texture uploaders, etc.) | 
 | [Graphics/HLSL2GLSLConverterLib](https://github.com/DiligentGraphics/DiligentCore/tree/master/Graphics/HLSL2GLSLConverterLib) | HLSL to GLSL source code converter library |
 | [Platforms/Basic](https://github.com/DiligentGraphics/DiligentCore/tree/master/Platforms/Basic)      | Interface for platform-specific routines and implementation of some common functionality |
 | [Platforms/Android](https://github.com/DiligentGraphics/DiligentCore/tree/master/Platforms/Android)  | Implementation of platform-specific routines on Android |
 | [Platforms/UWP](https://github.com/DiligentGraphics/DiligentCore/tree/master/Platforms/UWP)          | Implementation of platform-specific routines on Universal Windows platform |
 | [Platforms/Win32](https://github.com/DiligentGraphics/DiligentCore/tree/master/Platforms/Win32)      | Implementation of platform-specific routines on Win32 platform |
 | [Platforms/Linux](https://github.com/DiligentGraphics/DiligentCore/tree/master/Platforms/Linux)      | Implementation of platform-specific routines on Linux platform |
 | External/glew | Cross-platform library for loading OpenGL extensions |


# API Basics

## Initializing the Engine

Before you can use any functionality provided by the engine, you need to create a render device, an immediate context and a swap chain.

### Win32
On Win32 platform, you can create OpenGL, Direct3D11 or Direct3D12 device as shown below:

```cpp
void InitDevice(HWND hWnd, 
                IRenderDevice **ppRenderDevice, 
                IDeviceContext **ppImmediateContext,  
                ISwapChain **ppSwapChain, 
                DeviceType DevType)
{
    SwapChainDesc SCDesc;
    SCDesc.SamplesCount = 1;
    switch (DevType)
    {
        case DeviceType::D3D11:
        {
            EngineD3D11Attribs DeviceAttribs;
            DeviceAttribs.DebugFlags = (Uint32)EngineD3D11DebugFlags::VerifyCommittedShaderResources |
                                       (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance;

#ifdef ENGINE_DLL
            GetEngineFactoryD3D11Type GetEngineFactoryD3D11 = nullptr;
            // Load the dll and import GetEngineFactoryD3D11() function
            LoadGraphicsEngineD3D11(GetEngineFactoryD3D11);
#endif
            auto *pFactoryD3D11 = GetEngineFactoryD3D11();
            pFactoryD3D11->CreateDeviceAndContextsD3D11( DeviceAttribs, ppRenderDevice, ppImmediateContext, 0 );
            pFactoryD3D11->CreateSwapChainD3D11( *ppRenderDevice, *ppImmediateContext, SCDesc, hWnd, ppSwapChain );
        }
        break;

        case DeviceType::D3D12:
        {
#ifdef ENGINE_DLL
            GetEngineFactoryD3D12Type GetEngineFactoryD3D12 = nullptr;
            // Load the dll and import GetEngineFactoryD3D12() function
            LoadGraphicsEngineD3D12(GetEngineFactoryD3D12);
#endif

            auto *pFactoryD3D12 = GetEngineFactoryD3D12();
            EngineD3D12Attribs EngD3D12Attribs;
            EngineD3D12Attribs.GPUDescriptorHeapDynamicSize[0] = 32768;
            EngineD3D12Attribs.GPUDescriptorHeapSize[1] = 128;
            EngineD3D12Attribs.GPUDescriptorHeapDynamicSize[1] = 2048-128;
            EngineD3D12Attribs.DynamicDescriptorAllocationChunkSize[0] = 32;
            EngineD3D12Attribs.DynamicDescriptorAllocationChunkSize[1] = 8; // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
            pFactoryD3D12->CreateDeviceAndContextsD3D12( EngD3D12Attribs, ppRenderDevice, ppImmediateContext, 0);
            pFactoryD3D12->CreateSwapChainD3D12( *ppRenderDevice, *ppImmediateContext, SCDesc, hWnd, ppSwapChain );
        }
        break;

        case DeviceType::OpenGL:
        {
#ifdef ENGINE_DLL
            // Declare function pointer
            GetEngineFactoryOpenGLType GetEngineFactoryOpenGL = nullptr;
            // Load the dll and import GetEngineFactoryOpenGL() function
            LoadGraphicsEngineOpenGL(GetEngineFactoryOpenGL);
#endif
            EngineCreationAttribs EngineCreationAttribs;
            GetEngineFactoryOpenGL()->CreateDeviceAndSwapChainGL(
                EngineCreationAttribs, ppRenderDevice, ppImmediateContext, SCDesc, hWnd, ppSwapChain );
        }
        break;

        default:
            LOG_ERROR_AND_THROW("Unknown device type");
        break;
    }
}
```

On Windows, the engine can be statically linked to the application or built as a separate DLL. In the former case, 
factory functions `GetEngineFactoryOpenGL()`, `GetEngineFactoryD3D11()`, and `GetEngineFactoryD3D12()` can be called directly. 
In the latter case, you need to load the DLL into the process's address space using `LoadGraphicsEngineOpenGL()`, 
`LoadGraphicsEngineD3D11()` or `LoadGraphicsEngineD3D12()` function. Each function loads appropriate dynamic library and 
imports the functions required to initialize the engine. You need to include the following headers:

```cpp
#include "RenderDeviceFactoryD3D11.h"
#include "RenderDeviceFactoryD3D12.h"
#include "RenderDeviceFactoryOpenGL.h"

```
You also need to add the following directories to the include search paths:

* diligentcore/Graphics/GraphicsEngineD3D11/interface
* diligentcore/Graphics/GraphicsEngineD3D12/interface
* diligentcore/Graphics/GraphicsEngineOpenGL/interface

Also, enable Diligent namespace:

```cpp
using namespace Diligent;
```

`IEngineFactoryD3D11::CreateDeviceAndContextsD3D11()` and `IEngineFactoryD3D12::CreateDeviceAndContextsD3D12()` functions can 
also create a specified number of deferred contexts, which can be used for multi-threaded command recording. 
Deferred contexts can only be created during the initialization of the engine. The function populates an array of pointers 
to the contexts, where the immediate context goes at position 0, followed by all deferred contexts.

For more details, take a look at [WinMain.cpp](https://github.com/DiligentGraphics/DiligentSamples/blob/master/Samples/SampleBase/src/Win32/WinMain.cpp) file.

### Universal Windows Platform

On Universal Windows Platform, you can create Direct3D11 or Direct3D12 device. Only static linking is 
currently supported, but dynamic linking can also be implemented. Initialization is performed the same 
way as on Win32 Platform. The difference is that you first create the render device and device contexts by 
calling `IEngineFactoryD3D11::CreateDeviceAndContextsD3D11()` or `IEngineFactoryD3D12::CreateDeviceAndContextsD3D12()`. 
The swap chain is created later by a call to `IEngineFactoryD3D11::CreateSwapChainD3D11()` or `IEngineFactoryD3D12::CreateSwapChainD3D12()`. 
Please look at the [DeviceResources.cpp](https://github.com/DiligentGraphics/DiligentSamples/blob/master/Samples/SampleBase/src/UWP/Common/DeviceResources.cpp) file for more details.

### Linux

On Linux platform, the only API currently supported is OpenGL. Initialization of GL context on Linux is tightly 
coupled with window creation. As a result, Diligent Engine does not initialize the context, but
attaches to the one initialized by the app. An example of the engine initialization on Linux can be found in
[LinuxMain.cpp](https://github.com/DiligentGraphics/DiligentSamples/blob/master/Samples/SampleBase/src/Linux/LinuxMain.cpp).

### Android

On Android, you can only create OpenGLES device. The following code snippet shows an example:

```cpp
EngineCreationAttribs EngineCreationAttribs;
RefCntAutoPtr<IRenderDevice> pRenderDevice;
SwapChainDesc SwapChainDesc;
auto pFactory = GetEngineFactoryOpenGL();
pFactory->CreateDeviceAndSwapChainGL( EngineCreationAttribs, &pRenderDevice, &pDeviceContext_, 
                                      SwapChainDesc, app_->window, &pSwapChain_ );

IRenderDeviceGLES *pRenderDeviceOpenGLES;
pRenderDevice->QueryInterface( IID_RenderDeviceGLES, reinterpret_cast<IObject**>(&pRenderDeviceOpenGLES) );
```
 
If engine is built as dynamic library, the library needs to be loaded by the native activity. The following code shows one possible way:

```java
static
{
    try{
        System.loadLibrary("GraphicsEngineOpenGL");
    } catch (UnsatisfiedLinkError e) {
        Log.e("native-activity", "Failed to load GraphicsEngineOpenGL library.\n" + e);
    }
}
```

### Attaching to Already Initialized Graphics API

An alternative way to initialize the engine is to attach to existing D3D11/D3D12 device or OpenGL/GLES context. 
Refer to [Native API interoperability](http://diligentgraphics.com/diligent-engine/native-api-interoperability/) for more details.

## Creating Resources

Device resources are created by the render device. The two main resource types are buffers, 
which represent linear memory, and textures, which use memory layouts optimized for fast filtering. 
To create a buffer, you need to populate `BufferDesc` structure and call `IRenderDevice::CreateBuffer()`. 
The following code creates a uniform (constant) buffer:

```cpp
BufferDesc BuffDesc;
BufferDesc.Name = "Uniform buffer";
BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
BuffDesc.Usage = USAGE_DYNAMIC;
BuffDesc.uiSizeInBytes = sizeof(ShaderConstants);
BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
m_pDevice->CreateBuffer( BuffDesc, BufferData(), &m_pConstantBuffer );
```

Similar, to create a texture, populate `TextureDesc` structure and call `IRenderDevice::CreateTexture()` as in the following example:

```cpp
TextureDesc TexDesc;
TexDesc.Name = "My texture 2D";
TexDesc.Type = TEXTURE_TYPE_2D;
TexDesc.Width = 1024;
TexDesc.Height = 1024;
TexDesc.Format = TEX_FORMAT_RGBA8_UNORM;
TexDesc.Usage = USAGE_DEFAULT;
TexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET | BIND_UNORDERED_ACCESS;
TexDesc.Name = "Sample 2D Texture";
m_pRenderDevice->CreateTexture( TexDesc, TextureData(), &m_pTestTex );
```

There is only one function `CreateTexture()` that is capable of creating all types of textures. Type, format, 
array size and all other parameters are specified by the members of the `TextureDesc` structure.

For every bind flag specified during the texture creation time, the texture object creates a default view. 
Default shader resource view addresses the entire texture, default render target and depth stencil views reference 
all array slices in the most detailed mip level, and unordered access view references the entire texture. To get a 
default view from the texture, use `ITexture::GetDefaultView()` function. Note that this function does not increase 
reference counter on the returned interface. You can create additional texture views using `ITexture::CreateView()`. 
Use `IBuffer::CreateView()` to create additional views of a buffer.

## Initializing Pipeline State

Diligent Engine follows Direct3D12 style to configure the graphics/compute pipeline. One big Pipelines State Object (PSO) 
encompasses all required states (all shader stages, input layout description, depth stencil, rasterizer and blend state 
descriptions etc.)

### Creating Shaders

To create a shader, populate `ShaderCreationAttribs` structure. There are two ways to create a shader. 
The first way is to provide a pointer to the shader source code through  `ShaderCreationAttribs::Source` member. 
The second way is to provide a file name. Graphics Engine is entirely decoupled from the platform. Since the host 
file system is platform-dependent, the structure exposes `ShaderCreationAttribs::pShaderSourceStreamFactory` member 
that is intended to provide the engine access to the file system. If you provided the source file name, you must 
also provide non-null pointer to the shader source stream factory. If the shader source contains any `#include` directives, 
the source stream factory will also be used to load these files. The engine provides default implementation for every 
supported platform that should be sufficient in most cases. You can however define your own implementation.

An important member is `ShaderCreationAttribs::SourceLanguage`. The following are valid values for this member:

* `SHADER_SOURCE_LANGUAGE_DEFAULT` - The shader source format matches the underlying graphics API: HLSL for D3D11 or D3D12 mode, and GLSL for OpenGL and OpenGLES modes.
* `SHADER_SOURCE_LANGUAGE_HLSL`    - The shader source is in HLSL. For OpenGL and OpenGLES modes, the source code will be converted to GLSL. See shader converter for details.
* `SHADER_SOURCE_LANGUAGE_GLSL`    - The shader source is in GLSL. There is currently no GLSL to HLSL converter.

To allow grouping of resources based on the frequency of expected change, Diligent Engine introduces 
classification of shader variables:

* Static variables (`SHADER_VARIABLE_TYPE_STATIC`) are variables that are expected to be set only once. They may not be changed once a resource is bound to the variable. Such variables are intended to hold global constants such as camera attributes or global light attributes constant buffers.
* Mutable variables (`SHADER_VARIABLE_TYPE_MUTABLE`) define resources that are expected to change on a per-material frequency. Examples may include diffuse textures, normal maps etc.
* Dynamic variables (`SHADER_VARIABLE_TYPE_DYNAMIC`) are expected to change frequently and randomly.

[This post](http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0) gives more details about the 
resource binding model in Diligent Engine. To define variable types, prepare an array of `ShaderVariableDesc` structures and 
initialize `ShaderCreationAttribs::Desc::VariableDesc` and `ShaderCreationAttribs::Desc::NumVariables`. Also 
`ShaderCreationAttribs::Desc::DefaultVariableType` can be used to set the type that will be used if variable name is not provided.

When creating a shader, textures can be assigned static samplers. If static sampler is assigned, it will always be used instead 
of the one initialized in the texture shader resource view. To initialize static samplers, prepare an array of 
`StaticSamplerDesc` structures and intialize `ShaderCreationAttribs::Desc::StaticSamplers` and 
`ShaderCreationAttribs::Desc::NumStaticSamplers`. Notice that static samplers can be assigned to texture variable of any type, 
not necessarily static. It is highly recommended to use static samplers whenever possible.

Other members of the `ShaderCreationAttribs` structure define shader include search directories, shader macro definitions, 
shader entry point and other parameters. The following is an example of shader initialization:

```cpp
ShaderCreationAttribs Attrs;
Attrs.Desc.Name = "MyPixelShader";
Attrs.FilePath = "MyShaderFile.fx";
Attrs.SearchDirectories = "shaders;shaders\\inc;";
Attrs.EntryPoint = "MyPixelShader";
Attrs.Desc.ShaderType = SHADER_TYPE_PIXEL;
Attrs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
BasicShaderSourceStreamFactory BasicSSSFactory(Attrs.SearchDirectories);
Attrs.pShaderSourceStreamFactory = &BasicSSSFactory;

ShaderVariableDesc ShaderVars[] = 
{
    {"g_StaticTexture", SHADER_VARIABLE_TYPE_STATIC},
    {"g_MutableTexture", SHADER_VARIABLE_TYPE_MUTABLE},
    {"g_DynamicTexture", SHADER_VARIABLE_TYPE_DYNAMIC}
};
Attrs.Desc.VariableDesc = ShaderVars;
Attrs.Desc.NumVariables = _countof(ShaderVars);
Attrs.Desc.DefaultVariableType = SHADER_VARIABLE_TYPE_STATIC;

StaticSamplerDesc StaticSampler;
StaticSampler.Desc.MinFilter = FILTER_TYPE_LINEAR;
StaticSampler.Desc.MagFilter = FILTER_TYPE_LINEAR;
StaticSampler.Desc.MipFilter = FILTER_TYPE_LINEAR;
StaticSampler.TextureName = "g_MutableTexture";
Attrs.Desc.NumStaticSamplers = 1;
Attrs.Desc.StaticSamplers = &StaticSampler;

ShaderMacroHelper Macros;
Macros.AddShaderMacro("USE_SHADOWS", 1);
Macros.AddShaderMacro("NUM_SHADOW_SAMPLES", 4);
Macros.Finalize();
Attrs.Macros = Macros;

RefCntAutoPtr<IShader> pShader;
m_pDevice->CreateShader( Attrs, &pShader );
```

### Creating Pipeline State Object

To create a pipeline state object, define instance of `PipelineStateDesc` structure:

```cpp
PipelineStateDesc PSODesc;
```

Describe the pipeline specifics such as if the pipeline is a compute pipeline, number and format 
of render targets as well as depth-stencil format:

```cpp
// This is a graphics pipeline
PSODesc.IsComputePipeline = false;
PSODesc.GraphicsPipeline.NumRenderTargets = 1;
PSODesc.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM_SRGB;
PSODesc.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
```

Initialize depth-stencil state description structure DepthStencilStateDesc. Note that the constructor initializes the members with default values and you can only set the ones that are different from default.

```cpp
// Init depth-stencil state
DepthStencilStateDesc &DepthStencilDesc = PSODesc.GraphicsPipeline.DepthStencilDesc;
DepthStencilDesc.DepthEnable = true;
DepthStencilDesc.DepthWriteEnable = true;
```

Initialize blend state description structure `BlendStateDesc`:

```cpp
// Init blend state
BlendStateDesc &BSDesc = PSODesc.GraphicsPipeline.BlendDesc;
BSDesc.IndependentBlendEnable = False;
auto &RT0 = BSDesc.RenderTargets[0];
RT0.BlendEnable = True;
RT0.RenderTargetWriteMask = COLOR_MASK_ALL;
RT0.SrcBlend    = BLEND_FACTOR_SRC_ALPHA;
RT0.DestBlend   = BLEND_FACTOR_INV_SRC_ALPHA;
RT0.BlendOp     =  BLEND_OPERATION_ADD;
RT0.SrcBlendAlpha   = BLEND_FACTOR_SRC_ALPHA;
RT0.DestBlendAlpha  = BLEND_FACTOR_INV_SRC_ALPHA;
RT0.BlendOpAlpha    = BLEND_OPERATION_ADD;
```

Initialize rasterizer state description structure `RasterizerStateDesc`:

```cpp
// Init rasterizer state
RasterizerStateDesc &RasterizerDesc = PSODesc.GraphicsPipeline.RasterizerDesc;
RasterizerDesc.FillMode = FILL_MODE_SOLID;
RasterizerDesc.CullMode = CULL_MODE_NONE;
RasterizerDesc.FrontCounterClockwise = True;
RasterizerDesc.ScissorEnable = True;
//RSDesc.MultisampleEnable = false; // do not allow msaa (fonts would be degraded)
RasterizerDesc.AntialiasedLineEnable = False;
```

Initialize input layout description structure `InputLayoutDesc`:

```cpp
// Define input layout
InputLayoutDesc &Layout = PSODesc.GraphicsPipeline.InputLayout;
LayoutElement TextLayoutElems[] = 
{
    LayoutElement( 0, 0, 3, VT_FLOAT32, False ),
    LayoutElement( 1, 0, 4, VT_UINT8, True ),
    LayoutElement( 2, 0, 2, VT_FLOAT32, False ),
};
Layout.LayoutElements = TextLayoutElems;
Layout.NumElements = _countof( TextLayoutElems );
```

Finally, define primitive topology type, set shaders and call `IRenderDevice::CreatePipelineState()` to create the PSO:

```cpp
// Define shader and primitive topology
PSODesc.GraphicsPipeline.PrimitiveTopologyType = PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
PSODesc.GraphicsPipeline.pVS = m_pTextVS;
PSODesc.GraphicsPipeline.pPS = m_pTextPS;

PSODesc.Name = "My pipeline state";
m_pDev->CreatePipelineState(PSODesc, &m_pPSO);
```

## Binding Shader Resources

[Shader resource binding in Diligent Engine](http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/) 
is based on grouping variables in 3 different groups (static, mutable and dynamic). Static variables are variables that are 
expected to be set only once. They may not be changed once a resource is bound to the variable. Such variables are intended 
to hold global constants such as camera attributes or global light attributes constant buffers. They are bound directly to the 
shader object:

```cpp
PixelShader->GetShaderVariable( "g_tex2DShadowMap" )->Set( pShadowMapSRV );
```

Mutable and dynamic variables are bound via a new object called Shader Resource Binding (SRB), which is created by the pipeline state 
(`IPipelineState::CreateShaderResourceBinding()`):

```cpp
m_pPSO->CreateShaderResourceBinding(&m_pSRB);
```

Dynamic and mutable resources are then bound through SRB object:

```cpp
m_pSRB->GetVariable(SHADER_TYPE_VERTEX, "tex2DDiffuse")->Set(pDiffuseTexSRV);
m_pSRB->GetVariable(SHADER_TYPE_VERTEX, "cbRandomAttribs")->Set(pRandomAttrsCB);
```

The difference between mutable and dynamic resources is that mutable ones can only be set once for every instance 
of a shader resource binding. Dynamic resources can be set multiple times. It is important to properly set the variable type as 
this may affect performance. Static variables are generally most efficient, followed by mutable. Dynamic variables are most expensive 
from performance point of view.

An alternative way to bind shader resources is to create `IResourceMapping` interface that maps resource literal names to the 
actual resources:

```cpp
ResourceMappingEntry Entries[] = { 
    { "g_Texture", pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)},
    ResourceMappingEntry() 
};
ResourceMappingDesc ResMappingDesc;
ResMappingDesc.pEntries= Entries;
RefCntAutoPtr<IResourceMapping> pResMapping;
pRenderDevice->CreateResourceMapping( ResMappingDesc, &pResMapping );
```

The resource mapping can then be used to bind all resources in a shader (`IShader::BindResources()`):

```cpp
pPixelShader->BindResources(pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);
```

in a shader resource binding (`IShaderResourceBinding::BindResources()`):

```cpp
m_pSRB->BindResources(SHADER_TYPE_VERTEX|SHADER_TYPE_PIXEL, pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);
```

or in a pipeline state (`IPipelineState::BindShaderResources()`):

```cpp
m_pPSO->BindResources(pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);
```

The last parameter to all `BindResources()` functions defines how resources should be resolved:

* `BIND_SHADER_RESOURCES_RESET_BINDINGS`    - Reset all bindings. If this flag is specified, all bindings will be reset to null before new bindings are set. By default all existing bindings are preserved.
* `BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED` - If this flag is specified, only unresolved bindings will be updated. All resolved bindings will keep their original values. If this flag is not specified, every shader variable will be updated if the mapping contains corresponding resource.
* `BIND_SHADER_RESOURCES_ALL_RESOLVED`      - If this flag is specified, all shader bindings are expected be resolved after the call. If this is not the case, debug error will be displayed.

`BindResources()` may be called several times with different resource mappings to bind resources. 
However, it is recommended to use one large resource mapping as the size of the mapping does not affect element search time.

The engine performs run-time checks to verify that correct resources are being bound. For example, if you try to bind 
a constant buffer to a shader resource view variable, an error will be output to the debug console.

## Setting the Pipeline State and Invoking Draw Command

Before any draw command can be invoked, all required vertex and index buffers as well as the pipeline state should 
be bound to the device context:

```cpp
// Clear render target
const float zero[4] = {0, 0, 0, 0};
m_pContext->ClearRenderTarget(nullptr, zero);

// Set vertex and index buffers
IBuffer *buffer[] = {m_pVertexBuffer};
Uint32 offsets[] = {0};
Uint32 strides[] = {sizeof(MyVertex)};
m_pContext->SetVertexBuffers(0, 1, buffer, strides, offsets, SET_VERTEX_BUFFERS_FLAG_RESET);
m_pContext->SetIndexBuffer(m_pIndexBuffer, 0);

m_pContext->SetPipelineState(m_pPSO);
```

Also, all shader resources must be committed to the device context. This is accomplished by 
the `IDeviceContext::CommitShaderResources()` method:

```cpp
m_pContext->CommitShaderResources(m_pSRB, COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES);
```

If the method is not called, the engine will detect that resources are not committed and output 
debug message. Note that `CommitShaderResources()` must be called after the right pipeline state has been 
bound to the context. Note that the last parameter tells the system to transition resources to the correct states. 
If this flag is not specified, the resources must be explicitly transitioned to the right states by a call to 
`IDeviceContext::TransitionShaderResources()`:

```cpp
m_pContext->TransitionShaderResources(m_pPSO, m_pSRB);
```

Note that the method requires pointer to the pipeline state that created the shader resource binding.

When all required states and resources are bound, `IDeviceContext::Draw()` can be used to execute draw 
command or `IDeviceContext::DispatchCompute()` can be used to execute compute command. Note that for a draw command, 
graphics pipeline must be bound, and for dispatch command, compute pipeline must be bound. `Draw()` takes 
`DrawAttribs` structure as an argument. The structure members define all attributes required to perform the command 
(primitive topology, number of vertices or indices, if draw call is indexed or not, if draw call is instanced or not, 
if draw call is indirect or not, etc.). For example:

```cpp
DrawAttribs attrs;
attrs.IsIndexed = true;
attrs.IndexType = VT_UINT16;
attrs.NumIndices = 36;
attrs.Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
pContext->Draw(attrs);
```

`DispatchCompute()` takes DispatchComputeAttribs structure that defines compute grid dimensions:

```cpp
m_pContext->SetPipelineState(m_pComputePSO);
m_pContext->CommitShaderResources(m_pComputeSRB, COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES);
DispatchComputeAttribs DispatchAttrs(64, 64, 8);
m_pContext->DispatchCompute(DispatchAttrs);
```

You can learn more about the engine API by looking at the [engine samples' source code](https://github.com/DiligentGraphics/DiligentSamples) and the [API Reference][1].


# Low-level API interoperability
Diligent Engine extensively supports interoperability with underlying low-level APIs. The engine can be initialized 
by attaching to existing D3D11/D3D12 device or OpenGL/GLES context and provides access to the underlying native API 
objects. Refer to the following pages for more information:

[Direct3D11 Interoperability](http://diligentgraphics.com/diligent-engine/native-api-interoperability/direct3d11-interoperability/)

[Direct3D12 Interoperability](http://diligentgraphics.com/diligent-engine/native-api-interoperability/direct3d12-interoperability/)

[OpenGL/GLES Interoperability](http://diligentgraphics.com/diligent-engine/native-api-interoperability/openglgles-interoperability/)


# References

[Diligent Engine on the Web](http://diligentgraphics.com)

[Diligent Engine on Twitter](https://twitter.com/diligentengine)

[Diligent Engine on Facebook](https://www.facebook.com/DiligentGraphics/)

[Diligent Engine Architecture](http://diligentgraphics.com/diligent-engine/architecture/)

[API Basics](http://diligentgraphics.com/diligent-engine/api-basics/)

[API Reference][1]

# Version History

## v2.1.b

* Removed legacy Visual Studio solution and project files
* Added API reference

## v2.1.a

* Refactored build system to use CMake
* Added support for Linux platform

## v2.1

### New Features

#### Core

* Interoperability with native API
** Accessing internal objects and handles
** Createing diligent engine buffers/textures from native resources
** Attaching to existing D3D11/D3D12 device or GL context
** Resource state and command queue synchronization for D3D12
* Integraion with Unity
* Geometry shader support
* Tessellation support
* Performance optimizations

#### HLSL->GLSL converter
* Support for structured buffers
* HLSL->GLSL conversion is now a two-stage process:
** Creating conversion stream
** Creating GLSL source from the stream
* Geometry shader support
* Tessellation control and tessellation evaluation shader support
* Support for non-void shader functions
* Allowing structs as input parameters for shader functions


## v2.0 (alpha)

Alpha release of Diligent Engine 2.0. The engine has been updated to take advantages of Direct3D12:

* Pipeline State Object encompasses all coarse-grain state objects like Depth-Stencil State, Blend State, Rasterizer State, shader states etc.
* New shader resource binding model implemented to leverage Direct3D12

* OpenGL and Direct3D11 backends
* Alpha release is only available on Windows platform
* Direct3D11 backend is very thoroughly optimized and has very low overhead compared to native D3D11 implementation
* Direct3D12 implementation is preliminary and not yet optimized

### v1.0.0

Initial release

# License

Licensed under the [Apache License, Version 2.0](License.txt)



**Copyright 2015-2018 Egor Yusov**

[diligentgraphics.com](http://diligentgraphics.com)

[1]: https://cdn.rawgit.com/DiligentGraphics/DiligentCore/4949ec8a/doc/html/index.html
