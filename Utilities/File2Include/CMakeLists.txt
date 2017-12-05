cmake_minimum_required (VERSION 3.3)

project(File2Include)

set(SOURCE 
	File2String.cpp
)

add_executable(File2String ${SOURCE})

source_group("source" FILES ${SOURCE})

set_target_properties(File2String PROPERTIES
	FOLDER Utilities
)
