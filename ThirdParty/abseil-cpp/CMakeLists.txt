cmake_minimum_required (VERSION 3.11)

FetchContent_DeclareShallowGit(
    abseil-cpp
    GIT_REPOSITORY  https://chromium.googlesource.com/chromium/src/third_party/abseil-cpp
    GIT_TAG         07d2ef8bd61ab88f0b81b0d8c7fb2c7e19b1d01e
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)

if(DILIGENT_SANITIZER_COMPILE_OPTIONS)
    add_compile_options(${DILIGENT_SANITIZER_COMPILE_OPTIONS})
endif()

FetchContent_MakeAvailable(abseil-cpp)
install(FILES "${abseil-cpp_SOURCE_DIR}/LICENSE" DESTINATION "Licenses/ThirdParty/${DILIGENT_CORE_DIR}" RENAME abseil-cpp-License.txt)
