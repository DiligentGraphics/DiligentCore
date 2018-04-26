cmake_minimum_required (VERSION 3.10)

project(SPIRVCross CXX)

set(INCLUDE
    spirv.hpp
    spirv_cfg.hpp
    spirv_common.hpp
    spirv_cpp.hpp
    spirv_cross.hpp
    spirv_cross_util.hpp
    spirv_glsl.hpp
    spirv_hlsl.hpp
    spirv_msl.hpp
)

set(SRC
    spirv_cfg.cpp
    #spirv_cpp.cpp
    spirv_cross.cpp
    spirv_cross_util.cpp
    spirv_glsl.cpp
    spirv_hlsl.cpp
    #spirv_msl.cpp
)

add_library(SPIRVCross STATIC 
    ${SRC} ${INCLUDE}
    readme.md
)

set_source_files_properties(
    readme.md PROPERTIES HEADER_FILE_ONLY TRUE
)

target_include_directories(SPIRVCross PUBLIC .)

target_link_libraries(SPIRVCross PRIVATE BuildSettings)
set_common_target_properties(SPIRVCross)

source_group("src" FILES ${SRC})
source_group("include" FILES ${INCLUDE})
