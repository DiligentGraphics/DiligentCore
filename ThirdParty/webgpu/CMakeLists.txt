cmake_minimum_required (VERSION 3.11)

if (PLATFORM_EMSCRIPTEN)
    set(WEBGPU_GIT_TAG "emscripten")
else()
    set(WEBGPU_GIT_TAG "wgpu")
endif()

FetchContent_DeclareShallowGit(
    webgpu
    GIT_REPOSITORY         https://github.com/eliemichel/WebGPU-distribution
    GIT_TAG                ${WEBGPU_GIT_TAG}
)

FetchContent_MakeAvailable(webgpu)
FetchContent_GetProperties(webgpu SOURCE_DIR webgpu_SOURCE_DIR)

install(FILES "${webgpu_SOURCE_DIR}/LICENSE.txt" DESTINATION "Licenses/ThirdParty/${DILIGENT_CORE_DIR}" RENAME webgpu-License.txt)
