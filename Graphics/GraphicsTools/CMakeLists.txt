cmake_minimum_required (VERSION 3.10)

project(Diligent-GraphicsTools CXX)

set(INTERFACE
    interface/BufferSuballocator.h
    interface/BytecodeCache.h
    interface/CommonlyUsedStates.h
    interface/DynamicBuffer.hpp
    interface/DynamicTextureArray.hpp
    interface/DynamicTextureAtlas.h
    interface/DurationQueryHelper.hpp
    interface/GraphicsUtilities.h
    interface/MapHelper.hpp
    interface/OffScreenSwapChain.hpp
    interface/ResourceRegistry.hpp
    interface/ScopedDebugGroup.hpp
    interface/GPUCompletionAwaitQueue.hpp
    interface/ScopedQueryHelper.hpp
    interface/ScreenCapture.hpp
    interface/ShaderMacroHelper.hpp
    interface/StreamingBuffer.hpp
    interface/ShaderSourceFactoryUtils.h
    interface/ShaderSourceFactoryUtils.hpp
    interface/TextureUploader.hpp
    interface/TextureUploaderBase.hpp
    interface/XXH128Hasher.hpp
    interface/VertexPool.h
    interface/VertexPoolX.hpp
)

set(SOURCE
    src/BufferSuballocator.cpp
    src/BytecodeCache.cpp
    src/DurationQueryHelper.cpp
    src/DynamicBuffer.cpp
    src/DynamicTextureArray.cpp
    src/DynamicTextureAtlas.cpp
    src/GraphicsUtilities.cpp
    src/OffScreenSwapChain.cpp
    src/ScopedQueryHelper.cpp
    src/ScreenCapture.cpp
    src/ShaderSourceFactoryUtils.cpp
    src/TextureUploader.cpp
    src/XXH128Hasher.cpp
    src/VertexPool.cpp
)

set(INCLUDE include/ProxyPipelineState.hpp)

if(ARCHIVER_SUPPORTED)
    list(APPEND INTERFACE
        interface/RenderStateCache.h
        interface/RenderStateCache.hpp
    )
    list(APPEND INCLUDE
        include/AsyncPipelineState.hpp
        include/RenderStateCacheImpl.hpp
        include/ReloadableShader.hpp
        include/ReloadablePipelineState.hpp
    )
    list(APPEND SOURCE
        src/AsyncPipelineState.cpp
        src/RenderStateCacheImpl.cpp
        src/ReloadableShader.cpp
        src/ReloadablePipelineState.cpp
    )
    set(RENDER_STATE_CACHE_SUPPORTED TRUE CACHE INTERNAL "Render state cache is supported")
else()
    set(RENDER_STATE_CACHE_SUPPORTED FALSE CACHE INTERNAL "Render state cache is not supported")
endif()

if(DILIGENT_USE_OPENXR)
    list(APPEND SOURCE src/OpenXRUtilities.cpp)
    list(APPEND INTERFACE interface/OpenXRUtilities.h)
endif()

set(DEPENDENCIES)

if(D3D11_SUPPORTED)
    list(APPEND SOURCE src/TextureUploaderD3D11.cpp src/GraphicsUtilitiesD3D11.cpp)
    list(APPEND INTERFACE interface/TextureUploaderD3D11.hpp)
    list(APPEND DEPENDENCIES Diligent-GraphicsEngineD3D11Interface)
    if(DILIGENT_USE_OPENXR)
        list(APPEND SOURCE src/OpenXRUtilitiesD3D11.cpp)
    endif()
endif()

if(D3D12_SUPPORTED)
    list(APPEND DEPENDENCIES Diligent-GraphicsEngineD3D12Interface)
    list(APPEND SOURCE src/GraphicsUtilitiesD3D12.cpp)
    if(DILIGENT_USE_OPENXR)
        list(APPEND SOURCE src/OpenXRUtilitiesD3D12.cpp)
    endif()
endif()

if(VULKAN_SUPPORTED)
    list(APPEND SOURCE src/GraphicsUtilitiesVk.cpp)
    list(APPEND DEPENDENCIES Diligent-GraphicsEngineVk-static Vulkan::Headers)
    if(DILIGENT_USE_OPENXR)
        list(APPEND SOURCE src/OpenXRUtilitiesVk.cpp)
    endif()
endif()

if(D3D12_SUPPORTED OR VULKAN_SUPPORTED)
    list(APPEND SOURCE src/TextureUploaderD3D12_Vk.cpp)
    list(APPEND INTERFACE interface/TextureUploaderD3D12_Vk.hpp)
endif()

if(GL_SUPPORTED OR GLES_SUPPORTED)
    list(APPEND SOURCE src/TextureUploaderGL.cpp src/GraphicsUtilitiesGL.cpp)
    list(APPEND INTERFACE interface/TextureUploaderGL.hpp)
    list(APPEND DEPENDENCIES Diligent-GraphicsEngineOpenGL-static)
    if(DILIGENT_USE_OPENXR)
        list(APPEND SOURCE src/OpenXRUtilitiesGL.cpp)
    endif()
endif()

if(WEBGPU_SUPPORTED)
    list(APPEND SOURCE src/TextureUploaderWebGPU.cpp src/GraphicsUtilitiesWebGPU.cpp)
    list(APPEND INTERFACE interface/TextureUploaderWebGPU.hpp)
    list(APPEND DEPENDENCIES Diligent-GraphicsEngineWebGPU-static)
    if (NOT PLATFORM_WEB)
        list(APPEND DEPENDENCIES dawn_proc)
    endif()
endif()

if(METAL_SUPPORTED)
    list(APPEND SOURCE src/GraphicsUtilitiesMtl.mm)
    list(APPEND DEPENDENCIES Diligent-GraphicsEngineMetal-static)
endif()

add_library(Diligent-GraphicsTools STATIC ${SOURCE} ${INCLUDE} ${INTERFACE})

target_include_directories(Diligent-GraphicsTools
PUBLIC
    interface
PRIVATE
    ../GraphicsEngineD3DBase/include
    include
)

target_link_libraries(Diligent-GraphicsTools
PRIVATE
    Diligent-Common
    Diligent-BuildSettings
    Diligent-PlatformInterface
    Diligent-GraphicsAccessories
    Diligent-ShaderTools
    Diligent-GraphicsEngine
    xxHash::xxhash
    ${DEPENDENCIES}
PUBLIC
    Diligent-GraphicsEngineInterface
)

if(ARCHIVER_SUPPORTED)
    target_link_libraries(Diligent-GraphicsTools
    PRIVATE
        Diligent-Archiver-static
    )
endif()

if(RENDER_STATE_CACHE_SUPPORTED)
    target_compile_definitions(Diligent-GraphicsTools PUBLIC DILIGENT_RENDER_STATE_CACHE_SUPPORTED=1)
endif()

if(DILIGENT_USE_OPENXR)
    target_link_libraries(Diligent-GraphicsTools PRIVATE OpenXR::headers)
    target_compile_definitions(Diligent-GraphicsTools PUBLIC DILIGENT_USE_OPENXR=1)
endif()

if(D3D11_SUPPORTED OR D3D12_SUPPORTED)
    target_link_libraries(Diligent-GraphicsTools
    PRIVATE
        Diligent-GraphicsEngineD3DBase
    )
endif()

set_common_target_properties(Diligent-GraphicsTools)

source_group("src" FILES ${SOURCE})
source_group("interface" FILES ${INTERFACE})
source_group("include" FILES ${INCLUDE})

set_target_properties(Diligent-GraphicsTools PROPERTIES
    FOLDER DiligentCore/Graphics
)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-GraphicsTools)
endif()
