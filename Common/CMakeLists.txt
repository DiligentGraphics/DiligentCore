cmake_minimum_required (VERSION 3.3)

project(Common)

set(INCLUDE 
	include/AdaptiveFixedBlockAllocator.h
	include/BasicFileStream.h
	include/BasicTypes.h
	include/DataBlobImpl.h
	include/DebugUtilities.h
	include/DefaultRawMemoryAllocator.h
	include/Errors.h
	include/FileWrapper.h
	include/FixedBlockMemoryAllocator.h
	include/FormatMessage.h
	include/HashUtils.h
	include/InterfaceID.h
	include/LockHelper.h 
	include/ObjectBase.h
	include/pch.h
	include/RefCntAutoPtr.h
	include/RefCountedObjectImpl.h
	include/STDAllocator.h
	include/StringDataBlobImpl.h
	include/StringTools.h
	include/Timer.h
	include/UniqueIdentifier.h
	include/ValidatedCast.h
)

set(INTERFACE 
	interface/AdvancedMath.h
	interface/BasicMath.h
	interface/DataBlob.h
	interface/FileStream.h
	interface/MemoryAllocator.h
	interface/Object.h
	interface/ReferenceCounters.h
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
PUBLIC 
	include 
	interface
)

target_link_libraries(Common 
PRIVATE 
	BuildSettings 
	PlatformInterface
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(Common PROPERTIES
	FOLDER Core
)
