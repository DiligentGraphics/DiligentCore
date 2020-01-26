cmake_minimum_required (VERSION 3.6)

project(Diligent-HLSL2GLSLConverterLib CXX)

set(INCLUDE 
    include/GLSLDefinitions.h
    include/HLSL2GLSLConverterImpl.hpp
    include/HLSL2GLSLConverterObject.hpp
    include/HLSLKeywords.h
)

set(INTERFACE 
    interface/HLSL2GLSLConverter.h
)

set(SOURCE 
    src/HLSL2GLSLConverterImpl.cpp
    src/HLSL2GLSLConverterObject.cpp
)

add_library(Diligent-HLSL2GLSLConverterLib STATIC ${SOURCE} ${INTERFACE} ${INCLUDE} include/GLSLDefinitions_inc.h)
set_common_target_properties(Diligent-HLSL2GLSLConverterLib)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/GLSLDefinitions_inc.h
    PROPERTIES GENERATED TRUE
)

target_include_directories(Diligent-HLSL2GLSLConverterLib 
PUBLIC 
    interface
PRIVATE
    include
)

target_link_libraries(Diligent-HLSL2GLSLConverterLib 
PRIVATE 
    Diligent-BuildSettings 
    Diligent-Common 
    Diligent-PlatformInterface 
    Diligent-GraphicsEngine
PUBLIC
    Diligent-GraphicsEngineInterface
)

if(NOT FILE2STRING_PATH STREQUAL "")
    # Create custom target to convert GLSLDefinitions.h to GLSLDefinitions_inc.h
    add_custom_target(Diligent-ProcessGLSLDefinitions 
    SOURCES
        include/GLSLDefinitions.h
    )

    add_custom_command(TARGET Diligent-ProcessGLSLDefinitions 
                       # Unfortunately it is not possible to set TARGET directly to HLSL2GLSLConverterLib
                       # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
                       # PRE_BUILD is treated as PRE_LINK.
                       COMMAND ${FILE2STRING_PATH} include/GLSLDefinitions.h include/GLSLDefinitions_inc.h
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                       COMMENT "Processing GLSLDefinitions.h"
                       VERBATIM
    )

    add_dependencies(Diligent-HLSL2GLSLConverterLib Diligent-ProcessGLSLDefinitions)
    set_target_properties(Diligent-ProcessGLSLDefinitions PROPERTIES
        FOLDER DiligentCore/Graphics/Helper
    )
else()
    message(WARNING "File2String utility is currently unavailable on this host system. This is not an issues unless you modify GLSLDefinitions.h file")
endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})
source_group("generated" FILES include/GLSLDefinitions_inc.h)

set_target_properties(Diligent-HLSL2GLSLConverterLib PROPERTIES
    FOLDER DiligentCore/Graphics
)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-HLSL2GLSLConverterLib)
endif()