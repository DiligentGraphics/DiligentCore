cmake_minimum_required (VERSION 3.3)

project(WindowsPlatform CXX)

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

add_library(Win32Platform ${SOURCE} ${INCLUDE} ${PLATFORM_INTERFACE_HEADERS})
set_common_target_properties(Win32Platform)

target_include_directories(Win32Platform 
PUBLIC
    include
)

target_link_libraries(Win32Platform 
PUBLIC
    BuildSettings 
    BasicPlatform
    PlatformInterface
PRIVATE
    Shlwapi.lib
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${PLATFORM_INTERFACE_HEADERS})

set_target_properties(Win32Platform PROPERTIES
    FOLDER Core/Platforms
)
