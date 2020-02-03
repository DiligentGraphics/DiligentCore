cmake_minimum_required (VERSION 3.6)

project(Diligent-GLSLTools CXX)

set(INCLUDE 
    include/GLSLSourceBuilder.hpp
)

set(SOURCE 
    src/GLSLSourceBuilder.cpp
)

if(VULKAN_SUPPORTED)
    list(APPEND SOURCE 
        src/SPIRVShaderResources.cpp
    )
    list(APPEND INCLUDE 
        include/SPIRVShaderResources.hpp
    )

    if (NOT ${DILIGENT_NO_GLSLANG})
        list(APPEND SOURCE 
            src/SPIRVUtils.cpp
        )
        list(APPEND INCLUDE 
            include/SPIRVUtils.hpp
        )
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # Disable the following warning:
            #   moving a local object in a return statement prevents copy elision [-Wpessimizing-move]
            set_source_files_properties(src/SPIRVUtils.cpp
            PROPERTIES
                COMPILE_FLAGS -Wno-pessimizing-move
            )
        endif()
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

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-GLSLTools)
endif()
