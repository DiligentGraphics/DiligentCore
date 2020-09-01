cmake_minimum_required (VERSION 3.6)

project(Diligent-HLSLTools CXX)

set(INCLUDE 
    include/DXILUtils.hpp
)

set(SOURCE 
    src/DXILUtils.cpp
)

add_library(Diligent-HLSLTools STATIC ${SOURCE} ${INCLUDE})

target_include_directories(Diligent-HLSLTools 
PUBLIC
    include
PRIVATE
    ../GraphicsEngine/include
)

target_link_libraries(Diligent-HLSLTools 
PRIVATE 
    Diligent-BuildSettings
    Diligent-GraphicsAccessories
    Diligent-Common
PUBLIC
    Diligent-GraphicsEngineInterface
)

if(VULKAN_SUPPORTED)
    target_link_libraries(Diligent-HLSLTools PRIVATE Diligent-GLSLTools)
endif()

target_include_directories(Diligent-HLSLTools 
PUBLIC
    ../../ThirdParty/DirectXShaderCompiler
)

set_common_target_properties(Diligent-HLSLTools)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(Diligent-HLSLTools PROPERTIES
    FOLDER DiligentCore/Graphics
)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-HLSLTools)
endif()
