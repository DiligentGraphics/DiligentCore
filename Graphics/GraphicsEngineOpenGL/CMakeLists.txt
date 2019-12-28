cmake_minimum_required (VERSION 3.3)

project(Diligent-GraphicsEngineOpenGL CXX)

set(INCLUDE 
    include/AsyncWritableResource.h
    include/BufferGLImpl.h
    include/BufferViewGLImpl.h
    include/DeviceContextGLImpl.h 
    include/FBOCache.h
    include/FenceGLImpl.h
    include/GLContext.h
    include/GLContextState.h
    include/GLObjectWrapper.h
    include/GLProgramResourceCache.h
    include/GLPipelineResourceLayout.h
    include/GLProgramResources.h
    include/GLTypeConversions.h
    include/pch.h
    include/PipelineStateGLImpl.h
    include/RenderDeviceGLImpl.h
    include/SamplerGLImpl.h
    include/ShaderGLImpl.h
    include/ShaderResourceBindingGLImpl.h
    include/SwapChainGLBase.h
    include/TexRegionRender.h
    include/Texture1D_OGL.h
    include/Texture1DArray_OGL.h
    include/Texture2D_OGL.h
    include/Texture2DArray_OGL.h
    include/Texture3D_OGL.h
    include/TextureBaseGL.h
    include/TextureCube_OGL.h
    include/TextureCubeArray_OGL.h
    include/TextureViewGLImpl.h
    include/VAOCache.h
)

set(INTERFACE 
    interface/BaseInterfacesGL.h
    interface/BufferGL.h
    interface/BufferViewGL.h
    interface/DeviceContextGL.h
    interface/EngineFactoryOpenGL.h
    interface/FenceGL.h
    interface/PipelineStateGL.h
    interface/RenderDeviceGL.h
    interface/SamplerGL.h
    interface/ShaderGL.h
    interface/ShaderResourceBindingGL.h
    interface/SwapChainGL.h
    interface/TextureGL.h
    interface/TextureViewGL.h
)

set(SOURCE 
    src/BufferGLImpl.cpp
    src/BufferViewGLImpl.cpp
    src/DeviceContextGLImpl.cpp
    src/EngineFactoryOpenGL.cpp
    src/FBOCache.cpp
    src/FenceGLImpl.cpp
    src/GLContextState.cpp
    src/GLObjectWrapper.cpp
    src/GLProgramResourceCache.cpp
    src/GLPipelineResourceLayout.cpp
    src/GLProgramResources.cpp
    src/GLTypeConversions.cpp
    src/PipelineStateGLImpl.cpp
    src/RenderDeviceGLImpl.cpp
    src/SamplerGLImpl.cpp
    src/ShaderGLImpl.cpp
    src/ShaderResourceBindingGLImpl.cpp
    src/TexRegionRender.cpp
    src/Texture1D_OGL.cpp
    src/Texture1DArray_OGL.cpp
    src/Texture2D_OGL.cpp
    src/Texture2DArray_OGL.cpp
    src/Texture3D_OGL.cpp
    src/TextureBaseGL.cpp
    src/TextureCube_OGL.cpp
    src/TextureCubeArray_OGL.cpp
    src/TextureViewGLImpl.cpp
    src/VAOCache.cpp
)

if(PLATFORM_WIN32)
    list(APPEND SOURCE src/GLContextWindows.cpp)
    list(APPEND SOURCE src/SwapChainGLImpl.cpp)
    list(APPEND INCLUDE include/GLContextWindows.h)
    list(APPEND INCLUDE include/SwapChainGLImpl.h)
elseif(PLATFORM_ANDROID)
    list(APPEND SOURCE src/GLContextAndroid.cpp)
    list(APPEND SOURCE src/RenderDeviceGLESImpl.cpp)
    list(APPEND SOURCE src/GLStubsAndroid.cpp)
    list(APPEND SOURCE src/SwapChainGLImpl.cpp)

    list(APPEND INCLUDE include/GLContextAndroid.h)
    list(APPEND INCLUDE include/GLStubsAndroid.h)
    list(APPEND INCLUDE include/RenderDeviceGLESImpl.h)
    list(APPEND INCLUDE include/SwapChainGLImpl.h)

    list(APPEND INTERFACE interface/RenderDeviceGLES.h)
elseif(PLATFORM_LINUX)
    list(APPEND SOURCE src/GLContextLinux.cpp)
    list(APPEND SOURCE src/SwapChainGLImpl.cpp)
    list(APPEND INCLUDE include/GLContextLinux.h)
    list(APPEND INCLUDE include/SwapChainGLImpl.h)
elseif(PLATFORM_MACOS)
    list(APPEND SOURCE src/GLContextMacOS.mm)
    list(APPEND INCLUDE include/GLContextMacOS.h)
    list(APPEND SOURCE src/SwapChainGLImpl.cpp)
    list(APPEND INCLUDE include/SwapChainGLImpl.h)
