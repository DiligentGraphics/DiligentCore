cmake_minimum_required (VERSION 3.6)

project(Primitives CXX)

set(SOURCE
    src/DebugOutput.cpp
    src/test.cpp
)

set(INTERFACE
    interface/BasicTypes.h
    interface/DataBlob.h
    interface/DebugOutput.h
    interface/FlagEnum.h
    interface/Errors.h
    interface/FileStream.h
    interface/FormatString.h
    interface/InterfaceID.h
    interface/MemoryAllocator.h
    interface/Object.h
    interface/ReferenceCounters.h
)

# This should be an interface library. However, CMake does not show
# interface libraries in an IDE, so we have to create a static library instead
add_library(Primitives STATIC ${INTERFACE} ${SOURCE})
set_common_target_properties(Primitives)

target_link_libraries(Primitives 
PUBLIC
    BuildSettings
)

target_include_directories(Primitives
PUBLIC
    interface
)

source_group("interface" FILES ${INTERFACE})
source_group("src" FILES ${SOURCE})

set_target_properties(Primitives PROPERTIES
    FOLDER Core
)

if(INSTALL_DILIGENT_CORE)
    install_core_lib(Primitives)
endif()