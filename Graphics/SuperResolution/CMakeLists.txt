cmake_minimum_required (VERSION 3.11)

include(../../BuildTools/CMake/BuildUtils.cmake)

project(Diligent-SuperResolution CXX)

set(DILIGENT_DLSS_SUPPORTED FALSE CACHE INTERNAL "DLSS is not supported")
set(DILIGENT_DSR_SUPPORTED  FALSE CACHE INTERNAL "DirectSR is not supported")

if(PLATFORM_WIN32)
    set(DILIGENT_DLSS_SUPPORTED TRUE CACHE INTERNAL "DLSS is supported on Win32 platform")
    set(DILIGENT_DSR_SUPPORTED  TRUE CACHE INTERNAL "DirectSR is supported on Win32 platform")
endif()

if(${DILIGENT_NO_DLSS})
    set(DILIGENT_DLSS_SUPPORTED FALSE CACHE INTERNAL "DLSS is forcibly disabled")
endif()
if(${DILIGENT_NO_DSR})
    set(DILIGENT_DSR_SUPPORTED FALSE CACHE INTERNAL "DirectSR is forcibly disabled")
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
        GIT_REPOSITORY https://github.com/NVIDIA/DLSS.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(DLSS-Headers)
endif()

set(INCLUDE
    include/SuperResolutionBase.hpp
    include/SuperResolutionFactoryBase.hpp
    include/SuperResolutionVariants.hpp
    include/DLSSProviderD3D12.hpp
    include/DLSSProviderD3D11.hpp
    include/DLSSProviderVk.hpp
    include/DSRProviderD3D12.hpp
)

set(INTERFACE
    interface/SuperResolution.h
    interface/SuperResolutionFactory.h
    interface/SuperResolutionFactoryLoader.h
)

set(SOURCE
    src/SuperResolutionBase.cpp
    src/SuperResolutionFactoryBase.cpp
    src/SuperResolutionFactory.cpp
    src/SuperResolutionFactoryD3D12.cpp
    src/SuperResolutionFactoryD3D11.cpp
    src/SuperResolutionFactoryVk.cpp
    src/SuperResolutionFactoryMtl.cpp
    src/DLSSProviderD3D12.cpp
    src/DLSSProviderD3D11.cpp
    src/DLSSProviderVk.cpp
    src/DSRProviderD3D12.cpp
)

if(DILIGENT_DLSS_SUPPORTED)
    list(APPEND INCLUDE include/SuperResolutionDLSS.hpp)
    list(APPEND SOURCE src/SuperResolutionDLSS.cpp)
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
    ${SOURCE} ${INTERFACE} ${INCLUDE}
    readme.md
)
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
    target_compile_definitions(Diligent-SuperResolution-static PRIVATE DILIGENT_DLSS_SUPPORTED=1)
endif()

if(DILIGENT_DSR_SUPPORTED)
    target_compile_definitions(Diligent-SuperResolution-static PRIVATE DILIGENT_DSR_SUPPORTED=1)
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
)

if(DILIGENT_DSR_SUPPORTED)
    target_link_libraries(Diligent-SuperResolution-static PRIVATE Diligent-GraphicsEngineD3D12-static DirectSR-Headers)
    target_include_directories(Diligent-SuperResolution-static PRIVATE ../GraphicsEngineD3D12/include)
endif()

if(DILIGENT_DLSS_SUPPORTED)
    set(DLSS_SDK_DIR ${FETCHCONTENT_BASE_DIR}/dlss-headers-src)
    target_include_directories(Diligent-SuperResolution-static PRIVATE ${DLSS_SDK_DIR}/include)

    # Link NGX static library (dynamic CRT /MD variant)
    target_link_libraries(Diligent-SuperResolution-static PRIVATE
        debug     ${DLSS_SDK_DIR}/lib/Windows_x86_64/x64/nvsdk_ngx_d_dbg.lib
        optimized ${DLSS_SDK_DIR}/lib/Windows_x86_64/x64/nvsdk_ngx_d.lib
    )

    if(D3D12_SUPPORTED)
        target_link_libraries(Diligent-SuperResolution-static PRIVATE Diligent-GraphicsEngineD3D12-static)
        target_include_directories(Diligent-SuperResolution-static PRIVATE ../GraphicsEngineD3D12/include)
    endif()
    if(D3D11_SUPPORTED)
        target_link_libraries(Diligent-SuperResolution-static PRIVATE Diligent-GraphicsEngineD3D11-static)
        target_include_directories(Diligent-SuperResolution-static PRIVATE ../GraphicsEngineD3D11/include)
    endif()
    if(VULKAN_SUPPORTED)
        target_link_libraries(Diligent-SuperResolution-static PRIVATE Diligent-GraphicsEngineVk-static Vulkan::Headers)
        target_include_directories(Diligent-SuperResolution-static PRIVATE ../GraphicsEngineVulkan/include)
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
