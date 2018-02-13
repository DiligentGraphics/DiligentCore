cmake_minimum_required (VERSION 3.6)

project(AndroidPlatform CXX)

set(INCLUDE 
    include/AndroidDebug.h
    include/AndroidFileSystem.h
    include/AndroidPlatformDefinitions.h
    include/AndroidPlatformMisc.h
)

set(SOURCE 
    src/AndroidDebug.cpp
    src/AndroidFileSystem.cpp
)

add_library(AndroidPlatform ${SOURCE} ${INCLUDE} ${PLATFORM_INTERFACE_HEADERS})
set_common_target_properties(AndroidPlatform)

target_include_directories(AndroidPlatform 
PUBLIC
    include
)

target_link_libraries(AndroidPlatform 
PRIVATE 
    NDKHelper
    log
PUBLIC
    BuildSettings 
    PlatformInterface
    BasicPlatform
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${PLATFORM_INTERFACE_HEADERS})

set_target_properties(AndroidPlatform PROPERTIES
    FOLDER Core/Platforms
)
