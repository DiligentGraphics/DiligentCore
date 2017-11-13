
# GraphicsEngineD3D12

Implementation of Diligent Engine API using Direct3D12

# Initialization

The following code snippet shows how to initialize diligent engine in D3D12 mode.

```cpp
#include "RenderDeviceFactoryD3D12.h"
using namespace Diligent;

// ... 
#ifdef ENGINE_DLL
	GetEngineFactoryD3D12Type GetEngineFactoryD3D12 = nullptr;
	// Load the dll and import GetEngineFactoryD3D12() function
	LoadGraphicsEngineD3D12(GetEngineFactoryD3D12);
#endif
auto *pFactoryD3D11 = GetEngineFactoryD3D12();
EngineD3D12Attribs EngD3D12Attribs;
EngD3D12Attribs.CPUDescriptorHeapAllocationSize[0] = 1024;
EngD3D12Attribs.CPUDescriptorHeapAllocationSize[1] = 32;
EngD3D12Attribs.CPUDescriptorHeapAllocationSize[2] = 16;
EngD3D12Attribs.CPUDescriptorHeapAllocationSize[3] = 16;
EngD3D12Attribs.NumCommandsToFlushCmdList = 64;
RefCntAutoPtr<IRenderDevice> pRenderDevice;
RefCntAutoPtr<IDeviceContext> pImmediateContext;
SwapChainDesc SwapChainDesc;
RefCntAutoPtr<ISwapChain> pSwapChain;
pFactoryD3D11->CreateDeviceAndContextsD3D12( EngD3D12Attribs, &pRenderDevice, &pImmediateContext, 0 );
pFactoryD3D11->CreateSwapChainD3D12( pRenderDevice, pImmediateContext, SwapChainDesc, hWnd, &pSwapChain );
```

Alternatively, the engine can be initialized by attaching to existing D3D12 device (see below).

# Interoperability with Direct3D12

Diligent Engine exposes methods to access internal D3D12 objects, is able to create diligent engine buffers 
and textures from existing Direct3D12 resources, and can be initialized by attaching to existing D3D12 
device and provide synchronization tools.

## Accessing Native D3D12 Resources

Below are some of the methods that provide access to internal D3D12 objects:

|                              Function                                       |                              Description                                                                      |
|-----------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------|
| `ID3D12Resource* IBufferD3D12::GetD3D12Buffer(size_t &DataStartByteOffset, Uint32 ContextId)` | returns a pointer to the ID3D12Resource interface of the internal Direct3D12 buffer object. Note that dynamic buffers are suballocated from dynamic heap, and every context has its own dynamic heap. Offset from the beginning of the dynamic heap for a context identified by ContextId is returned in DataStartByteOffset parameter |
| `void IBufferD3D12::SetD3D12ResourceState(D3D12_RESOURCE_STATES state)`                        | sets the buffer usage state. This method should be used when an application transitions the buffer to inform diligent engine about the current usage state |
| `D3D12_CPU_DESCRIPTOR_HANDLE IBufferViewD3D12::GetCPUDescriptorHandle()`                      | returns CPU descriptor handle of the buffer view |
| `ID3D12Resource* ITextureD3D12::GetD3D12Texture()`                                            |  returns a pointer to the ID3D12Resource interface of the internal Direct3D12 texture object |
| `void ITextureD3D12::SetD3D12ResourceState(D3D12_RESOURCE_STATES state)`                      | sets the texture usage state. This method should be used when an application transitions the texture to inform diligent engine about the current usage state |
| `D3D12_CPU_DESCRIPTOR_HANDLE ITextureViewD3D12::GetCPUDescriptorHandle()`                     | returns CPU descriptor handle of the texture view |
| `void IDeviceContextD3D12::TransitionTextureState(ITexture *pTexture, D3D12_RESOURCE_STATES State)` | transitions internal D3D12 texture object to a specified state |
| `void IDeviceContextD3D12::TransitionBufferState(IBuffer *pBuffer, D3D12_RESOURCE_STATES State)`    | transitions internal D3D12 buffer object to a specified state |
| `ID3D12PipelineState* IPipelineStateD3D12::GetD3D12PipelineState()`                           | returns ID3D12PipelineState interface of the internal D3D12 pipeline state object object |
| `ID3D12RootSignature* IPipelineStateD3D12::GetD3D12RootSignature()`                           | returns a pointer to the root signature object associated with this pipeline state |
| `D3D12_CPU_DESCRIPTOR_HANDLE ISamplerD3D12::GetCPUDescriptorHandle()`                         | returns a CPU descriptor handle of the D3D12 sampler object |
| `ID3D12Device* IRenderDeviceD3D12::GetD3D12Device()`                                          | returns ID3D12Device interface of the internal Direct3D12 device object |

