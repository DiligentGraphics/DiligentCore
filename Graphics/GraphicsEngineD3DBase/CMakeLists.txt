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

# Set paths to D3Dcompiler_47.dll, dxcompiler.dll, and dxil.dll
if(MSVC)
    if("${WINDOWS_SDK_VERSION}" VERSION_GREATER_EQUAL "10.0" AND WINDOWS_SDK_BIN_DIR)
        set(D3D_COMPILER_PATH "${WINDOWS_SDK_BIN_DIR}/D3Dcompiler_47.dll")

        # DXC is only present in Windows SDK starting with version 10.0.17763.0
        if("${WINDOWS_SDK_VERSION}" VERSION_GREATER_EQUAL "10.0.17763.0")
            set(DXC_COMPILER_PATH "${WINDOWS_SDK_BIN_DIR}/dxcompiler.dll")
            set(DXIL_SIGNER_PATH  "${WINDOWS_SDK_BIN_DIR}/dxil.dll")
        endif()
    elseif("${WINDOWS_SDK_VERSION}" VERSION_EQUAL "8.1")
        # D3Dcompiler_47.dll from Win8.1 SDK is ancient (from 2013) and fails to
        # compile a number of test shaders. Use the compiler from Visual Studio
        # executable path instead
        if(CMAKE_AR AND IS_ABSOLUTE "${CMAKE_AR}")
            get_filename_component(CMAKE_AR_DIR "${CMAKE_AR}" DIRECTORY)
            set(D3D_COMPILER_PATH "${CMAKE_AR_DIR}/D3Dcompiler_47.dll")
        endif()
    endif()

    if(D3D_COMPILER_PATH)
        if(EXISTS "${D3D_COMPILER_PATH}")
            message(STATUS "Found D3Dcompiler_47.dll: ${D3D_COMPILER_PATH}")
            set(D3D_COMPILER_PATH "${D3D_COMPILER_PATH}" CACHE INTERNAL "D3Dcompiler_47.dll path")
        else()
            message(WARNING "Cannot find D3Dcompiler_47.dll. File does not exist: ${D3D_COMPILER_PATH}")
            unset(D3D_COMPILER_PATH CACHE)
        endif()
    endif()

    if (DXC_COMPILER_PATH)
        if (EXISTS "${DXC_COMPILER_PATH}")
		    message(STATUS "Found dxcompiler.dll: ${DXC_COMPILER_PATH}")
		    set(DXC_COMPILER_PATH "${DXC_COMPILER_PATH}" CACHE INTERNAL "dxcompiler.dll path")
	    else()
		    message(WARNING "Cannot find dxcompiler.dll. File does not exist: ${DXC_COMPILER_PATH}")
		    unset(DXC_COMPILER_PATH CACHE)
        endif()
    endif()

    if (DXIL_SIGNER_PATH)
		if (EXISTS "${DXIL_SIGNER_PATH}")
			message(STATUS "Found dxil.dll: ${DXIL_SIGNER_PATH}")
			set(DXIL_SIGNER_PATH "${DXIL_SIGNER_PATH}" CACHE INTERNAL "dxil.dll path")
		else()
			message(WARNING "Cannot find dxil.dll. File does not exist: ${DXIL_SIGNER_PATH}")
			unset(DXIL_SIGNER_PATH CACHE)
		endif()
    endif()
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
