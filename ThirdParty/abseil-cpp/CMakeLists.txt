cmake_minimum_required (VERSION 3.11)

FetchContent_DeclareShallowGit(
    abseil-cpp
    GIT_REPOSITORY  https://chromium.googlesource.com/chromium/src/third_party/abseil-cpp
    GIT_TAG         54ea9e3a7fe017384713d773373761de7c4a1154
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(abseil-cpp)
install(FILES "${abseil-cpp_SOURCE_DIR}/LICENSE" DESTINATION "Licenses/ThirdParty/${DILIGENT_CORE_DIR}" RENAME abseil-cpp-License.txt)
