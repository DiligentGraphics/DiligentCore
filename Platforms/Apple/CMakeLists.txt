cmake_minimum_required (VERSION 3.6)

project(Diligent-ApplePlatform CXX)

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


add_library(Diligent-ApplePlatform ${SOURCE} ${INTERFACE} ${PLATFORM_INTERFACE_HEADERS})
set_common_target_properties(Diligent-ApplePlatform)

target_include_directories(Diligent-ApplePlatform
PUBLIC
    interface
)

target_link_libraries(Diligent-ApplePlatform
PUBLIC
    Diligent-BuildSettings 
    Diligent-BasicPlatform
    Diligent-PlatformInterface
)

if(PLATFORM_MACOS)
    target_link_libraries(Diligent-ApplePlatform
    PRIVATE
        ${APP_KIT}
    )
elseif(PLATFORM_IOS)
    target_link_libraries(Diligent-ApplePlatform
    PRIVATE
        ${CORE_FOUNDATION}
        ${FOUNDATION}
    )
endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${PLATFORM_INTERFACE_HEADERS})

set_target_properties(Diligent-ApplePlatform PROPERTIES
    FOLDER DiligentCore/Platforms
)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-ApplePlatform)
endif()
