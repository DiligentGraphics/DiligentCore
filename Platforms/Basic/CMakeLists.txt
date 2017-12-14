cmake_minimum_required (VERSION 3.10)

project(BasicPlatform CXX)

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

set(PLATFORM_INTERFACE_SRC
	../interface/Atomics.h
	../interface/FileSystem.h
	../interface/PlatformDebug.h
	../interface/PlatformDefinitions.h
	../interface/PlatformMisc.h
)

add_library(BasicPlatform STATIC ${SOURCE} ${INCLUDE} ${PLATFORM_INTERFACE_SRC})
set_common_target_properties(BasicPlatform)

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

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${PLATFORM_INTERFACE_SRC})

set_target_properties(BasicPlatform PROPERTIES
	FOLDER Core/Platforms
)