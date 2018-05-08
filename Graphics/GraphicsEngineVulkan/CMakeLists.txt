cmake_minimum_required (VERSION 3.10)

project(GraphicsEngineVk CXX)

set(INCLUDE 
    include/BufferVkImpl.h
    include/BufferViewVkImpl.h
    include/CommandContext.h
    include/CommandListVkImpl.h
    include/CommandQueueVkImpl.h
    include/VulkanResourceBase.h
    include/VulkanTypeConversions.h
    include/DescriptorHeap.h
    include/DeviceContextVkImpl.h
    include/DynamicUploadHeap.h
    include/FramebufferCache.h
    include/GenerateMips.h
    include/pch.h
    include/PipelineLayout.h
    include/PipelineStateVkImpl.h
    include/RenderDeviceVkImpl.h
    include/SamplerVkImpl.h
    include/ShaderVkImpl.h
    include/ShaderResourceBindingVkImpl.h
    include/ShaderResourceCacheVk.h
    include/ShaderResourceLayoutVk.h
    include/SwapChainVkImpl.h
    include/TextureVkImpl.h
    include/TextureViewVkImpl.h
    include/VulkanErrors.h
)

set(VULKAN_UTILS_INCLUDE 
    include/VulkanUtilities/VulkanCommandBuffer.h
    include/VulkanUtilities/VulkanCommandBufferPool.h
    include/VulkanUtilities/VulkanDebug.h
    include/VulkanUtilities/VulkanFencePool.h
    include/VulkanUtilities/VulkanInstance.h
    include/VulkanUtilities/VulkanLogicalDevice.h
    include/VulkanUtilities/VulkanObjectWrappers.h
    include/VulkanUtilities/VulkanPhysicalDevice.h
)


set(INTERFACE 
    interface/BufferVk.h
    interface/BufferViewVk.h
    interface/CommandQueueVk.h
    interface/DeviceContextVk.h
    interface/PipelineStateVk.h
    interface/RenderDeviceVk.h
    interface/RenderDeviceFactoryVk.h
    interface/SamplerVk.h
    interface/ShaderVk.h
    interface/ShaderResourceBindingVk.h
    interface/SwapChainVk.h
    interface/TextureVk.h
    interface/TextureViewVk.h
)


set(SRC 
    src/BufferVkImpl.cpp
    src/BufferViewVkImpl.cpp
    src/CommandContext.cpp
    src/CommandQueueVkImpl.cpp
    src/VulkanResourceBase.cpp
    src/VulkanTypeConversions.cpp
    src/DescriptorHeap.cpp
    src/DeviceContextVkImpl.cpp
    src/DynamicUploadHeap.cpp
    src/FramebufferCache.cpp
    src/GenerateMips.cpp
    src/PipelineLayout.cpp
    src/PipelineStateVkImpl.cpp
    src/RenderDeviceVkImpl.cpp
    src/RenderDeviceFactoryVk.cpp
    src/SamplerVkImpl.cpp
    src/ShaderVkImpl.cpp
    src/ShaderResourceBindingVkImpl.cpp
    src/ShaderResourceCacheVk.cpp
    src/ShaderResourceLayoutVk.cpp
    src/SwapChainVkImpl.cpp
    src/TextureVkImpl.cpp
    src/TextureViewVkImpl.cpp
)

set(VULKAN_UTILS_SRC
    src/VulkanUtilities/VulkanCommandBuffer.cpp
    src/VulkanUtilities/VulkanCommandBufferPool.cpp
    src/VulkanUtilities/VulkanDebug.cpp
    src/VulkanUtilities/VulkanFencePool.cpp
    src/VulkanUtilities/VulkanInstance.cpp
    src/VulkanUtilities/VulkanLogicalDevice.cpp
    src/VulkanUtilities/VulkanPhysicalDevice.cpp
)

#set(SHADERS
#    shaders/GenerateMips/GenerateMipsGammaCS.hlsl
#    shaders/GenerateMips/GenerateMipsGammaOddCS.hlsl
#    shaders/GenerateMips/GenerateMipsGammaOddXCS.hlsl
#    shaders/GenerateMips/GenerateMipsGammaOddYCS.hlsl
#    shaders/GenerateMips/GenerateMipsLinearCS.hlsl
#    shaders/GenerateMips/GenerateMipsLinearOddCS.hlsl
#    shaders/GenerateMips/GenerateMipsLinearOddXCS.hlsl
#    shaders/GenerateMips/GenerateMipsLinearOddYCS.hlsl
#)
#
#set_source_files_properties(${SHADERS} PROPERTIES 
#    VS_SHADER_TYPE Compute
#    VS_SHADER_ENTRYPOINT main
#    VS_SHADER_MODEL 5.0
#    VS_SHADER_VARIABLE_NAME "g_p%(Filename)"
#    VS_SHADER_OUTPUT_HEADER_FILE "${CMAKE_CURRENT_BINARY_DIR}/CompiledShaders/GenerateMips/%(Filename).h"
#    #VS_SHADER_FLAGS "/O3"
#)

