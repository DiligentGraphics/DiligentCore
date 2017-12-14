cmake_minimum_required (VERSION 3.10)

project(GraphicsEngineD3D12 CXX)

set(INCLUDE 
	include/BufferD3D12Impl.h
	include/BufferViewD3D12Impl.h
	include/CommandContext.h
	include/CommandListD3D12Impl.h
	include/CommandListManager.h
	include/CommandQueueD3D12Impl.h
	include/D3D12ResourceBase.h
	include/D3D12TypeConversions.h
	include/D3D12TypeDefinitions.h
	include/D3D12Utils.h
	include/d3dx12_win.h
	include/DescriptorHeap.h
	include/DeviceContextD3D12Impl.h
	include/DynamicUploadHeap.h
	include/GenerateMips.h
	include/pch.h
	include/PipelineStateD3D12Impl.h
	include/RenderDeviceD3D12Impl.h
	include/RootSignature.h
	include/SamplerD3D12Impl.h
	include/ShaderD3D12Impl.h
	include/ShaderResourceBindingD3D12Impl.h
	include/ShaderResourceCacheD3D12.h
	include/ShaderResourceLayoutD3D12.h
	include/ShaderResourcesD3D12.h
	include/SwapChainD3D12Impl.h
	include/TextureD3D12Impl.h
	include/TextureViewD3D12Impl.h
)

set(INTERFACE 
	interface/BufferD3D12.h
	interface/BufferViewD3D12.h
	interface/CommandQueueD3D12.h
	interface/DeviceContextD3D12.h
	interface/PipelineStateD3D12.h
	interface/RenderDeviceD3D12.h
	interface/RenderDeviceFactoryD3D12.h
	interface/SamplerD3D12.h
	interface/ShaderD3D12.h
	interface/ShaderResourceBindingD3D12.h
	interface/SwapChainD3D12.h
	interface/TextureD3D12.h
	interface/TextureViewD3D12.h
)


set(SRC 
	src/BufferD3D12Impl.cpp
	src/BufferViewD3D12Impl.cpp
	src/CommandContext.cpp
	src/CommandListManager.cpp
	src/CommandQueueD3D12Impl.cpp
	src/D3D12ResourceBase.cpp
	src/D3D12TypeConversions.cpp
	src/D3D12Utils.cpp
	src/DescriptorHeap.cpp
	src/DeviceContextD3D12Impl.cpp
	src/DynamicUploadHeap.cpp
	src/GenerateMips.cpp
	src/PipelineStateD3D12Impl.cpp
	src/RenderDeviceD3D12Impl.cpp
	src/RenderDeviceFactoryD3D12.cpp
	src/RootSignature.cpp
	src/SamplerD3D12Impl.cpp
	src/ShaderD3D12Impl.cpp
	src/ShaderResourceBindingD3D12Impl.cpp
	src/ShaderResourceCacheD3D12.cpp
	src/ShaderResourceLayoutD3D12.cpp
	src/ShaderResourcesD3D12.cpp
	src/SwapChainD3D12Impl.cpp
	src/TextureD3D12Impl.cpp
	src/TextureViewD3D12Impl.cpp
)

set(SHADERS
	shaders/GenerateMips/GenerateMipsGammaCS.hlsl
	shaders/GenerateMips/GenerateMipsGammaOddCS.hlsl
	shaders/GenerateMips/GenerateMipsGammaOddXCS.hlsl
	shaders/GenerateMips/GenerateMipsGammaOddYCS.hlsl
	shaders/GenerateMips/GenerateMipsLinearCS.hlsl
	shaders/GenerateMips/GenerateMipsLinearOddCS.hlsl
	shaders/GenerateMips/GenerateMipsLinearOddXCS.hlsl
	shaders/GenerateMips/GenerateMipsLinearOddYCS.hlsl
)

set_property(SOURCE ${SHADERS} PROPERTY VS_SHADER_TYPE Compute)
set_property(SOURCE ${SHADERS} PROPERTY VS_SHADER_ENTRYPOINT main)
set_property(SOURCE ${SHADERS} PROPERTY VS_SHADER_MODEL 5.0)
set_property(SOURCE ${SHADERS} PROPERTY VS_SHADER_VARIABLE_NAME "g_p%(Filename)")
set_property(SOURCE ${SHADERS} PROPERTY VS_SHADER_OUTPUT_HEADER_FILE "${CMAKE_CURRENT_BINARY_DIR}/CompiledShaders/GenerateMips/%(Filename).h")
#set_property(SOURCE ${SHADERS} PROPERTY VS_SHADER_FLAGS "/O3")

add_library(GraphicsEngineD3D12-static STATIC 
	${SRC} ${INTERFACE} ${INCLUDE} ${SHADERS}
	readme.md
	shaders/GenerateMips/GenerateMipsCS.hlsli
)

add_library(GraphicsEngineD3D12-shared SHARED 
	${SRC} ${INTERFACE} ${INCLUDE} ${SHADERS}
	src/DLLMain.cpp
	src/GraphicsEngineD3D12.def
	readme.md
	shaders/GenerateMips/GenerateMipsCS.hlsli
)

target_include_directories(GraphicsEngineD3D12-static 
PUBLIC
	interface
PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}/CompiledShaders
	include
)

target_include_directories(GraphicsEngineD3D12-shared 
PUBLIC
	interface
PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}/CompiledShaders
	include
)

set(PRIVATE_DEPENDENCIES 
	BuildSettings 
	Common 
	GraphicsEngineD3DBase 
	GraphicsTools 
	TargetPlatform 
	dxgi.lib
	D3D12.lib 
	d3dcompiler.lib
)

set(PUBLIC_DEPENDENCIES 
	GraphicsEngine 
)

target_link_libraries(GraphicsEngineD3D12-static PRIVATE ${PRIVATE_DEPENDENCIES} PUBLIC ${PUBLIC_DEPENDENCIES})

target_link_libraries(GraphicsEngineD3D12-shared PRIVATE ${PRIVATE_DEPENDENCIES} PUBLIC ${PUBLIC_DEPENDENCIES})
target_compile_definitions(GraphicsEngineD3D12-shared PUBLIC ENGINE_DLL)

set_target_properties(GraphicsEngineD3D12-shared PROPERTIES
	OUTPUT_NAME_DEBUG GraphicsEngineD3D12${DLL_DBG_SUFFIX}
	OUTPUT_NAME_RELEASE GraphicsEngineD3D12${DLL_REL_SUFFIX}
	OUTPUT_NAME_RELWITHDEBINFO GraphicsEngineD3D12${DLL_REL_SUFFIX}
	OUTPUT_NAME_MINSIZEREL GraphicsEngineD3D12${DLL_REL_SUFFIX}
)

set_common_target_properties(GraphicsEngineD3D12-shared)
set_common_target_properties(GraphicsEngineD3D12-static)

source_group("src" FILES ${SRC})

source_group("dll" FILES 
	src/DLLMain.cpp
	src/GraphicsEngineD3D12.def
)

source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})
source_group("shaders" FILES 
	${SHADERS}
	shaders/GenerateMips/GenerateMipsCS.hlsli
)

set_target_properties(GraphicsEngineD3D12-static PROPERTIES
	FOLDER Core/Graphics
)
set_target_properties(GraphicsEngineD3D12-shared PROPERTIES
	FOLDER Core/Graphics
)

set_source_files_properties(
    readme.md PROPERTIES HEADER_FILE_ONLY TRUE
)
