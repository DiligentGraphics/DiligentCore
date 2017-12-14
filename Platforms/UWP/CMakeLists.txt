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

add_library(UniversalWindowsPlatform ${SOURCE} ${INCLUDE})

target_include_directories(UniversalWindowsPlatform 
PUBLIC
	include
)

target_link_libraries(UniversalWindowsPlatform 
PRIVATE 
	BuildSettings 
	Common 
PUBLIC
	PlatformInterface
	BasicPlatform
)

if(MSVC)
	# Enable link-time code generation for release builds (I was not able to 
	# find any way to set these settings through interface library BuildSettings)
	set_target_properties(UniversalWindowsPlatform PROPERTIES
		STATIC_LIBRARY_FLAGS_RELEASE /LTCG
		STATIC_LIBRARY_FLAGS_MINSIZEREL /LTCG
		STATIC_LIBRARY_FLAGS_RELWITHDEBINFO /LTCG
	)
endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})

set_target_properties(UniversalWindowsPlatform PROPERTIES
	FOLDER Core/Platforms
)
