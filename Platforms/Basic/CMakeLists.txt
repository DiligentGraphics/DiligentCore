cmake_minimum_required (VERSION 3.3)

project(BasicPlatform)

set(SOURCE 
	src/BasicFileSystem.cpp
)

set(INCLUDE 
	include/BasicAtomics.h
	include/BasicFileSystem.h
	include/BasicPlatformDebug.h
	include/BasicPlatformMisc.h
	include/pch.h
)

add_library(BasicPlatform STATIC ${SOURCE} ${INCLUDE})

target_include_directories(BasicPlatform 
PUBLIC
	include
)

target_link_libraries(BasicPlatform 
PRIVATE 
	BuildSettings 
	Common 
	PlatformInterface
)

source_group("source" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})

set_target_properties(BasicPlatform PROPERTIES
	FOLDER Core/Platforms
)