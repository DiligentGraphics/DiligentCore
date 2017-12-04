cmake_minimum_required (VERSION 3.3)

project(HLSL2GLSLConverterLib)

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
	include/GLSLDefinitions_inc.h
)

set_source_files_properties(
    include/GLSLDefinitions_inc.h
    PROPERTIES GENERATED TRUE
)

add_library(HLSL2GLSLConverterLib STATIC ${SOURCE} ${INTERFACE} ${INCLUDE})

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


# Create custom target to convert GLSLDefinitions.h to GLSLDefinitions_inc.h
add_custom_target(ProcessGLSLDefinitions)
add_dependencies(ProcessGLSLDefinitions File2String)

add_custom_command(TARGET ProcessGLSLDefinitions 
				   # Unfortunately it is not possible to set TARGET directly to GraphicsEngineD3DBase
				   # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
				   # PRE_BUILD is treated as PRE_LINK.
                   COMMAND ${CMAKE_CURRENT_BINARY_DIR}/../../Utilities/File2Include/File2String.exe include/GLSLDefinitions.h include/GLSLDefinitions_inc.h
				   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   COMMENT "Processing GLSLDefinitions.h"
                   VERBATIM
)

add_dependencies(HLSL2GLSLConverterLib ProcessGLSLDefinitions)


source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})
