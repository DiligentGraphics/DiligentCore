cmake_minimum_required (VERSION 3.6)

project(Diligent-BasicPlatform CXX)

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

add_library(Diligent-BasicPlatform STATIC ${SOURCE} ${INTERFACE})
set_common_target_properties(Diligent-BasicPlatform)

target_include_directories(Diligent-BasicPlatform 
PUBLIC
    interface
)

target_link_libraries(Diligent-BasicPlatform 
PUBLIC
    Diligent-BuildSettings 
    Diligent-Primitives
)

source_group("src" FILES ${SOURCE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(Diligent-BasicPlatform PROPERTIES
    FOLDER DiligentCore/Platforms
)


if(INSTALL_DILIGENT_CORE)
    install_core_lib(Diligent-BasicPlatform)
    get_target_relative_dir(Diligent-BasicPlatform BASIC_PLATFORM_DIR)
    install(DIRECTORY    "../interface"
            DESTINATION  "headers/${BASIC_PLATFORM_DIR}/../"
    )
endif()