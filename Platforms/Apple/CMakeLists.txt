cmake_minimum_required (VERSION 3.6)

project(ApplePlatform CXX)

set(INCLUDE
    include/CFObjectWrapper.h
	include/AppleDebug.h
	include/AppleFileSystem.h
	include/ApplePlatformDefinitions.h
	include/ApplePlatformMisc.h
)

set(SOURCE 
	src/AppleDebug.mm
	src/AppleFileSystem.cpp
)


add_library(ApplePlatform ${SOURCE} ${INCLUDE} ${PLATFORM_INTERFACE_HEADERS})
set_common_target_properties(ApplePlatform)

target_include_directories(ApplePlatform
PUBLIC
	include
)

target_link_libraries(ApplePlatform
PUBLIC
	BuildSettings 
	BasicPlatform
	PlatformInterface
PRIVATE
    ${APP_KIT}
)

if(PLATFORM_IOS)
    if(NOT CORE_FOUNDATION)
        message(FATAL_ERROR "Cannot find CoreFoundation framework")
    endif()

    if(NOT FOUNDATION)
        message(FATAL_ERROR "Cannot find Foundation framework")
    endif()

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
