cmake_minimum_required (VERSION 3.6)

project(LinuxPlatform CXX)

set(INCLUDE 
    include/LinuxDebug.h
    include/LinuxFileSystem.h
    include/LinuxPlatformDefinitions.h
    include/LinuxPlatformMisc.h
)

set(SOURCE 
    src/LinuxDebug.cpp
    src/LinuxFileSystem.cpp
)

add_library(LinuxPlatform ${SOURCE} ${INCLUDE} ${PLATFORM_INTERFACE_HEADERS})
set_common_target_properties(LinuxPlatform)

target_include_directories(LinuxPlatform 
PUBLIC
    include
)

target_link_libraries(LinuxPlatform 
PUBLIC
    BuildSettings 
    BasicPlatform
    PlatformInterface
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${PLATFORM_INTERFACE_HEADERS})

set_target_properties(LinuxPlatform PROPERTIES
    FOLDER Core/Platforms
)
