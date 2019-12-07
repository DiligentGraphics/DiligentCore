cmake_minimum_required (VERSION 3.6)

project(DiligentCoreAPITest)

file(GLOB COMMON_SOURCE src/*)
file(GLOB COMMON_INCLUDE include/*)

set(SOURCE ${COMMON_SOURCE})
set(INCLUDE ${COMMON_INCLUDE})

add_executable(DiligentCoreAPITest ${SOURCE} ${INCLUDE})
set_common_target_properties(DiligentCoreAPITest)

get_supported_backends(ENGINE_LIBRARIES)

target_link_libraries(DiligentCoreAPITest 
PRIVATE 
    gtest
    Diligent-BuildSettings 
    Diligent-TargetPlatform
    Diligent-GraphicsAccessories
    Diligent-Common
    ${ENGINE_LIBRARIES}
)


target_include_directories(DiligentCoreAPITest PRIVATE include)


if(PLATFORM_WIN32)
    #set_target_properties(DiligentCoreAPITest PROPERTIES
    #    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    #)
    copy_required_dlls(DiligentCoreAPITest)
endif()


source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE} ${INCLUDE})

set_target_properties(DiligentCoreAPITest PROPERTIES
    FOLDER "DiligentCore/Tests"
)
