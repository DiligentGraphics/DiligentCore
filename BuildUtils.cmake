if(PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS)
    
    function(copy_required_dlls TARGET_NAME)
        if(D3D11_SUPPORTED)
            list(APPEND ENGINE_DLLS Diligent-GraphicsEngineD3D11-shared)
        endif()
        if(D3D12_SUPPORTED)
            list(APPEND ENGINE_DLLS Diligent-GraphicsEngineD3D12-shared)
        endif()
        if(GL_SUPPORTED)
            list(APPEND ENGINE_DLLS Diligent-GraphicsEngineOpenGL-shared)
        endif()
        if(VULKAN_SUPPORTED)
            list(APPEND ENGINE_DLLS Diligent-GraphicsEngineVk-shared)
        endif()
        if(METAL_SUPPORTED)
            list(APPEND ENGINE_DLLS Diligent-GraphicsEngineMetal-shared)
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

    # Set dll output name by adding _{32|64}{r|d} suffix
    function(set_dll_output_name TARGET_NAME OUTPUT_NAME_WITHOUT_SUFFIX)
        foreach(DBG_CONFIG ${DEBUG_CONFIGURATIONS})
            set_target_properties(${TARGET_NAME} PROPERTIES
                OUTPUT_NAME_${DBG_CONFIG} ${OUTPUT_NAME_WITHOUT_SUFFIX}${DLL_DBG_SUFFIX}
            )
        endforeach()

        foreach(REL_CONFIG ${RELEASE_CONFIGURATIONS})
            set_target_properties(${TARGET_NAME} PROPERTIES
                OUTPUT_NAME_${REL_CONFIG} ${OUTPUT_NAME_WITHOUT_SUFFIX}${DLL_REL_SUFFIX}
            )
        endforeach()
    endfunction()

endif(PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS)


function(set_common_target_properties TARGET)
    
    if(COMMAND custom_configure_target)
        custom_configure_target(${TARGET})
        if(TARGET_CONFIGURATION_COMPLETE)
            return()
        endif()
    endif()

    get_target_property(TARGET_TYPE ${TARGET} TYPE)

    if(MSVC)
        # For msvc, enable link-time code generation for release builds (I was not able to 
        # find any way to set these settings through interface library BuildSettings)
        if(TARGET_TYPE STREQUAL STATIC_LIBRARY)

            foreach(REL_CONFIG ${RELEASE_CONFIGURATIONS})
                set_target_properties(${TARGET} PROPERTIES
                    STATIC_LIBRARY_FLAGS_${REL_CONFIG} /LTCG
                )
            endforeach()

        else()

            foreach(REL_CONFIG ${RELEASE_CONFIGURATIONS})
                set_target_properties(${TARGET} PROPERTIES
                    LINK_FLAGS_${REL_CONFIG} "/LTCG /OPT:REF /INCREMENTAL:NO"
                )
            endforeach()

            if(PLATFORM_UNIVERSAL_WINDOWS)
                # On UWP, disable incremental link to avoid linker warnings
                foreach(DBG_CONFIG ${DEBUG_CONFIGURATIONS})
                    set_target_properties(${TARGET} PROPERTIES
                        LINK_FLAGS_${DBG_CONFIG} "/INCREMENTAL:NO"
                    )
                endforeach()
            endif()
        endif()
    else()
        set_target_properties(${TARGET} PROPERTIES
            CXX_VISIBILITY_PRESET hidden # -fvisibility=hidden
            C_VISIBILITY_PRESET hidden # -fvisibility=hidden
            VISIBILITY_INLINES_HIDDEN TRUE

            # Without -fPIC option GCC fails to link static libraries into dynamic library:
            #  -fPIC  
            #      If supported for the target machine, emit position-independent code, suitable for 
            #      dynamic linking and avoiding any limit on the size of the global offset table.
            POSITION_INDEPENDENT_CODE ON

            # It is crucial to set CXX_STANDARD flag to only affect c++ files and avoid failures compiling c-files:
            # error: invalid argument '-std=c++11' not allowed with 'C/ObjC'
            CXX_STANDARD 11
            CXX_STANDARD_REQUIRED ON
        )

        if(NOT MINGW_BUILD)
            # Do not disable extensions when building with MinGW!
            set_target_properties(${TARGET} PROPERTIES
                CXX_EXTENSIONS OFF
            )
        endif()
    endif()
    
    if(PLATFORM_IOS)
        set_target_properties(${TARGET} PROPERTIES
            XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET 10.0
        )
    endif()
endfunction()

