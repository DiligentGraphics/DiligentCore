cmake_minimum_required (VERSION 3.6)

project(GraphicsTools CXX)

set(INCLUDE 
	include/BasicShaderSourceStreamFactory.h
	include/CommonlyUsedStates.h
	include/GraphicsUtilities.h
	include/pch.h
	include/ShaderMacroHelper.h
	include/RingBuffer.h
	include/TextureUploader.h
	include/TextureUploaderBase.h
	include/VariableSizeAllocationsManager.h
	include/VariableSizeGPUAllocationsManager.h
)

set(SOURCE 
	src/BasicShaderSourceStreamFactory.cpp
	src/GraphicsUtilities.cpp
	src/pch.cpp
	src/TextureUploader.cpp
)

if(D3D11_SUPPORTED)
	list(APPEND SOURCE src/TextureUploaderD3D11.cpp)
	list(APPEND INCLUDE include/TextureUploaderD3D11.h)
endif()

if(D3D12_SUPPORTED)
	list(APPEND SOURCE src/TextureUploaderD3D12.cpp)
	list(APPEND INCLUDE include/TextureUploaderD3D12.h)
endif()

if(GL_SUPPORTED OR GLES_SUPPORTED)
	list(APPEND SOURCE src/TextureUploaderGL.cpp)
	list(APPEND INCLUDE include/TextureUploaderGL.h)
endif()

add_library(GraphicsTools STATIC ${SOURCE} ${INCLUDE})

target_include_directories(GraphicsTools 
PUBLIC
	include
PRIVATE
	../GraphicsEngineD3D11/interface
	../GraphicsEngineD3D12/interface
	../GraphicsEngineD3DBase/include
	../GraphicsEngineOpenGL/interface
	../GraphicsEngine/interface
	../TextureLoader/interface
)

target_link_libraries(GraphicsTools 
PRIVATE 
	Common 
	BuildSettings 
	PlatformInterface
)

set_common_target_properties(GraphicsTools)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(GraphicsTools PROPERTIES
	FOLDER Core/Graphics
)
