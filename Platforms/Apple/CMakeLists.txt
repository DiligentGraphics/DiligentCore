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
