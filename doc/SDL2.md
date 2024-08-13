# SDL2 Integration

## General

Create flags and set attributes for SDL2 relevant for your target backend.
```cpp
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_WindowFlags flags = {};

    auto window_width = 1280;
    auto window_height = 720;
```

E.g. set flag for Vulkan
```cpp
    flags = flags | SDL_WINDOW_VULKAN
```

Or set flag and attributes for OpenGL 4.6
```cpp
    flags = flags | SDL_WINDOW_OPENGL
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
```
    
No flag needed for DirectX

Create the window
```cpp
    auto window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, flags);
```

Create GL Context if using OpenGL
```cpp
    auto gl_context = SDL_GL_CreateContext(window);
    // Check if succeeded, etc.
```
    
If necessary, get a handle to the platform's native window, then set it in the Diligent NativeWindow struct. Example for Windows:
```cpp
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(window, &info);
    NativeWindow native_window;
    native_window.hWnd = info.info.win.window;
```
    
Finally setup the Diligent swapchain as normal (using the NativeWindow if needed)

### Handling window events

Depending on your platform, certain window events may require making some sort of call into your graphics engine.
```cpp
SDL_Event event;
while (true)
{
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_WINDOWEVENT:
            {
                switch (event.window.event) 
                {
                    case SDL_WINDOWEVENT_FOCUS_LOST: 
                    {
                        // Suspend or destroy swapchain
                        break;
                    }
                    case SDL_WINDOWEVENT_FOCUS_GAINED: 
                    {
                        // Resume or recreate swapchain
                        break;
                    }
                    case SDL_WINDOWEVENT_RESIZED:
                    {
                        // Handle window resize
                        break;
                    }
                }
                break;
            }
        }
    }
}
```

## Mac OSX

Set the Vulkan and HighDPI flags when creating window
```cpp
auto window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
```


