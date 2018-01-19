cmake_minimum_required (VERSION 3.6)

add_library(PlatformInterface INTERFACE)

target_include_directories(PlatformInterface INTERFACE interface)

set(PLATFORM_INTERFACE_HEADERS
	../interface/Atomics.h
	../interface/FileSystem.h
	../interface/PlatformDebug.h
	../interface/PlatformDefinitions.h
	../interface/PlatformMisc.h
)

add_subdirectory(Basic)

if(PLATFORM_WIN32)
	add_subdirectory(Win32)
	add_library(TargetPlatform ALIAS Win32Platform)
elseif(PLATFORM_UNIVERSAL_WINDOWS)
	add_subdirectory(UWP)
	add_library(TargetPlatform ALIAS UniversalWindowsPlatform)
elseif(PLATFORM_ANDROID)
	add_subdirectory(Android)
	add_library(TargetPlatform ALIAS AndroidPlatform)
elseif(PLATFORM_LINUX)
	add_subdirectory(Linux)
	add_library(TargetPlatform ALIAS LinuxPlatform)
elseif(PLATFORM_MACOS)
	add_subdirectory(MacOS)
	add_library(TargetPlatform ALIAS MacOSPlatform)
else()
	message(FATAL_ERROR "No PLATFORM_XXX variable defined. Make sure that 'DiligentCore' folder is processed first")
endif()
