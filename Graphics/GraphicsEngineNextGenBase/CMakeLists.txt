cmake_minimum_required (VERSION 3.6)

project(Diligent-GraphicsEngineNextGenBase CXX)

set(INCLUDE 
    include/DeviceContextNextGenBase.h
    include/DynamicHeap.h
    include/RenderDeviceNextGenBase.h
)

set(SOURCE 
    src/dummy.cpp
)

add_library(Diligent-GraphicsEngineNextGenBase STATIC ${SOURCE} ${INCLUDE})
set_common_target_properties(Diligent-GraphicsEngineNextGenBase)

target_include_directories(Diligent-GraphicsEngineNextGenBase
PUBLIC
    include
)

target_link_libraries(Diligent-GraphicsEngineNextGenBase 
PUBLIC
    Diligent-BuildSettings
    Diligent-PlatformInterface
    Diligent-Common
    Diligent-GraphicsAccessories
    Diligent-GraphicsEngine
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})

set_target_properties(Diligent-GraphicsEngineNextGenBase PROPERTIES
    FOLDER DiligentCore/Graphics
)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-GraphicsEngineNextGenBase)
endif()