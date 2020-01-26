cmake_minimum_required (VERSION 3.10)

project(Diligent-GraphicsEngineD3DBase CXX)

set(INCLUDE 
    include/D3DErrors.hpp
    include/D3DShaderResourceLoader.hpp
    include/D3DTypeConversionImpl.hpp
    include/D3DViewDescConversionImpl.hpp
    include/DXGITypeConversions.h
    include/EngineFactoryD3DBase.hpp
    include/HLSLDefinitions.fxh
    include/RenderDeviceD3DBase.hpp
    include/ShaderD3DBase.hpp
    include/ShaderResources.hpp
    include/ShaderVariableD3DBase.hpp
    include/SwapChainD3DBase.hpp
)

set(INTERFACE
    interface/ShaderD3D.h
)

set(SOURCE 
    src/DXGITypeConversions.cpp
    src/ShaderD3DBase.cpp
    src/ShaderResources.cpp
)

add_library(Diligent-GraphicsEngineD3DBase STATIC 
    ${SOURCE} ${INCLUDE} ${INTERFACE} include/HLSLDefinitions_inc.fxh
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/HLSLDefinitions_inc.fxh
    PROPERTIES GENERATED TRUE
)

# Create custom target to convert HLSLDefinitions.fxh to HLSLDefinitions_inc.fxh
add_custom_target(Diligent-ProcessHLSLDefinitions
SOURCES
    include/HLSLDefinitions.fxh
)

add_custom_command(TARGET Diligent-ProcessHLSLDefinitions 
                   # Unfortunately it is not possible to set TARGET directly to Diligent-GraphicsEngineD3DBase
                   # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
                   # PRE_BUILD is treated as PRE_LINK.
                   COMMAND ${FILE2STRING_PATH} include/HLSLDefinitions.fxh include/HLSLDefinitions_inc.fxh
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   COMMENT "Processing HLSLDefinitions.fxh"
                   VERBATIM
)

add_dependencies(Diligent-GraphicsEngineD3DBase Diligent-ProcessHLSLDefinitions)

add_library(Diligent-GraphicsEngineD3DBaseInterface INTERFACE)
target_link_libraries(Diligent-GraphicsEngineD3DBaseInterface INTERFACE Diligent-GraphicsEngineInterface)
target_include_directories(Diligent-GraphicsEngineD3DBaseInterface INTERFACE interface)

target_include_directories(Diligent-GraphicsEngineD3DBase
PUBLIC
    include
)

target_link_libraries(Diligent-GraphicsEngineD3DBase 
PUBLIC
    Diligent-BuildSettings 
    Diligent-GraphicsEngine 
    Diligent-GraphicsEngineD3DBaseInterface
)

set_common_target_properties(Diligent-GraphicsEngineD3DBase)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})
source_group("generated" FILES include/HLSLDefinitions_inc.fxh)

set_target_properties(Diligent-GraphicsEngineD3DBase PROPERTIES
    FOLDER DiligentCore/Graphics
)
set_target_properties(Diligent-ProcessHLSLDefinitions PROPERTIES
    FOLDER DiligentCore/Graphics/Helper
)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-GraphicsEngineD3DBase)
endif()