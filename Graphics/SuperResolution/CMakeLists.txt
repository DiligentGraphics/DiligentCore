cmake_minimum_required (VERSION 3.11)

include(../../BuildTools/CMake/BuildUtils.cmake)

project(Diligent-SuperResolution CXX)

set(DILIGENT_DLSS_SUPPORTED FALSE CACHE INTERNAL "DLSS is not supported")
set(DILIGENT_DSR_SUPPORTED  FALSE CACHE INTERNAL "DirectSR is not supported")
set(DILIGENT_FSR_SUPPORTED  TRUE CACHE INTERNAL "FSR is not supported")

if(PLATFORM_WIN32 AND NOT MINGW_BUILD AND (CMAKE_SIZEOF_VOID_P EQUAL 8) AND ("${WINDOWS_SDK_VERSION}" VERSION_GREATER_EQUAL "10.0.26100.0"))
    if (D3D11_SUPPORTED OR D3D12_SUPPORTED OR VULKAN_SUPPORTED)
        set(DILIGENT_DLSS_SUPPORTED TRUE CACHE INTERNAL "DLSS is supported on Win32 platform")
    endif()
    if (D3D12_SUPPORTED)
        set(DILIGENT_DSR_SUPPORTED  TRUE CACHE INTERNAL "DirectSR is supported on Win32 platform")
    endif()
endif()

if(${DILIGENT_NO_DLSS})
    set(DILIGENT_DLSS_SUPPORTED FALSE CACHE INTERNAL "DLSS is forcibly disabled")
endif()
if(${DILIGENT_NO_DSR})
    set(DILIGENT_DSR_SUPPORTED FALSE CACHE INTERNAL "DirectSR is forcibly disabled")
endif()
if(${DILIGENT_NO_FSR})
    set(DILIGENT_FSR_SUPPORTED FALSE CACHE INTERNAL "FSR is forcibly disabled")
endif()

if(DILIGENT_DSR_SUPPORTED)
    # Fetch DirectSR headers
    FetchContent_DeclareShallowGit(DirectSR-Headers
        GIT_REPOSITORY https://github.com/DiligentGraphics/DirectSR-Headers.git
        GIT_TAG        master
    )
    FetchContent_MakeAvailable(DirectSR-Headers)
    if(TARGET DirectSR-AgilitySDK)
        set_target_properties(DirectSR-AgilitySDK PROPERTIES FOLDER DiligentCore/ThirdParty)
    endif()
endif()

if(DILIGENT_DLSS_SUPPORTED)
    # Fetch NVIDIA DLSS SDK headers
    FetchContent_DeclareShallowGit(DLSS-Headers
        GIT_REPOSITORY https://github.com/DiligentGraphics/DLSS-Headers.git
        GIT_TAG        master
    )
    FetchContent_MakeAvailable(DLSS-Headers)
endif()

set(INCLUDE
    include/SuperResolutionBase.hpp
    include/SuperResolutionVariants.hpp
    include/SuperResolutionProvider.hpp
)

set(INTERFACE
    interface/SuperResolution.h
    interface/SuperResolutionFactory.h
    interface/SuperResolutionFactoryLoader.h
)

set(SOURCE
    src/SuperResolutionBase.cpp
    src/SuperResolutionFactory.cpp
)

set(FSR_SHADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/FSR/shaders)

