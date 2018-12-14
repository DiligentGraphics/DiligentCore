cmake_minimum_required (VERSION 3.10)

project(GraphicsEngineVk CXX)

set(INCLUDE 
    include/BufferVkImpl.h
    include/BufferViewVkImpl.h
    include/CommandListVkImpl.h
    include/CommandPoolManager.h
    include/CommandQueueVkImpl.h
    include/DescriptorPoolManager.h
    include/DeviceContextVkImpl.h
    include/FenceVkImpl.h
    include/VulkanDynamicHeap.h
    include/FramebufferCache.h
    include/GenerateMipsVkHelper.h
    include/pch.h
    include/PipelineLayout.h
    include/PipelineStateVkImpl.h
    include/RenderDeviceVkImpl.h
    include/RenderPassCache.h
    include/SamplerVkImpl.h
    include/ShaderVkImpl.h
    include/ShaderResourceBindingVkImpl.h
    include/ShaderResourceCacheVk.h
    include/ShaderResourceLayoutVk.h
    include/ShaderVariableVk.h
    include/SwapChainVkImpl.h
    include/TextureVkImpl.h
    include/TextureViewVkImpl.h
    include/VulkanErrors.h
    include/VulkanTypeConversions.h
    include/VulkanUploadHeap.h
)

set(VULKAN_UTILS_INCLUDE 
    include/VulkanUtilities/VulkanCommandBuffer.h
    include/VulkanUtilities/VulkanCommandBufferPool.h
    include/VulkanUtilities/VulkanDebug.h
    include/VulkanUtilities/VulkanFencePool.h
    include/VulkanUtilities/VulkanInstance.h
    include/VulkanUtilities/VulkanLogicalDevice.h
    include/VulkanUtilities/VulkanMemoryManager.h
    include/VulkanUtilities/VulkanObjectWrappers.h
    include/VulkanUtilities/VulkanPhysicalDevice.h
)


set(INTERFACE 
    interface/BufferVk.h
    interface/BufferViewVk.h
    interface/CommandQueueVk.h
    interface/DeviceContextVk.h
    interface/FenceVk.h
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
    src/CommandPoolManager.cpp
    src/CommandQueueVkImpl.cpp
    src/DescriptorPoolManager.cpp
    src/DeviceContextVkImpl.cpp
    src/FenceVkImpl.cpp
    src/VulkanDynamicHeap.cpp
    src/FramebufferCache.cpp
    src/GenerateMipsVkHelper.cpp
    src/PipelineLayout.cpp
    src/PipelineStateVkImpl.cpp
    src/RenderDeviceVkImpl.cpp
    src/RenderPassCache.cpp
    src/RenderDeviceFactoryVk.cpp
    src/SamplerVkImpl.cpp
    src/ShaderVkImpl.cpp
    src/ShaderResourceBindingVkImpl.cpp
    src/ShaderResourceCacheVk.cpp
    src/ShaderResourceLayoutVk.cpp
    src/ShaderVariableVk.cpp
    src/SwapChainVkImpl.cpp
    src/TextureVkImpl.cpp
    src/TextureViewVkImpl.cpp
    src/VulkanTypeConversions.cpp
    src/VulkanUploadHeap.cpp
)

set(VULKAN_UTILS_SRC
    src/VulkanUtilities/VulkanCommandBuffer.cpp
    src/VulkanUtilities/VulkanCommandBufferPool.cpp
    src/VulkanUtilities/VulkanDebug.cpp
    src/VulkanUtilities/VulkanFencePool.cpp
    src/VulkanUtilities/VulkanInstance.cpp
    src/VulkanUtilities/VulkanLogicalDevice.cpp
    src/VulkanUtilities/VulkanMemoryManager.cpp
    src/VulkanUtilities/VulkanPhysicalDevice.cpp
)

set(GENERATE_MIPS_SHADER
    shaders/GenerateMipsCS.csh
)
set(GENERATE_MIPS_SHADER_INC
    shaders/GenerateMipsCS_inc.h
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/${GENERATE_MIPS_SHADER_INC}
    PROPERTIES GENERATED TRUE
)

# Create custom target to convert GenerateMipsCS.csh to GenerateMipsCS_inc.h
add_custom_target(ProcessGenerateMipsVkShader
SOURCES
    ${GENERATE_MIPS_SHADER}
)

