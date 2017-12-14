cmake_minimum_required (VERSION 3.10)

project(GraphicsEngineD3DBase CXX)

set(INCLUDE 
	include/D3DErrors.h
	include/D3DShaderResourceLoader.h
	include/D3DTypeConversionImpl.h
	include/D3DViewDescConversionImpl.h
	include/DXGITypeConversions.h
	include/HLSLDefinitions.fxh
	include/RenderDeviceD3DBase.h
	include/ShaderD3DBase.h
	include/ShaderResources.h
	include/ShaderVariableD3DBase.h
)

set(SOURCE 
	src/DXGITypeConversions.cpp
	src/ShaderD3DBase.cpp
	src/ShaderResources.cpp
)

add_library(GraphicsEngineD3DBase STATIC 
	${SOURCE} ${INCLUDE} include/HLSLDefinitions_inc.fxh
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/HLSLDefinitions_inc.fxh
    PROPERTIES GENERATED TRUE
)

# Create custom target to convert HLSLDefinitions.fxh to HLSLDefinitions_inc.fxh
add_custom_target(ProcessHLSLDefinitions
SOURCES
	include/HLSLDefinitions.fxh
)

if(PLATFORM_UNIVERSAL_WINDOWS)
	# On Universal Windows Platform, we cannot run File2String utility and will use prebuilt Win32 version
	get_target_property(FILE2STRING_SOURCE_DIR File2String SOURCE_DIR)
	set(FILE2STRING_PATH "${FILE2STRING_SOURCE_DIR}/File2String.exe")
else()
	add_dependencies(ProcessHLSLDefinitions File2String)
	set(FILE2STRING_PATH $<TARGET_FILE:File2String>)
endif()

add_custom_command(TARGET ProcessHLSLDefinitions 
				   # Unfortunately it is not possible to set TARGET directly to GraphicsEngineD3DBase
				   # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
				   # PRE_BUILD is treated as PRE_LINK.
                   COMMAND ${FILE2STRING_PATH} include/HLSLDefinitions.fxh include/HLSLDefinitions_inc.fxh
				   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   COMMENT "Processing HLSLDefinitions.fxh"
                   VERBATIM
)

add_dependencies(GraphicsEngineD3DBase ProcessHLSLDefinitions)

target_include_directories(GraphicsEngineD3DBase
PUBLIC
	include
	interface
PRIVATE
	../GraphicsTools/include
)

target_link_libraries(GraphicsEngineD3DBase 
PRIVATE 
	BuildSettings 
	GraphicsEngine 
	PlatformInterface 
	Common
)
set_common_target_properties(GraphicsEngineD3DBase)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("generated" FILES include/HLSLDefinitions_inc.fxh)

set_target_properties(GraphicsEngineD3DBase PROPERTIES
	FOLDER Core/Graphics
)
set_target_properties(ProcessHLSLDefinitions PROPERTIES
	FOLDER Core/Graphics/Helper
)