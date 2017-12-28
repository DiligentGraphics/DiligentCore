cmake_minimum_required (VERSION 3.8)

project(UniversalWindowsPlatform CXX)

set(INCLUDE 
	include/UWPDebug.h
	include/UWPFileSystem.h
	include/UWPDefinitions.h
	include/pch.h
	../Win32/include/Win32Atomics.h
)

set(SOURCE 
	src/UWPDebug.cpp
	src/UWPFileSystem.cpp
	../Win32/src/Win32Atomics.cpp
)

add_library(UniversalWindowsPlatform ${SOURCE} ${INCLUDE} ${PLATFORM_INTERFACE_HEADERS})
set_common_target_properties(UniversalWindowsPlatform)

target_include_directories(UniversalWindowsPlatform 
PUBLIC
	../Win32/include
	include
)

target_link_libraries(UniversalWindowsPlatform 
PUBLIC
	BuildSettings 
	BasicPlatform
	PlatformInterface
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${PLATFORM_INTERFACE_HEADERS})

set_target_properties(UniversalWindowsPlatform PROPERTIES
	FOLDER Core/Platforms
)
