cmake_minimum_required (VERSION 3.6)

project(BasicPlatform CXX)

set(SOURCE 
    src/BasicFileSystem.cpp
    src/BasicPlatformDebug.cpp
)

set(INTERFACE 
    interface/BasicAtomics.h
    interface/BasicFileSystem.h
    interface/BasicPlatformDebug.h
    interface/BasicPlatformMisc.h
    interface/DebugUtilities.h
)

if(PLATFORM_LINUX OR PLATFORM_WIN32 OR PLATFORM_MACOS OR PLATFORM_IOS)
    list(APPEND SOURCE src/StandardFile.cpp)
    list(APPEND INTERFACE interface/StandardFile.h)
endif()

add_library(BasicPlatform STATIC ${SOURCE} ${INTERFACE})
set_common_target_properties(BasicPlatform)

target_include_directories(BasicPlatform 
PUBLIC
    interface
)

target_link_libraries(BasicPlatform 
PUBLIC
    BuildSettings 
    Primitives
)

source_group("src" FILES ${SOURCE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(BasicPlatform PROPERTIES
    FOLDER Core/Platforms
)
