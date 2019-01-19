cmake_minimum_required (VERSION 3.6)

if(PLATFORM_WIN32 OR PLATFORM_LINUX OR PLATFORM_MACOS)
	add_subdirectory(glew)
    if(INSTALL_DILIGENT_CORE)
        install(TARGETS glew-static
                ARCHIVE DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/lib"
                LIBRARY DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/lib"
                RUNTIME DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/bin"
        )
    endif()
endif()

if(PLATFORM_ANDROID)
	add_subdirectory(Android)
endif()

if(VULKAN_SUPPORTED)
    # We only need to have SPIRV-Headers folder, but we
    # do not really need any target from the project.
    #add_subdirectory(SPIRV-Headers)
    #set_directory_root_folder("SPIRV-Headers" "Core/External/SPIRV-Headers")

    option(DILIGENT_NO_GLSLANG "Do NOT build glslang compiler" OFF)
    
    set(SPIRV-Headers_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Headers")
    set(SPIRV_SKIP_TESTS ON CACHE BOOL "Skip SPIRV tests")
    set(SPIRV_BUILD_COMPRESSION OFF CACHE BOOL "Do not build SPIRV compressing codec")
    set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Skip building SPIRV executable and tests along with the library")
    set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "Skip SPIRV tools installation")
    set(SPIRV_WERROR OFF CACHE BOOL "Do not enable error on warning")
    set(CMAKE_MACOSX_RPATH TRUE)
    add_subdirectory(SPIRV-Tools)
    set_directory_root_folder("SPIRV-Tools" "Core/External/SPIRV-Tools")
    # We only need SPIRV-Tools-opt for glslang that also depends on SPIRV-Tools.
    # Exclude all other targets from build to save build time
    set(SPIRV_TOOLS_EXCLUDE_TARGETS SPIRV-Tools-link SPIRV-Tools-reduce SPIRV-Tools-shared)
    set_target_properties(${SPIRV_TOOLS_EXCLUDE_TARGETS} PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)

    set(INSTALL_TARGETS SPIRV-Tools SPIRV-Tools-opt)

    if(NOT ${DILIGENT_NO_GLSLANG})    
        set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "Do not build glslang binaries")
        set(ENABLE_SPVREMAPPER OFF CACHE BOOL "Do not build SPIRV remapper")
        set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "Skip glslang installation")
        add_subdirectory(glslang)
        set_directory_root_folder("glslang" "Core/External/glslang")

        # glslang "kindly" sets global CMAKE_DEBUG_POSTFIX to "d", which we have to unset now
        unset(CMAKE_DEBUG_POSTFIX CACHE)

        list(APPEND INSTALL_TARGETS glslang OGLCompiler OSDependent SPIRV HLSL)
    endif()

    list(APPEND INSTALL_TARGETS SPIRVCross)

    add_subdirectory(SPIRV-Cross)
    set_target_properties(SPIRVCross PROPERTIES
        FOLDER Core/External
    )

    if(INSTALL_DILIGENT_CORE)
        install(TARGETS ${INSTALL_TARGETS}
                ARCHIVE DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/lib"
                LIBRARY DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/lib"
                RUNTIME DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/bin"
        )
    endif()

endif()
