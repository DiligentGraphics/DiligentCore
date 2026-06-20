# Finds Windows SDK and sets the following cache variables:
# - WINDOWS_SDK_VERSION (e.g. 10.0.22621.0). If not found, set to 0.0
# - WINDOWS_SDK_BIN_DIR (e.g. C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64)
# - D3D_COMPILER_PATH (e.g. C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/D3Dcompiler_47.dll)
# - DXC_COMPILER_PATH (e.g. C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/dxcompiler.dll)
# - DXIL_SIGNER_PATH (e.g. C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/dxil.dll)
function(find_windows_sdk)
    # Determine Windows SDK version
    if (CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        set(WINDOWS_SDK_VERSION ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION} CACHE INTERNAL "Windows SDK version")
    elseif(DEFINED ENV{WindowsSDKVersion})
        set(WINDOWS_SDK_VERSION $ENV{WindowsSDKVersion})
        # For unbeknown reason, the value ends with a backslash, so we need to remove it
        string(REPLACE "\\" "" WINDOWS_SDK_VERSION ${WINDOWS_SDK_VERSION})
        set(WINDOWS_SDK_VERSION ${WINDOWS_SDK_VERSION} CACHE INTERNAL "Windows SDK version")
    else()
        set(WINDOWS_SDK_VERSION "0.0" CACHE INTERNAL "Windows SDK version")
        message(WARNING "Unable to determine Windows SDK version: neither the CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION CMake variable nor the WindowsSDKVersion environment variable is set")
    endif()

    if ("${WINDOWS_SDK_VERSION}" VERSION_GREATER "0.0")
        message("")
        message("Windows SDK version: ${WINDOWS_SDK_VERSION}")
    endif()

    unset(WINDOWS_SDK_BIN_DIR CACHE)
    unset(D3D_COMPILER_PATH CACHE)
    unset(DXC_COMPILER_PATH CACHE)
    unset(DXIL_SIGNER_PATH CACHE)

    if ("${WINDOWS_SDK_VERSION}" VERSION_GREATER_EQUAL "10.0")
        # Get Windows SDK root directory from registry
        get_filename_component(
        WINDOWS_KITS_ROOT
            "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
            ABSOLUTE
        )
        if (NOT WINDOWS_KITS_ROOT)
            message(WARNING "Unable to read Windows SDK root directory from registry")
        elseif(NOT EXISTS "${WINDOWS_KITS_ROOT}")
            message(WARNING "Windows SDK root directory does not exist: ${WINDOWS_KITS_ROOT}")
            unset(WINDOWS_KITS_ROOT)
        endif()

        if(WINDOWS_KITS_ROOT)
            # NOTE: CMAKE_GENERATOR_PLATFORM is not set for single-configuration generators (e.g. Makefile, Ninja)
            if (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
                if (("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64") OR ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "AARCH64"))
                    set(ARCH_SUFFIX "arm64")
                else()
                    set(ARCH_SUFFIX "x64")
                endif()
            else()
                if (("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm") OR ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "ARM"))
                    set(ARCH_SUFFIX "arm")
                else()
                    set(ARCH_SUFFIX "x86")
                endif()
            endif()

            set(WINDOWS_SDK_BIN_DIR "${WINDOWS_KITS_ROOT}/bin/${WINDOWS_SDK_VERSION}/${ARCH_SUFFIX}")
            if (EXISTS "${WINDOWS_SDK_BIN_DIR}")
                set(WINDOWS_SDK_BIN_DIR "${WINDOWS_SDK_BIN_DIR}" CACHE INTERNAL "Windows SDK bin directory")
                message(STATUS "Windows SDK bin directory: " ${WINDOWS_SDK_BIN_DIR})

                set(D3D_COMPILER_PATH "${WINDOWS_SDK_BIN_DIR}/D3Dcompiler_47.dll")

                # DXC is only present in Windows SDK starting with version 10.0.17763.0
                if("${WINDOWS_SDK_VERSION}" VERSION_GREATER_EQUAL "10.0.17763.0")
                    set(DXC_COMPILER_PATH "${WINDOWS_SDK_BIN_DIR}/dxcompiler.dll")
                    set(DXIL_SIGNER_PATH  "${WINDOWS_SDK_BIN_DIR}/dxil.dll")
                endif()
            else()
                message(WARNING "Windows SDK bin directory does not exist: ${WINDOWS_SDK_BIN_DIR}")
                unset(WINDOWS_SDK_BIN_DIR)
            endif()
        endif()
    elseif("${WINDOWS_SDK_VERSION}" VERSION_EQUAL "8.1")
        # D3Dcompiler_47.dll from Win8.1 SDK is ancient (from 2013) and fails to
        # compile a number of test shaders. Use the compiler from Visual Studio
        # executable path instead
        if(CMAKE_AR AND IS_ABSOLUTE "${CMAKE_AR}")
            get_filename_component(CMAKE_AR_DIR "${CMAKE_AR}" DIRECTORY)
            set(D3D_COMPILER_PATH "${CMAKE_AR_DIR}/D3Dcompiler_47.dll")
        endif()
    endif()

    if(D3D_COMPILER_PATH)
        if(EXISTS "${D3D_COMPILER_PATH}")
            message(STATUS "Found D3Dcompiler_47.dll: ${D3D_COMPILER_PATH}")
            set(D3D_COMPILER_PATH "${D3D_COMPILER_PATH}" CACHE INTERNAL "D3Dcompiler_47.dll path")
        else()
            message(WARNING "Cannot find D3Dcompiler_47.dll. File does not exist: ${D3D_COMPILER_PATH}")
        endif()
    endif()

    if (DXC_COMPILER_PATH)
        if (EXISTS "${DXC_COMPILER_PATH}")
            message(STATUS "Found dxcompiler.dll: ${DXC_COMPILER_PATH}")
            set(DXC_COMPILER_PATH "${DXC_COMPILER_PATH}" CACHE INTERNAL "dxcompiler.dll path")
        else()
            message(WARNING "Cannot find dxcompiler.dll. File does not exist: ${DXC_COMPILER_PATH}")
        endif()
    endif()

    if (DXIL_SIGNER_PATH)
        if (EXISTS "${DXIL_SIGNER_PATH}")
            message(STATUS "Found dxil.dll: ${DXIL_SIGNER_PATH}")
            set(DXIL_SIGNER_PATH "${DXIL_SIGNER_PATH}" CACHE INTERNAL "dxil.dll path")
        else()
            message(WARNING "Cannot find dxil.dll. File does not exist: ${DXIL_SIGNER_PATH}")
        endif()
    endif()

    message("")

endfunction() # find_windows_sdk
