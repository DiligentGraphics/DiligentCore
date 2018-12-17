cmake_minimum_required (VERSION 3.6)

set(DEBUG_CONFIGURATIONS DEBUG CACHE INTERNAL "Debug configurations")
set(RELEASE_CONFIGURATIONS RELEASE RELWITHDEBINFO MINSIZEREL CACHE INTERNAL "Release configurations")

if(BUILD_CONFIGURATION_FILE)
    message("Using build configuration file " ${CUSTOM_BUILD_SCRIPT})
    include(${CMAKE_SOURCE_DIR}/${BUILD_CONFIGURATION_FILE})
    
    if(COMMAND custom_configure_build)
        custom_configure_build()
    else()
        message("custom_configure_build() function not found in " ${CUSTOM_BUILD_SCRIPT})
    endif()
endif()


set(PLATFORM_WIN32 FALSE CACHE INTERNAL "")
set(PLATFORM_UNIVERSAL_WINDOWS FALSE CACHE INTERNAL "")
set(PLATFORM_ANDROID FALSE CACHE INTERNAL "")
set(PLATFORM_LINUX FALSE CACHE INTERNAL "")
set(PLATFORM_MACOS FALSE CACHE INTERNAL "")
set(PLATFORM_IOS FALSE CACHE INTERNAL "")
set(D3D11_SUPPORTED FALSE CACHE INTERNAL "D3D11 is not spported")
set(D3D12_SUPPORTED FALSE CACHE INTERNAL "D3D12 is not spported")
set(GL_SUPPORTED FALSE CACHE INTERNAL "GL is not spported")
set(GLES_SUPPORTED FALSE CACHE INTERNAL "GLES is not spported")
set(VULKAN_SUPPORTED FALSE CACHE INTERNAL "Vulkan is not spported")

set(CMAKE_OBJECT_PATH_MAX 4096)

if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    set(ARCH 64 CACHE INTERNAL "64-bit architecture")
else()
    set(ARCH 32 CACHE INTERNAL "32-bit architecture")
endif()

if(WIN32)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "WindowsStore")
        set(PLATFORM_UNIVERSAL_WINDOWS TRUE CACHE INTERNAL "Target platform: Windows Store")
        message("Target platform: Universal Windows. SDK Version: " ${CMAKE_SYSTEM_VERSION})
    else()
        set(PLATFORM_WIN32 TRUE CACHE INTERNAL "Target platform: Win32") #WIN32 is a variable, so we cannot use string "WIN32"
        message("Target platform: Win32. SDK Version: " ${CMAKE_SYSTEM_VERSION})
    endif()
else()
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
        set(PLATFORM_ANDROID TRUE CACHE INTERNAL "Target platform: Android")
        message("Target platform: Android")
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(PLATFORM_LINUX TRUE CACHE INTERNAL "Target platform: Linux")
        message("Target Platform: Linux")
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        if(IOS)
            set(PLATFORM_IOS TRUE CACHE INTERNAL "Target platform: iOS")
            message("Target Platform: iOS")
        else()
            set(PLATFORM_MACOS TRUE CACHE INTERNAL "Target platform: MacOS")
            message("Target Platform: MacOS")
        endif()
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
endif(WIN32)

add_library(BuildSettings INTERFACE)

if(PLATFORM_WIN32)
    if(MSVC)
        set(D3D11_SUPPORTED TRUE CACHE INTERNAL "D3D11 is supported on Win32 platform")
        if(NOT CMAKE_SYSTEM_VERSION STREQUAL "8.1")
            set(D3D12_SUPPORTED TRUE CACHE INTERNAL "D3D12 is supported on Win32 platform")
        endif()
    else()
        set(D3D11_SUPPORTED FALSE CACHE INTERNAL "D3D11 requires compiling with MSVC")
        set(D3D12_SUPPORTED FALSE CACHE INTERNAL "D3D12 requires compiling with MSVC")
    endif()

    set(GL_SUPPORTED TRUE CACHE INTERNAL "OpenGL is supported on Win32 platform")
	if(${ARCH} EQUAL 64)
        set(VULKAN_SUPPORTED TRUE CACHE INTERNAL "Vulkan is supported on Win64 platform")
    endif()
    target_compile_definitions(BuildSettings INTERFACE PLATFORM_WIN32=1)
elseif(PLATFORM_UNIVERSAL_WINDOWS)
    set(D3D11_SUPPORTED TRUE CACHE INTERNAL "D3D11 is supported on Univeral Windows platform")
	if(NOT CMAKE_SYSTEM_VERSION STREQUAL "8.1")
		set(D3D12_SUPPORTED TRUE CACHE INTERNAL "D3D12 is supported on Univeral Windows platform")
	endif()
    target_compile_definitions(BuildSettings INTERFACE PLATFORM_UNIVERSAL_WINDOWS=1)
elseif(PLATFORM_ANDROID)
    set(GLES_SUPPORTED TRUE CACHE INTERNAL "OpenGLES is supported on Android platform")
    target_compile_definitions(BuildSettings INTERFACE PLATFORM_ANDROID=1)
elseif(PLATFORM_LINUX)
    set(GL_SUPPORTED TRUE CACHE INTERNAL "OpenGL is supported on Linux platform")
    if(${ARCH} EQUAL 64)
        set(VULKAN_SUPPORTED TRUE CACHE INTERNAL "Vulkan is supported on Linux64 platform")
    endif()
    target_compile_definitions(BuildSettings INTERFACE PLATFORM_LINUX=1)
