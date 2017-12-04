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
