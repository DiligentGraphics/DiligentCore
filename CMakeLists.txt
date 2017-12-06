cmake_minimum_required (VERSION 3.3)

if(WIN32)
	set(PLATFORM_WIN32 TRUE CACHE INTERNAL "Target platform: Win32") #WIN32 is a variable, so we cannot use string "WIN32"
else()
	message(FATAL_ERROR "Unknown platform")
endif(WIN32)

add_library(BuildSettings INTERFACE)

if(PLATFORM_WIN32)
	target_compile_definitions(BuildSettings INTERFACE PLATFORM_WIN32=1)
elseif(PLATFORM_UNVIRSAL_WINDOWS)
	target_compile_definitions(BuildSettings INTERFACE PLATFORM_UNIVERSAL_WINDOWS=1)
elseif(PLATFORM_ANDROID)
	target_compile_definitions(BuildSettings INTERFACE PLATFORM_ANDROID=1)
else()
	message(FATAL_ERROR "No PLATFORM_XXX variable defined. Make sure that 'DiligentCore' folder is processed first")
endif()

if(MSVC)
	# For msvc, enable level 4 warnings except for
	# - w4100 - unreferenced formal parameter
	# - w4505 - unreferenced local function has been removed
	target_compile_options(BuildSettings INTERFACE /W4 /wd4100 /wd4505)
	# In all releases mode also:
	# - disable w4189 - local variable is initialized but not referenced
	# - Enable AVX2 instruction set
	set(MSVC_ALL_RELEASE_COMPILE_OPTIONS /arch:AVX2 /wd4189)
	#target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:RELEASE>:/arch:AVX2 /wd4189 /Ot")
	# In RELEASE mode also set favor fast code option (/Ot)
	set(MSVC_RELEASE_COMPILE_OPTIONS /arch:AVX2 /wd4189 /Ot)
	set(MSVC_MINSIZEREL_COMPILE_OPTIONS /arch:AVX2 /wd4189 /Os)
	set(MSVC_RELWITHDEBINFO_COMPILE_OPTIONS /arch:AVX2 /wd4189)
	target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:RELEASE>:${MSVC_RELEASE_COMPILE_OPTIONS}>")
	target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:MINSIZEREL>:${MSVC_MINSIZEREL_COMPILE_OPTIONS}>")
	target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:RELWITHDEBINFO>:${MSVC_RELWITHDEBINFO_COMPILE_OPTIONS}>")
	# !!!NOTE!!! For some reason above is the only form of generator expression that works
	# For instance, this way
	# target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:RELEASE>:/Ot>")
	# does not work as expected
endif(MSVC)

# https://cmake.org/cmake/help/v3.8/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html#prop_gbl:CMAKE_CXX_KNOWN_FEATURES
target_compile_features(BuildSettings INTERFACE cxx_std_11)

add_subdirectory(Utilities)
add_subdirectory(Platforms)
add_subdirectory(External)
add_subdirectory(Common)
add_subdirectory(Graphics)