add_library(GraphicsEngineVkInterface INTERFACE)
target_include_directories(GraphicsEngineVkInterface
INTERFACE
    interface
)
target_link_libraries(GraphicsEngineVkInterface 
INTERFACE 
    GraphicsEngineInterface
)

add_library(GraphicsEngineVk-static STATIC 
    ${SRC} ${VULKAN_UTILS_SRC} ${INTERFACE} ${INCLUDE} ${VULKAN_UTILS_INCLUDE} ${SHADERS}
    readme.md
    #shaders/GenerateMips/GenerateMipsCS.hlsli
)

add_library(GraphicsEngineVk-shared SHARED 
    ${SRC} ${VULKAN_UTILS_SRC} ${INTERFACE} ${INCLUDE} ${VULKAN_UTILS_INCLUDE} ${SHADERS}
    src/DLLMain.cpp
    src/GraphicsEngineVk.def
    readme.md
    #shaders/GenerateMips/GenerateMipsCS.hlsli
)

target_include_directories(GraphicsEngineVk-static 
PRIVATE
    #${CMAKE_CURRENT_BINARY_DIR}/CompiledShaders
    include
    ../../External/vulkan
)

target_include_directories(GraphicsEngineVk-shared 
PRIVATE
    #${CMAKE_CURRENT_BINARY_DIR}/CompiledShaders
    include
    ../../External/vulkan
)

set(PRIVATE_DEPENDENCIES 
    BuildSettings 
    Common 
    TargetPlatform
    GraphicsEngine
    GLSLTools
)

if(PLATFORM_WIN32)
    find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS ../../External/vulkan/)    
    list(APPEND PRIVATE_DEPENDENCIES ${Vulkan_LIBRARY})
endif()

set(PUBLIC_DEPENDENCIES 
    GraphicsEngineVkInterface
)

target_link_libraries(GraphicsEngineVk-static PRIVATE ${PRIVATE_DEPENDENCIES} PUBLIC ${PUBLIC_DEPENDENCIES})

target_link_libraries(GraphicsEngineVk-shared PRIVATE ${PRIVATE_DEPENDENCIES} PUBLIC ${PUBLIC_DEPENDENCIES})
target_compile_definitions(GraphicsEngineVk-shared PUBLIC ENGINE_DLL=1)


if(PLATFORM_WIN32)
    set(PRIVATE_COMPILE_DEFINITIONS VK_USE_PLATFORM_WIN32_KHR=1 NOMINMAX)
endif()
target_compile_definitions(GraphicsEngineVk-shared PRIVATE ${PRIVATE_COMPILE_DEFINITIONS})
target_compile_definitions(GraphicsEngineVk-static PRIVATE ${PRIVATE_COMPILE_DEFINITIONS})

# Set output name to GraphicsEngineVk_{32|64}{r|d}
set_dll_output_name(GraphicsEngineVk-shared GraphicsEngineVk)

set_common_target_properties(GraphicsEngineVk-shared)
set_common_target_properties(GraphicsEngineVk-static)

source_group("src" FILES ${SRC})
source_group("src\\Vulkan Utilities" FILES ${VULKAN_UTILS_SRC})

source_group("dll" FILES 
    src/DLLMain.cpp
    src/GraphicsEngineVk.def
)

source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})
source_group("include\\Vulkan Utilities" FILES ${VULKAN_UTILS_INCLUDE})
#source_group("shaders" FILES 
#    ${SHADERS}
#    shaders/GenerateMips/GenerateMipsCS.hlsli
#)

set_target_properties(GraphicsEngineVk-static PROPERTIES
    FOLDER Core/Graphics
)
set_target_properties(GraphicsEngineVk-shared PROPERTIES
    FOLDER Core/Graphics
)

set_source_files_properties(
    readme.md PROPERTIES HEADER_FILE_ONLY TRUE
)
