
# GraphicsEngineOpenGL

Implementation of Diligent Engine API using OpenGL/GLES

# Initialization

The following code snippet shows how to initialize diligent engine in OpenGL/GLES mode.

```cpp
#include "RenderDeviceFactoryOpenGL.h"
using namespace Diligent;

// ... 

#ifdef ENGINE_DLL
    GetEngineFactoryOpenGLType GetEngineFactoryOpenGL;
    if( !LoadGraphicsEngineOpenGL(GetEngineFactoryOpenGL) )
        return FALSE;
#endif
RefCntAutoPtr<IRenderDevice> pRenderDevice;
RefCntAutoPtr<IDeviceContext> pImmediateContext;
SwapChainDesc SwapChainDesc;
RefCntAutoPtr<ISwapChain> pSwapChain;
GetEngineFactoryOpenGL()->CreateDeviceAndSwapChainGL(
    EngineCreationAttribs(), &pRenderDevice, &pImmediateContext, SwapChainDesc, hWnd, &pSwapChain );
```

Alternatively, the engine can be initialized by attaching to existing OpenGL context (see [below](#initializing-the-engine-by-attaching-to-existing-gl-context)).

# Interoperability with OpenGL/GLES

Diligent Engine exposes methods to access internal OpenGL/GLES objects, is able to create diligent engine buffers 
and textures from existing GL buffer and texture handles, and can be initialized by attaching to existing GL 
context.

## Accessing Native GL objects

Below are some of the methods that provide access to internal D3D11 objects:

|                       Function                    |                              Description                                                                      |
|---------------------------------------------------|---------------------------------------------------------------------------------------------------------------|
| `GLuint IBufferGL::GetGLBufferHandle()`           | returns GL buffer handle                    |
| `bool IDeviceContextGL::UpdateCurrentGLContext()` | attaches to the active GL context in the thread. Returns false if there is no active context, and true otherwise.  If an application uses multiple GL contexts, this method must be called before any other command to let the engine update the active context. |
| `GLuint ITextureGL::GetGLTextureHandle()`         | returns GL texture handle                    |
| `GLenum ITextureGL::GetBindTarget()`              | returns GL texture bind target               |

## Creating Diligent Engine Objects from OpenGL Handles

* `void IRenderDeviceGL::CreateTextureFromGLHandle(Uint32 GLHandle, const TextureDesc &TexDesc, ITexture **ppTexture)`  - 
	creates a diligent engine texture from OpenGL handle. The method takes OpenGL handle GLHandle, texture description TexDesc, 
	and writes the pointer to the created texture object at the memory address pointed to by ppTexture. The engine can automatically 
	set texture width, height, depth, mip levels count, and format, but the remaining field of TexDesc structure must be populated by 
	the application. Note that diligent engine texture object does not take ownership of the GL resource, and the application must 
	not destroy it while it is in use by the engine.
* `void IRenderDeviceGL::CreateBufferFromGLHandle(Uint32 GLHandle, const BufferDesc &BuffDesc, IBuffer **ppBuffer)` -
    creates a diligent engine buffer from OpenGL handle. The method takes OpenGL handle GLHandle, buffer description BuffDesc, 
    and writes the pointer to the created buffer object at the memory address pointed to by ppBuffer. The engine can automatically 
    set the buffer size, but the rest of the fields need to be set by the client. Note that diligent engine buffer object does not 
    take ownership of the GL resource, and the application must not destroy it while it is in use by the engine.

## Initializing the Engine by Attaching to Existing GL Context

The code snippet below shows how diligent engine can be attached to existing GL context

```cpp
auto *pFactoryGL = GetEngineFactoryOpenGL();
EngineCreationAttribs Attribs;
pFactoryGL->AttachToActiveGLContext(Attribs, &m_Device, &m_Context);
```

For more information about interoperability with OpenGL, please visit [Diligent Engine web site](http://diligentgraphics.com/diligent-engine/native-api-interoperability/openglgles-interoperability/)

# References

[Diligent Engine](http://diligentgraphics.com/diligent-engine)

[Interoperability with OpenGL/GLES](http://diligentgraphics.com/diligent-engine/native-api-interoperability/openglgles-interoperability/)

# Release Notes

## 2.1

### New features

* Interoperability with OpenGL/GLES
  - Accessing GL handles of internal texture/buffer objects
  - Createing diligent engine buffers/textures from OpenGL handles
  - Attaching to existing OpenGL context
* Integraion with Unity
* Geometry shader support
* Tessellation support
* Support ofr multiple GL contexts: VAO, FBO & Program Pipelines are created and cached for multiple native contexts. 
  `IDeviceContextGL::UpdateCurrentGLContext()` sets the active GL context in the thread

### API Changes

* Updated map interface: removed MAP_WRITE_DISCARD and MAP_WRITE_NO_OVERWRITE map types and added MAP_FLAG_DISCARD and MAP_FLAG_DO_NOT_SYNCHRONIZE flags instead

## 2.0

Reworked the API to follow D3D12 style

## 1.0

Initial release



**Copyright 2015-2018 Egor Yusov**

[diligentgraphics.com](http://diligentgraphics.com)