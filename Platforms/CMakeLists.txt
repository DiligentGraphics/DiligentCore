cmake_minimum_required (VERSION 3.6)

add_library(PlatformInterface INTERFACE)

target_include_directories(PlatformInterface INTERFACE interface)

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
else()
	message(FATAL_ERROR "No PLATFORM_XXX variable defined. Make sure that 'DiligentCore' folder is processed first")
endif()
