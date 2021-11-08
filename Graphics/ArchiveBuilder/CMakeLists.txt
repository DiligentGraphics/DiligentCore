cmake_minimum_required (VERSION 3.6)

project(Diligent-Archiver CXX)

set(INCLUDE
    include/ArchiverImpl.hpp
    include/SerializationDeviceImpl.hpp
    include/SerializableShaderImpl.hpp
    include/SerializableRenderPassImpl.hpp
    include/SerializableResourceSignatureImpl.hpp
    include/SerializedMemory.hpp
)

set(INTERFACE
    interface/Archiver.h
    interface/ArchiverFactory.h
    interface/ArchiverFactoryLoader.h
    interface/SerializationDevice.h
)

set(SOURCE
    src/ArchiverImpl.cpp
    src/ArchiverImpl_PRS.cpp
    src/ArchiverImpl_PSO.cpp
    src/ArchiverFactory.cpp
    src/SerializationDeviceImpl.cpp
    src/SerializableShaderImpl.cpp
    src/SerializableResourceSignatureImpl.cpp
    src/SerializableRenderPassImpl.cpp
    src/SerializedMemory.cpp
)

add_library(Diligent-Archiver-interface INTERFACE)

target_include_directories(Diligent-Archiver-interface INTERFACE interface)

add_library(Diligent-Archiver SHARED
    ${SOURCE} ${INTERFACE} ${INCLUDE}
)

target_link_libraries(Diligent-Archiver PUBLIC Diligent-Archiver-interface)

target_include_directories(Diligent-Archiver
PRIVATE
    include
    ../GraphicsEngine/include
    ../GraphicsEngineD3DBase/include
    ../GraphicsEngineNextGenBase/include
    ../ShaderTools/include
)

target_link_libraries(Diligent-Archiver
PRIVATE
    Diligent-BuildSettings
PUBLIC
    Diligent-PlatformInterface
    Diligent-Common
    Diligent-GraphicsAccessories
)

if(D3D11_SUPPORTED)
    target_link_libraries(Diligent-Archiver PRIVATE Diligent-GraphicsEngineD3D11-static)
    target_include_directories(Diligent-Archiver PRIVATE ../GraphicsEngineD3D11/include)
endif()

if(D3D12_SUPPORTED)
    target_link_libraries(Diligent-Archiver PRIVATE Diligent-GraphicsEngineD3D12-static)
    target_include_directories(Diligent-Archiver PRIVATE ../GraphicsEngineD3D12/include)
endif()

if(VULKAN_SUPPORTED)
    target_link_libraries(Diligent-Archiver PRIVATE Diligent-GraphicsEngineVk-static)
    target_include_directories(Diligent-Archiver
    PRIVATE
        ../GraphicsEngineVulkan/include
        ../../ThirdParty/Vulkan-Headers/include
    )
    target_compile_definitions(Diligent-Archiver PRIVATE DILIGENT_NO_GLSLANG=$<BOOL:${DILIGENT_NO_GLSLANG}>)
endif()

if(GL_SUPPORTED OR GLES_SUPPORTED)
    target_link_libraries(Diligent-Archiver PRIVATE Diligent-GraphicsEngineOpenGL-static)
    target_include_directories(Diligent-Archiver PRIVATE ../GraphicsEngineOpenGL/include)
endif()

if(METAL_SUPPORTED)
    target_link_libraries(Diligent-Archiver PRIVATE Diligent-GraphicsEngineMetal-static)
    target_include_directories(Diligent-Archiver PRIVATE ../GraphicsEngineMetal/include)
endif()

if(MSVC)
    target_sources(Diligent-Archiver PRIVATE
        src/DLLMain.cpp
        src/Archiver.def
    )
endif()

source_group("source" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

source_group("dll" FILES
    src/DLLMain.cpp
    src/Archiver.def
)

set_target_properties(Diligent-Archiver PROPERTIES
    FOLDER DiligentCore/Graphics
)

if(PLATFORM_WIN32)
    # Do not add 'lib' prefix when building with MinGW
    set_target_properties(Diligent-Archiver PROPERTIES PREFIX "")

    # Set output name to Archiver{32|64}{r|d}
    set_dll_output_name(Diligent-Archiver Archiver)

else()
    set_target_properties(Diligent-Archiver PROPERTIES
        OUTPUT_NAME Archiver
    )
endif()
