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
	endfunction()

endif(PLATFORM_WIN32 OR PLATFORM_UNVIRSAL_WINDOWS)


function(set_common_target_properties TARGET)
	
	get_target_property(TARGET_TYPE ${TARGET} TYPE)

	if(MSVC)
		# For msvc, enable link-time code generation for release builds (I was not able to 
		# find any way to set these settings through interface library BuildSettings)
		if(TARGET_TYPE STREQUAL STATIC_LIBRARY)
			set_target_properties(${TARGET} PROPERTIES
				STATIC_LIBRARY_FLAGS_RELEASE /LTCG
				STATIC_LIBRARY_FLAGS_MINSIZEREL /LTCG
				STATIC_LIBRARY_FLAGS_RELWITHDEBINFO /LTCG
			)
		else()
			set_target_properties(${TARGET} PROPERTIES
				LINK_FLAGS_RELEASE "/LTCG /OPT:REF"
				LINK_FLAGS_MINSIZEREL "/LTCG /OPT:REF"
				LINK_FLAGS_RELWITHDEBINFO "/LTCG /OPT:REF"
			)

			if(PLATFORM_UNIVERSAL_WINDOWS)
				# On UWP, disable incremental link to avoid linker warnings
				set_target_properties(${TARGET} PROPERTIES
					LINK_FLAGS_DEBUG /INCREMENTAL:NO
				)
			endif()
		endif()
	endif()

	if(PLATFORM_ANDROID)
		# target_compile_features(BuildSettings INTERFACE cxx_std_11) generates an error in gradle build on Android
		# It is crucial to set CXX_STANDARD flag to only affect c++ files and avoid failures compiling c-files:
		# error: invalid argument '-std=c++11' not allowed with 'C/ObjC'
		set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 11)
	endif()

endfunction()