add_custom_command(TARGET ProcessGenerateMipsVkShader 
                   # Unfortunately it is not possible to set TARGET directly to GraphicsEngineVk-*
                   # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
                   # PRE_BUILD is treated as PRE_LINK.
                   COMMAND ${FILE2STRING_PATH} ${GENERATE_MIPS_SHADER} ${GENERATE_MIPS_SHADER_INC}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   COMMENT "Processing GenerateMipsCS.csh"
                   VERBATIM
)


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
    ${SRC} ${VULKAN_UTILS_SRC} ${INTERFACE} ${INCLUDE} ${VULKAN_UTILS_INCLUDE} ${GENERATE_MIPS_SHADER} ${GENERATE_MIPS_SHADER_INC}
    readme.md
)

add_library(GraphicsEngineVk-shared SHARED 
    readme.md
)

if(MSVC)
    target_sources(GraphicsEngineVk-shared PRIVATE
        src/DLLMain.cpp
        src/GraphicsEngineVk.def
    )
endif()

add_dependencies(GraphicsEngineVk-static ProcessGenerateMipsVkShader)

target_include_directories(GraphicsEngineVk-static 
PRIVATE
    include
    ../../External/vulkan
)

set(PRIVATE_DEPENDENCIES 
    BuildSettings 
    Common 
    TargetPlatform
    GraphicsEngineNextGenBase
    GLSLTools
    SPIRV-Tools-opt
)

if(PLATFORM_WIN32)
    find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS "../../External/vulkan/libs")
else()
    find_library(Vulkan_LIBRARY NAMES vulkan HINTS "$ENV{VULKAN_SDK}/lib" "../../External/vulkan/libs" REQUIRED)
endif()
list(APPEND PRIVATE_DEPENDENCIES ${Vulkan_LIBRARY})

set(PUBLIC_DEPENDENCIES 
    GraphicsEngineVkInterface
)

target_link_libraries(GraphicsEngineVk-static PRIVATE ${PRIVATE_DEPENDENCIES} PUBLIC ${PUBLIC_DEPENDENCIES})

target_link_libraries(GraphicsEngineVk-shared PUBLIC GraphicsEngineVk-static)
target_compile_definitions(GraphicsEngineVk-shared PUBLIC ENGINE_DLL=1)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR 
	CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set_target_properties(GraphicsEngineVk-shared PROPERTIES CXX_VISIBILITY_PRESET hidden) # -fvisibility=hidden
endif()

if(PLATFORM_WIN32)
    set(PRIVATE_COMPILE_DEFINITIONS VK_USE_PLATFORM_WIN32_KHR=1 NOMINMAX)
elseif(PLATFORM_LINUX)
    set(PRIVATE_COMPILE_DEFINITIONS VK_USE_PLATFORM_XCB_KHR=1)
else()
    message(FATAL_ERROR "Unknown platform")
endif()
target_compile_definitions(GraphicsEngineVk-shared PRIVATE ${PRIVATE_COMPILE_DEFINITIONS})
target_compile_definitions(GraphicsEngineVk-static PRIVATE ${PRIVATE_COMPILE_DEFINITIONS})

if(PLATFORM_WIN32)

    # Do not add 'lib' prefix when building with MinGW
    set_target_properties(GraphicsEngineVk-shared PROPERTIES PREFIX "")

    # Set output name to GraphicsEngineVk_{32|64}{r|d}
    set_dll_output_name(GraphicsEngineVk-shared GraphicsEngineVk)

else()
    set_target_properties(GraphicsEngineVk-shared PROPERTIES
        OUTPUT_NAME GraphicsEngineVk
    )
endif()

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
source_group("shaders" FILES
    ${GENERATE_MIPS_SHADER}
)
source_group("shaders\\generated" FILES
    ${GENERATE_MIPS_SHADER_INC}
)

set_target_properties(GraphicsEngineVk-static PROPERTIES
    FOLDER Core/Graphics
)
set_target_properties(GraphicsEngineVk-shared PROPERTIES
    FOLDER Core/Graphics
)

set_source_files_properties(
    readme.md PROPERTIES HEADER_FILE_ONLY TRUE
)

set_target_properties(ProcessGenerateMipsVkShader PROPERTIES
    FOLDER Core/Graphics/Helper
)