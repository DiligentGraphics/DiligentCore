if(NOT VULKAN_SDK)
	set(VULKAN_SDK $ENV{VULKAN_SDK})
endif()

if(VULKAN_SDK AND (PLATFORM_WIN32 OR PLATFORM_MACOS OR PLATFORM_LINUX))
	message("VULKAN_SDK: " ${VULKAN_SDK})
	if (PLATFORM_WIN32)
		set(VK_SDK_LIB_PATH ${VULKAN_SDK}/Lib)
		set(VK_SDK_INCLUDE_PATH ${VULKAN_SDK}/Include)
	elseif (PLATFORM_MACOS)
		set(VK_SDK_LIB_PATH ${VULKAN_SDK}/macOS/lib)
		set(VK_SDK_INCLUDE_PATH ${VULKAN_SDK}/macOS/include)
	endif ()

	find_library(SPIRV-Tools-lib     "SPIRV-Tools"	   "${VK_SDK_LIB_PATH}" NO_CACHE)
	find_library(SPIRV-Tools-opt-lib "SPIRV-Tools-opt" "${VK_SDK_LIB_PATH}" NO_CACHE)
	if (SPIRV-Tools-lib AND SPIRV-Tools-opt-lib)
		add_library(SPIRV-Tools-static INTERFACE)
		target_link_libraries(SPIRV-Tools-static INTERFACE ${SPIRV-Tools-lib})
	
		add_library(SPIRV-Tools-opt INTERFACE)
		target_link_libraries(SPIRV-Tools-opt INTERFACE ${SPIRV-Tools-opt-lib} SPIRV-Tools-static)
		target_include_directories(SPIRV-Tools-opt INTERFACE "${VK_SDK_INCLUDE_PATH}")
	else ()
		message("Unable to find required SPIRV-Tools libraries at the specified Vulkan SDK path")
	endif ()


	find_library(glslang-lib			"glslang"			 "${VK_SDK_LIB_PATH}" NO_CACHE)
	find_library(GenericCodeGen-lib		"GenericCodeGen"	 "${VK_SDK_LIB_PATH}" NO_CACHE)
	find_library(OGLCompiler-lib		"OGLCompiler"		 "${VK_SDK_LIB_PATH}" NO_CACHE)
	find_library(OSDependent-lib		"OSDependent"		 "${VK_SDK_LIB_PATH}" NO_CACHE)
	find_library(SPIRV-lib		        "SPIRV"				 "${VK_SDK_LIB_PATH}" NO_CACHE)
	find_library(HLSL-lib		        "HLSL"				 "${VK_SDK_LIB_PATH}" NO_CACHE)
	find_library(MachineIndependent-lib "MachineIndependent" "${VK_SDK_LIB_PATH}" NO_CACHE)
	if (GenericCodeGen-lib AND glslang-lib AND OGLCompiler-lib AND OSDependent-lib AND SPIRV-lib AND HLSL-lib AND MachineIndependent-lib)

		add_library(GenericCodeGen INTERFACE)
		target_link_libraries(GenericCodeGen INTERFACE ${GenericCodeGen-lib})
		target_include_directories(GenericCodeGen INTERFACE "${VK_SDK_INCLUDE_PATH}/glslang")

		add_library(OGLCompiler INTERFACE)
		target_link_libraries(OGLCompiler INTERFACE ${OGLCompiler-lib})
		target_include_directories(OGLCompiler INTERFACE "${VK_SDK_INCLUDE_PATH}/glslang")
		
		add_library(OSDependent INTERFACE)
		target_link_libraries(OSDependent INTERFACE ${OSDependent-lib})
		target_include_directories(OSDependent INTERFACE "${VK_SDK_INCLUDE_PATH}/glslang")
		
		add_library(SPIRV INTERFACE)
		target_link_libraries(SPIRV INTERFACE ${SPIRV-lib})
		target_include_directories(SPIRV INTERFACE "${VK_SDK_INCLUDE_PATH}/glslang")
		
		add_library(HLSL INTERFACE)
		target_link_libraries(HLSL INTERFACE ${HLSL-lib})
		target_include_directories(HLSL INTERFACE "${VK_SDK_INCLUDE_PATH}/glslang")
		
		add_library(MachineIndependent INTERFACE)
		target_link_libraries(MachineIndependent INTERFACE ${MachineIndependent-lib})
		target_include_directories(MachineIndependent INTERFACE "${VK_SDK_INCLUDE_PATH}/glslang")

		add_library(glslang INTERFACE)
		target_include_directories(glslang INTERFACE "${VK_SDK_INCLUDE_PATH}/glslang")
		target_link_libraries(glslang INTERFACE
			${glslang-lib}
			GenericCodeGen
			OGLCompiler
			OSDependent
			SPIRV
			HLSL
			MachineIndependent
		)
	else ()
		message("Unable to find required glslang libraries at the specified Vulkan SDK path")
	endif ()

	find_library(spirv-cross-core-lib "spirv-cross-core" "${VK_SDK_LIB_PATH}" NO_CACHE)
	find_library(spirv-cross-msl-lib  "spirv-cross-msl"	 "${VK_SDK_LIB_PATH}" NO_CACHE)
	find_library(spirv-cross-glsl-lib "spirv-cross-glsl" "${VK_SDK_LIB_PATH}" NO_CACHE)

	if (spirv-cross-core-lib AND spirv-cross-msl-lib AND spirv-cross-glsl-lib)
		add_library(spirv-cross-core INTERFACE)
		target_link_libraries(spirv-cross-core INTERFACE ${spirv-cross-core-lib})
		target_include_directories(spirv-cross-core INTERFACE "${VK_SDK_INCLUDE_PATH}/spirv_cross")

		add_library(spirv-cross-msl INTERFACE)
		target_link_libraries(spirv-cross-msl INTERFACE ${spirv-cross-msl-lib})
		target_include_directories(spirv-cross-msl INTERFACE "${VK_SDK_INCLUDE_PATH}/spirv_cross")

		add_library(spirv-cross-glsl INTERFACE)
		target_link_libraries(spirv-cross-glsl INTERFACE ${spirv-cross-glsl-lib})
		target_include_directories(spirv-cross-glsl INTERFACE "${VK_SDK_INCLUDE_PATH}/spirv_cross")
	endif ()

endif()