elseif(PLATFORM_MACOS)
    set(GL_SUPPORTED TRUE CACHE INTERNAL "OpenGL is supported on MacOS platform")
    target_compile_definitions(BuildSettings INTERFACE PLATFORM_MACOS=1)
elseif(PLATFORM_IOS)
    set(GLES_SUPPORTED TRUE CACHE INTERNAL "OpenGLES is supported on iOS platform")
    target_compile_definitions(BuildSettings INTERFACE PLATFORM_IOS=1)
else()
    message(FATAL_ERROR "No PLATFORM_XXX variable defined. Make sure that 'DiligentCore' folder is processed first")
endif()

message("D3D11_SUPPORTED: " ${D3D11_SUPPORTED})
message("D3D12_SUPPORTED: " ${D3D12_SUPPORTED})
message("GL_SUPPORTED: " ${GL_SUPPORTED})
message("GLES_SUPPORTED: " ${GLES_SUPPORTED})
message("VULKAN_SUPPORTED: " ${VULKAN_SUPPORTED})

target_compile_definitions(BuildSettings 
INTERFACE 
    D3D11_SUPPORTED=$<BOOL:${D3D11_SUPPORTED}>
    D3D12_SUPPORTED=$<BOOL:${D3D12_SUPPORTED}>
    GL_SUPPORTED=$<BOOL:${GL_SUPPORTED}>
    GLES_SUPPORTED=$<BOOL:${GLES_SUPPORTED}>
    VULKAN_SUPPORTED=$<BOOL:${VULKAN_SUPPORTED}>
)


if(MSVC)
    # For msvc, enable level 4 warnings except for
    # - w4100 - unreferenced formal parameter
    # - w4505 - unreferenced local function has been removed
    target_compile_options(BuildSettings INTERFACE /W4 /wd4100 /wd4505)
    # In all release modes also:
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
	# - Enable full optimization (/Ox)
    set(MSVC_RELEASE_COMPILE_OPTIONS ${MSVC_ALL_RELEASE_COMPILE_OPTIONS} /Ot /Oi /Ox)
    # In MINSIZEREL mode set favor small code option (/Os)
    set(MSVC_MINSIZEREL_COMPILE_OPTIONS ${MSVC_ALL_RELEASE_COMPILE_OPTIONS} /Os)
    set(MSVC_RELWITHDEBINFO_COMPILE_OPTIONS ${MSVC_ALL_RELEASE_COMPILE_OPTIONS} /Ot /Oi /Ob2 /Ox)
    target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:RELEASE>:${MSVC_RELEASE_COMPILE_OPTIONS}>")
    target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:MINSIZEREL>:${MSVC_MINSIZEREL_COMPILE_OPTIONS}>")
    target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:RELWITHDEBINFO>:${MSVC_RELWITHDEBINFO_COMPILE_OPTIONS}>")
    # !!!NOTE!!! For some reason above is the only form of generator expression that works
    # For instance, this way
    # target_compile_options(BuildSettings INTERFACE "$<$<CONFIG:RELEASE>:/Ot>")
    # does not work as expected

    set(DEBUG_MACROS DEVELOPMENT)
    target_compile_definitions(BuildSettings INTERFACE "$<$<CONFIG:DEBUG>:${DEBUG_MACROS}>")
else()

    set(DEBUG_MACROS _DEBUG DEBUG DEVELOPMENT)
    set(RELEASE_MACROS NDEBUG)

    foreach(DBG_CONFIG ${DEBUG_CONFIGURATIONS})
        target_compile_definitions(BuildSettings INTERFACE "$<$<CONFIG:${DBG_CONFIG}>:${DEBUG_MACROS}>")
    endforeach()

    foreach(REL_CONFIG ${RELEASE_CONFIGURATIONS})
        target_compile_definitions(BuildSettings INTERFACE "$<$<CONFIG:${REL_CONFIG}>:${RELEASE_MACROS}>")
    endforeach()

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


if(PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS OR PLATFORM_MACOS)

    # https://cmake.org/cmake/help/v3.8/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html#prop_gbl:CMAKE_CXX_KNOWN_FEATURES
    target_compile_features(BuildSettings INTERFACE cxx_std_11) # Generates an error in gradle build on Android

elseif(PLATFORM_ANDROID)

    # LOCAL_CPP_FEATURES := exceptions rtti

elseif(PLATFORM_IOS)

    
endif()

if(PLATFORM_MACOS)
    find_library(APP_KIT AppKit)
    if (NOT APP_KIT)
            message(FATAL_ERROR "AppKit not found")
    endif()
elseif(PLATFORM_IOS)
    find_library(CORE_FOUNDATION CoreFoundation)
    if(NOT CORE_FOUNDATION)
        message(FATAL_ERROR "Cannot find CoreFoundation framework")
    endif()

    find_library(FOUNDATION Foundation)
    if(NOT FOUNDATION)
        message(FATAL_ERROR "Cannot find Foundation framework")
    endif()

    find_library(OPENGLES OpenGLES)
    if(NOT OPENGLES)
        message(FATAL_ERROR "Cannot find OpenGLES framework")
    endif()
endif()

if(NOT DILIGENT_CORE_INSTALL_DIR)
    set(DILIGENT_CORE_INSTALL_DIR "DiligentCore")
endif()

include(BuildUtils.cmake)

add_subdirectory(Utilities)
add_subdirectory(Primitives)
add_subdirectory(Platforms)
add_subdirectory(External)
add_subdirectory(Common)
add_subdirectory(Graphics)
