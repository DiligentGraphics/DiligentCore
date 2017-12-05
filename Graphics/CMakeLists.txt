cmake_minimum_required (VERSION 3.3)

if(PLATFORM_WIN32 OR PLATFORM_UNVIRSAL_WINDOWS)
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
	   set(ARCH 64)
	else("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
	   set(ARCH 32)
	endif("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
endif()

set(DLL_REL_SUFFIX _${ARCH}r)
set(DLL_DBG_SUFFIX _${ARCH}d)

add_subdirectory(GraphicsEngine)

if(PLATFORM_WIN32 OR PLATFORM_UNVIRSAL_WINDOWS)
	add_subdirectory(GraphicsEngineD3DBase)
	add_subdirectory(GraphicsEngineD3D11)
	add_subdirectory(GraphicsEngineD3D12)
endif()

add_subdirectory(GraphicsTools)

if(PLATFORM_WIN32 OR PLATFORM_ANDROID)
	add_subdirectory(HLSL2GLSLConverterLib)
	add_subdirectory(GraphicsEngineOpenGL)
endif()
