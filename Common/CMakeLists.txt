cmake_minimum_required (VERSION 3.6)

project(Common CXX)

set(INCLUDE 
	include/pch.h
)

set(INTERFACE 
	interface/AdvancedMath.h
	interface/BasicMath.h
	interface/AdaptiveFixedBlockAllocator.h
	interface/BasicFileStream.h
	interface/DataBlobImpl.h
	interface/DefaultRawMemoryAllocator.h
	interface/FileWrapper.h
	interface/FixedBlockMemoryAllocator.h
	interface/HashUtils.h
	interface/LockHelper.h 
	interface/ObjectBase.h
	interface/RefCntAutoPtr.h
	interface/RefCountedObjectImpl.h
	interface/STDAllocator.h
	interface/StringDataBlobImpl.h
	interface/StringTools.h
	interface/Timer.h
	interface/UniqueIdentifier.h
	interface/ValidatedCast.h
)

set(SOURCE 
	src/BasicFileStream.cpp
	src/DataBlobImpl.cpp
	src/DefaultRawMemoryAllocator.cpp
	src/FixedBlockMemoryAllocator.cpp
	src/Timer.cpp
)

add_library(Common STATIC ${SOURCE} ${INCLUDE} ${INTERFACE})

target_include_directories(Common 
PRIVATE
	include
PUBLIC 
	interface
)

target_link_libraries(Common 
PUBLIC
	BuildSettings
	TargetPlatform 
)
set_common_target_properties(Common)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(Common PROPERTIES
	FOLDER Core
)
