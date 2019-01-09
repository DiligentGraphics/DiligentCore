cmake_minimum_required (VERSION 3.6)

project(GLSLTools CXX)

set(INCLUDE 
    include/GLSLSourceBuilder.h
)

set(SOURCE 
    src/GLSLSourceBuilder.cpp
)

if(VULKAN_SUPPORTED)
    list(APPEND SOURCE 
        src/SPIRVShaderResources.cpp
    )
    list(APPEND INCLUDE 
        include/SPIRVShaderResources.h
    )

    if (NOT ${NO_GLSLANG})
        list(APPEND SOURCE 
            src/SPIRVUtils.cpp
        )
        list(APPEND INCLUDE 
            include/SPIRVUtils.h
        )
    endif()
endif()

add_library(GLSLTools STATIC ${SOURCE} ${INCLUDE})

target_include_directories(GLSLTools 
PUBLIC
    include
PRIVATE
    ../HLSL2GLSLConverterLib/include
    ../GraphicsEngine/include
)

target_link_libraries(GLSLTools 
PRIVATE 
    BuildSettings
    GraphicsAccessories
    Common
PUBLIC
    HLSL2GLSLConverterLib
    GraphicsEngineInterface
)

if(VULKAN_SUPPORTED)
    target_link_libraries(GLSLTools 
    PRIVATE
        SPIRVCross
    )

    if (NOT ${NO_GLSLANG})
        target_link_libraries(GLSLTools 
        PRIVATE
            glslang
            SPIRV
        )

        target_include_directories(GLSLTools 
        PRIVATE
            ../../External/glslang
        )
    endif()
endif()

set_common_target_properties(GLSLTools)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(GLSLTools PROPERTIES
    FOLDER Core/Graphics
)

if(INSTALL_DILIGENT_CORE)
    install_core_lib(GLSLTools)
endif()