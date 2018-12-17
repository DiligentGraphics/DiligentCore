cmake_minimum_required (VERSION 3.10)

project(GraphicsEngineD3D11 CXX)


set(INCLUDE 
    include/BufferD3D11Impl.h
    include/BufferViewD3D11Impl.h
    include/CommandListD3D11Impl.h
    include/D3D11TypeConversions.h
    include/D3D11TypeDefinitions.h
    include/DeviceContextD3D11Impl.h
    include/EngineD3D11Defines.h
    include/pch.h
	include/FenceD3D11Impl.h
    include/PipelineStateD3D11Impl.h
    include/RenderDeviceD3D11Impl.h
    include/SamplerD3D11Impl.h
    include/ShaderD3D11Impl.h
    include/ShaderResourceBindingD3D11Impl.h
    include/ShaderResourceCacheD3D11.h
    include/ShaderResourceLayoutD3D11.h
    include/ShaderResourcesD3D11.h
    include/SwapChainD3D11Impl.h
    include/targetver.h
    include/Texture1D_D3D11.h
    include/Texture2D_D3D11.h
    include/Texture3D_D3D11.h
    include/TextureBaseD3D11.h
    include/TextureViewD3D11Impl.h
)

set(INTERFACE 
    interface/BufferD3D11.h
    interface/BufferViewD3D11.h
    interface/DeviceContextD3D11.h
    interface/EngineD3D11Attribs.h
    interface/FenceD3D11.h
    interface/PipelineStateD3D11.h
    interface/RenderDeviceD3D11.h
    interface/RenderDeviceFactoryD3D11.h
    interface/SamplerD3D11.h
    interface/ShaderD3D11.h
    interface/ShaderResourceBindingD3D11.h
    interface/SwapChainD3D11.h
    interface/TextureD3D11.h
    interface/TextureViewD3D11.h
)


set(SRC 
    src/BufferD3D11Impl.cpp
    src/BufferViewD3D11Impl.cpp
    src/CommandListD3D11Impl.cpp
    src/D3D11TypeConversions.cpp
    src/DeviceContextD3D11Impl.cpp
    src/FenceD3D11Impl.cpp
    src/GUIDDef.cpp
    src/PipelineStateD3D11Impl.cpp
    src/RenderDeviceD3D11Impl.cpp
    src/RenderDeviceFactoryD3D11.cpp
    src/SamplerD3D11Impl.cpp
    src/ShaderD3D11Impl.cpp
    src/ShaderResourceBindingD3D11Impl.cpp
    src/ShaderResourceCacheD3D11.cpp
    src/ShaderResourceLayoutD3D11.cpp
    src/ShaderResourcesD3D11.cpp
    src/SwapChainD3D11Impl.cpp
    src/Texture1D_D3D11.cpp
    src/Texture2D_D3D11.cpp
    src/Texture3D_D3D11.cpp
    src/TextureBaseD3D11.cpp
    src/TextureViewD3D11Impl.cpp
)

add_library(GraphicsEngineD3D11Interface INTERFACE)
target_include_directories(GraphicsEngineD3D11Interface
INTERFACE
    interface
)
target_link_libraries(GraphicsEngineD3D11Interface 
INTERFACE 
    GraphicsEngineInterface
)


add_library(GraphicsEngineD3D11-static STATIC 
    ${SRC} ${INTERFACE} ${INCLUDE}
    readme.md
)

add_library(GraphicsEngineD3D11-shared SHARED 
    src/DLLMain.cpp
    src/GraphicsEngineD3D11.def
    readme.md
)

# Set output name to GraphicsEngineD3D11_{32|64}{r|d}
set_dll_output_name(GraphicsEngineD3D11-shared GraphicsEngineD3D11)

set_common_target_properties(GraphicsEngineD3D11-shared)
set_common_target_properties(GraphicsEngineD3D11-static)

target_include_directories(GraphicsEngineD3D11-static 
PRIVATE
    include
)

target_link_libraries(GraphicsEngineD3D11-static
PRIVATE
    BuildSettings 
    GraphicsEngineD3DBase 
    TargetPlatform
    Common
    dxgi.lib
    d3d11.lib 
    d3dcompiler.lib
PUBLIC
    GraphicsEngineD3D11Interface
)

target_link_libraries(GraphicsEngineD3D11-shared PUBLIC GraphicsEngineD3D11-static)
target_compile_definitions(GraphicsEngineD3D11-shared PUBLIC ENGINE_DLL=1)

source_group("src" FILES ${SRC} )

source_group("dll" FILES 
    src/DLLMain.cpp
    src/GraphicsEngineD3D11.def
)

source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(GraphicsEngineD3D11-static PROPERTIES
    FOLDER Core/Graphics
)
set_target_properties(GraphicsEngineD3D11-shared PROPERTIES
    FOLDER Core/Graphics
)

set_source_files_properties(
    readme.md PROPERTIES HEADER_FILE_ONLY TRUE
)

if(INSTALL_DILIGENT_CORE)
    install_core_lib(GraphicsEngineD3D11-shared)
    install_core_lib(GraphicsEngineD3D11-static)
endif()