cmake_minimum_required (VERSION 3.6)

project(HLSL2GLSLConverterLib CXX)

set(INCLUDE 
	include/GLSLDefinitions.h
	include/HLSL2GLSLConverterImpl.h
	include/HLSL2GLSLConverterObject.h
	include/HLSLKeywords.h
)

set(INTERFACE 
	interface/HLSL2GLSLConverter.h
)

set(SOURCE 
	src/HLSL2GLSLConverterImpl.cpp
	src/HLSL2GLSLConverterObject.cpp
)

add_library(HLSL2GLSLConverterLib STATIC ${SOURCE} ${INTERFACE} ${INCLUDE} include/GLSLDefinitions_inc.h)
set_common_target_properties(HLSL2GLSLConverterLib)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/GLSLDefinitions_inc.h
    PROPERTIES GENERATED TRUE
)

target_include_directories(HLSL2GLSLConverterLib 
PUBLIC 
	interface
PRIVATE
	include
)

target_link_libraries(HLSL2GLSLConverterLib 
PRIVATE 
	BuildSettings 
	Common 
	PlatformInterface 
	GraphicsEngine 
	GraphicsTools
)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
	# Create custom target to convert GLSLDefinitions.h to GLSLDefinitions_inc.h
	add_custom_target(ProcessGLSLDefinitions 
	SOURCES
		include/GLSLDefinitions.h
	)

	if(PLATFORM_WIN32)
		set(FILE2STRING_PATH $<TARGET_FILE:File2String>)
		add_dependencies(ProcessGLSLDefinitions File2String)	
	else()
		# We cannot run File2String utility and will use prebuilt Win32 version
		set(FILE2STRING_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/File2Include/bin/Win32/File2String.exe")
	endif()

	add_custom_command(TARGET ProcessGLSLDefinitions 
					   # Unfortunately it is not possible to set TARGET directly to GraphicsEngineD3DBase
					   # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
					   # PRE_BUILD is treated as PRE_LINK.
					   COMMAND COMMAND ${FILE2STRING_PATH} include/GLSLDefinitions.h include/GLSLDefinitions_inc.h
					   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
					   COMMENT "Processing GLSLDefinitions.h"
					   VERBATIM
	)

	add_dependencies(HLSL2GLSLConverterLib ProcessGLSLDefinitions)
	set_target_properties(ProcessGLSLDefinitions PROPERTIES
		FOLDER Core/Graphics/Helper
	)
endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})
source_group("generated" FILES include/GLSLDefinitions_inc.h)

set_target_properties(HLSL2GLSLConverterLib PROPERTIES
	FOLDER Core/Graphics
)
