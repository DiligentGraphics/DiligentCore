cmake_minimum_required (VERSION 3.6)

project(DiligentCoreTest)

file(GLOB_RECURSE SOURCE LIST_DIRECTORIES false src/*)
set(INCLUDE)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Disable the following warning:
    #   explicitly moving variable of type '(anonymous namespace)::SmartPtr' (aka 'RefCntAutoPtr<(anonymous namespace)::Object>') to itself [-Wself-move]
    set_source_files_properties(src/Common/RefCntAutoPtrTest.cpp
    PROPERTIES
        COMPILE_FLAGS "-Wno-self-move -Wno-self-assign"
    )
endif()

add_executable(DiligentCoreTest ${SOURCE} ${INCLUDE})
set_common_target_properties(DiligentCoreTest)

target_link_libraries(DiligentCoreTest
PRIVATE
    gtest_main
    Diligent-BuildSettings
    Diligent-TargetPlatform
    Diligent-GraphicsAccessories
    Diligent-Common
    Diligent-GraphicsTools
    Diligent-GraphicsEngine
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE} ${INCLUDE})

set_target_properties(DiligentCoreTest PROPERTIES
    FOLDER "DiligentCore/Tests"
)
