cmake_minimum_required (VERSION 3.3)

project(GraphicsEngineOpenGL)

set(INCLUDE 
	include/AsyncWritableResource.h
	include/BufferGLImpl.h
	include/BufferViewGLImpl.h
	include/DeviceContextGLImpl.h 
	include/FBOCache.h
	include/GLContext.h
	include/GLContextAndroid.h
	include/GLContextState.h
	include/GLContextWindows.h
	include/GLObjectWrapper.h
	include/GLProgram.h
	include/GLProgramResources.h
	include/GLStubs.h
	include/GLTypeConversions.h
	include/PipelineStateGLImpl.h
	include/RenderDeviceGLESImpl.h
	include/RenderDeviceGLImpl.h
	include/SamplerGLImpl.h
	include/ShaderGLImpl.h
	include/ShaderResourceBindingGLImpl.h
	include/SwapChainGLImpl.h
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
	interface/PipelineStateGL.h
	interface/RenderDeviceFactoryOpenGL.h
	interface/RenderDeviceGL.h
	interface/RenderDeviceGLES.h
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
	src/FBOCache.cpp
	src/GLContextState.cpp
	src/GLObjectWrapper.cpp
	src/GLProgram.cpp
	src/GLProgramResources.cpp
	src/GLTypeConversions.cpp
	src/PipelineStateGLImpl.cpp
	src/RenderDeviceFactoryOpenGL.cpp
	src/RenderDeviceGLImpl.cpp
	src/SamplerGLImpl.cpp
	src/ShaderGLImpl.cpp
	src/ShaderResourceBindingGLImpl.cpp
	src/SwapChainGLImpl.cpp
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
if(WIN32)
	list(APPEND SOURCE src/GLContextWindows.cpp)
	list(APPEND SOURCE src/RenderDeviceGLImpl.cpp)
elseif(ANRDOID)
	list(APPEND SOURCE src/GLContextAndroid.cpp)
	list(APPEND SOURCE src/RenderDeviceGLESImpl.cpp)
	list(APPEND SOURCE src/GLStubs.cpp)
endif()


add_library(GraphicsEngineOpenGL-static STATIC 
	${SOURCE} ${INTERFACE} ${INCLUDE}
)

add_library(GraphicsEngineOpenGL-shared SHARED 
	${SOURCE} ${INTERFACE} ${INCLUDE}
	src/DLLMain.cpp
	src/GraphicsEngineOpenGL.def
)

target_include_directories(GraphicsEngineOpenGL-static
PUBLIC
	include
	interface
PRIVATE
	../HLSL2GLSLConverterLib/include
)

target_include_directories(GraphicsEngineOpenGL-shared
PUBLIC
	include
	interface
PRIVATE
	../HLSL2GLSLConverterLib/include
)


set(PRIVATE_DEPENDENCIES 
	BuildSettings 
	glew-static 
	PlatformInterface 
	Common 
	GraphicsEngine 
	GraphicsTools 
	glew-static
	opengl32.lib
	TargetPlatform
)

set(PUBLIC_DEPENDENCIES 
	HLSL2GLSLConverterLib
)

target_link_libraries(GraphicsEngineOpenGL-static PRIVATE ${PRIVATE_DEPENDENCIES} PUBLIC ${PUBLIC_DEPENDENCIES})
target_link_libraries(GraphicsEngineOpenGL-shared PRIVATE ${PRIVATE_DEPENDENCIES} PUBLIC ${PUBLIC_DEPENDENCIES})
target_compile_definitions(GraphicsEngineOpenGL-shared PUBLIC ENGINE_DLL)

set_target_properties(GraphicsEngineOpenGL-shared PROPERTIES
	OUTPUT_NAME_DEBUG GraphicsEngineOpenGL${DLL_DBG_SUFFIX}
	OUTPUT_NAME_RELEASE GraphicsEngineOpenGL${DLL_REL_SUFFIX}
	OUTPUT_NAME_RELWITHDEBINFO GraphicsEngineOpenGL${DLL_REL_SUFFIX}
	OUTPUT_NAME_MINSIZEREL GraphicsEngineOpenGL${DLL_REL_SUFFIX}
)

source_group("src" FILES 
	${SOURCE}
	src/DLLMain.cpp
)

source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})
