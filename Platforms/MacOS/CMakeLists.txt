cmake_minimum_required (VERSION 3.6)

project(MacOSPlatform CXX)

set(INCLUDE 
	include/MacOSDebug.h
	include/MacOSFileSystem.h
	include/MacOSPlatformDefinitions.h
	include/MacOSPlatformMisc.h
)

set(SOURCE 
	src/MacOSDebug.cpp
	src/MacOSFileSystem.cpp
)

add_library(MacOSPlatform ${SOURCE} ${INCLUDE} ${PLATFORM_INTERFACE_HEADERS})
set_common_target_properties(MacOSPlatform)

target_include_directories(MacOSPlatform 
PUBLIC
	include
)

target_link_libraries(MacOSPlatform 
PUBLIC
	BuildSettings 
	BasicPlatform
	PlatformInterface
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${PLATFORM_INTERFACE_HEADERS})

set_target_properties(MacOSPlatform PROPERTIES
	FOLDER Core/Platforms
)