elseif(PLATFORM_IOS)
    list(APPEND SOURCE src/GLContextIOS.mm)
    list(APPEND INCLUDE include/GLContextIOS.h)
    list(APPEND INCLUDE include/GLStubsIOS.h)
    list(APPEND SOURCE src/SwapChainGLIOS.mm)
    list(APPEND INCLUDE include/SwapChainGLIOS.h)
else()
    message(FATAL_ERROR "Unknown platform")
endif()

add_library(Diligent-GraphicsEngineOpenGLInterface INTERFACE)
target_include_directories(Diligent-GraphicsEngineOpenGLInterface
INTERFACE
    interface
)
target_link_libraries(Diligent-GraphicsEngineOpenGLInterface 
INTERFACE 
    Diligent-GraphicsEngineInterface
)


add_library(Diligent-GraphicsEngineOpenGL-static STATIC 
    ${SOURCE} ${INTERFACE} ${INCLUDE}
    readme.md
)

add_library(Diligent-GraphicsEngineOpenGL-shared SHARED 
    readme.md
)
if(MSVC)
    target_sources(Diligent-GraphicsEngineOpenGL-shared 
    PRIVATE	
        src/DLLMain.cpp
        src/GraphicsEngineOpenGL.def
    )
endif()

target_include_directories(Diligent-GraphicsEngineOpenGL-static
PRIVATE
    include
    ../HLSL2GLSLConverterLib/include
)


set(PRIVATE_DEPENDENCIES 
    Diligent-BuildSettings 
    Diligent-Common 
    Diligent-TargetPlatform
    Diligent-GraphicsEngine
    Diligent-GLSLTools
    Diligent-HLSL2GLSLConverterLib
)

set(PUBLIC_DEPENDENCIES 
    Diligent-GraphicsEngineOpenGLInterface
)

if(PLATFORM_WIN32)
    set(PRIVATE_DEPENDENCIES ${PRIVATE_DEPENDENCIES} glew-static opengl32.lib)
elseif(PLATFORM_ANDROID)
    set(PRIVATE_DEPENDENCIES ${PRIVATE_DEPENDENCIES} GLESv3 EGL android)
elseif(PLATFORM_LINUX)
    set(PRIVATE_DEPENDENCIES ${PRIVATE_DEPENDENCIES} glew-static)
elseif(PLATFORM_MACOS)
    find_package(OpenGL REQUIRED)
    set(PRIVATE_DEPENDENCIES ${PRIVATE_DEPENDENCIES} glew-static ${OPENGL_LIBRARY})
elseif(PLATFORM_IOS)
    set(PRIVATE_DEPENDENCIES ${PRIVATE_DEPENDENCIES} ${OPENGLES})
else()
    message(FATAL_ERROR "Unknown platform")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Disable the following clang warning
    #    '<function name>' hides overloaded virtual function
    # as hiding is intended
    target_compile_options(Diligent-GraphicsEngineOpenGL-static PRIVATE -Wno-overloaded-virtual)
    target_compile_options(Diligent-GraphicsEngineOpenGL-shared PRIVATE -Wno-overloaded-virtual)
endif()

target_link_libraries(Diligent-GraphicsEngineOpenGL-static
PRIVATE
    ${PRIVATE_DEPENDENCIES}
PUBLIC
    ${PUBLIC_DEPENDENCIES}
)
target_link_libraries(Diligent-GraphicsEngineOpenGL-shared
PRIVATE
    Diligent-BuildSettings
    ${WHOLE_ARCHIVE_FLAG} Diligent-GraphicsEngineOpenGL-static ${NO_WHOLE_ARCHIVE_FLAG}
PUBLIC
    ${PUBLIC_DEPENDENCIES}
)

target_compile_definitions(Diligent-GraphicsEngineOpenGL-shared PUBLIC ENGINE_DLL=1)

if(PLATFORM_WIN32)

    # Do not add 'lib' prefix when building with MinGW
    set_target_properties(Diligent-GraphicsEngineOpenGL-shared PROPERTIES PREFIX "")

    # Set output name to GraphicsEngineOpenGL_{32|64}{r|d}
    set_dll_output_name(Diligent-GraphicsEngineOpenGL-shared GraphicsEngineOpenGL)

else()
    set_target_properties(Diligent-GraphicsEngineOpenGL-shared PROPERTIES
        OUTPUT_NAME GraphicsEngineOpenGL
    )
endif()

set_common_target_properties(Diligent-GraphicsEngineOpenGL-shared)
set_common_target_properties(Diligent-GraphicsEngineOpenGL-static)

source_group("src" FILES ${SOURCE})
if(PLATFORM_WIN32)
    source_group("dll" FILES 
        src/DLLMain.cpp
        src/GraphicsEngineOpenGL.def
    )
endif()

source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(Diligent-GraphicsEngineOpenGL-static PROPERTIES
    FOLDER DiligentCore/Graphics
)
set_target_properties(Diligent-GraphicsEngineOpenGL-shared PROPERTIES
    FOLDER DiligentCore/Graphics
)

set_source_files_properties(
    readme.md PROPERTIES HEADER_FILE_ONLY TRUE
)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-GraphicsEngineOpenGL-shared)
    install_core_lib(Diligent-GraphicsEngineOpenGL-static)
endif()
