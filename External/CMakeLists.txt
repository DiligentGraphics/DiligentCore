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
    set_target_properties(glslang OGLCompiler OSDependent SPIRV SPVRemapper HLSL PROPERTIES
        FOLDER Core/External/glslang
    )

    add_subdirectory(SPIRV-Cross)
    set_target_properties(SPIRVCross PROPERTIES
        FOLDER Core/External
    )

endif()
