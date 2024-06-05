cmake_minimum_required (VERSION 3.11)

FetchContent_DeclareShallowGit(
    tint
    GIT_REPOSITORY         https://dawn.googlesource.com/tint
    GIT_TAG                5b68c1266eb91bd29c26751a820919f1afa31260 # fdb8787e9c1b79770bd98a8faf37fbe48a3077a4
)

set(TINT_BUILD_AS_OTHER_OS ON  CACHE BOOL "" FORCE)
set(TINT_BUILD_SPV_READER  ON  CACHE BOOL "" FORCE)
set(TINT_BUILD_WGSL_WRITER ON  CACHE BOOL "" FORCE)
set(TINT_BUILD_WGSL_READER OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_GLSL_WRITER OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_HLSL_WRITER OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_MSL_WRITER  OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_SPV_WRITER  OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_IR          OFF CACHE BOOL "" FORCE)

set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_SAMPLES   OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_DOCS      OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_TESTS     OFF CACHE BOOL "" FORCE)

set(TINT_SPIRV_TOOLS_DIR   "${spirv-tools_SOURCE_DIR}"   CACHE STRING "Directory in which to find SPIRV-Tools"   FORCE)
set(TINT_SPIRV_HEADERS_DIR "${SPIRV-Headers_SOURCE_DIR}" CACHE STRING "Directory in which to find SPIRV-Headers" FORCE)
set(TINT_ABSEIL_DIR        "${abseil-cpp_SOURCE_DIR}"    CACHE STRING "Directory in which to find Abseil"        FORCE)
FetchContent_MakeAvailable(tint)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(tint_api PRIVATE -Wno-switch-default)
endif()

FetchContent_GetProperties(tint SOURCE_DIR tint_SOURCE_DIR)
install(FILES "${tint_SOURCE_DIR}/LICENSE" DESTINATION "Licenses/ThirdParty/${DILIGENT_CORE_DIR}" RENAME tint-License.txt)
