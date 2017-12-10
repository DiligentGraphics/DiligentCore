cmake_minimum_required (VERSION 3.3)

project(glew-static)

set(SOURCE 
	src/glew.c
)

set(INCLUDE 
	include/GL/glew.h
	include/GL/wglew.h
)

add_library(glew-static STATIC ${SOURCE} ${INCLUDE})

target_compile_definitions(glew-static 
PUBLIC
	-DGLEW_STATIC
)

target_include_directories(glew-static 
PRIVATE
	include
INTERFACE
	include/GL
)

source_group("source" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})

target_link_libraries(glew-static PRiVATE BuildSettings)

if(MSVC)
	# Enable link-time code generation for release builds (I was not able to 
	# find any way to set these settings through interface library BuildSettings)
	set_target_properties(glew-static PROPERTIES
		STATIC_LIBRARY_FLAGS_RELEASE /LTCG
		STATIC_LIBRARY_FLAGS_MINSIZEREL /LTCG
		STATIC_LIBRARY_FLAGS_RELWITHDEBINFO /LTCG
	)
endif()

set_target_properties(glew-static PROPERTIES
	FOLDER Core/External
)
