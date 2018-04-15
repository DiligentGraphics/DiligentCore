cmake_minimum_required (VERSION 3.6)

if(PLATFORM_WIN32 OR PLATFORM_LINUX OR PLATFORM_MACOS)
	add_subdirectory(glew)
endif()

if(PLATFORM_ANDROID)
	add_subdirectory(Android)
endif()

if(PLATFORM_WIN32)
    set(ENABLE_GLSLANG_BINARIES FALSE)
    set(SKIP_GLSLANG_INSTALL TRUE)
    add_subdirectory(glslang)
    set_target_properties(glslang OGLCompiler OSDependent SPIRV SPVRemapper PROPERTIES
        FOLDER Core/External/glslang/glslang
    )
    set_target_properties(HLSL PROPERTIES
        FOLDER Core/External/glslang/hlsl
    )

endif()
