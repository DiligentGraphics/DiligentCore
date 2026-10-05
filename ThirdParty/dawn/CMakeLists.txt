cmake_minimum_required (VERSION 3.22)

set(DAWN_SUBMODULES
    third_party/jinja2
    third_party/markupsafe
    third_party/webgpu-headers/src
)

FetchContent_DeclareShallowGit(
    dawn
    GIT_REPOSITORY  https://dawn.googlesource.com/dawn
    # Dawn and Emdawnwebgpu snapshot v20261002.154047.
    GIT_TAG         b1236a9bb4a47f1262bc3acb253bb1c2181e90dd
    GIT_SUBMODULES  "${DAWN_SUBMODULES}"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    # Enable inline expansion in Debug builds to improve Dawn and Tint runtime performance.
    add_compile_options("$<$<CONFIG:DEBUG>:/Ob1>")
endif()

set(TINT_BUILD_AS_OTHER_OS ${PLATFORM_WEB} CACHE BOOL "" FORCE)
set(TINT_BUILD_SPV_READER  ON CACHE BOOL "" FORCE)
set(TINT_BUILD_WGSL_WRITER ON CACHE BOOL "" FORCE)
set(TINT_BUILD_WGSL_READER ON CACHE BOOL "" FORCE)
set(TINT_BUILD_IR_BINARY  OFF CACHE BOOL "" FORCE)
if (PLATFORM_WEB)
    set(TINT_BUILD_HLSL_WRITER OFF CACHE BOOL "" FORCE)
else()
    set(TINT_BUILD_HLSL_WRITER ON CACHE BOOL "" FORCE)
endif()

set(TINT_BUILD_GLSL_WRITER     OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_MSL_WRITER      OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_SPV_WRITER      OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_GLSL_VALIDATOR  OFF CACHE BOOL "" FORCE)

set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_SAMPLES   OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_DOCS      OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_TESTS     OFF CACHE BOOL "" FORCE)

set(DAWN_BUILD_SAMPLES            OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_TESTS              OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_PROTOBUF           OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_MONOLITHIC_LIBRARY OFF CACHE STRING "" FORCE)
set(DAWN_USE_X11                  OFF CACHE BOOL "" FORCE)
set(DAWN_USE_WAYLAND              OFF CACHE BOOL "" FORCE)

set(DAWN_ENABLE_VULKAN     OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_D3D11      OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_DESKTOP_GL OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_OPENGLES   OFF CACHE BOOL "" FORCE)
if (PLATFORM_WEB)
    set(DAWN_ENABLE_D3D12 OFF CACHE BOOL "" FORCE)
else()
    set(DAWN_ENABLE_D3D12 ON CACHE BOOL "" FORCE)
endif()

set(DAWN_USE_GLFW                OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_NULL             OFF CACHE BOOL "" FORCE)
set(DAWN_USE_WINDOWS_UI          OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_SPIRV_VALIDATION OFF CACHE BOOL "" FORCE)

set(DAWN_SPIRV_TOOLS_DIR    "${spirv-tools_SOURCE_DIR}"    CACHE STRING "Directory in which to find SPIRV-Tools"    FORCE)
set(DAWN_SPIRV_HEADERS_DIR  "${SPIRV-Headers_SOURCE_DIR}"  CACHE STRING "Directory in which to find SPIRV-Headers"  FORCE)

set(BUILD_SHARED_LIBS OFF)

# Reuse the Abseil dependency configured by Diligent for all consumers.
FetchContent_GetProperties(abseil-cpp)
set(DAWN_ABSEIL_DIR "${abseil-cpp_SOURCE_DIR}" CACHE STRING "Directory in which to find Abseil" FORCE)

FetchContent_MakeAvailable(dawn)

if (PLATFORM_WEB)
    find_targets_in_directory(DAWN_TARGETS ${dawn_SOURCE_DIR})
    set_targets_emscripten_properties(${DAWN_TARGETS})
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(tint_api PRIVATE -Wno-switch-default)
    target_compile_options(tint_utils_strconv PRIVATE -Wno-nullability-extension)
endif()

install(FILES "${dawn_SOURCE_DIR}/LICENSE" DESTINATION "Licenses/ThirdParty/${DILIGENT_CORE_DIR}" RENAME dawn-License.txt)
