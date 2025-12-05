cmake_minimum_required (VERSION 3.11)

set(DAWN_SUBMODULES
    third_party/abseil-cpp
    third_party/jinja2
    third_party/markupsafe
    third_party/libprotobuf-mutator
    third_party/protobuf
    third_party/webgpu-headers
)

FetchContent_DeclareShallowGit(
    dawn
    GIT_REPOSITORY  https://dawn.googlesource.com/dawn
    GIT_TAG         d9daee7f75b60657aa3bc54b406882d10b693f89
    GIT_SUBMODULES  "${DAWN_SUBMODULES}"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (PLATFORM_WEB)
    set(TINT_BUILD_AS_OTHER_OS ON CACHE BOOL "" FORCE)
else()
    set(TINT_BUILD_AS_OTHER_OS OFF CACHE BOOL "" FORCE)
endif()
set(TINT_BUILD_SPV_READER  ON CACHE BOOL "" FORCE)
set(TINT_BUILD_WGSL_WRITER ON CACHE BOOL "" FORCE)
set(TINT_BUILD_WGSL_READER ON CACHE BOOL "" FORCE)
set(TINT_BUILD_IR_BINARY   ON CACHE BOOL "" FORCE)
if (PLATFORM_WEB)
    set(TINT_BUILD_HLSL_WRITER OFF CACHE BOOL "" FORCE)
else()
    set(TINT_BUILD_HLSL_WRITER ON CACHE BOOL "" FORCE)
endif()

set(TINT_BUILD_GLSL_WRITER     OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_MSL_WRITER      OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_SPV_WRITER      OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_GLSL_VALIDATOR  OFF CACHE BOOL "" FORCE)
set(TINT_ENABLE_IR_VALIDATION  OFF CACHE BOOL "" FORCE)

set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_SAMPLES   OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_DOCS      OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_TESTS     OFF CACHE BOOL "" FORCE)

set(DAWN_BUILD_SAMPLES     OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_TESTS       OFF CACHE BOOL "" FORCE)
set(DAWN_USE_X11           OFF CACHE BOOL "" FORCE)
set(DAWN_USE_WAYLAND       OFF CACHE BOOL "" FORCE)

set(DAWN_ENABLE_VULKAN     OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_D3D11      OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_DESKTOP_GL OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_OPENGLES   OFF CACHE BOOL "" FORCE)
# Dawn's CMake defines DAWN_EMSCRIPTEN_TOOLCHAIN through the option command, which
# sets its default value to OFF instead of ""
set(DAWN_EMSCRIPTEN_TOOLCHAIN "" CACHE STRING "" FORCE)
if (PLATFORM_WEB)
    set(DAWN_ENABLE_D3D12 OFF  CACHE BOOL "" FORCE)
else()
    set(DAWN_ENABLE_D3D12 ON CACHE BOOL "" FORCE)
endif()

set(DAWN_USE_GLFW                 OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_NULL              OFF CACHE BOOL "" FORCE)
set(DAWN_USE_WINDOWS_UI           OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_SPIRV_VALIDATION  OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_MONOLITHIC_LIBRARY OFF CACHE STRING "" FORCE)

set(DAWN_SPIRV_TOOLS_DIR    "${spirv-tools_SOURCE_DIR}"    CACHE STRING "Directory in which to find SPIRV-Tools"   FORCE)
set(DAWN_SPIRV_HEADERS_DIR  "${SPIRV-Headers_SOURCE_DIR}"  CACHE STRING "Directory in which to find SPIRV-Headers" FORCE)
set(DAWN_GOOGLETEST_DIR     "${GTEST_SOURCE_DIR}"          CACHE STRING "Directory in which to find GoogleTest"    FORCE)

set(BUILD_SAMPLES      OFF CACHE BOOL "")
set(BUILD_SHARED_LIBS  OFF CACHE BOOL "")

FetchContent_MakeAvailable(dawn)

if (PLATFORM_WEB)
    find_targets_in_directory(DAWN_TARGETS ${dawn_SOURCE_DIR})
    set_targets_emscripten_properties(${DAWN_TARGETS})
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(tint_api PRIVATE -Wno-switch-default)
    target_compile_options(tint_utils_strconv PRIVATE -Wno-nullability-extension)
endif()

install(FILES "${Dawn_SOURCE_DIR}/LICENSE" DESTINATION "Licenses/ThirdParty/${DILIGENT_CORE_DIR}" RENAME dawn-License.txt)
