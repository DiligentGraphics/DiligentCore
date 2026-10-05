cmake_minimum_required (VERSION 3.11)

FetchContent_DeclareShallowGit(
    abseil-cpp
    GIT_REPOSITORY  https://chromium.googlesource.com/chromium/src/third_party/abseil-cpp
    # Keep this revision aligned with the Abseil dependency of Dawn.
    GIT_TAG         af5aede14a0b4013be6890148d34e0fd7b1fac69
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)

if(DILIGENT_SANITIZER_COMPILE_OPTIONS)
    add_compile_options(${DILIGENT_SANITIZER_COMPILE_OPTIONS})
endif()

FetchContent_MakeAvailable(abseil-cpp)

if (PLATFORM_WEB)
    find_targets_in_directory(ABSEIL_TARGETS ${abseil-cpp_SOURCE_DIR})
    set_targets_emscripten_properties(${ABSEIL_TARGETS})
endif()

install(FILES "${abseil-cpp_SOURCE_DIR}/LICENSE" DESTINATION "Licenses/ThirdParty/${DILIGENT_CORE_DIR}" RENAME abseil-cpp-License.txt)
