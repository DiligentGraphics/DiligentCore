cmake_minimum_required (VERSION 3.3)

project(GraphicsEngine CXX)

set(INCLUDE 
	include/BufferBase.h
	include/BufferViewBase.h
	include/CommandListBase.h
	include/Defines.h
	include/DeviceContextBase.h
	include/DeviceObjectBase.h
	include/EngineMemory.h
	include/pch.h
	include/PipelineStateBase.h
	include/RenderDeviceBase.h
	include/ResourceMappingImpl.h
	include/SamplerBase.h
	include/ShaderBase.h
	include/ShaderResourceBindingBase.h
	include/StateObjectsRegistry.h
	include/SwapChainBase.h
	include/TextureBase.h
	include/TextureViewBase.h
)

set(INTERFACE 
	interface/BlendState.h
	interface/Buffer.h
	interface/BufferView.h
	interface/CommandList.h
	interface/Constants.h
	interface/DepthStencilState.h
	interface/DeviceCaps.h
	interface/DeviceContext.h
	interface/DeviceObject.h
	interface/GraphicsTypes.h
	interface/InputLayout.h
	interface/MapHelper.h
	interface/PipelineState.h
	interface/RasterizerState.h
	interface/RenderDevice.h
	interface/ResourceMapping.h
	interface/Sampler.h
	interface/Shader.h
	interface/ShaderResourceBinding.h
	interface/SwapChain.h
	interface/Texture.h
	interface/TextureView.h
)

set(SOURCE 
	src/EngineMemory.cpp
	src/ResourceMapping.cpp
	src/Texture.cpp
)

add_library(GraphicsEngine STATIC ${SOURCE} ${INTERFACE} ${INCLUDE})

target_include_directories(GraphicsEngine 
PUBLIC
	include
	interface
PRIVATE
	../GraphicsTools/include
)

set_target_properties(GraphicsEngine PROPERTIES
	FOLDER Core/Graphics
)
target_link_libraries(GraphicsEngine PRIVATE BuildSettings PlatformInterface Common)

if(MSVC)
	# Enable link-time code generation for release builds (I was not able to 
	# find any way to set these settings through interface library BuildSettings)
	set_target_properties(GraphicsEngine PROPERTIES
		STATIC_LIBRARY_FLAGS_RELEASE /LTCG
		STATIC_LIBRARY_FLAGS_MINSIZEREL /LTCG
		STATIC_LIBRARY_FLAGS_RELWITHDEBINFO /LTCG
	)
endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})
