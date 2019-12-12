cmake_minimum_required (VERSION 3.6)

project(DiligentCoreAPITest)

file(GLOB COMMON_SOURCE LIST_DIRECTORIES false src/*)
file(GLOB COMMON_INCLUDE LIST_DIRECTORIES false include/*)
file(GLOB_RECURSE SHADERS LIST_DIRECTORIES false assets/shaders/*.*)

set_source_files_properties(${SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")

set(SOURCE ${COMMON_SOURCE})
set(INCLUDE ${COMMON_INCLUDE})

if(NOT GL_SUPPORTED AND NOT GLES_SUPPORTED)
    #list(REMOVE_ITEM SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderConverterTest.cpp)
    list(REMOVE_ITEM SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/CreateObjFromNativeResGL.cpp)
    #list(REMOVE_ITEM INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include/ShaderConverterTest.h)
    list(REMOVE_ITEM INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include/CreateObjFromNativeResGL.h)
endif()

if(NOT D3D11_SUPPORTED)
    list(REMOVE_ITEM SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/CreateObjFromNativeResD3D11.cpp)
    list(REMOVE_ITEM INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include/CreateObjFromNativeResD3D11.h)
endif()

if(NOT D3D12_SUPPORTED)
    list(REMOVE_ITEM SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/CreateObjFromNativeResD3D12.cpp)
    list(REMOVE_ITEM INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include/CreateObjFromNativeResD3D12.h)
endif()

if(NOT VULKAN_SUPPORTED)
    list(REMOVE_ITEM SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/CreateObjFromNativeResVK.cpp)
    list(REMOVE_ITEM INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include/CreateObjFromNativeResVK.h)
endif()

if(PLATFORM_WIN32)
    file(GLOB SOURCE_WIN32 LIST_DIRECTORIES false src/Win32/*)
    list(APPEND SOURCE ${SOURCE_WIN32})
endif()

add_executable(DiligentCoreAPITest ${SOURCE} ${INCLUDE} ${SHADERS})
set_common_target_properties(DiligentCoreAPITest)

get_supported_backends(ENGINE_LIBRARIES)

target_link_libraries(DiligentCoreAPITest 
PRIVATE 
    gtest
    Diligent-BuildSettings 
    Diligent-TargetPlatform
    Diligent-GraphicsAccessories
    Diligent-Common
    Diligent-GraphicsTools
    ${ENGINE_LIBRARIES}
)

if(GL_SUPPORTED OR GLES_SUPPORTED)
    target_link_libraries(DiligentCoreAPITest PRIVATE Diligent-HLSL2GLSLConverterLib)
endif()

if(GL_SUPPORTED)
    target_link_libraries(DiligentCoreAPITest PRIVATE glew-static)
endif()


target_include_directories(DiligentCoreAPITest
PRIVATE
    include
    ../../ThirdParty/vulkan
)


set_target_properties(DiligentCoreAPITest
PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

if(PLATFORM_WIN32)
    copy_required_dlls(DiligentCoreAPITest)
endif()


source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE} ${INCLUDE} ${SHADERS})

set_target_properties(DiligentCoreAPITest PROPERTIES
    FOLDER "DiligentCore/Tests"
)
