cmake_minimum_required (VERSION 3.6)

if(PLATFORM_WIN32 OR PLATFORM_LINUX OR PLATFORM_MACOS)
	add_subdirectory(glew)
    if(INSTALL_DILIGENT_CORE)
        install(TARGETS glew-static
                ARCHIVE DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/lib/$<CONFIG>"
                LIBRARY DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/lib/$<CONFIG>"
                RUNTIME DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/bin/$<CONFIG>"
        )
    endif()
    install(FILES glew/LICENSE.txt DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/Licenses/ThirdParty" RENAME GLEW-License.txt)
endif()

if(VULKAN_SUPPORTED)
    # We only need to have SPIRV-Headers folder, but we
    # do not really need any target from the project.
    #add_subdirectory(SPIRV-Headers)
    #set_directory_root_folder("SPIRV-Headers" "DiligentCore/ThirdParty/SPIRV-Headers")
    install(FILES SPIRV-Headers/LICENSE DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/Licenses/ThirdParty" RENAME SPIRV-Headers-License.txt)

    option(DILIGENT_NO_GLSLANG "Do NOT build glslang compiler" OFF)

    set(SPIRV-Headers_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Headers")
    set(SPIRV_SKIP_TESTS ON CACHE BOOL "Skip SPIRV tests")
    set(SPIRV_BUILD_COMPRESSION OFF CACHE BOOL "Do not build SPIRV compressing codec")
    set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Skip building SPIRV executable and tests along with the library")
    set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "Skip SPIRV tools installation")
    set(SPIRV_WERROR OFF CACHE BOOL "Do not enable error on warning")
    set(CMAKE_MACOSX_RPATH TRUE)
    add_subdirectory(SPIRV-Tools)
    set_directory_root_folder("SPIRV-Tools" "DiligentCore/ThirdParty/SPIRV-Tools")
    # We only need SPIRV-Tools-opt for glslang that also depends on SPIRV-Tools.
    # Exclude all other targets from build to save build time
    set(SPIRV_TOOLS_EXCLUDE_TARGETS SPIRV-Tools-link SPIRV-Tools-reduce SPIRV-Tools-shared)
    set_target_properties(${SPIRV_TOOLS_EXCLUDE_TARGETS} PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)
        
    set(INSTALL_TARGETS SPIRV-Tools SPIRV-Tools-opt)
    install(FILES SPIRV-Tools/LICENSE DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/Licenses/ThirdParty" RENAME SPIRV-Tools-License.txt)

    if(NOT ${DILIGENT_NO_GLSLANG})    
        set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "Do not build glslang binaries")
        set(ENABLE_SPVREMAPPER OFF CACHE BOOL "Do not build SPIRV remapper")
        set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "Skip glslang installation")
        set(ENABLE_CTEST OFF CACHE BOOL "Disable testing")
        add_subdirectory(glslang)
        set_directory_root_folder("glslang" "DiligentCore/ThirdParty/glslang")

        # glslang "kindly" sets global CMAKE_DEBUG_POSTFIX to "d", which we have to unset now
        unset(CMAKE_DEBUG_POSTFIX CACHE)

        list(APPEND INSTALL_TARGETS glslang OGLCompiler OSDependent SPIRV HLSL)
        install(FILES glslang/LICENSE.txt DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/Licenses/ThirdParty" RENAME GLSLang-License.txt)
    endif()

    set(SPIRV_CROSS_CLI OFF CACHE BOOL "Build the CLI binary. Requires SPIRV_CROSS_STATIC.")
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "Enable SPIRV-Cross tests.")
    set(SPIRV_CROSS_ENABLE_GLSL OFF CACHE BOOL "Enable GLSL support.")
    set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "Enable HLSL target support.")
    set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "Enable MSL target support.")
    set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "Enable C++ target support.")
    set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "Enable JSON reflection target support.")
    set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "Enable C API wrapper support in static library.")
    set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "Enable util module support.")
    set(SPIRV_CROSS_FORCE_PIC ON CACHE BOOL "Force position-independent code.")
    set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "Skip SPIRV-Cross installation.")
    add_subdirectory(SPIRV-Cross)
    set_directory_root_folder("SPIRV-Cross" "DiligentCore/ThirdParty/SPIRV-Cross")

    list(APPEND INSTALL_TARGETS spirv-cross-core)
    install(FILES SPIRV-Cross/LICENSE DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/Licenses/ThirdParty" RENAME SPIRV-Cross-License.txt)

    if(INSTALL_DILIGENT_CORE)
        install(TARGETS ${INSTALL_TARGETS}
                ARCHIVE DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/lib/$<CONFIG>"
                LIBRARY DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/lib/$<CONFIG>"
                RUNTIME DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/bin/$<CONFIG>"
        )
    endif()

endif()
