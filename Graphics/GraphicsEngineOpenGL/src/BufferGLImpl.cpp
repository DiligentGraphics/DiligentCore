/*     Copyright 2015 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include "pch.h"

#include "BufferGLImpl.h"
#include "RenderDeviceGLImpl.h"
#include "GLTypeConversions.h"
#include "BufferViewGLImpl.h"
#include "DeviceContextGLImpl.h"

namespace Diligent
{

BufferGLImpl::BufferGLImpl(class RenderDeviceGLImpl *pDeviceGL, const BufferDesc& BuffDesc, const BufferData &BuffData /*= BufferData()*/, bool IsDeviceInternal /*= false*/) : 
    TBufferBase( pDeviceGL, BuffDesc, IsDeviceInternal ),
    m_GlBuffer(true), // Create buffer immediately
    m_uiMapTarget(0),
    m_GLUsageHint(0),
    m_bUseMapWriteDiscardBugWA(False)
{
    // On Intel GPUs, mapping buffer with GL_MAP_UNSYNCHRONIZED_BIT does not
    // work as expected. To workaround this issue, use glBufferData() to
    // orphan previous buffer storage https://www.opengl.org/wiki/Buffer_Object_Streaming
    if( pDeviceGL->GetGPUInfo().Vendor == GPU_VENDOR::INTEL )
        m_bUseMapWriteDiscardBugWA = True;

    if( BuffDesc.Usage == USAGE_STATIC && BuffData.pData == nullptr )
        LOG_ERROR_AND_THROW("Static buffer must be initialized with data at creation time");

    // TODO: find out if it affects performance if the buffer is originally bound to one target
    // and then bound to another (such as first to GL_ARRAY_BUFFER and then to GL_UNIFORM_BUFFER)
    glBindBuffer(GL_ARRAY_BUFFER, m_GlBuffer);
    VERIFY(BuffData.pData == nullptr || BuffData.DataSize >= BuffDesc.uiSizeInBytes, "Data pointer is null or data size is not consistent with buffer size" );
    GLsizeiptr DataSize = BuffDesc.uiSizeInBytes;
 	const GLvoid *pData = nullptr;
    if( BuffData.pData && BuffData.DataSize >= BuffDesc.uiSizeInBytes )
    {
        pData = BuffData.pData;
        DataSize = BuffData.DataSize;
    }
    // Create and initialize a buffer object's data store

    // Target must be one of GL_ARRAY_BUFFER, GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 
    // GL_ELEMENT_ARRAY_BUFFER, GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER, GL_TEXTURE_BUFFER, 
    // GL_TRANSFORM_FEEDBACK_BUFFER, or GL_UNIFORM_BUFFER.

    // Usage must be one of GL_STREAM_DRAW, GL_STREAM_READ, GL_STREAM_COPY, GL_STATIC_DRAW, 
    // GL_STATIC_READ, GL_STATIC_COPY, GL_DYNAMIC_DRAW, GL_DYNAMIC_READ, or GL_DYNAMIC_COPY.

    //The frequency of access may be one of these:
    //
    //STREAM
    //  The data store contents will be modified once and used at most a few times.
    //
    //STATIC
    //  The data store contents will be modified once and used many times.
    //
    //DYNAMIC
    //  The data store contents will be modified repeatedly and used many times.
    //
    
    //The nature of access may be one of these:
    //
    //DRAW
    //  The data store contents are modified by the application, and used as the source for GL 
    //  drawing and image specification commands.
    //
    //READ
    //  The data store contents are modified by reading data from the GL, and used to return that 
    //  data when queried by the application.
    //
    //COPY
    //  The data store contents are modified by reading data from the GL, and used as the source 
    //  for GL drawing and image specification commands.

    // See also http://www.informit.com/articles/article.aspx?p=2033340&seqNum=2

    m_GLUsageHint = UsageToGLUsage(BuffDesc.Usage);
    // All buffer bind targets (GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER etc.) relate to the same 
    // kind of objects. As a result they are all equivalent from a transfer point of view.
    glBufferData(GL_ARRAY_BUFFER, DataSize, pData, m_GLUsageHint);
    CHECK_GL_ERROR_AND_THROW("glBufferData() failed");
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

BufferGLImpl::~BufferGLImpl()
{
    static_cast<RenderDeviceGLImpl*>( static_cast<IRenderDevice*>( GetDevice() ) )->m_VAOCache.OnDestroyBuffer(this);
}

IMPLEMENT_QUERY_INTERFACE( BufferGLImpl, IID_BufferGL, TBufferBase )

void BufferGLImpl :: UpdateData(IDeviceContext *pContext, Uint32 Offset, Uint32 Size, const PVoid pData)
{
    TBufferBase::UpdateData( pContext, Offset, Size, pData );

    CHECK_DYNAMIC_TYPE( DeviceContextGLImpl, pContext );
    auto *pDeviceContextGL = static_cast<DeviceContextGLImpl*>(pContext);

    BufferMemoryBarrier(
        GL_BUFFER_UPDATE_BARRIER_BIT,// Reads or writes to buffer objects via any OpenGL API functions that allow 
                                     // modifying their contents will reflect data written by shaders prior to the barrier. 
                                     // Additionally, writes via these commands issued after the barrier will wait on 
                                     // the completion of any shader writes to the same memory initiated prior to the barrier.
        pDeviceContextGL->GetContextState());
    
    glBindBuffer(GL_ARRAY_BUFFER, m_GlBuffer);
    // All buffer bind targets (GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER etc.) relate to the same 
    // kind of objects. As a result they are all equivalent from a transfer point of view.
    glBufferSubData(GL_ARRAY_BUFFER, Offset, Size, pData);
    CHECK_GL_ERROR("glBufferSubData() failed");
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void BufferGLImpl :: CopyData(IDeviceContext *pContext, IBuffer *pSrcBuffer, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size)
{
    TBufferBase::CopyData( pContext, pSrcBuffer, SrcOffset, DstOffset, Size );

    CHECK_DYNAMIC_TYPE( DeviceContextGLImpl, pContext );
    auto *pDeviceContextGL = static_cast<DeviceContextGLImpl*>(pContext);

    auto *pSrcBufferGL = static_cast<BufferGLImpl*>( pSrcBuffer );

    BufferMemoryBarrier(
        GL_BUFFER_UPDATE_BARRIER_BIT,// Reads or writes to buffer objects via any OpenGL API functions that allow 
                                     // modifying their contents will reflect data written by shaders prior to the barrier. 
                                     // Additionally, writes via these commands issued after the barrier will wait on 
                                     // the completion of any shader writes to the same memory initiated prior to the barrier.
        pDeviceContextGL->GetContextState());
    pSrcBufferGL->BufferMemoryBarrier( GL_BUFFER_UPDATE_BARRIER_BIT, pDeviceContextGL->GetContextState() );

    // Whilst glCopyBufferSubData() can be used to copy data between buffers bound to any two targets, 
    // the targets GL_COPY_READ_BUFFER and GL_COPY_WRITE_BUFFER are provided specifically for this purpose. 
    // Neither target is used for anything else by OpenGL, and so you can safely bind buffers to them for 
    // the purposes of copying or staging data without disturbing OpenGL state or needing to keep track of 
    // what was bound to the target before your copy.
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_GlBuffer);
    glBindBuffer(GL_COPY_READ_BUFFER, pSrcBufferGL->m_GlBuffer);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, SrcOffset, DstOffset, Size);
    CHECK_GL_ERROR("glCopyBufferSubData() failed");
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void BufferGLImpl :: Map(IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData)
{
    TBufferBase::Map( pContext, MapType, MapFlags, pMappedData );
    VERIFY( m_uiMapTarget == 0, "Buffer is already mapped");

    CHECK_DYNAMIC_TYPE( DeviceContextGLImpl, pContext );
    auto *pDeviceContextGL = static_cast<DeviceContextGLImpl*>(pContext);

    BufferMemoryBarrier( 
        GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT,// Access by the client to persistent mapped regions of buffer 
                                            // objects will reflect data written by shaders prior to the barrier. 
                                            // Note that this may cause additional synchronization operations.
        pDeviceContextGL->GetContextState()); 
    m_uiMapTarget = ( MapType == MAP_READ ) ? GL_COPY_READ_BUFFER : GL_COPY_WRITE_BUFFER;
    glBindBuffer(m_uiMapTarget, m_GlBuffer);

    // !!!WARNING!!! GL_MAP_UNSYNCHRONIZED_BIT is not the same thing as MAP_FLAG_DO_NOT_WAIT.
    // If GL_MAP_UNSYNCHRONIZED_BIT flag is set, OpenGL will not attempt to synchronize operations 
    // on the buffer. This does not mean that map will fail if the buffer still in use. It is thus
    // what WRITE_NO_OVERWRITE does

    GLbitfield Access = 0;
    switch(MapType)
    {
        case MAP_READ: 
            Access |= GL_MAP_READ_BIT;
        break;

        case MAP_WRITE:
            Access |= GL_MAP_WRITE_BIT;
        break;

        case MAP_READ_WRITE:
            Access |= GL_MAP_WRITE_BIT | GL_MAP_READ_BIT;
        break;

        case MAP_WRITE_DISCARD:
           
            if( m_bUseMapWriteDiscardBugWA )
            {
                // On Intel GPU, mapping buffer with GL_MAP_UNSYNCHRONIZED_BIT does not
                // work as expected. To workaround this issue, use glBufferData() to
                // orphan previous buffer storage https://www.opengl.org/wiki/Buffer_Object_Streaming

                // It is important to specify the exact same buffer size and usage to allow the 
                // implementation to simply reallocate storage for that buffer object under-the-hood.
                // Since NULL is passed, if there wasn't a need for synchronization to begin with, 
                // this can be reduced to a no-op.
                glBufferData(m_uiMapTarget, m_Desc.uiSizeInBytes, nullptr, m_GLUsageHint);
                CHECK_GL_ERROR("glBufferData() failed");
                Access |= GL_MAP_WRITE_BIT;
            }
            else
            {
                // Use GL_MAP_INVALIDATE_BUFFER_BIT flag to discard previous buffer contents

                // If GL_MAP_INVALIDATE_BUFFER_BIT is specified, the entire contents of the buffer may 
                // be discarded and considered invalid, regardless of the specified range. Any data 
                // lying outside the mapped range of the buffer object becomes undefined,as does any 
                // data within the range but not subsequently written by the application.This flag may 
                // not be used with GL_MAP_READ_BIT.

                Access |= GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_WRITE_BIT;
            }
        break;

        case MAP_WRITE_NO_OVERWRITE:
            // If GL_MAP_UNSYNCHRONIZED_BIT flag is set, OpenGL will not attempt to synchronize 
            // operations on the buffer. 
            Access |= GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
        break;

        default: UNEXPECTED( "Unknown map type" );
    }

    pMappedData = glMapBufferRange(m_uiMapTarget, 0, m_Desc.uiSizeInBytes,  Access);
    CHECK_GL_ERROR("glMapBufferRange() failed");
    VERIFY( pMappedData, "Map failed" );
    glBindBuffer(m_uiMapTarget, 0);
}

void BufferGLImpl::Unmap( IDeviceContext *pContext )
{
    TBufferBase::Unmap(pContext);

    glBindBuffer(m_uiMapTarget, m_GlBuffer);
    auto Result = glUnmapBuffer(m_uiMapTarget);
    // glUnmapBuffer() returns TRUE unless data values in the buffer’s data store have
    // become corrupted during the period that the buffer was mapped. Such corruption
    // can be the result of a screen resolution change or other window system - dependent
    // event that causes system heaps such as those for high - performance graphics memory
    // to be discarded. GL implementations must guarantee that such corruption can
    // occur only during the periods that a buffer’s data store is mapped. If such corruption
    // has occurred, glUnmapBuffer() returns FALSE, and the contents of the buffer’s
    // data store become undefined.
    VERIFY( Result != GL_FALSE, "Failed to unmap buffer. The data may have been corrupted" );
    glBindBuffer(m_uiMapTarget, 0);
    m_uiMapTarget = 0;
}

void BufferGLImpl::BufferMemoryBarrier( Uint32 RequiredBarriers, GLContextState &GLContextState )
{
    const Uint32 BufferBarriers = 
        GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | 
        GL_ELEMENT_ARRAY_BARRIER_BIT | 
        GL_UNIFORM_BARRIER_BIT | 
        GL_COMMAND_BARRIER_BIT | 
        GL_BUFFER_UPDATE_BARRIER_BIT |
        GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT | 
        GL_SHADER_STORAGE_BARRIER_BIT | 
        GL_TEXTURE_FETCH_BARRIER_BIT;
    VERIFY( (RequiredBarriers & BufferBarriers) !=0,   "At least one buffer memory barrier flag should be set" );
    VERIFY( (RequiredBarriers & ~BufferBarriers) == 0, "Inappropriate buffer memory barrier flag" );

    GLContextState.EnsureMemoryBarrier( RequiredBarriers, this );
}

void BufferGLImpl::CreateViewInternal( const BufferViewDesc &OrigViewDesc, class IBufferView **ppView, bool bIsDefaultView )
{
    VERIFY( ppView != nullptr, "Buffer view pointer address is null" );
    if( !ppView )return;
    VERIFY( *ppView == nullptr, "Overwriting reference to existing object may cause memory leaks" );
    
    *ppView = nullptr;

    try
    {
        auto ViewDesc = OrigViewDesc;
        CorrectBufferViewDesc( ViewDesc );

        auto pContext = ValidatedCast<RenderDeviceGLImpl>( GetDevice() )->GetImmediateContext();
        VERIFY( pContext, "Immediate context has been released" );
        *ppView = new BufferViewGLImpl( GetDevice(), pContext, ViewDesc, this, bIsDefaultView );
        
        if( !bIsDefaultView )
            (*ppView)->AddRef();
    }
    catch( const std::runtime_error & )
    {
        const auto *ViewTypeName = GetBufferViewTypeLiteralName(OrigViewDesc.ViewType);
        LOG_ERROR("Failed to create view \"", OrigViewDesc.Name ? OrigViewDesc.Name : "", "\" (", ViewTypeName, ") for buffer \"", m_Desc.Name ? m_Desc.Name : "", "\"" )
    }
}

}
