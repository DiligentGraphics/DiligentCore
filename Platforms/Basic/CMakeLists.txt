cmake_minimum_required (VERSION 3.8)

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

if(MSVC)
	# Enable link-time code generation for release builds (I was not able to 
	# find any way to set these settings through interface library BuildSettings)
	set_target_properties(BasicPlatform PROPERTIES
		STATIC_LIBRARY_FLAGS_RELEASE /LTCG
		STATIC_LIBRARY_FLAGS_MINSIZEREL /LTCG
		STATIC_LIBRARY_FLAGS_RELWITHDEBINFO /LTCG
	)
endif()

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