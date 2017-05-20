/*     Copyright 2015-2017 Egor Yusov
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

#pragma once

#include "BufferGL.h"
#include "BufferBase.h"
#include "GLObjectWrapper.h"
#include "AsyncWritableResource.h"
#include "BaseInterfacesGL.h"
#include "BufferViewGLImpl.h"


namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Implementation of the Diligent::IBufferGL interface
class BufferGLImpl : public BufferBase<IBufferGL, BufferViewGLImpl, FixedBlockMemoryAllocator, FixedBlockMemoryAllocator>, public AsyncWritableResource
{
public:
    typedef BufferBase<IBufferGL, BufferViewGLImpl, FixedBlockMemoryAllocator, FixedBlockMemoryAllocator> TBufferBase;

    BufferGLImpl(FixedBlockMemoryAllocator &BufferObjMemAllocator, 
                 FixedBlockMemoryAllocator &BuffViewObjMemAllocator, 
                 class RenderDeviceGLImpl *pDeviceGL, 
                 const BufferDesc& BuffDesc, 
                 const BufferData &BuffData = BufferData(), 
                 bool IsDeviceInternal = false);
    ~BufferGLImpl();
    
    /// Queries the specific interface, see IObject::QueryInterface() for details
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;

    virtual void UpdateData(IDeviceContext *pContext, Uint32 Offset, Uint32 Size, const PVoid pData)override;
    virtual void CopyData( IDeviceContext *pContext, IBuffer *pSrcBuffer, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size )override;
    virtual void Map( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData )override;
    virtual void Unmap( IDeviceContext *pContext, MAP_TYPE MapType )override;

    void BufferMemoryBarrier( Uint32 RequiredBarriers, class GLContextState &GLContextState );

    const GLObjectWrappers::GLBufferObj& GetGLBufferHandle(){ return m_GlBuffer; }

private:
    virtual void CreateViewInternal( const struct BufferViewDesc &ViewDesc, class IBufferView **ppView, bool bIsDefaultView )override;

    friend class DeviceContextGLImpl;
    friend class VAOCache;

    GLObjectWrappers::GLBufferObj m_GlBuffer;
    Uint32 m_uiMapTarget;
    GLenum m_GLUsageHint;
    Bool m_bUseMapWriteDiscardBugWA;
};

}
