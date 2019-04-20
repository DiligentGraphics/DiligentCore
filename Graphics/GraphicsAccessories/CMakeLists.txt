cmake_minimum_required (VERSION 3.6)

project(GraphicsAccessories CXX)

set(INTERFACE 
    interface/ColorConversion.h
    interface/GraphicsAccessories.h
    interface/ResourceReleaseQueue.h
    interface/RingBuffer.h
    interface/SRBMemoryAllocator.h
    interface/VariableSizeAllocationsManager.h
    interface/VariableSizeGPUAllocationsManager.h
)

set(SOURCE
    src/ColorConversion.cpp
    src/SRBMemoryAllocator.cpp
    src/GraphicsAccessories.cpp
)

add_library(GraphicsAccessories STATIC ${SOURCE} ${INTERFACE})

target_include_directories(GraphicsAccessories 
PUBLIC
    interface
)

target_link_libraries(GraphicsAccessories 
PUBLIC
    BuildSettings
    Common 
    GraphicsEngineInterface
)

set_common_target_properties(GraphicsAccessories)

source_group("src" FILES ${SOURCE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(GraphicsAccessories PROPERTIES
    FOLDER Core/Graphics
)

if(INSTALL_DILIGENT_CORE)
    install_core_lib(GraphicsAccessories)
endif()