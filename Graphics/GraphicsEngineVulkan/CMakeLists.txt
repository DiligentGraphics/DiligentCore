cmake_minimum_required (VERSION 3.10)

project(Diligent-GraphicsEngineVk CXX)

set(INCLUDE 
    include/BufferVkImpl.hpp
    include/BufferViewVkImpl.hpp
    include/CommandListVkImpl.hpp
    include/CommandPoolManager.hpp
    include/CommandQueueVkImpl.hpp
    include/DescriptorPoolManager.hpp
    include/DeviceContextVkImpl.hpp
    include/FenceVkImpl.hpp
    include/VulkanDynamicHeap.hpp
    include/FramebufferCache.hpp
    include/GenerateMipsVkHelper.hpp
    include/pch.h
    include/PipelineLayout.hpp
    include/PipelineStateVkImpl.hpp
    include/QueryManagerVk.hpp
    include/QueryVkImpl.hpp
    include/RenderDeviceVkImpl.hpp
    include/RenderPassCache.hpp
    include/SamplerVkImpl.hpp
    include/ShaderVkImpl.hpp
    include/ManagedVulkanObject.hpp
    include/ShaderResourceBindingVkImpl.hpp
    include/ShaderResourceCacheVk.hpp
    include/ShaderResourceLayoutVk.hpp
    include/ShaderVariableVk.hpp
    include/SwapChainVkImpl.hpp
    include/TextureVkImpl.hpp
    include/TextureViewVkImpl.hpp
    include/VulkanErrors.hpp
    include/VulkanTypeConversions.hpp
    include/VulkanUploadHeap.hpp
)

set(VULKAN_UTILS_INCLUDE 
    include/VulkanUtilities/VulkanCommandBuffer.hpp
    include/VulkanUtilities/VulkanCommandBufferPool.hpp
    include/VulkanUtilities/VulkanDebug.hpp
    include/VulkanUtilities/VulkanFencePool.hpp
    include/VulkanUtilities/VulkanInstance.hpp
    include/VulkanUtilities/VulkanLogicalDevice.hpp
    include/VulkanUtilities/VulkanMemoryManager.hpp
    include/VulkanUtilities/VulkanObjectWrappers.hpp
    include/VulkanUtilities/VulkanPhysicalDevice.hpp
)


set(INTERFACE 
    interface/BufferVk.h
    interface/BufferViewVk.h
    interface/CommandQueueVk.h
    interface/DeviceContextVk.h
    interface/EngineFactoryVk.h
    interface/FenceVk.h
    interface/PipelineStateVk.h
    interface/QueryVk.h
    interface/RenderDeviceVk.h
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
    src/EngineFactoryVk.cpp
    src/FenceVkImpl.cpp
    src/VulkanDynamicHeap.cpp
    src/FramebufferCache.cpp
    src/GenerateMipsVkHelper.cpp
    src/PipelineLayout.cpp
    src/PipelineStateVkImpl.cpp
    src/QueryManagerVk.cpp
    src/QueryVkImpl.cpp
    src/RenderDeviceVkImpl.cpp
    src/RenderPassCache.cpp
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

set(GENERATE_MIPS_SHADER shaders/GenerateMipsCS.csh)

# We must use the full path, otherwise the build system will not be able to properly detect
# changes and shader conversion custom command will run every time
set(GENERATE_MIPS_SHADER_INC ${CMAKE_CURRENT_SOURCE_DIR}/shaders/GenerateMipsCS_inc.h)
set_source_files_properties(
    ${GENERATE_MIPS_SHADER_INC}
    PROPERTIES GENERATED TRUE
)

add_custom_command(OUTPUT ${GENERATE_MIPS_SHADER_INC} # We must use full path here!
                   COMMAND ${FILE2STRING_PATH} ${GENERATE_MIPS_SHADER} shaders/GenerateMipsCS_inc.h
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   COMMENT "Processing GenerateMipsCS.csh"
                   MAIN_DEPENDENCY ${GENERATE_MIPS_SHADER}
                   VERBATIM
)


add_library(Diligent-GraphicsEngineVkInterface INTERFACE)
target_include_directories(Diligent-GraphicsEngineVkInterface
INTERFACE
    interface
)
target_link_libraries(Diligent-GraphicsEngineVkInterface 
INTERFACE 
    Diligent-GraphicsEngineInterface
)

add_library(Diligent-GraphicsEngineVk-static STATIC 
    ${SRC} ${VULKAN_UTILS_SRC} ${INTERFACE} ${INCLUDE} ${VULKAN_UTILS_INCLUDE} ${GENERATE_MIPS_SHADER}
    
    # A target created in the same directory (CMakeLists.txt file) that specifies any output of the 
    # custom command as a source file is given a rule to generate the file using the command at build time. 
    ${GENERATE_MIPS_SHADER_INC}

    readme.md
)

add_library(Diligent-GraphicsEngineVk-shared SHARED 
    readme.md
)

if(MSVC)
    target_sources(Diligent-GraphicsEngineVk-shared PRIVATE
        src/DLLMain.cpp
        src/GraphicsEngineVk.def
    )
endif()

target_include_directories(Diligent-GraphicsEngineVk-static 
PRIVATE
    include
    ../../ThirdParty/vulkan
)

set(PRIVATE_DEPENDENCIES 
    Diligent-BuildSettings 
    Diligent-Common 
    Diligent-TargetPlatform
    Diligent-GraphicsEngineNextGenBase
    Diligent-GLSLTools
    SPIRV-Tools-opt
)

if(PLATFORM_WIN32)
    if(${ARCH} EQUAL 64)
        set(VULKAN_LIB_FOLDER x64)
    else()
        set(VULKAN_LIB_FOLDER x86)
    endif()
    find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS "../../ThirdParty/vulkan/libs/windows/${VULKAN_LIB_FOLDER}")