Copy [SurfaceHelper.mm](https://github.com/DiligentGraphics/DiligentSamples/blob/master/Samples/GLFWDemo/src/SurfaceHelper.mm) file from the GLFW sample into your project.
Modify the following from:
```objective-c
#define GLFW_EXPOSE_NATIVE_COCOA 1
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

// ...

void* GetNSWindowView(GLFWwindow* wnd)
{
    id Window = glfwGetCocoaWindow(wnd);

    // ...
}
```
to:
```objective-c
#include <SDL.h>
#include <SDL_syswm.h>
#include <Foundation/Foundation.h>
#include <Cocoa/Cocoa.h>

// ...

void* GetNSWindowView(SDL_Window* wnd)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(wnd, &info);
    
    // ...
}
```

Use the helper function to get the NSWindowView, setting it in the NativeWindow struct. Then create the Vulkan swapchain as normal.

## Android

### Project template

Follow the [official documentation for setting up an android project](https://github.com/libsdl-org/SDL/blob/main/docs/README-android.md) for SDL2.

#### Optional: Handle devices with screen notch in your derived SDLActivity class:
```java
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        }
    }
```

### Building Diligent Engine separately

The following is a very rough guide of building Diligent separately to then be used in your own project.

Build Diligent Engine using CMake with an official NDK toolchain:
```bash
mkdir DiligentEngine_android_build
cd DiligentEngine_android_build
cmake -G"Ninja" \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DCMAKE_SYSTEM_NAME="Android" \
    -DCMAKE_SYSTEM_VERSION=19 \
    -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
    -DCMAKE_ANDROID_NDK=$ANDROID_NDK_HOME \
    -DANDROID_APP_PLATFORM=android-19 \
    -DANDROID_STL=c++_static \
    -DANDROID_NDK=$ANDROID_NDK_HOME \
    -DANDROID_TOOLCHAIN=clang \
    -DANDROID_PLATFORM=android-19 \
    -DANDROID_ABI=arm64-v8a \
    ../DiligentEngine
ninja -j9
```

Setup your build system to be able to find the interface headers and built libraries. Here the variables `DILIGENT_PATH` and `DILIGENT_BUILD_PATH` refer to the locations of your root DiligentEngine repo copy and your build folder respectively.
```cmake
target_include_directories(app
    PRIVATE
    ${DILIGENT_PATH}/DiligentCore
    ${DILIGENT_PATH}/DiligentFX
    ${DILIGENT_PATH}/DiligentTools)

target_link_directories(app
    PRIVATE
    ${DILIGENT_BUILD_PATH}/DiligentCore/Common
    ${DILIGENT_BUILD_PATH}/DiligentCore/Graphics/Archiver
    ${DILIGENT_BUILD_PATH}/DiligentCore/Graphics/GraphicsAccessories
    ${DILIGENT_BUILD_PATH}/DiligentCore/Graphics/GraphicsEngine
    ${DILIGENT_BUILD_PATH}/DiligentCore/Graphics/GraphicsEngineOpenGL
    ${DILIGENT_BUILD_PATH}/DiligentCore/Graphics/GraphicsEngineVulkan
    ${DILIGENT_BUILD_PATH}/DiligentCore/Graphics/GraphicsTools
    ${DILIGENT_BUILD_PATH}/DiligentCore/Graphics/HLSL2GLSLConverterLib
    ${DILIGENT_BUILD_PATH}/DiligentCore/Graphics/ShaderTools
    ${DILIGENT_BUILD_PATH}/DiligentCore/Platforms/Android
    ${DILIGENT_BUILD_PATH}/DiligentCore/Platforms/Basic
    ${DILIGENT_BUILD_PATH}/DiligentCore/Primitives
    ${DILIGENT_BUILD_PATH}/DiligentCore/ThirdParty/SPIRV-Cross
    ${DILIGENT_BUILD_PATH}/DiligentCore/ThirdParty/SPIRV-Tools/source
    ${DILIGENT_BUILD_PATH}/DiligentCore/ThirdParty/SPIRV-Tools/source/diff
    ${DILIGENT_BUILD_PATH}/DiligentCore/ThirdParty/SPIRV-Tools/source/opt
    ${DILIGENT_BUILD_PATH}/DiligentCore/ThirdParty/glslang/SPIRV
    ${DILIGENT_BUILD_PATH}/DiligentCore/ThirdParty/glslang/glslang
    ${DILIGENT_BUILD_PATH}/DiligentCore/ThirdParty/glslang/glslang/OSDependent/Unix
    ${DILIGENT_BUILD_PATH}/DiligentCore/ThirdParty/xxHash/cmake_unofficial
    ${DILIGENT_BUILD_PATH}/DiligentFX
    ${DILIGENT_BUILD_PATH}/DiligentTools/AssetLoader
    ${DILIGENT_BUILD_PATH}/DiligentTools/Imgui
    ${DILIGENT_BUILD_PATH}/DiligentTools/RenderStateNotation
    ${DILIGENT_BUILD_PATH}/DiligentTools/TextureLoader
    ${DILIGENT_BUILD_PATH}/DiligentTools/ThirdParty
    ${DILIGENT_BUILD_PATH}/DiligentTools/ThirdParty/libjpeg-9e
    ${DILIGENT_BUILD_PATH}/DiligentTools/ThirdParty/libpng
    ${DILIGENT_BUILD_PATH}/DiligentTools/ThirdParty/libtiff)

target_link_libraries(app
    PRIVATE
    Diligent-AndroidPlatform
    Diligent-Archiver-static
    Diligent-AssetLoader
    Diligent-BasicPlatform
    Diligent-Common
    Diligent-GraphicsAccessories
    Diligent-GraphicsEngine
    Diligent-GraphicsEngineOpenGL-static
    Diligent-GraphicsEngineVK-static
    Diligent-GraphicsTools
    Diligent-HLSL2GLSLConverterLib
    Diligent-Imgui
    Diligent-Primitives
    Diligent-RenderStateNotation
    Diligent-ShaderTools
    Diligent-TextureLoader
    DiligentFX
    EGL
    GLESv3
    GenericCodeGen
    LibJpeg
    LibTiff
    MachineIndependent
    OSDependent
    SPIRV
    SPIRV-Tools
    SPIRV-Tools-diff
    SPIRV-Tools-opt
    ZLib
    android
    glslang
    glslang-default-resource-limits
    png16
    spirv-cross-core
    spirv-cross-glsl
    xxhash)
```
  
### Vulkan

Set the Vulkan and fullscreen / resizable window flags when creating the window. 
```cpp
auto window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN);
```

Then create the swapchain as normal. Keep the swapchain description to be used.
```cpp
auto *pFactoryVk = GetEngineFactoryVk();

// ...

// Keep the SCDesc to be reused
pFactoryVk->CreateSwapChainVk(*device, *immediate_context, SCDesc,
    native_window, swapchain);
```

#### Recreate swapchain

When the app goes out of focus, destroy the swapchain
```cpp
if (device->GetDeviceInfo().IsVulkanDevice()) {
    swapchain.Release();
    device->IdleGPU();
}
```

When the app regains focus, create the swapchain again
```cpp
if (device->GetDeviceInfo().IsVulkanDevice())
{
    auto *pFactoryVk = GetEngineFactoryVk();

    // ...

    // Reuse the SCDesc from the original
    pFactoryVk->CreateSwapChainVk(*device, *immediate_context, SCDesc,
        native_window, swapchain);
}
```

### OpenGLES

Set the attributes Diligent expects, set the OpenGL and fullscreen / resizable window flags, create the window, create the context, and finally create the swapchain
```cpp
const auto color_size = 8;
const auto depth_size = 24;
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, color_size);
SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, color_size);
SDL_GL_SetAttribute(SDL_GL_RED_SIZE, color_size);
SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, color_size);
SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depth_size);
SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
flags = flags | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN;

// ...

// Create the window
auto window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, flags);

// ...

// Create the gl context
auto gl_context = SDL_GL_CreateContext(window);

// Create the opengl swapchain. Note that the android native window pointer is not required.
// ...
```

When focus is lost, call suspend on the swapchain
```cpp
if (device->GetDeviceInfo().IsGLDevice())
{
    static_cast<IRenderDeviceGLES*>(device.RawPtr())->Suspend();
}
```

When focus is resumed, call resume on the swapchain (Native window pointer not required)
```cpp
if (device->GetDeviceInfo().IsGLDevice())
{
    static_cast<IRenderDeviceGLES*>(device.RawPtr())->Resume(nullptr);
}
```

### Handling orientation changes and window resizes

- Look at [GetSurfacePretransformMatrix()](https://github.com/DiligentGraphics/DiligentSamples/blob/9be93225a7fdf135583146c1175c232217f310b2/SampleBase/src/SampleBase.cpp#L140) for an example of handling the view matrix.
- `SDL_GetDisplayOrientation()` can be used to get the device's orientation. Alternatively, listen for the `SDL_DISPLAYEVENT_ORIENTATION` event.
- `SDL_GetDisplayMode()` can be used to determine max resolution.