if(DILIGENT_FSR_SUPPORTED)
    set(FSR_SHADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/FSR_FullQuad.fx
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/FSRStructures.fxh
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/FSR_EdgeAdaptiveUpsampling.fx
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/FSR_ContrastAdaptiveSharpening.fx
        ${FSR_SHADERS_DIR}/ffx_common_types.h
        ${FSR_SHADERS_DIR}/ffx_core.h
        ${FSR_SHADERS_DIR}/ffx_core_cpu.h
        ${FSR_SHADERS_DIR}/ffx_core_glsl.h
        ${FSR_SHADERS_DIR}/ffx_core_gpu_common.h
        ${FSR_SHADERS_DIR}/ffx_core_gpu_common_half.h
        ${FSR_SHADERS_DIR}/ffx_core_hlsl.h
        ${FSR_SHADERS_DIR}/ffx_core_portability.h
        ${FSR_SHADERS_DIR}/ffx_fsr1.h
    )
    set_source_files_properties(${FSR_SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")

    # Convert shaders to headers and generate master header with the list of all files
    set(FSR_SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders_inc/FSR)
    file(MAKE_DIRECTORY ${FSR_SHADER_OUTPUT_DIR})
    set(FSR_SHADERS_LIST_FILE ${FSR_SHADER_OUTPUT_DIR}/FSRShaderList.h)
    convert_shaders_to_headers("${FSR_SHADERS}" ${FSR_SHADER_OUTPUT_DIR} ${FSR_SHADERS_LIST_FILE} FSR_SHADERS_INC_LIST)

    list(APPEND SOURCE src/FSRProvider.cpp)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set_source_files_properties(src/FSRProvider.cpp PROPERTIES COMPILE_OPTIONS "-Wno-unused-function")
    endif()
endif()

if(DILIGENT_DLSS_SUPPORTED)
    list(APPEND INCLUDE include/SuperResolutionDLSS.hpp)
    list(APPEND SOURCE src/SuperResolutionDLSS.cpp)
    if (D3D11_SUPPORTED)
        list(APPEND SOURCE src/DLSSProviderD3D11.cpp)
    endif()
    if (D3D12_SUPPORTED)
        list(APPEND SOURCE src/DLSSProviderD3D12.cpp)
    endif()
    if (VULKAN_SUPPORTED)
        list(APPEND SOURCE src/DLSSProviderVk.cpp)
    endif()
endif()

if (DILIGENT_DSR_SUPPORTED)
    if (D3D12_SUPPORTED)
        list(APPEND SOURCE src/DSRProviderD3D12.cpp)
    endif()
endif()

set(DLL_SOURCE
    src/DLLMain.cpp
    src/SuperResolution.def
)

add_library(Diligent-SuperResolutionInterface INTERFACE)
target_link_libraries     (Diligent-SuperResolutionInterface INTERFACE Diligent-GraphicsEngineInterface)
target_include_directories(Diligent-SuperResolutionInterface INTERFACE interface)
target_compile_definitions(Diligent-SuperResolutionInterface INTERFACE SUPER_RESOLUTION_SUPPORTED=1)

add_library(Diligent-SuperResolution-static STATIC
    ${SOURCE} 
    ${INTERFACE} 
    ${INCLUDE}
    ${FSR_SHADERS}
    ${FSR_SHADERS_INC_LIST}
    ${FSR_SHADERS_LIST_FILE}
    readme.md
)
source_group("shaders/FSR" FILES ${FSR_SHADERS})
source_group("generated/FSR" FILES ${FSR_SHADERS_INC_LIST} ${FSR_SHADERS_LIST_FILE})

add_library(Diligent-SuperResolution-shared SHARED
    readme.md
)

if((PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS) AND NOT MINGW_BUILD)
    target_sources(Diligent-SuperResolution-shared PRIVATE ${DLL_SOURCE})
endif()

target_include_directories(Diligent-SuperResolution-static
PRIVATE
    include
    ../GraphicsEngine/include
    ../GraphicsEngineD3DBase/include
    ../GraphicsEngineNextGenBase/include
)

if(DILIGENT_DLSS_SUPPORTED)
    if (D3D11_SUPPORTED)
        target_compile_definitions(Diligent-SuperResolution-static PRIVATE DILIGENT_DLSS_D3D11_SUPPORTED=1)
    endif()
    if (D3D12_SUPPORTED)
        target_compile_definitions(Diligent-SuperResolution-static PRIVATE DILIGENT_DLSS_D3D12_SUPPORTED=1)
    endif()
    if (VULKAN_SUPPORTED)
        target_compile_definitions(Diligent-SuperResolution-static PRIVATE DILIGENT_DLSS_VK_SUPPORTED=1)
    endif()
endif()

if(DILIGENT_DSR_SUPPORTED)
    if (D3D12_SUPPORTED)
        target_compile_definitions(Diligent-SuperResolution-static PRIVATE DILIGENT_DSR_D3D12_SUPPORTED=1)
    endif()
endif()

if(DILIGENT_FSR_SUPPORTED)
    target_compile_definitions(Diligent-SuperResolution-static PRIVATE DILIGENT_FSR_SUPPORTED=1)
    target_include_directories(Diligent-SuperResolution-static PRIVATE ${FSR_SHADER_OUTPUT_DIR})
endif()

target_compile_definitions(Diligent-SuperResolution-shared PUBLIC DILIGENT_SUPER_RESOLUTION_SHARED=1)

target_link_libraries(Diligent-SuperResolution-static
PUBLIC
    Diligent-SuperResolutionInterface
PRIVATE
    Diligent-BuildSettings
    Diligent-Common
    Diligent-GraphicsAccessories
    Diligent-ShaderTools
    Diligent-GraphicsTools
)

if(DILIGENT_DSR_SUPPORTED)
    target_link_libraries(Diligent-SuperResolution-static PRIVATE Diligent-GraphicsEngineD3D12-static DirectSR-Headers)
endif()

if(DILIGENT_DLSS_SUPPORTED)
    target_link_libraries(Diligent-SuperResolution-static PRIVATE DLSS-Headers DLSS-NGX)

    if(D3D12_SUPPORTED)
        target_link_libraries(Diligent-SuperResolution-static PRIVATE Diligent-GraphicsEngineD3D12Interface)
    endif()
    if(D3D11_SUPPORTED)
        target_link_libraries(Diligent-SuperResolution-static PRIVATE Diligent-GraphicsEngineD3D11Interface)
    endif()
    if(VULKAN_SUPPORTED)
        target_link_libraries(Diligent-SuperResolution-static PRIVATE Diligent-GraphicsEngineVk-static Vulkan::Headers)
    endif()
endif()

target_link_libraries(Diligent-SuperResolution-shared
PUBLIC
    Diligent-SuperResolutionInterface
PRIVATE
    Diligent-BuildSettings
)
target_link_whole_archive(Diligent-SuperResolution-shared Diligent-SuperResolution-static)

if(PLATFORM_WIN32)
    # Do not add 'lib' prefix when building with MinGW
    set_target_properties(Diligent-SuperResolution-shared PROPERTIES PREFIX "")

    # Set output name to SuperResolution{32|64}{r|d}
    set_dll_output_name(Diligent-SuperResolution-shared SuperResolution)
else()
    set_target_properties(Diligent-SuperResolution-shared PROPERTIES
        OUTPUT_NAME SuperResolution
    )
endif()

if (MINGW_BUILD)
    # Restrict export to GetSuperResolutionFactory
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/export.map
        "{ global: *CreateSuperResolutionFactory*; local: *; };"
    )
    target_link_options(Diligent-SuperResolution-shared PRIVATE LINKER:--version-script=export.map)
endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})
source_group("dll" FILES ${DLL_SOURCE})

set_source_files_properties(
    readme.md PROPERTIES HEADER_FILE_ONLY TRUE
)

set_target_properties(Diligent-SuperResolution-static Diligent-SuperResolution-shared PROPERTIES
    FOLDER DiligentCore/Graphics
)

set_common_target_properties(Diligent-SuperResolution-static)
set_common_target_properties(Diligent-SuperResolution-shared)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-SuperResolution-shared)
    install_core_lib(Diligent-SuperResolution-static)
endif()
