cmake_minimum_required (VERSION 3.6)

if(PLATFORM_WIN32 OR PLATFORM_LINUX OR PLATFORM_MACOS)
	project(File2Include)

	set(SOURCE 
		File2String.cpp
	)

	add_executable(File2String ${SOURCE})

	if(PLATFORM_WIN32)
		if(MSVC)
			set(MSVC_DBG_COMPILE_OPTIONS /MTd)
			set(MSVC_REL_COMPILE_OPTIONS /MT)
			target_compile_options(File2String PRIVATE /wd4996)
			target_compile_options(File2String PRIVATE "$<$<CONFIG:DEBUG>:${MSVC_DBG_COMPILE_OPTIONS}>")
			target_compile_options(File2String PRIVATE "$<$<CONFIG:RELEASE>:${MSVC_REL_COMPILE_OPTIONS}>")
			target_compile_options(File2String PRIVATE "$<$<CONFIG:MINSIZEREL>:${MSVC_REL_COMPILE_OPTIONS}>")
			target_compile_options(File2String PRIVATE "$<$<CONFIG:RELWITHDEBINFO>:${MSVC_REL_COMPILE_OPTIONS}>")
		endif()
		set_target_properties(File2String PROPERTIES 
			RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/Win32/x${ARCH}"
		)
	elseif(PLATFORM_LINUX)
		set_target_properties(File2String PROPERTIES 
			RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/Linux"
		)
    elseif(PLATFORM_MACOS)
        set_target_properties(File2String PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/MacOS"
        )
        target_compile_features(File2String PRIVATE cxx_std_11)
	endif()
	source_group("source" FILES ${SOURCE})

	set_target_properties(File2String PROPERTIES
		FOLDER Utilities
	)
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
	# Use prebuilt 32-bit version
	set(FILE2STRING_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/Win32/x32/File2String.exe" CACHE INTERNAL "File2String utility")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
	# Use prebuilt version
	set(FILE2STRING_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/Linux/File2String" CACHE INTERNAL "File2String utility")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    # Use prebuilt version
    set(FILE2STRING_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/MacOS/File2String" CACHE INTERNAL "File2String utility")
else()
	set(FILE2STRING_PATH "" CACHE INTERNAL "File2String utility unavailable on this host system")
endif()
