cmake_minimum_required (VERSION 3.6)

project(ApplePlatform CXX)

set(INTERFACE
    interface/CFObjectWrapper.h
    interface/AppleDebug.h
    interface/AppleFileSystem.h
    interface/ApplePlatformDefinitions.h
    interface/ApplePlatformMisc.h
)

set(SOURCE 
    src/AppleDebug.mm
    src/AppleFileSystem.cpp
)


add_library(ApplePlatform ${SOURCE} ${INTERFACE} ${PLATFORM_INTERFACE_HEADERS})
set_common_target_properties(ApplePlatform)

target_include_directories(ApplePlatform
PUBLIC
    interface
)

target_link_libraries(ApplePlatform
PUBLIC
    BuildSettings 
    BasicPlatform
    PlatformInterface
)

if(PLATFORM_MACOS)
    target_link_libraries(ApplePlatform
    PRIVATE
        ${APP_KIT}
    )
elseif(PLATFORM_IOS)
    target_link_libraries(ApplePlatform
    PRIVATE
        ${CORE_FOUNDATION}
        ${FOUNDATION}
    )
endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${PLATFORM_INTERFACE_HEADERS})

set_target_properties(ApplePlatform PROPERTIES
    FOLDER Core/Platforms
)

if(INSTALL_DILIGENT_CORE)
    install_core_lib(ApplePlatform)
endif()
