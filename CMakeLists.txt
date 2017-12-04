cmake_minimum_required (VERSION 3.3)

add_library(BuildSettings INTERFACE)
if(WIN32)
	target_compile_definitions(BuildSettings INTERFACE PLATFORM_WIN32=1)
elseif(UWP)
	target_compile_definitions(BuildSettings INTERFACE PLATFORM_UNIVERSAL_WINDOWS=1)
elseif(ANDROID)
	target_compile_definitions(BuildSettings INTERFACE PLATFORM_ANDROID=1)
endif()

# https://cmake.org/cmake/help/v3.8/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html#prop_gbl:CMAKE_CXX_KNOWN_FEATURES
target_compile_features(BuildSettings INTERFACE cxx_std_11)

add_subdirectory(Utilities)
add_subdirectory(Platforms)
add_subdirectory(External)
add_subdirectory(Common)
add_subdirectory(Graphics)



