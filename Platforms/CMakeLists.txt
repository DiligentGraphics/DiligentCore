cmake_minimum_required (VERSION 3.3)

add_subdirectory(Basic)

if(WIN32)
	add_subdirectory(Win32)
	add_library(TargetPlatform ALIAS WindowsPlatform)
elseif(ANDROID)
	add_subdirectory(Android)
	add_library(TargetPlatform ALIAS AndroidPlatform)
endif()

set(INTERFACE
	../interface/Atomics.h
	../interface/FileSystem.h
	../interface/PlatformDebug.h
	../interface/PlatformDefinitions.h
	../interface/PlatformMisc.h
)

add_library(PlatformInterface INTERFACE)

target_include_directories(PlatformInterface INTERFACE interface)

source_group("interface" FILES ${INTERFACE})
