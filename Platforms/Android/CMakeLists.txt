cmake_minimum_required (VERSION 3.6)

project(AndroidPlatform CXX)

set(INTERFACE 
    interface/AndroidDebug.h
    interface/AndroidFileSystem.h
    interface/AndroidPlatformDefinitions.h
    interface/AndroidPlatformMisc.h
)

set(SOURCE 
    src/AndroidDebug.cpp
    src/AndroidFileSystem.cpp
)

add_library(AndroidPlatform ${SOURCE} ${INTERFACE} ${PLATFORM_INTERFACE_HEADERS})
set_common_target_properties(AndroidPlatform)

target_include_directories(AndroidPlatform 
PUBLIC
    interface
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

if(INSTALL_DILIGENT_CORE)
    install_core_lib(AndroidPlatform)
endif()