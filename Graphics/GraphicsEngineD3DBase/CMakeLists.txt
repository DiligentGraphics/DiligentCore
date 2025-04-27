cmake_minimum_required (VERSION 3.10)

project(Diligent-GraphicsEngineD3DBase CXX)

set(DILIGENT_NVAPI_PATH "" CACHE PATH "Path to NVAPI to enable D3D11/D3D12 extensions")

set(INCLUDE
    include/D3DCommonTypeConversions.hpp
    include/D3DErrors.hpp
    include/D3DTileMappingHelper.hpp
    include/D3DShaderResourceLoader.hpp
    include/D3DShaderResourceValidation.hpp
    include/D3DTypeConversionImpl.hpp
    include/D3DViewDescConversionImpl.hpp
    include/DXGITypeConversions.hpp
    include/EngineFactoryD3DBase.hpp
    include/RenderDeviceD3DBase.hpp
    include/ShaderD3DBase.hpp
    include/ShaderResources.hpp
    include/ShaderVariableD3D.hpp
    include/SwapChainD3DBase.hpp
    include/NVApiLoader.hpp
)

set(INTERFACE
    interface/ShaderD3D.h
)

set(SOURCE
    src/D3DCommonTypeConversions.cpp
    src/D3DShaderResourceValidation.cpp
    src/DXGITypeConversions.cpp
    src/ShaderD3DBase.cpp
    src/ShaderResources.cpp
    src/SwapChainD3DBase.cpp
)

add_library(Diligent-GraphicsEngineD3DBase STATIC
    ${SOURCE} ${INCLUDE} ${INTERFACE}
)

add_library(Diligent-GraphicsEngineD3DBaseInterface INTERFACE)
target_link_libraries     (Diligent-GraphicsEngineD3DBaseInterface INTERFACE Diligent-GraphicsEngineInterface)
target_include_directories(Diligent-GraphicsEngineD3DBaseInterface INTERFACE interface)

target_include_directories(Diligent-GraphicsEngineD3DBase
PUBLIC
    include
)

target_link_libraries(Diligent-GraphicsEngineD3DBase
PRIVATE
    Diligent-BuildSettings
    Diligent-ShaderTools
PUBLIC
    Diligent-GraphicsEngine
    Diligent-GraphicsEngineD3DBaseInterface
)

set_common_target_properties(Diligent-GraphicsEngineD3DBase)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(Diligent-GraphicsEngineD3DBase PROPERTIES
    FOLDER DiligentCore/Graphics
)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-GraphicsEngineD3DBase)
endif()

if(EXISTS "${DILIGENT_NVAPI_PATH}/nvapi.h")
    message(STATUS "Using NVApi from ${DILIGENT_NVAPI_PATH}")
    if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
        set(DILIGENT_NVAPI_LIB_PATH "${DILIGENT_NVAPI_PATH}/amd64/nvapi64.lib" CACHE INTERNAL "NVAPI lib path")
    else()
        set(DILIGENT_NVAPI_LIB_PATH "${DILIGENT_NVAPI_PATH}/x86/nvapi.lib" CACHE INTERNAL "NVAPI lib path")
    endif()
    target_include_directories(Diligent-GraphicsEngineD3DBase PUBLIC ${DILIGENT_NVAPI_PATH})
    target_link_libraries(Diligent-GraphicsEngineD3DBase PRIVATE ${DILIGENT_NVAPI_LIB_PATH})
    target_compile_definitions(Diligent-GraphicsEngineD3DBase PUBLIC DILIGENT_ENABLE_D3D_NVAPI)
endif()
