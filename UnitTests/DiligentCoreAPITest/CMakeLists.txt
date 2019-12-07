cmake_minimum_required (VERSION 3.6)

project(DiligentCoreAPITest)

file(GLOB COMMON_SOURCE src/*)
file(GLOB COMMON_INCLUDE include/*)

set(SOURCE ${COMMON_SOURCE})
set(INCLUDE ${COMMON_INCLUDE})

add_executable(DiligentCoreAPITest ${SOURCE} ${INCLUDE})
set_common_target_properties(DiligentCoreAPITest)

if(GL_SUPPORTED OR GLES_SUPPORTED)
    list(APPEND BACKENDS Diligent-GraphicsEngineOpenGL-static)
endif()

if(D3D11_SUPPORTED)
    list(APPEND BACKENDS Diligent-GraphicsEngineD3D11-static)
endif()

if(D3D12_SUPPORTED)
    list(APPEND BACKENDS Diligent-GraphicsEngineD3D12-static)
endif()

if(VULKAN_SUPPORTED)
    list(APPEND BACKENDS Diligent-GraphicsEngineVk-static)
endif()

if(METAL_SUPPORTED)
    list(APPEND BACKENDS Diligent-GraphicsEngineMtl-static)
endif()


target_link_libraries(DiligentCoreAPITest 
PRIVATE 
    gtest
    Diligent-BuildSettings 
    Diligent-TargetPlatform
    Diligent-GraphicsAccessories
    Diligent-Common
    ${BACKENDS}
)


target_include_directories(DiligentCoreAPITest PRIVATE include)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE} ${INCLUDE})

set_target_properties(DiligentCoreAPITest PROPERTIES
    FOLDER "DiligentCore/Tests"
)
