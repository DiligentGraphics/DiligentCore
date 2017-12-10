cmake_minimum_required (VERSION 3.3)

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

if(MSVC)
	# Enable link-time code generation for release builds (I was not able to 
	# find any way to set these settings through interface library BuildSettings)
	set_target_properties(HLSL2GLSLConverterLib PROPERTIES
		STATIC_LIBRARY_FLAGS_RELEASE /LTCG
		STATIC_LIBRARY_FLAGS_MINSIZEREL /LTCG
		STATIC_LIBRARY_FLAGS_RELWITHDEBINFO /LTCG
	)
endif()

# Create custom target to convert GLSLDefinitions.h to GLSLDefinitions_inc.h
add_custom_target(ProcessGLSLDefinitions 
SOURCES
	include/GLSLDefinitions.h
)
add_dependencies(ProcessGLSLDefinitions File2String)

add_custom_command(TARGET ProcessGLSLDefinitions 
				   # Unfortunately it is not possible to set TARGET directly to GraphicsEngineD3DBase
				   # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
				   # PRE_BUILD is treated as PRE_LINK.
                   COMMAND COMMAND $<TARGET_FILE:File2String> include/GLSLDefinitions.h include/GLSLDefinitions_inc.h
				   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   COMMENT "Processing GLSLDefinitions.h"
                   VERBATIM
)

add_dependencies(HLSL2GLSLConverterLib ProcessGLSLDefinitions)


source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})
source_group("generated" FILES include/GLSLDefinitions_inc.h)

set_target_properties(HLSL2GLSLConverterLib PROPERTIES
	FOLDER Core/Graphics
)
set_target_properties(ProcessGLSLDefinitions PROPERTIES
	FOLDER Core/Graphics/Helper
)