## Synchronization Tools

| `Uint64 IRenderDeviceD3D12::GetNextFenceValue()`              | returns the fence value that will be signaled by the GPU command queue when the next command list is submitted for execution |
| `Bool IRenderDeviceD3D12::IsFenceSignaled(Uint64 FenceValue)` | checks if the fence value has been signaled by the GPU. True means that all associated work has been finished |
| `void IRenderDeviceD3D12::FinishFrame()`                      |  this method should be called at the end of the frame when attached to existing D3D12 device. Otherwise the method is automatically called before present |

## Creating Diligent Engine Objects from D3D12 Resources

* `void IRenderDeviceD3D12::CreateTextureFromD3DResource(ID3D12Resource *pd3d12Texture, ITexture **ppTexture)` - 
   creates a diligent engine texture object from native D3D12 resource.
* `void IRenderDeviceD3D12::CreateBufferFromD3DResource(ID3D12Resource *pd3d12Buffer, const BufferDesc& BuffDesc, IBuffer **ppBuffer)` - 
   creates a diligent engine buffer object from native D3D12 resource.
   The method takes a pointer to the native d3d12 resiyrce pd3d12Buffer, buffer description BuffDesc and writes a pointer to the IBuffer 
   interface at the memory location pointed to by ppBuffer. The system can recover buffer size, but the rest of the fields of 
   BuffDesc structure need to be populated by the client as they cannot be recovered from d3d12 resource description.


## Initializing the Engine by Attaching to Existing D3D12 Device

To attach diligent engine to existing D3D12 device, use the following factory function:

```cpp
void IEngineFactoryD3D12::AttachToD3D12Device(void *pd3d12NativeDevice,
                                              class ICommandQueueD3D12 *pCommandQueue,
                                              const EngineD3D12Attribs& EngineAttribs,
                                              IRenderDevice **ppDevice,
                                              IDeviceContext **ppContexts,
                                              Uint32 NumDeferredContexts);
```

The method takes a pointer to the native D3D12 device `pd3d12NativeDevice`, initialization parameters `EngineAttribs`, 
and returns diligent engine device interface in `ppDevice`, and diligent engine contexts in `ppContexts`. Pointer to the 
immediate goes at position 0. If `NumDeferredContexts` > 0, pointers to deferred contexts go afterwards. 
The function also takes a pointer to the command queue object `pCommandQueue`, which needs to implement 
`ICommandQueueD3D12` interface.

For more information about interoperability with D3D12, please visit [Diligent Engine web site](http://diligentgraphics.com/diligent-engine/native-api-interoperability/direct3d12-interoperability/)

# References

[Diligent Engine](http://diligentgraphics.com/diligent-engine)

[Interoperability with Direct3D12](http://diligentgraphics.com/diligent-engine/native-api-interoperability/direct3d12-interoperability/)

[Architecture of D3D12-based implementation](http://diligentgraphics.com/diligent-engine/architecture/D3D12)

# Release Notes

## 2.1

### New fatures

* Interoperability with Direct3D12
** Accessing internal D3D12 objects and handles
** Createing diligent engine buffers/textures from D3D12 resources
** Attaching to existing D3D12 device
** Resource state and command queue synchronization
* Integraion with Unity
* Geometry shader support
* Tessellation support
* Support for dynamic buffers with SRV/UAV bind flags
* Support for structured buffers in HSLS shaders
* Performance optimizations

### API Changes

* Updated map interface: removed MAP_WRITE_DISCARD and MAP_WRITE_NO_OVERWRITE map types and added MAP_FLAG_DISCARD and MAP_FLAG_DO_NOT_SYNCHRONIZE flags instead

### Bug fixes

* Issues with deferred contexts
  - Never flush deferred D3D12 context
  - Invalidate context state from FinishCommandList() and ExecuteCommandList()
  - Not binding default RTV & DSV for deferred contexts
* Issues with idling GPU
* Object relase bug: when releasing last reference to a device object, render device was destroyed before the object was fully released
* Fixed mipmap generation for SRGB textures
* Fixed mipmap generation for texture arrays


## 2.0

Reworked the API to follow D3D12 style

## 1.0

Initial release



**Copyright 2015-2017 Egor Yusov**

[Diligent Graphics](http://diligentgraphics.com)
