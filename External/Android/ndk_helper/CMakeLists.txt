cmake_minimum_required (VERSION 3.6)

project(NdkHelper CXX)

set(SOURCE 
	src/gestureDetector.cpp
	src/JNIHelper.cpp
	src/perfMonitor.cpp
	src/sensorManager.cpp
	src/tapCamera.cpp
	src/vecmath.cpp
)

set(INCLUDE 
	include/gestureDetector.h
	include/JNIHelper.h
	include/perfMonitor.h
	include/sensorManager.h
	include/tapCamera.h
)

add_library(NdkHelper STATIC ${SOURCE} ${INCLUDE})
set_common_target_properties(NdkHelper)

target_include_directories(NdkHelper
PUBLIC
	include
)

target_link_libraries(NdkHelper 
PRIVATE 
	BuildSettings
PUBLIC
	NativeAppGlue
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})

set_target_properties(NdkHelper PROPERTIES
	FOLDER Core/External
)
