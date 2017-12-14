cmake_minimum_required (VERSION 3.8)

project(GraphicsEngineD3D11 CXX)


set(INCLUDE 
	include/BufferD3D11Impl.h
	include/BufferViewD3D11Impl.h
	include/CommandListD3D11Impl.h
	include/D3D11DebugUtilities.h
	include/D3D11TypeConversions.h
	include/D3D11TypeDefinitions.h
	include/DeviceContextD3D11Impl.h
	include/EngineD3D11Defines.h
	include/pch.h
	include/PipelineStateD3D11Impl.h
	include/RenderDeviceD3D11Impl.h
	include/SamplerD3D11Impl.h
	include/ShaderD3D11Impl.h
	include/ShaderResourceBindingD3D11Impl.h
	include/ShaderResourceCacheD3D11.h
	include/ShaderResourceLayoutD3D11.h
	include/ShaderResourcesD3D11.h
	include/SwapChainD3D11Impl.h
	include/targetver.h
	include/Texture1D_D3D11.h
	include/Texture2D_D3D11.h
	include/Texture3D_D3D11.h
	include/TextureBaseD3D11.h
	include/TextureViewD3D11Impl.h
)

set(INTERFACE 
	interface/BufferD3D11.h
	interface/BufferViewD3D11.h
	interface/DeviceContextD3D11.h
	interface/EngineD3D11Attribs.h
	interface/PipelineStateD3D11.h
	interface/RenderDeviceD3D11.h
	interface/RenderDeviceFactoryD3D11.h
	interface/SamplerD3D11.h
	interface/ShaderD3D11.h
	interface/ShaderResourceBindingD3D11.h
	interface/SwapChainD3D11.h
	interface/TextureD3D11.h
	interface/TextureViewD3D11.h
)


set(SRC 
	src/BufferD3D11Impl.cpp
	src/BufferViewD3D11Impl.cpp
	src/CommandListD3D11Impl.cpp
	src/D3D11DebugUtilities.cpp
	src/D3D11TypeConversions.cpp
	src/DeviceContextD3D11Impl.cpp
	src/PipelineStateD3D11Impl.cpp
	src/RenderDeviceD3D11Impl.cpp
	src/RenderDeviceFactoryD3D11.cpp
	src/SamplerD3D11Impl.cpp
	src/ShaderD3D11Impl.cpp
	src/ShaderResourceBindingD3D11Impl.cpp
	src/ShaderResourceCacheD3D11.cpp
	src/ShaderResourceLayoutD3D11.cpp
	src/ShaderResourcesD3D11.cpp
	src/SwapChainD3D11Impl.cpp
	src/Texture1D_D3D11.cpp
	src/Texture2D_D3D11.cpp
	src/Texture3D_D3D11.cpp
	src/TextureBaseD3D11.cpp
	src/TextureViewD3D11Impl.cpp
)

add_library(GraphicsEngineD3D11-static STATIC 
	${SRC} ${INTERFACE} ${INCLUDE}
	readme.md
)

add_library(GraphicsEngineD3D11-shared SHARED 
	${SRC} ${INTERFACE} ${INCLUDE}
	src/DLLMain.cpp
	src/GraphicsEngineD3D11.def
	readme.md
)

set_target_properties(GraphicsEngineD3D11-shared PROPERTIES
	OUTPUT_NAME_DEBUG GraphicsEngineD3D11${DLL_DBG_SUFFIX}
	OUTPUT_NAME_RELEASE GraphicsEngineD3D11${DLL_REL_SUFFIX}
	OUTPUT_NAME_RELWITHDEBINFO GraphicsEngineD3D11${DLL_REL_SUFFIX}
	OUTPUT_NAME_MINSIZEREL GraphicsEngineD3D11${DLL_REL_SUFFIX}
)

if(MSVC)
	# Enable link-time code generation for release builds (I was not able to 
	# find any way to set these settings through interface library BuildSettings)
	set_target_properties(GraphicsEngineD3D11-shared PROPERTIES
		LINK_FLAGS_RELEASE /LTCG
		LINK_FLAGS_MINSIZEREL /LTCG
		LINK_FLAGS_RELWITHDEBINFO /LTCG
	)
	set_target_properties(GraphicsEngineD3D11-static PROPERTIES
		STATIC_LIBRARY_FLAGS_RELEASE /LTCG
		STATIC_LIBRARY_FLAGS_MINSIZEREL /LTCG
		STATIC_LIBRARY_FLAGS_RELWITHDEBINFO /LTCG
	)
endif()

target_include_directories(GraphicsEngineD3D11-static 
PUBLIC
	interface
PRIVATE
	include
)

target_include_directories(GraphicsEngineD3D11-shared
PUBLIC
	interface
PRIVATE
	include
)

set(PRIVATE_DEPENDENCIES 
	BuildSettings 
	Common
	GraphicsEngineD3DBase 
	GraphicsTools
	TargetPlatform
	d3d11.lib 
	d3dcompiler.lib
)

set(PUBLIC_DEPENDENCIES 
	GraphicsEngine 
)

target_link_libraries(GraphicsEngineD3D11-static PRIVATE ${PRIVATE_DEPENDENCIES} PUBLIC ${PUBLIC_DEPENDENCIES})

target_link_libraries(GraphicsEngineD3D11-shared PRIVATE ${PRIVATE_DEPENDENCIES} PUBLIC ${PUBLIC_DEPENDENCIES})
target_compile_definitions(GraphicsEngineD3D11-shared PUBLIC ENGINE_DLL)

source_group("src" FILES ${SRC} )

source_group("dll" FILES 
	src/DLLMain.cpp
	src/GraphicsEngineD3D11.def
)

source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(GraphicsEngineD3D11-static PROPERTIES
	FOLDER Core/Graphics
)
set_target_properties(GraphicsEngineD3D11-shared PROPERTIES
	FOLDER Core/Graphics
)

set_source_files_properties(
    readme.md PROPERTIES HEADER_FILE_ONLY TRUE
)
