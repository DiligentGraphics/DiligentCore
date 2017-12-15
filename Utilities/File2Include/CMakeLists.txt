cmake_minimum_required (VERSION 3.10)

project(File2Include)

set(SOURCE 
	File2String.cpp
)

add_executable(File2String ${SOURCE})

if(PLATFORM_WIN32)
	if(MSVC)
		set(MSVC_DBG_COMPILE_OPTIONS /MTd)
		set(MSVC_REL_COMPILE_OPTIONS /MT)
		target_compile_options(File2String PRIVATE "$<$<CONFIG:DEBUG>:${MSVC_DBG_COMPILE_OPTIONS}>")
		target_compile_options(File2String PRIVATE "$<$<CONFIG:RELEASE>:${MSVC_REL_COMPILE_OPTIONS}>")
		target_compile_options(File2String PRIVATE "$<$<CONFIG:MINSIZEREL>:${MSVC_REL_COMPILE_OPTIONS}>")
		target_compile_options(File2String PRIVATE "$<$<CONFIG:RELWITHDEBINFO>:${MSVC_REL_COMPILE_OPTIONS}>")
	endif()
endif()
source_group("source" FILES ${SOURCE})

set_target_properties(File2String PROPERTIES
	FOLDER Utilities
)