elseif(PLATFORM_MACOS)
    find_library(Vulkan_LIBRARY NAMES vulkan.1 vulkan PATHS "../../ThirdParty/vulkan/libs/macOS")
elseif(PLATFORM_IOS)
    set(Vulkan_LIBRARY ${MoltenVK_LIBRARY})
elseif(PLATFORM_LINUX)
    find_library(Vulkan_LIBRARY NAMES vulkan HINTS "$ENV{VULKAN_SDK}/lib" "../../ThirdParty/vulkan/libs/linux")
else()
    find_library(Vulkan_LIBRARY NAMES vulkan)
endif()
list(APPEND PRIVATE_DEPENDENCIES ${Vulkan_LIBRARY})

set(PUBLIC_DEPENDENCIES 
    Diligent-GraphicsEngineVkInterface
)

target_link_libraries(Diligent-GraphicsEngineVk-static
PRIVATE
    ${PRIVATE_DEPENDENCIES}
PUBLIC
    ${PUBLIC_DEPENDENCIES}
)
target_link_libraries(Diligent-GraphicsEngineVk-shared
PRIVATE
    Diligent-BuildSettings
    ${WHOLE_ARCHIVE_FLAG} Diligent-GraphicsEngineVk-static ${NO_WHOLE_ARCHIVE_FLAG}
PUBLIC
    ${PUBLIC_DEPENDENCIES}
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Disable the following clang warning
    #    '<function name>' hides overloaded virtual function
    # as hiding is intended
    target_compile_options(Diligent-GraphicsEngineVk-static PRIVATE -Wno-overloaded-virtual)
    target_compile_options(Diligent-GraphicsEngineVk-shared PRIVATE -Wno-overloaded-virtual)

    # Disable the following warning:
    #   moving a local object in a return statement prevents copy elision [-Wpessimizing-move]
    set_source_files_properties(src/CommandPoolManager.cpp src/VulkanUtilities/VulkanDebug.cpp
    PROPERTIES
        COMPILE_FLAGS -Wno-pessimizing-move
    )
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set_target_properties(Diligent-GraphicsEngineVk-shared PROPERTIES
        # Disallow missing direct and indirect dependencies to enssure that .so is self-contained
        LINK_FLAGS "-Wl,--no-undefined -Wl,--no-allow-shlib-undefined"
    )
endif()

if(PLATFORM_WIN32)
    set(PRIVATE_COMPILE_DEFINITIONS VK_USE_PLATFORM_WIN32_KHR=1 NOMINMAX)
elseif(PLATFORM_LINUX)
    set(PRIVATE_COMPILE_DEFINITIONS VK_USE_PLATFORM_XCB_KHR=1)
elseif(PLATFORM_MACOS)
    set(PRIVATE_COMPILE_DEFINITIONS VK_USE_PLATFORM_MACOS_MVK=1)
elseif(PLATFORM_IOS)
    set(PRIVATE_COMPILE_DEFINITIONS VK_USE_PLATFORM_IOS_MVK=1)
elseif(PLATFORM_ANDROID)
    set(PRIVATE_COMPILE_DEFINITIONS VK_USE_PLATFORM_ANDROID_KHR=1)
else()
    message(FATAL_ERROR "Unknown platform")
endif()

if (${DILIGENT_NO_GLSLANG})
    message("GLSLang is not being built. Vulkan backend will only be able to consume SPIRV byte code.")
endif()

target_compile_definitions(Diligent-GraphicsEngineVk-static PRIVATE ${PRIVATE_COMPILE_DEFINITIONS} NO_GLSLANG=$<BOOL:${DILIGENT_NO_GLSLANG}>)
target_compile_definitions(Diligent-GraphicsEngineVk-shared PRIVATE ${PRIVATE_COMPILE_DEFINITIONS} ENGINE_DLL=1)

if(PLATFORM_WIN32)

    # Do not add 'lib' prefix when building with MinGW
    set_target_properties(Diligent-GraphicsEngineVk-shared PROPERTIES PREFIX "")

    # Set output name to GraphicsEngineVk_{32|64}{r|d}
    set_dll_output_name(Diligent-GraphicsEngineVk-shared GraphicsEngineVk)

else()
    set_target_properties(Diligent-GraphicsEngineVk-shared PROPERTIES
        OUTPUT_NAME GraphicsEngineVk
    )
endif()

if(PLATFORM_LINUX)
    get_filename_component(VULKAN_LIBRARY_PATH ${Vulkan_LIBRARY} DIRECTORY)
    set_target_properties(Diligent-GraphicsEngineVk-shared PROPERTIES
        INSTALL_RPATH "${VULKAN_LIBRARY_PATH}"
    )
endif()

set_common_target_properties(Diligent-GraphicsEngineVk-shared)
set_common_target_properties(Diligent-GraphicsEngineVk-static)

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

set_target_properties(Diligent-GraphicsEngineVk-static PROPERTIES
    FOLDER DiligentCore/Graphics
)
set_target_properties(Diligent-GraphicsEngineVk-shared PROPERTIES
    FOLDER DiligentCore/Graphics
)

set_source_files_properties(
    readme.md PROPERTIES HEADER_FILE_ONLY TRUE
)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-GraphicsEngineVk-shared)
    install_core_lib(Diligent-GraphicsEngineVk-static)
endif()
