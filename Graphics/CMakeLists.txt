cmake_minimum_required (VERSION 3.8)

if(PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS)
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
	   set(ARCH 64)
	else("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
	   set(ARCH 32)
	endif("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
endif()

set(DLL_REL_SUFFIX _${ARCH}r)
set(DLL_DBG_SUFFIX _${ARCH}d)

add_subdirectory(GraphicsEngine)

if(D3D11_SUPPORTED OR D3D12_SUPPORTED)
	add_subdirectory(GraphicsEngineD3DBase)
endif()

if(D3D11_SUPPORTED)
	add_subdirectory(GraphicsEngineD3D11)
endif()

if(D3D12_SUPPORTED)
	add_subdirectory(GraphicsEngineD3D12)
endif()

add_subdirectory(GraphicsTools)

if(GL_SUPPORTED OR GLES_SUPPORTED)
	add_subdirectory(HLSL2GLSLConverterLib)
	add_subdirectory(GraphicsEngineOpenGL)
endif()
