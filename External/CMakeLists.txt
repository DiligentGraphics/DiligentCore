cmake_minimum_required (VERSION 3.6)

if(PLATFORM_WIN32 OR PLATFORM_LINUX OR PLATFORM_MACOS)
	add_subdirectory(glew)
endif()

if(PLATFORM_ANDROID)
	add_subdirectory(Android)
endif()

if(PLATFORM_WIN32 OR PLATFORM_LINUX)
    # We only need to have SPIRV-Headers folder, but we
    # do not really need any target from the project.
    #add_subdirectory(SPIRV-Headers)
    #set_directory_root_folder("SPIRV-Headers" "Core/External/SPIRV-Headers")

    set(SPIRV-Headers_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Headers")
    set(SPIRV_SKIP_TESTS ON CACHE BOOL "Skip SPIRV tests")
    set(SPIRV_BUILD_COMPRESSION OFF CACHE BOOL "Do not build SPIRV compressing codec")
    set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Skip building SPIRV executable and tests along with the library")
    set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "Skip SPIRV tools installation")
    set(SPIRV_WERROR OFF CACHE BOOL "Do not enable error on warning")
    add_subdirectory(SPIRV-Tools)
    set_directory_root_folder("SPIRV-Tools" "Core/External/SPIRV-Tools")
    # We only need SPIRV-Tools-opt for glslang that also depends on SPIRV-Tools.
    # Exclude all other targets from build to save build time
    find_targets_in_directory(SPIRV_Tools_Targets "SPIRV-Tools")
    list(REMOVE_ITEM SPIRV_Tools_Targets SPIRV-Tools SPIRV-Tools-opt)
    set_target_properties(${SPIRV_Tools_Targets} PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)

    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "Do not build glslang binaries")
    set(ENABLE_SPVREMAPPER OFF CACHE BOOL "Do not build SPIRV remapper")
    set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "Skip glslang installation")
    add_subdirectory(glslang)
    set_directory_root_folder("glslang" "Core/External/glslang")

    # glslang "kindly" sets global CMAKE_DEBUG_POSTFIX to "d", which we have to unset now
    unset(CMAKE_DEBUG_POSTFIX CACHE)

    add_subdirectory(SPIRV-Cross)
    set_target_properties(SPIRVCross PROPERTIES
        FOLDER Core/External
    )

endif()
