cmake_minimum_required (VERSION 3.6)

if(PLATFORM_WIN32 OR PLATFORM_LINUX OR PLATFORM_MACOS)
	add_subdirectory(glew)
endif()

if(PLATFORM_ANDROID)
	add_subdirectory(Android)
endif()

if(PLATFORM_WIN32)
    # We only need to have SPIRV-Headers folder, but we
    # do not really need any target from the project.
    #add_subdirectory(SPIRV-Headers)
    #set_directory_root_folder("SPIRV-Headers" "Core/External/SPIRV-Headers")

    set(SPIRV-Headers_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Headers")
    set(SPIRV_SKIP_TESTS ON)
    set(SPIRV_SKIP_EXECUTABLES ON)
    set(SKIP_SPIRV_TOOLS_INSTALL ON)
    add_subdirectory(SPIRV-Tools)
    set_directory_root_folder("SPIRV-Tools" "Core/External/SPIRV-Tools")

    option(ENABLE_GLSLANG_BINARIES OFF)
    option(ENABLE_SPVREMAPPER OFF)
    set(SKIP_GLSLANG_INSTALL ON)
    add_subdirectory(glslang)
    set_directory_root_folder("glslang" "Core/External/glslang")

    # glslang "kindly" sets global CMAKE_DEBUG_POSTFIX to "d", which we have to unset now
    unset(CMAKE_DEBUG_POSTFIX CACHE)

    add_subdirectory(SPIRV-Cross)
    set_target_properties(SPIRVCross PROPERTIES
        FOLDER Core/External
    )

endif()
