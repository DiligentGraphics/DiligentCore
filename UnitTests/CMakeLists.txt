cmake_minimum_required (VERSION 3.6)

project(DiligentCoreTest)

file(GLOB COMMON_SOURCE src/Common/*)
file(GLOB COMMON_INCLUDE include/Common/*)

set(SOURCE ${COMMON_SOURCE})
set(INCLUDE ${COMMON_INCLUDE})

add_executable(DiligentCoreTest ${SOURCE} ${INCLUDE})
set_common_target_properties(DiligentCoreTest)

target_link_libraries(DiligentCoreTest 
PRIVATE 
    gtest_main
    Diligent-BuildSettings 
    Diligent-TargetPlatform
    Diligent-Common
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE} ${INCLUDE})

set_target_properties(DiligentCoreTest PROPERTIES
    FOLDER "DiligentCore/Tests"
)
