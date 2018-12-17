cmake_minimum_required (VERSION 3.6)

project(GraphicsEngineNextGenBase CXX)

set(INCLUDE 
    include/DeviceContextNextGenBase.h
    include/DynamicHeap.h
    include/RenderDeviceNextGenBase.h
)

set(SOURCE 
    src/dummy.cpp
)

add_library(GraphicsEngineNextGenBase STATIC ${SOURCE} ${INCLUDE})
set_common_target_properties(GraphicsEngineNextGenBase)

target_include_directories(GraphicsEngineNextGenBase
PUBLIC
    include
)

target_link_libraries(GraphicsEngineNextGenBase 
PUBLIC
    BuildSettings
    PlatformInterface
    Common
    GraphicsAccessories
    GraphicsEngine
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})

set_target_properties(GraphicsEngineNextGenBase PROPERTIES
    FOLDER Core/Graphics
)

if(INSTALL_DILIGENT_CORE)
    install_core_lib(GraphicsEngineNextGenBase)
endif()