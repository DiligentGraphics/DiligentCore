cmake_minimum_required (VERSION 3.11)

FetchContent_DeclareShallowGit(
    abseil-cpp
    GIT_REPOSITORY         https://github.com/abseil/abseil-cpp.git
    GIT_TAG                54e1f14c6f9b1a764ffdf8c1aea7e823f95f3d01
)

set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(abseil-cpp)
FetchContent_GetProperties(abseil-cpp SOURCE_DIR abseil-cpp_SOURCE_DIR)
install(FILES "${abseil-cpp_SOURCE_DIR}/LICENSE" DESTINATION "Licenses/ThirdParty/${DILIGENT_CORE_DIR}" RENAME abseil-cpp-License.txt)

# Hack for tint
add_library(libabsl ALIAS absl_base)
