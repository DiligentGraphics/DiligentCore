cmake_minimum_required (VERSION 3.11)

include(../../BuildTools/CMake/BuildUtils.cmake)

project(Diligent-SuperResolution CXX)

set(INCLUDE
    include/SuperResolutionBase.hpp
    include/SuperResolutionFactoryBase.hpp
)

set(INTERFACE
    interface/SuperResolution.h
    interface/SuperResolutionFactory.h
    interface/SuperResolutionFactoryLoader.h
)

set(SOURCE
    src/SuperResolutionFactoryBase.cpp
)

if(D3D12_SUPPORTED)
    list(APPEND
    SOURCE
        src/SuperResolutionD3D12.cpp
        src/SuperResolutionFactoryD3D12.cpp
    )
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

if(D3D12_SUPPORTED)
    target_link_libraries(Diligent-SuperResolution-static PRIVATE Diligent-GraphicsEngineD3D12-static)
    target_include_directories(Diligent-SuperResolution-static PRIVATE ../GraphicsEngineD3D12/include)
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
