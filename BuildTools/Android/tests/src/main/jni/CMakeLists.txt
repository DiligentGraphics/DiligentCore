cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(DE.Android)

add_subdirectory("../../../../../.." Core)

add_library(native_app_glue STATIC "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
target_include_directories(native_app_glue PUBLIC "${ANDROID_NDK}/sources/android/native_app_glue")
target_link_libraries(native_app_glue PUBLIC android log)
	
add_library(DETestLauncher STATIC main.cpp)

target_link_libraries(DETestLauncher
PUBLIC
	native_app_glue
	Diligent-Common
	Diligent-GraphicsEngineOpenGL-static
	Diligent-GraphicsEngineVk-static)
	
set_target_properties(DETestLauncher PROPERTIES LINK_FLAGS "-u ANativeActivity_onCreate")
