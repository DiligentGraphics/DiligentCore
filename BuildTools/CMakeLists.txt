cmake_minimum_required (VERSION 3.10)

add_subdirectory(File2Include)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(CLANG_FORMAT_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/FormatValidation/clang-format_10.0.0.exe" CACHE INTERNAL "clang-format executable path")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(CLANG_FORMAT_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/FormatValidation/clang-format_linux_10.0.0" CACHE INTERNAL "clang-format executable path")
    execute_process(
        COMMAND "${CLANG_FORMAT_EXECUTABLE}" --version
        RESULT_VARIABLE CLANG_FORMAT_EXIT_CODE
    )
    if (NOT CLANG_FORMAT_EXIT_CODE EQUAL 0)
        message(WARNING "${CLANG_FORMAT_EXECUTABLE} failed to run. Format validation will be disabled.")
        unset(CLANG_FORMAT_EXECUTABLE CACHE)
    endif()    
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(CLANG_FORMAT_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/FormatValidation/clang-format_mac_10.0.0" CACHE INTERNAL "clang-format executable path")
endif()
