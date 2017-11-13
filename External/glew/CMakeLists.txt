cmake_minimum_required (VERSION 3.3)

project(glew-static)

add_definitions(-DGLEW_STATIC)


set(SOURCE 
	src/glew.c
)

set(INCLUDE 
	include/GL/glew.h
	include/GL/wglew.h
)

include_directories(
	include
)

add_library(
	glew-static ${SOURCE} ${INCLUDE}
)

source_group("source" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
