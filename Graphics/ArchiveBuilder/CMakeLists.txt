cmake_minimum_required (VERSION 3.6)

project(Diligent-ArchiveBuilder CXX)

set(INCLUDE
    include/ArchiveBuilderImpl.hpp
    include/DummyRenderDevice.hpp
    include/SerializableShaderImpl.hpp
    include/SerializableRenderPassImpl.hpp
    include/SerializableResourceSignatureImpl.hpp
    include/SerializedMemory.hpp
)

set(INTERFACE
    interface/ArchiveBuilder.h
    interface/ArchiveBuilderFactory.h
    interface/ArchiveBuilderFactoryLoader.h
)

set(SOURCE
    src/ArchiveBuilderImpl.cpp
    src/ArchiveBuilderImpl_PRS.cpp
    src/ArchiveBuilderImpl_PSO.cpp
    src/ArchiveBuilderFactory.cpp
    src/DummyRenderDevice.cpp
    src/SerializableShaderImpl.cpp
    src/SerializableResourceSignatureImpl.cpp
    src/SerializableRenderPassImpl.cpp
    src/SerializedMemory.cpp
)

add_library(Diligent-ArchiveBuilder-interface INTERFACE)

target_include_directories(Diligent-ArchiveBuilder-interface INTERFACE interface)

add_library(Diligent-ArchiveBuilder SHARED
    ${SOURCE} ${INTERFACE} ${INCLUDE}
)

target_link_libraries(Diligent-ArchiveBuilder PUBLIC Diligent-ArchiveBuilder-interface)

target_include_directories(Diligent-ArchiveBuilder
PRIVATE
    include
    ../GraphicsEngine/include
    ../GraphicsEngineD3DBase/include
    ../GraphicsEngineNextGenBase/include
    ../ShaderTools/include
)

target_link_libraries(Diligent-ArchiveBuilder
PRIVATE
    Diligent-BuildSettings
PUBLIC
    Diligent-PlatformInterface
    Diligent-Common
    Diligent-GraphicsAccessories
)

if(D3D11_SUPPORTED)
    target_link_libraries(Diligent-ArchiveBuilder PRIVATE Diligent-GraphicsEngineD3D11-static)
    target_include_directories(Diligent-ArchiveBuilder PRIVATE ../GraphicsEngineD3D11/include)
endif()

if(D3D12_SUPPORTED)
    target_link_libraries(Diligent-ArchiveBuilder PRIVATE Diligent-GraphicsEngineD3D12-static)
    target_include_directories(Diligent-ArchiveBuilder PRIVATE ../GraphicsEngineD3D12/include)
endif()

if(VULKAN_SUPPORTED)
    target_link_libraries(Diligent-ArchiveBuilder PRIVATE Diligent-GraphicsEngineVk-static)
    target_include_directories(Diligent-ArchiveBuilder
    PRIVATE
        ../GraphicsEngineVulkan/include
        ../../ThirdParty/Vulkan-Headers/include
    )
    target_compile_definitions(Diligent-ArchiveBuilder PRIVATE DILIGENT_NO_GLSLANG=$<BOOL:${DILIGENT_NO_GLSLANG}>)
endif()

if(GL_SUPPORTED OR GLES_SUPPORTED)
    target_link_libraries(Diligent-ArchiveBuilder PRIVATE Diligent-GraphicsEngineOpenGL-static)
    target_include_directories(Diligent-ArchiveBuilder PRIVATE ../GraphicsEngineOpenGL/include)
endif()

if(METAL_SUPPORTED)
    target_link_libraries(Diligent-ArchiveBuilder PRIVATE Diligent-GraphicsEngineMetal-static)
    target_include_directories(Diligent-ArchiveBuilder PRIVATE ../GraphicsEngineMetal/include)
endif()

if(MSVC)
    target_sources(Diligent-ArchiveBuilder PRIVATE
        src/DLLMain.cpp
        src/ArchiveBuilder.def
    )
endif()

source_group("source" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

source_group("dll" FILES
    src/DLLMain.cpp
    src/ArchiveBuilder.def
)

set_target_properties(Diligent-ArchiveBuilder PROPERTIES
    FOLDER DiligentCore/Graphics
)

if(PLATFORM_WIN32)
    # Do not add 'lib' prefix when building with MinGW
    set_target_properties(Diligent-ArchiveBuilder PROPERTIES PREFIX "")

    # Set output name to ArchiveBuilder{32|64}{r|d}
    set_dll_output_name(Diligent-ArchiveBuilder ArchiveBuilder)

else()
    set_target_properties(Diligent-ArchiveBuilder PROPERTIES
        OUTPUT_NAME ArchiveBuilder
    )
endif()
