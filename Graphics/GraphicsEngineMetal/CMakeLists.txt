cmake_minimum_required (VERSION 3.10)

project(GraphicsEngineMetal CXX)


set(INCLUDE 
    include/BufferMtlImpl.h
    include/BufferViewMtlImpl.h
    include/CommandListMtlImpl.h
    include/MtlTypeConversions.h
    include/DeviceContextMtlImpl.h
	include/FenceMtlImpl.h
    include/PipelineStateMtlImpl.h
    include/RenderDeviceMtlImpl.h
    include/SamplerMtlImpl.h
    include/ShaderMtlImpl.h
    include/ShaderResourceBindingMtlImpl.h
    include/SwapChainMtlImpl.h
    include/TextureMtlImpl.h
    include/TextureViewMtlImpl.h
)

set(INTERFACE 
    interface/BufferMtl.h
    interface/BufferViewMtl.h
    interface/DeviceContextMtl.h
    interface/EngineMtlAttribs.h
    interface/FenceMtl.h
    interface/PipelineStateMtl.h
    interface/RenderDeviceMtl.h
    interface/RenderDeviceFactoryMtl.h
    interface/SamplerMtl.h
    interface/ShaderMtl.h
    interface/ShaderResourceBindingMtl.h
    interface/SwapChainMtl.h
    interface/TextureMtl.h
    interface/TextureViewMtl.h
)


set(SRC 
    src/BufferMtlImpl.mm
    src/BufferViewMtlImpl.mm
    src/CommandListMtlImpl.mm
    src/MtlTypeConversions.mm
    src/DeviceContextMtlImpl.mm
    src/FenceMtlImpl.mm
    src/PipelineStateMtlImpl.mm
    src/RenderDeviceMtlImpl.mm
    src/RenderDeviceFactoryMtl.mm
    src/SamplerMtlImpl.mm
    src/ShaderMtlImpl.mm
    src/ShaderResourceBindingMtlImpl.mm
    src/SwapChainMtlImpl.mm
    src/TextureMtlImpl.mm
    src/TextureViewMtlImpl.mm
)

add_library(GraphicsEngineMetalInterface INTERFACE)
target_include_directories(GraphicsEngineMetalInterface
INTERFACE
    interface
)
target_link_libraries(GraphicsEngineMetalInterface 
INTERFACE 
    GraphicsEngineInterface
)


add_library(GraphicsEngineMetal-static STATIC 
    ${SRC} ${INTERFACE} ${INCLUDE}
    readme.md
)

add_library(GraphicsEngineMetal-shared SHARED 
    readme.md
)

set_target_properties(GraphicsEngineMetal-shared PROPERTIES
    OUTPUT_NAME GraphicsEngineMetal
)

set_common_target_properties(GraphicsEngineMetal-shared)
set_common_target_properties(GraphicsEngineMetal-static)

target_include_directories(GraphicsEngineMetal-static 
PRIVATE
    include
)

find_library(METAL_FRAMEWORK Metal)
if (NOT METAL_FRAMEWORK)
    message(FATAL_ERROR "Metal framework is not found")
endif()

target_link_libraries(GraphicsEngineMetal-static
PRIVATE
    BuildSettings 
    TargetPlatform
    Common
    GraphicsEngine
    ${METAL_FRAMEWORK}
PUBLIC
    GraphicsEngineMetalInterface
)

target_link_libraries(GraphicsEngineMetal-shared PUBLIC GraphicsEngineMetal-static)
target_compile_definitions(GraphicsEngineMetal-shared PUBLIC ENGINE_DLL=1)


if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR 
    CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set_target_properties(GraphicsEngineMetal-shared PROPERTIES CXX_VISIBILITY_PRESET hidden) # -fvisibility=hidden
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Disable the following clang warning
    #    '<function name>' hides overloaded virtual function
    # as hiding is intended
    target_compile_options(GraphicsEngineMetal-static PRIVATE -Wno-overloaded-virtual)
    target_compile_options(GraphicsEngineMetal-shared PRIVATE -Wno-overloaded-virtual)
endif()


source_group("src" FILES ${SRC} )
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(GraphicsEngineMetal-static PROPERTIES
    FOLDER Core/Graphics
)
set_target_properties(GraphicsEngineMetal-shared PROPERTIES
    FOLDER Core/Graphics
)

set_source_files_properties(
    readme.md PROPERTIES HEADER_FILE_ONLY TRUE
)

if(INSTALL_DILIGENT_CORE)
    install_core_lib(GraphicsEngineMetal-shared)
    install_core_lib(GraphicsEngineMetal-static)
endif()
