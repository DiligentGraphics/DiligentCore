cmake_minimum_required (VERSION 3.8)

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
	${CMAKE_CURRENT_BINARY_DIR}
	include
)

target_include_directories(GraphicsEngineD3D12-shared 
PUBLIC
	interface
PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}	
	include
)

# Create custom target to compile shaders, which GraphicsEngineD3D12-static/shared will depend on
add_custom_target(GenerateMipsShaders 
SOURCES 
	shaders/GenerateMips/GenerateMipsCS.hlsli
)

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/GenerateMips)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

foreach(FILE ${SHADERS})
  get_filename_component(FILE_NAME_WE ${FILE} NAME_WE)
  get_filename_component(FILE_NAME ${FILE} NAME)
  set(OUT_FILE ${SHADER_OUTPUT_DIR}/${FILE_NAME_WE}.h)
  add_custom_command(TARGET GenerateMipsShaders 
					 # Unfortunately it is not possible to set TARGET directly to GraphicsEngineD3D12-static/shared
					 # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
					 # PRE_BUILD is treated as PRE_LINK.
                     COMMAND ${FXC} /T cs_5_0 ${FILE_NAME} /E "main" /O3 /Vn g_p${FILE_NAME_WE} /Fh ${OUT_FILE} /nologo
                     MAIN_DEPENDENCY ${FILE} # the primary input source file to the command
					 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shaders/GenerateMips/"
                     COMMENT "Compiling shader ${FILE}"
                     VERBATIM
  )
endforeach(FILE)

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
add_dependencies(GraphicsEngineD3D12-static GenerateMipsShaders)

target_link_libraries(GraphicsEngineD3D12-shared PRIVATE ${PRIVATE_DEPENDENCIES} PUBLIC ${PUBLIC_DEPENDENCIES})
add_dependencies(GraphicsEngineD3D12-shared GenerateMipsShaders)
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
set_target_properties(GenerateMipsShaders PROPERTIES
	FOLDER Core/Graphics/Helper
)

set_source_files_properties(
    readme.md PROPERTIES HEADER_FILE_ONLY TRUE
)

foreach(FILE ${SHADERS})
	set_source_files_properties(
		${FILE} PROPERTIES HEADER_FILE_ONLY TRUE
	)
endforeach(FILE)
