cmake_minimum_required (VERSION 3.10)

if (PLATFORM_WEB)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

project(DiligentCoreTest)

file(GLOB_RECURSE SOURCE  src/*.*)
file(GLOB_RECURSE SHADERS assets/shaders/*.*)

if(NOT GL_SUPPORTED AND NOT GLES_SUPPORTED AND NOT VULKAN_SUPPORTED)
    list(REMOVE_ITEM SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderTools/GLSLUtilsTest.cpp)
endif()

if(NOT WEBGPU_SUPPORTED)
    list(REMOVE_ITEM SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderTools/WGSLUtilsTest.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderTools/WGSLShaderResourcesTest.cpp
    )
endif()

set_source_files_properties(${SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Disable the following warning:
    #   explicitly moving variable of type '(anonymous namespace)::SmartPtr' (aka 'RefCntAutoPtr<(anonymous namespace)::Object>') to itself [-Wself-move]
    set_source_files_properties(src/Common/RefCntAutoPtrTest.cpp
    PROPERTIES
        COMPILE_FLAGS "-Wno-self-move -Wno-self-assign"
    )
endif()

add_executable(DiligentCoreTest ${SOURCE} ${SHADERS})
set_common_target_properties(DiligentCoreTest 17)

if (PLATFORM_WEB)  
    set(RESOURCE_PATH "${PROJECT_SOURCE_DIR}/assets/")
    set(HTML_TEMPLATE_FILE ${PROJECT_SOURCE_DIR}/resources/emscripten_template.html)      
    target_link_options(DiligentCoreTest PRIVATE "SHELL: -s SINGLE_FILE -s ALLOW_MEMORY_GROWTH=1 --preload-file '${RESOURCE_PATH}@'")      
    target_link_options(DiligentCoreTest PRIVATE "SHELL: --shell-file '${HTML_TEMPLATE_FILE}'") 
endif()

target_link_libraries(DiligentCoreTest
PRIVATE
    gtest_main
    Diligent-BuildSettings
    Diligent-TargetPlatform
    Diligent-TestFramework
    Diligent-GraphicsAccessories
    Diligent-Common
    Diligent-GraphicsTools
    Diligent-GraphicsEngine
    Diligent-ShaderTools
)

if(WEBGPU_SUPPORTED)
    target_link_libraries(DiligentCoreTest PRIVATE libtint)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE} ${SHADERS}})

set_target_properties(DiligentCoreTest
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

set_target_properties(DiligentCoreTest PROPERTIES
    FOLDER "DiligentCore/Tests"
)
