cmake_minimum_required (VERSION 3.6)

set(PLATFORM_WIN32 FALSE CACHE INTERNAL "")
set(PLATFORM_UNIVERSAL_WINDOWS FALSE CACHE INTERNAL "")
set(PLATFORM_ANDROID FALSE CACHE INTERNAL "")
set(PLATFORM_LINUX FALSE CACHE INTERNAL "")
set(D3D11_SUPPORTED FALSE CACHE INTERNAL "D3D11 is not spported")
set(D3D12_SUPPORTED FALSE CACHE INTERNAL "D3D12 is not spported")
set(GL_SUPPORTED FALSE CACHE INTERNAL "GL is not spported")
set(GLES_SUPPORTED FALSE CACHE INTERNAL "GLES is not spported")

set(CMAKE_OBJECT_PATH_MAX 4096)

if(WIN32)
	if(${CMAKE_SYSTEM_NAME} STREQUAL "WindowsStore")
		set(PLATFORM_UNIVERSAL_WINDOWS TRUE CACHE INTERNAL "Target platform: Windows Store")
		message("Target platform: Universal Windows")
	else()
		set(PLATFORM_WIN32 TRUE CACHE INTERNAL "Target platform: Win32") #WIN32 is a variable, so we cannot use string "WIN32"
		message("Target platform: Win32")
	endif()
else()
	if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
		set(PLATFORM_ANDROID TRUE CACHE INTERNAL "Target platform: Android")
		message("Target platform: Android")
	elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
		set(PLATFORM_LINUX TRUE CACHE INTERNAL "Target platform: Linux")
		message("Target Platform: Linux")
	else()
		message(FATAL_ERROR "Unsupported platform")
	endif()
endif(WIN32)

add_library(BuildSettings INTERFACE)

if(PLATFORM_WIN32)
	set(D3D11_SUPPORTED TRUE CACHE INTERNAL "D3D11 supported on Win32 platform")
	set(D3D12_SUPPORTED TRUE CACHE INTERNAL "D3D12 supported on Win32 platform")
	set(GL_SUPPORTED TRUE CACHE INTERNAL "OpenGL supported on Win32 platform")
	target_compile_definitions(BuildSettings INTERFACE PLATFORM_WIN32=1)
elseif(PLATFORM_UNIVERSAL_WINDOWS)
	set(D3D11_SUPPORTED TRUE CACHE INTERNAL "D3D11 supported on Univeral Windows platform")
	set(D3D12_SUPPORTED TRUE CACHE INTERNAL "D3D12 supported on Univeral Windows platform")
	target_compile_definitions(BuildSettings INTERFACE PLATFORM_UNIVERSAL_WINDOWS=1)
elseif(PLATFORM_ANDROID)
	set(GLES_SUPPORTED TRUE CACHE INTERNAL "OpenGLES supported on Android platform")
	target_compile_definitions(BuildSettings INTERFACE PLATFORM_ANDROID=1)
elseif(PLATFORM_LINUX)
	set(GL_SUPPORTED TRUE CACHE INTERNAL "OpenGL supported on Linux platform")
	target_compile_definitions(BuildSettings INTERFACE PLATFORM_LINUX=1)
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
	# - Enable string pooling (/GF)
	set(MSVC_ALL_RELEASE_COMPILE_OPTIONS /arch:AVX2 /wd4189 /GR- /GL /GF)
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
else()

	set(DEBUG_MACROS _DEBUG DEBUG)
	set(RELEASE_MACROS NDEBUG)
	target_compile_definitions(BuildSettings INTERFACE "$<$<CONFIG:DEBUG>:${DEBUG_MACROS}>")
	target_compile_definitions(BuildSettings INTERFACE "$<$<CONFIG:RELEASE>:${RELEASE_MACROS}>")
	target_compile_definitions(BuildSettings INTERFACE "$<$<CONFIG:MINSIZEREL>:${RELEASE_MACROS}>")
	target_compile_definitions(BuildSettings INTERFACE "$<$<CONFIG:RELWITHDEBINFO>:${RELEASE_MACROS}>")

endif(MSVC)

if(PLATFORM_LINUX)
	if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		# Without -fPIC option GCC fails to link static libraries into dynamic library:
		#  -fPIC  
		#      If supported for the target machine, emit position-independent code, suitable for 
		#      dynamic linking and avoiding any limit on the size of the global offset table.
		target_compile_options(BuildSettings INTERFACE -fPIC)
	endif()
endif()

if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
	set(ARCH 64 CACHE INTERNAL "64-bit architecture")
else()
	set(ARCH 32 CACHE INTERNAL "32-bit architecture")
endif()

if(PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS)

	# https://cmake.org/cmake/help/v3.8/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html#prop_gbl:CMAKE_CXX_KNOWN_FEATURES
	target_compile_features(BuildSettings INTERFACE cxx_std_11) # Generates an error in gradle build on Android

elseif(PLATFORM_ANDROID)

	# LOCAL_CPP_FEATURES := exceptions rtti
	
endif()

include(BuildUtils.cmake)

add_subdirectory(Utilities)
add_subdirectory(Primitives)
add_subdirectory(Platforms)
add_subdirectory(External)
add_subdirectory(Common)
add_subdirectory(Graphics)
