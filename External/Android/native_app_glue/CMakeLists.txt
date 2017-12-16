cmake_minimum_required (VERSION 3.6)

project(NativeAppGlue CXX)

# build native_app_glue as a static lib
#get_filename_component(ANDROID_NATIVE_APP_GLUE_C 
#	${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
#	ABSOLUTE
# )

#configure_file(
#	${ANDROID_NATIVE_APP_GLUE_C}
#	${CMAKE_CURRENT_SOURCE_DIR}/android_native_app_glue.c
#	COPYONLY
#)

# No matter what I do, the build fails when I try to reference android_native_app_glue.c
# directly in the ndk the same way as in this example:
# https://github.com/googlesamples/android-ndk/blob/master/teapots/classic-teapot/src/main/cpp/CMakeLists.txt
# Copying file locally also fails
add_library(NativeAppGlue STATIC
	# ${ANDROID_NATIVE_APP_GLUE_C}
	# ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
	android_native_app_glue.c
)

# The linker will strip this as "unused" since this is a static library, but we
# need to keep it around since it's the interface for JNI.
# Refer to: https://github.com/android-ndk/ndk/issues/381.
#set_target_properties(NativeAppGlue PROPERTIES
#	STATIC_LIBRARY_FLAGS "-u ANativeActivity_onCreate"
#)

target_include_directories(NativeAppGlue 
INTERFACE 
	${ANDROID_NDK}/sources/android/native_app_glue
)

set_target_properties(NativeAppGlue PROPERTIES
	FOLDER Core/External
)
