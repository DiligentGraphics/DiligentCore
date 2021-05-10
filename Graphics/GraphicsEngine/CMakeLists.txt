cmake_minimum_required (VERSION 3.6)

project(Diligent-GraphicsEngine CXX)

set(INCLUDE 
    include/BufferBase.hpp
    include/BufferViewBase.hpp
    include/CommandListBase.hpp
    include/DefaultShaderSourceStreamFactory.h
    include/Defines.h
    include/DeviceContextBase.hpp
    include/DeviceObjectBase.hpp
    include/EngineFactoryBase.hpp
    include/EngineMemory.h
    include/FenceBase.hpp
    include/FramebufferBase.hpp
    include/IndexWrapper.hpp
    include/PipelineStateBase.hpp
    include/PrivateConstants.h
    include/QueryBase.hpp
    include/RenderDeviceBase.hpp
    include/RenderPassBase.hpp
    include/ResourceMappingImpl.hpp
    include/SamplerBase.hpp
    include/ShaderBase.hpp
    include/ShaderResourceBindingBase.hpp
    include/ShaderResourceCacheCommon.hpp
    include/ShaderResourceVariableBase.hpp
    include/StateObjectsRegistry.hpp
    include/SwapChainBase.hpp
    include/TextureBase.hpp
    include/TextureViewBase.hpp
    include/BottomLevelASBase.hpp
    include/TopLevelASBase.hpp
    include/ShaderBindingTableBase.hpp
    include/PipelineResourceSignatureBase.hpp
)

set(INTERFACE 
    interface/APIInfo.h
    interface/BlendState.h
    interface/Buffer.h
    interface/BufferView.h
    interface/CommandList.h
    interface/Constants.h
    interface/CommandQueue.h
    interface/DepthStencilState.h
    interface/DeviceContext.h
    interface/DeviceObject.h
    interface/EngineFactory.h
    interface/Fence.h
    interface/Framebuffer.h
    interface/GraphicsTypes.h
    interface/InputLayout.h
    interface/PipelineState.h
    interface/PipelineResourceSignature.h
    interface/Query.h
    interface/RasterizerState.h
    interface/RenderDevice.h
    interface/RenderPass.h
    interface/ResourceMapping.h
    interface/Sampler.h
    interface/Shader.h
    interface/ShaderResourceBinding.h
    interface/ShaderResourceVariable.h
    interface/SwapChain.h
    interface/Texture.h
    interface/TextureView.h
    interface/BottomLevelAS.h
    interface/TopLevelAS.h
    interface/ShaderBindingTable.h
)

set(SOURCE
    src/APIInfo.cpp
    src/BottomLevelASBase.cpp
    src/BufferBase.cpp
    src/DefaultShaderSourceStreamFactory.cpp
    src/DeviceContextBase.cpp
    src/EngineMemory.cpp
    src/EngineFactoryBase.cpp
    src/FramebufferBase.cpp
    src/PipelineResourceSignatureBase.cpp
    src/PipelineStateBase.cpp
    src/RenderDeviceBase.cpp
    src/ResourceMappingBase.cpp
    src/ShaderBindingTableBase.cpp
    src/RenderPassBase.cpp
    src/TextureBase.cpp
    src/TopLevelASBase.cpp
)

add_library(Diligent-GraphicsEngine STATIC ${SOURCE} ${INTERFACE} ${INCLUDE})
set_common_target_properties(Diligent-GraphicsEngine)

target_include_directories(Diligent-GraphicsEngine 
PUBLIC
    include
    interface
)

target_link_libraries(Diligent-GraphicsEngine 
PRIVATE
    Diligent-BuildSettings
PUBLIC
    Diligent-PlatformInterface
    Diligent-Common
    Diligent-GraphicsAccessories
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(Diligent-GraphicsEngine PROPERTIES
    FOLDER DiligentCore/Graphics
)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-GraphicsEngine)
endif()