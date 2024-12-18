cmake_minimum_required (VERSION 3.11)

include(FetchContent)

# openxr_loader - From github.com/KhronosGroup
set(BUILD_TESTS OFF)
set(BUILD_API_LAYERS ON)
FetchContent_Declare(
    OpenXR
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL https://github.com/KhronosGroup/OpenXR-SDK-Source/archive/refs/tags/release-1.0.34.zip
)
FetchContent_MakeAvailable(OpenXR)
set_directory_root_folder("${openxr_SOURCE_DIR}" "DiligentCore/ThirdParty/OpenXR")
