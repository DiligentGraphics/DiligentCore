cmake_minimum_required (VERSION 3.10)

project(GraphicsEngineD3DBase CXX)

set(INCLUDE 
    include/D3DErrors.h
    include/D3DShaderResourceLoader.h
    include/D3DTypeConversionImpl.h
    include/D3DViewDescConversionImpl.h
    include/DXGITypeConversions.h
    include/EngineFactoryD3DBase.h
    include/HLSLDefinitions.fxh
    include/RenderDeviceD3DBase.h
    include/ShaderD3DBase.h
    include/ShaderResources.h
    include/ShaderVariableD3DBase.h
    include/SwapChainD3DBase.h
)

set(INTERFACE
    interface/ShaderD3D.h
)

set(SOURCE 
    src/DXGITypeConversions.cpp
    src/ShaderD3DBase.cpp
    src/ShaderResources.cpp
)

add_library(GraphicsEngineD3DBase STATIC 
    ${SOURCE} ${INCLUDE} ${INTERFACE} include/HLSLDefinitions_inc.fxh
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/HLSLDefinitions_inc.fxh
    PROPERTIES GENERATED TRUE
)

# Create custom target to convert HLSLDefinitions.fxh to HLSLDefinitions_inc.fxh
add_custom_target(ProcessHLSLDefinitions
SOURCES
    include/HLSLDefinitions.fxh
)

add_custom_command(TARGET ProcessHLSLDefinitions 
                   # Unfortunately it is not possible to set TARGET directly to GraphicsEngineD3DBase
                   # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
                   # PRE_BUILD is treated as PRE_LINK.
                   COMMAND ${FILE2STRING_PATH} include/HLSLDefinitions.fxh include/HLSLDefinitions_inc.fxh
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   COMMENT "Processing HLSLDefinitions.fxh"
                   VERBATIM
)

add_dependencies(GraphicsEngineD3DBase ProcessHLSLDefinitions)

add_library(GraphicsEngineD3DBaseInterface INTERFACE)
target_link_libraries(GraphicsEngineD3DBaseInterface INTERFACE GraphicsEngineInterface)
target_include_directories(GraphicsEngineD3DBaseInterface INTERFACE interface)

target_include_directories(GraphicsEngineD3DBase
PUBLIC
    include
)

target_link_libraries(GraphicsEngineD3DBase 
PUBLIC
    BuildSettings 
    GraphicsEngine 
    GraphicsEngineD3DBaseInterface
)
if(D3D12_SUPPORTED)
    target_link_libraries(GraphicsEngineD3DBase PRIVATE D3D12.lib)
endif()
set_common_target_properties(GraphicsEngineD3DBase)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})
source_group("generated" FILES include/HLSLDefinitions_inc.fxh)

set_target_properties(GraphicsEngineD3DBase PROPERTIES
    FOLDER Core/Graphics
)
set_target_properties(ProcessHLSLDefinitions PROPERTIES
    FOLDER Core/Graphics/Helper
)

if(INSTALL_DILIGENT_CORE)
    install_core_lib(GraphicsEngineD3DBase)
endif()