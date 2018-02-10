
# GraphicsEngineD3D11

Implementation of Diligent Engine API using Direct3D11

# Initialization

The following code snippet shows how to initialize diligent engine in D3D11 mode.

```cpp
#include "RenderDeviceFactoryD3D11.h"
using namespace Diligent;

// ... 

EngineD3D11Attribs DeviceAttribs;
DeviceAttribs.DebugFlags = 
    (Uint32)EngineD3D11DebugFlags::VerifyCommittedShaderResources |
    (Uint32)EngineD3D11DebugFlags::VerifyCommittedResourceRelevance;

// Get pointer to the function that returns the factory
#if ENGINE_DLL
    GetEngineFactoryD3D11Type GetEngineFactoryD3D11 = nullptr;
    // Load the dll and import GetEngineFactoryD3D11() function
    LoadGraphicsEngineD3D11(GetEngineFactoryD3D11);
#endif
auto *pFactoryD3D11 = GetEngineFactoryD3D11();

RefCntAutoPtr<IRenderDevice> pRenderDevice;
RefCntAutoPtr<IDeviceContext> pImmediateContext;
SwapChainDesc SwapChainDesc;
RefCntAutoPtr<ISwapChain> pSwapChain;
pFactoryD3D11->CreateDeviceAndContextsD3D11( DeviceAttribs, &pRenderDevice, &pImmediateContext, 0 );
pFactoryD3D11->CreateSwapChainD3D11( pRenderDevice, pImmediateContext, SwapChainDesc, hWnd, &pSwapChain );
```

Alternatively, the engine can be initialized by attaching to existing D3D11 device and immediate context (see below).

# Interoperability with Direct3D11

Diligent Engine exposes methods to access internal D3D11 objects, is able to create diligent engine buffers 
and textures from existing Direct3D11 buffers and textures, and can be initialized by attaching to existing D3D11 
device and immediate context.

## Accessing Native D3D11 objects

Below are some of the methods that provide access to internal D3D11 objects:

|                              Function                                       |                              Description                                                                      |
|-----------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------|
| `ID3D11Buffer *IBufferD3D11::GetD3D11Buffer()`                              | returns a pointer to the ID3D11Buffer interface of the internal Direct3D11 buffer object                      |
| `ID3D11Resource* ITextureD3D11::GetD3D11Texture()`                          | returns a pointer to the ID3D11Resource interface of the internal Direct3D11 texture object                   |
| `ID3D11View* IBufferViewD3D11()::GetD3D11View()`                            | returns a pointer to the ID3D11View interface of the internal d3d11 object representing the buffer view       |
| `ID3D11View* ITextureViewD3D11::GetD3D11View()`                             | returns a pointer to the ID3D11View interface of the internal d3d11 object representing the texture view      |
| `ID3D11Device* IRenderDeviceD3D11::GetD3D11Device()`                        | returns a pointer to the native D3D11 device object                                                           |
| `ID3D11DeviceContext* IDeviceContextD3D11::GetD3D11DeviceContext()`         | returns a pointer to the native ID3D11DeviceContext object                                                    |

## Creating Diligent Engine Objects from D3D11 Resources

* `void IRenderDeviceD3D11::CreateBufferFromD3DResource(ID3D11Buffer *pd3d11Buffer, const BufferDesc& BuffDesc, IBuffer **ppBuffer)` - 
   creates a diligent engine buffer object from the native d3d11 buffer
* `void IRenderDeviceD3D11::CreateTextureFromD3DResource(ID3D11Texture1D *pd3d11Texture, ITexture **ppTexture)` - 
   create a diligent engine texture object from the native D3D11 1D texture
* `void IRenderDeviceD3D11::CreateTextureFromD3DResource(ID3D11Texture2D *pd3d11Texture, ITexture **ppTexture)` - 
   create a diligent engine texture object from the native D3D11 2D texture
* `void IRenderDeviceD3D11::CreateTextureFromD3DResource(ID3D11Texture3D *pd3d11Texture, ITexture **ppTexture)` - 
   create a diligent engine texture object from the native D3D11 3D texture

## Initializing the Engine by Attaching to Existing D3D11 Device and Immediate Context

The code snippet below shows how diligent engine can be attached to D3D11 device returned by Unity

```cpp
IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
ID3D11Device* d3d11NativeDevice = d3d->GetDevice();
CComPtr<ID3D11DeviceContext> d3d11ImmediateContext;
d3d11NativeDevice->GetImmediateContext(&d3d11ImmediateContext);
auto *pFactoryD3d11 = GetEngineFactoryD3D11();
EngineD3D11Attribs Attribs;
pFactoryD3d11->AttachToD3D11Device(d3d11NativeDevice, d3d11ImmediateContext, Attribs, &m_Device, &m_Context, 0);
```

For more information about interoperability with D3D11, please visit [Diligent Engine web site](http://diligentgraphics.com/diligent-engine/native-api-interoperability/direct3d11-interoperability/)

# References

[Diligent Engine on the Web](http://diligentgraphics.com/diligent-engine)

[Interoperability with Direct3D11](http://diligentgraphics.com/diligent-engine/native-api-interoperability/direct3d11-interoperability/)

[Architecture of D3D11-based implementation](http://diligentgraphics.com/diligent-engine/architecture/d3d11)

# Release Notes

## 2.1

### New features

* Interoperability with Direct3D11
  - Accessing internal D3D11 objects and handles
  - Createing diligent engine buffers/textures from D3D11 resources
  - Attaching to existing D3D11 device
* Integraion with Unity
* Geometry shader support
* Tessellation support
* Support for structured buffers in HSLS shaders

### API Changes

* Updated map interface: removed MAP_WRITE_DISCARD and MAP_WRITE_NO_OVERWRITE map types and added MAP_FLAG_DISCARD and MAP_FLAG_DO_NOT_SYNCHRONIZE flags instead

### Bug fixes

* Fixed issue with unaligned uniform buffers

## 2.0

Reworked the API to follow D3D12 style

## 1.0

Initial release



**Copyright 2015-2018 Egor Yusov**

[diligentgraphics.com](http://diligentgraphics.com)