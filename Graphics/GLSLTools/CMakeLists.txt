cmake_minimum_required (VERSION 3.6)

project(Diligent-GLSLTools CXX)

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

    if (NOT ${DILIGENT_NO_GLSLANG})
        list(APPEND SOURCE 
            src/SPIRVUtils.cpp
        )
        list(APPEND INCLUDE 
            include/SPIRVUtils.h
        )
    endif()
endif()

add_library(Diligent-GLSLTools STATIC ${SOURCE} ${INCLUDE})

target_include_directories(Diligent-GLSLTools 
PUBLIC
    include
PRIVATE
    ../HLSL2GLSLConverterLib/include
    ../GraphicsEngine/include
)

target_link_libraries(Diligent-GLSLTools 
PRIVATE 
    Diligent-BuildSettings
    Diligent-GraphicsAccessories
    Diligent-Common
PUBLIC
    Diligent-HLSL2GLSLConverterLib
    Diligent-GraphicsEngineInterface
)

if(VULKAN_SUPPORTED)
    target_link_libraries(Diligent-GLSLTools 
    PRIVATE
        spirv-cross-core
    )

    if (NOT ${DILIGENT_NO_GLSLANG})
        target_link_libraries(Diligent-GLSLTools 
        PRIVATE
            glslang
            SPIRV
        )

        target_include_directories(Diligent-GLSLTools 
        PRIVATE
            ../../ThirdParty/glslang
        )
    endif()
endif()

set_common_target_properties(Diligent-GLSLTools)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(Diligent-GLSLTools PROPERTIES
    FOLDER DiligentCore/Graphics
)

if(INSTALL_DILIGENT_CORE)
    install_core_lib(Diligent-GLSLTools)
endif()