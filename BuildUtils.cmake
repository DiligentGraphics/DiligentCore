if(PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS)
    
    function(copy_required_dlls TARGET_NAME)
        set(ENGINE_DLLS 
            GraphicsEngineD3D11-shared 
        )
        if(D3D12_SUPPORTED)
            list(APPEND ENGINE_DLLS GraphicsEngineD3D12-shared )
        endif()
        if(GL_SUPPORTED)
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
                    LINK_FLAGS_${REL_CONFIG} "/LTCG /OPT:REF"
                )
            endforeach()

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
    
    if(PLATFORM_IOS)
        # Feature detection fails for iOS build, so we have to set CXX_STANDARD
        # as a workaround
        set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 11)

        set_target_properties(${TARGET} PROPERTIES
            XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET 10.0
        )
    endif()
endfunction()
