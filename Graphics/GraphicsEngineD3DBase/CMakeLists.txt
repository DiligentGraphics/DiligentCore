cmake_minimum_required (VERSION 3.10)

project(Diligent-GraphicsEngineD3DBase CXX)

set(INCLUDE 
    include/D3DErrors.hpp
    include/D3DShaderResourceLoader.hpp
    include/D3DTypeConversionImpl.hpp
    include/D3DViewDescConversionImpl.hpp
    include/DXGITypeConversions.hpp
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

set(HLSL_DEFINITIONS include/HLSLDefinitions.fxh)

# We must use the full path, otherwise the build system will not be able to properly detect
# changes and shader conversion custom command will run every time
set(HLSL_DEFINITIONS_INC ${CMAKE_CURRENT_SOURCE_DIR}/include/HLSLDefinitions_inc.fxh)
set_source_files_properties(${HLSL_DEFINITIONS_INC} PROPERTIES GENERATED TRUE)

add_library(Diligent-GraphicsEngineD3DBase STATIC 
    ${SOURCE} ${INCLUDE} ${INTERFACE}

    # A target created in the same directory (CMakeLists.txt file) that specifies any output of the 
    # custom command as a source file is given a rule to generate the file using the command at build time. 
    ${HLSL_DEFINITIONS_INC}
)

add_custom_command(OUTPUT ${HLSL_DEFINITIONS_INC} # We must use full path here!
                   COMMAND ${FILE2STRING_PATH} ${HLSL_DEFINITIONS} include/HLSLDefinitions_inc.fxh
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   MAIN_DEPENDENCY ${HLSL_DEFINITIONS}
                   COMMENT "Processing HLSLDefinitions.fxh"
                   VERBATIM
)

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

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-GraphicsEngineD3DBase)
endif()