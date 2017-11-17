
APP_PLATFORM := android-19
APP_ABI := armeabi-v7a
NDK_TOOLCHAIN_VERSION := 4.9
APP_STL := gnustl_static
# NDK_TOOLCHAIN_VERSION := clang
# APP_STL := c++_static

ifeq ($(APP_OPTIM),release)
	APP_CFLAGS := -O2 -DNDEBUG -g $(APP_CFLAGS)
else
	APP_CFLAGS := -O0 -g -D_DEBUG $(APP_CFLAGS)
	APP_CPPFLAGS := -frtti $(APP_CPPFLAGS)
endif