function(find_targets_in_directory _RESULT _DIR)
    get_property(_subdirs DIRECTORY "${_DIR}" PROPERTY SUBDIRECTORIES)
    foreach(_subdir IN LISTS _subdirs)
        find_targets_in_directory(${_RESULT} "${_subdir}")
    endforeach()
    get_property(_SUB_TARGETS DIRECTORY "${_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    set(${_RESULT} ${${_RESULT}} ${_SUB_TARGETS} PARENT_SCOPE)
endfunction()

function(set_directory_root_folder _DIRECTORY _ROOT_FOLDER)
    find_targets_in_directory(_TARGETS ${_DIRECTORY})
    foreach(_TARGET IN LISTS _TARGETS)
        get_target_property(_FOLDER ${_TARGET} FOLDER)
        if(_FOLDER)
            set_target_properties(${_TARGET} PROPERTIES FOLDER "${_ROOT_FOLDER}/${_FOLDER}")
        else()
            set_target_properties(${_TARGET} PROPERTIES FOLDER "${_ROOT_FOLDER}")
        endif()
    endforeach()
endfunction()


# Returns default backend library type (static/dynamic) for the current platform
function(get_backend_libraries_type _LIB_TYPE)
    if(PLATFORM_WIN32 OR PLATFORM_LINUX OR PLATFORM_ANDROID)
        set(LIB_TYPE "shared")
    elseif(PLATFORM_UNIVERSAL_WINDOWS OR PLATFORM_MACOS)
        set(LIB_TYPE "static")
    elseif(PLATFORM_IOS)
        # Statically link with the engine on iOS.
        # It is also possible to link dynamically by
        # putting the library into the framework.
        set(LIB_TYPE "static")
    else()
        message(FATAL_ERROR "Undefined platform")
    endif()
    set(${_LIB_TYPE} ${LIB_TYPE} PARENT_SCOPE)
endfunction()


# Adds the list of supported backend targets to variable ${_TARGETS} in parent scope.
# Second argument to the function may override the target type (static/dynamic). If It
# is not given, default target type for the platform is used.
function(get_supported_backends _TARGETS)
    if(${ARGC} GREATER 1)
        set(LIB_TYPE ${ARGV1})
    else()
        get_backend_libraries_type(LIB_TYPE)
    endif()

    if(D3D11_SUPPORTED)
	    list(APPEND BACKENDS Diligent-GraphicsEngineD3D11-${LIB_TYPE})
    endif()
    if(D3D12_SUPPORTED)
	    list(APPEND BACKENDS Diligent-GraphicsEngineD3D12-${LIB_TYPE})
    endif()
    if(GL_SUPPORTED OR GLES_SUPPORTED)
	    list(APPEND BACKENDS Diligent-GraphicsEngineOpenGL-${LIB_TYPE})
    endif()
    if(VULKAN_SUPPORTED)
	    list(APPEND BACKENDS Diligent-GraphicsEngineVk-${LIB_TYPE})
    endif()
    if(METAL_SUPPORTED)
	    list(APPEND BACKENDS Diligent-GraphicsEngineMetal-${LIB_TYPE})
    endif()
    # ${_TARGETS} == ENGINE_LIBRARIES
    # ${${_TARGETS}} == ${ENGINE_LIBRARIES}
    set(${_TARGETS} ${${_TARGETS}} ${BACKENDS} PARENT_SCOPE)
endfunction()


# Returns path to the library relative to the DiligentCore root
function(get_core_library_relative_dir _TARGET _DIR)
    # Use the directory of Primitives (the first target processed) as reference
    get_target_property(PRIMITIVES_SOURCE_DIR Diligent-Primitives SOURCE_DIR)
    get_target_property(TARGET_SOURCE_DIR ${_TARGET} SOURCE_DIR)
    file(RELATIVE_PATH TARGET_RELATIVE_PATH "${PRIMITIVES_SOURCE_DIR}/.." "${TARGET_SOURCE_DIR}")
    set(${_DIR} ${TARGET_RELATIVE_PATH} PARENT_SCOPE)
endfunction()

# Performs installation steps for the core library
function(install_core_lib _TARGET)
    get_core_library_relative_dir(${_TARGET} TARGET_RELATIVE_PATH)

    get_target_property(TARGET_TYPE ${_TARGET} TYPE)
    if(TARGET_TYPE STREQUAL STATIC_LIBRARY)
        list(APPEND DILIGENT_CORE_INSTALL_LIBS_LIST ${_TARGET})
        set(DILIGENT_CORE_INSTALL_LIBS_LIST ${DILIGENT_CORE_INSTALL_LIBS_LIST} CACHE INTERNAL "Core libraries installation list")
    elseif(TARGET_TYPE STREQUAL SHARED_LIBRARY)
        install(TARGETS				 ${_TARGET}
                ARCHIVE DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/lib/$<CONFIG>"
                LIBRARY DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/bin/$<CONFIG>"
                RUNTIME DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/bin/$<CONFIG>"
        )
        if (DILIGENT_INSTALL_PDB)
            install(FILES $<TARGET_PDB_FILE:${_TARGET}> DESTINATION "${DILIGENT_CORE_INSTALL_DIR}/bin/$<CONFIG>" OPTIONAL)
        endif()
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/interface")
        install(DIRECTORY    interface
                DESTINATION  "${DILIGENT_CORE_INSTALL_DIR}/headers/${TARGET_RELATIVE_PATH}/"
        )
    endif()
endfunction()
