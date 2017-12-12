cmake_minimum_required (VERSION 3.3)

if(WIN32)
	set(PLATFORM_WIN32 TRUE CACHE INTERNAL "Target platform: Win32") #WIN32 is a variable, so we cannot use string "WIN32"
else()
	message(FATAL_ERROR "Unknown platform")
endif(WIN32)

add_library(BuildSettings INTERFACE)

if(PLATFORM_WIN32)
	set(D3D11_SUPPORTED TRUE CACHE INTERNAL "D3D11 supported on Win32 platform")
	set(D3D12_SUPPORTED TRUE CACHE INTERNAL "D3D12 supported on Win32 platform")
	set(GL_SUPPORTED TRUE CACHE INTERNAL "OpenGL supported on Win32 platform")
	target_compile_definitions(BuildSettings INTERFACE PLATFORM_WIN32=1)
elseif(PLATFORM_UNVIRSAL_WINDOWS)
	set(D3D11_SUPPORTED TRUE CACHE INTERNAL "D3D11 supported on Univeral Windows platform")
	set(D3D12_SUPPORTED TRUE CACHE INTERNAL "D3D12 supported on Univeral Windows platform")
	target_compile_definitions(BuildSettings INTERFACE PLATFORM_UNIVERSAL_WINDOWS=1)
elseif(PLATFORM_ANDROID)
	set(GLES_SUPPORTED TRUE CACHE INTERNAL "OpenGLES supported on Android platform")
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
	# - Enable AVX2 instruction set (/arch:AVX2)
	# - Disable RTTI (/GR-)
	# - Enable whole program optimizaion (/GL)
	set(MSVC_ALL_RELEASE_COMPILE_OPTIONS /arch:AVX2 /wd4189 /GR- /GL)
	#target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:RELEASE>:/arch:AVX2 /wd4189 /Ot")
	# In RELEASE mode:
	# - Set favor fast code option (/Ot)
	# - Enable intrinsic functions (/Oi)
	set(MSVC_RELEASE_COMPILE_OPTIONS ${MSVC_ALL_RELEASE_COMPILE_OPTIONS} /Ot /Oi)
	# In MINSIZEREL mode set favor small code option (/Os)
	set(MSVC_MINSIZEREL_COMPILE_OPTIONS ${MSVC_ALL_RELEASE_COMPILE_OPTIONS} /Os)
	set(MSVC_RELWITHDEBINFO_COMPILE_OPTIONS ${MSVC_ALL_RELEASE_COMPILE_OPTIONS})
	target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:RELEASE>:${MSVC_RELEASE_COMPILE_OPTIONS}>")
	target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:MINSIZEREL>:${MSVC_MINSIZEREL_COMPILE_OPTIONS}>")
	target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:RELWITHDEBINFO>:${MSVC_RELWITHDEBINFO_COMPILE_OPTIONS}>")
	# !!!NOTE!!! For some reason above is the only form of generator expression that works
	# For instance, this way
	# target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:RELEASE>:/Ot>")
	# does not work as expected
endif(MSVC)

if(PLATFORM_WIN32 OR PLATFORM_UNVIRSAL_WINDOWS)
	# FXC is already set when building from Visual Studio 2017
	if(NOT FXC)
		set(DEFAULT_FXC_PATH "C:/Program Files (x86)/Windows Kits/10/bin/x86/fxc.exe")
		if(EXISTS ${DEFAULT_FXC_PATH})
			message("Path to fxc compiler is not specified. Using default location: " ${DEFAULT_FXC_PATH})
			set(FXC ${DEFAULT_FXC_PATH} CACHE FILEPATH "Default path to fxc compiler")
		else()
			message(FATAL_ERROR "Unable to find fxc compiler at default location: " ${DEFAULT_FXC_PATH} ". Please use -D FXC=<path to fxc>")
		endif()
	endif(NOT FXC)
endif()

# https://cmake.org/cmake/help/v3.8/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html#prop_gbl:CMAKE_CXX_KNOWN_FEATURES
target_compile_features(BuildSettings INTERFACE cxx_std_11)

include(BuildUtils.cmake)

add_subdirectory(Utilities)
add_subdirectory(Platforms)
add_subdirectory(External)
add_subdirectory(Common)
add_subdirectory(Graphics)
