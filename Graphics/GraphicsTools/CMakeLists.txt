cmake_minimum_required (VERSION 3.6)

project(GraphicsTools CXX)

set(INCLUDE 
    include/CommonlyUsedStates.h
    include/GraphicsUtilities.h
    include/pch.h
    include/ScreenCapture.h
    include/ShaderMacroHelper.h
    include/TextureUploader.h
    include/TextureUploaderBase.h
)

set(SOURCE 
    src/GraphicsUtilities.cpp
    src/ScreenCapture.cpp
    src/pch.cpp
    src/TextureUploader.cpp
)

set(DEPENDENCIES)

if(D3D11_SUPPORTED)
    list(APPEND SOURCE src/TextureUploaderD3D11.cpp)
    list(APPEND INCLUDE include/TextureUploaderD3D11.h)
    list(APPEND DEPENDENCIES GraphicsEngineD3D11Interface)
endif()

if(D3D12_SUPPORTED OR VULKAN_SUPPORTED)
    list(APPEND SOURCE src/TextureUploaderD3D12_Vk.cpp)
    list(APPEND INCLUDE include/TextureUploaderD3D12_Vk.h)
endif()

if(GL_SUPPORTED OR GLES_SUPPORTED)
    list(APPEND SOURCE src/TextureUploaderGL.cpp)
    list(APPEND INCLUDE include/TextureUploaderGL.h)
    list(APPEND DEPENDENCIES GraphicsEngineOpenGLInterface)
endif()

add_library(GraphicsTools STATIC ${SOURCE} ${INCLUDE})

target_include_directories(GraphicsTools 
PUBLIC
    include
PRIVATE
    ../GraphicsEngineD3DBase/include
)

target_link_libraries(GraphicsTools 
PRIVATE 
    Common 
    BuildSettings
    PlatformInterface
    GraphicsAccessories
    ${DEPENDENCIES}
PUBLIC
    GraphicsEngineInterface
)

set_common_target_properties(GraphicsTools)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(GraphicsTools PROPERTIES
    FOLDER Core/Graphics
)

if(INSTALL_DILIGENT_CORE)
    install_core_lib(GraphicsTools)
endif()