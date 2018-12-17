cmake_minimum_required (VERSION 3.8)

project(UniversalWindowsPlatform CXX)

set(INTERFACE 
    interface/UWPDebug.h
    interface/UWPFileSystem.h
    interface/UWPDefinitions.h
    ../Win32/interface/Win32Atomics.h
)

set(SOURCE 
    src/UWPDebug.cpp
    src/UWPFileSystem.cpp
    ../Win32/src/Win32Atomics.cpp
)

add_library(UniversalWindowsPlatform ${SOURCE} ${INTERFACE} ${PLATFORM_INTERFACE_HEADERS})
set_common_target_properties(UniversalWindowsPlatform)

target_include_directories(UniversalWindowsPlatform 
PUBLIC
    ../Win32/interface
    interface
)

target_link_libraries(UniversalWindowsPlatform 
PUBLIC
    BuildSettings 
    BasicPlatform
    PlatformInterface
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${PLATFORM_INTERFACE_HEADERS})

set_target_properties(UniversalWindowsPlatform PROPERTIES
    FOLDER Core/Platforms
)

if(INSTALL_DILIGENT_CORE)
    install_core_lib(UniversalWindowsPlatform)
endif()
