cmake_minimum_required (VERSION 3.6)

project(Diligent-GraphicsAccessories CXX)

set(INTERFACE 
    interface/ColorConversion.h
    interface/GraphicsAccessories.h
    interface/GraphicsTypesOutputInserters.h
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

add_library(Diligent-GraphicsAccessories STATIC ${SOURCE} ${INTERFACE})

target_include_directories(Diligent-GraphicsAccessories 
PUBLIC
    interface
)

target_link_libraries(Diligent-GraphicsAccessories 
PUBLIC
    Diligent-BuildSettings
    Diligent-Common 
    Diligent-GraphicsEngineInterface
)

set_common_target_properties(Diligent-GraphicsAccessories)

source_group("src" FILES ${SOURCE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(Diligent-GraphicsAccessories PROPERTIES
    FOLDER DiligentCore/Graphics
)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-GraphicsAccessories)
endif()