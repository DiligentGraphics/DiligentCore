cmake_minimum_required (VERSION 3.6)

if(PLATFORM_WIN32 OR PLATFORM_LINUX OR PLATFORM_MACOS)
	add_subdirectory(glew)
endif()

if(PLATFORM_ANDROID)
	add_subdirectory(Android)
endif()

if(PLATFORM_WIN32)
    option(ENABLE_GLSLANG_BINARIES OFF)
    option(SKIP_GLSLANG_INSTALL ON)
    add_subdirectory(glslang)
    set_target_properties(glslang OGLCompiler OSDependent SPIRV SPVRemapper HLSL PROPERTIES
        FOLDER Core/External/glslang
    )

    # glslang "kindly" sets global CMAKE_DEBUG_POSTFIX to "d", which we have to unset now
    unset(CMAKE_DEBUG_POSTFIX CACHE)

    add_subdirectory(SPIRV-Cross)
    set_target_properties(SPIRVCross PROPERTIES
        FOLDER Core/External
    )

endif()
