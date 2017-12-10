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

# https://cmake.org/cmake/help/v3.8/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html#prop_gbl:CMAKE_CXX_KNOWN_FEATURES
target_compile_features(BuildSettings INTERFACE cxx_std_11)

if(PLATFORM_WIN32 OR PLATFORM_UNVIRSAL_WINDOWS)
	function(copy_required_dlls TARGET_NAME)
		set(ENGINE_DLLS 
			GraphicsEngineD3D11-shared 
			GraphicsEngineD3D12-shared 
		)
		if(PLATFORM_WIN32)
			list(APPEND ENGINE_DLLS GraphicsEngineOpenGL-shared)
		endif()

		foreach(DLL ${ENGINE_DLLS})
			add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
					"\"$<TARGET_FILE:${DLL}>\""
					"\"$<TARGET_FILE_DIR:${TARGET_NAME}>\"")
		endforeach(DLL)

		# Copy D3Dcompiler_47.dll 
		if(MSVC)
			if(WIN64)
				set(D3D_COMPILER_PATH "\"$(VC_ExecutablePath_x64_x64)\\D3Dcompiler_47.dll\"")
			else()
				set(D3D_COMPILER_PATH "\"$(VC_ExecutablePath_x86_x86)\\D3Dcompiler_47.dll\"")
			endif()
			add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
					${D3D_COMPILER_PATH}
					"\"$<TARGET_FILE_DIR:${TARGET_NAME}>\"")
		endif()
	endfunction(copy_required_dlls)
endif()

add_subdirectory(Utilities)
add_subdirectory(Platforms)
add_subdirectory(External)
add_subdirectory(Common)
add_subdirectory(Graphics)



