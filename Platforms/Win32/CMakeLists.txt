cmake_minimum_required (VERSION 3.3)

project(WindowsPlatform)

set(INCLUDE 
	include/Win32Atomics.h
	include/Win32Debug.h
	include/Win32FileSystem.h
	include/Win32PlatformDefinitions.h
	include/Win32PlatformMisc.h
)

set(SOURCE 
	src/Win32Atomics.cpp
	src/Win32Debug.cpp
	src/Win32FileSystem.cpp
)

add_library(Win32Platform ${SOURCE} ${INCLUDE})

target_include_directories(Win32Platform 
PUBLIC
	include
)

target_link_libraries(Win32Platform 
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
	set_target_properties(Win32Platform PROPERTIES
		STATIC_LIBRARY_FLAGS_RELEASE /LTCG
		STATIC_LIBRARY_FLAGS_MINSIZEREL /LTCG
		STATIC_LIBRARY_FLAGS_RELWITHDEBINFO /LTCG
	)
endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})

set_target_properties(Win32Platform PROPERTIES
	FOLDER Core/Platforms
)
