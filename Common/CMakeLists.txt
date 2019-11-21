cmake_minimum_required (VERSION 3.6)

project(Diligent-Common CXX)

set(INCLUDE 
    include/pch.h
)

set(INTERFACE 
    interface/AdvancedMath.h
    interface/Align.h
    interface/BasicMath.h
    interface/BasicFileStream.h
    interface/DataBlobImpl.h
    interface/DefaultRawMemoryAllocator.h
    interface/FileWrapper.h
    interface/FixedBlockMemoryAllocator.h
    interface/HashUtils.h
    interface/LockHelper.h 
    interface/MemoryFileStream.h 
    interface/ObjectBase.h
    interface/RefCntAutoPtr.h
    interface/RefCountedObjectImpl.h
    interface/STDAllocator.h
    interface/StringDataBlobImpl.h
    interface/StringTools.h
    interface/StringPool.h
    interface/ThreadSignal.h
    interface/Timer.h
    interface/UniqueIdentifier.h
    interface/ValidatedCast.h
)

set(SOURCE 
    src/BasicFileStream.cpp
    src/DataBlobImpl.cpp
    src/DefaultRawMemoryAllocator.cpp
    src/FixedBlockMemoryAllocator.cpp
    src/LockHelper.cpp
    src/MemoryFileStream.cpp
    src/Timer.cpp
)

add_library(Diligent-Common STATIC ${SOURCE} ${INCLUDE} ${INTERFACE})

target_include_directories(Diligent-Common 
PRIVATE
    include
PUBLIC 
    interface
)

target_link_libraries(Diligent-Common 
PUBLIC
    Diligent-BuildSettings
    Diligent-TargetPlatform 
)
set_common_target_properties(Diligent-Common)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(Diligent-Common PROPERTIES
    FOLDER DiligentCore
)

if(DILIGENT_INSTALL_CORE)
    install_core_lib(Diligent-Common)
endif()
