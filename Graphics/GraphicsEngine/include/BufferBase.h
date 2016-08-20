/*     Copyright 2015-2016 Egor Yusov
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

/// \file
/// Implementation of the Diligent::BufferBase template class

#include "Buffer.h"
#include "DeviceObjectBase.h"
#include "GraphicsUtilities.h"
#include "STDAllocator.h"
#include <memory>

namespace Diligent
{

/// Template class implementing base functionality for a buffer object

/// \tparam BaseInterface - base interface that this class will inheret 
///                         (Diligent::IBufferD3D11, Diligent::IBufferD3D12 or Diligent::IBufferGL).
/// \tparam BufferViewImplType - type of the buffer view implementation
///                              (Diligent::BufferViewD3D11Impl, Diligent::BufferViewD3D12Impl or Diligent::BufferViewGLImpl)
/// \tparam TBuffObjAllocator - type of the allocator that is used to allocate memory for the buffer object instances
/// \tparam TBuffViewObjAllocator - type of the allocator that is used to allocate memory for the buffer view object instances
template<class BaseInterface, class BufferViewImplType, class TBuffObjAllocator, class TBuffViewObjAllocator>
class BufferBase : public DeviceObjectBase < BaseInterface, BufferDesc, TBuffObjAllocator >
{
public:
    typedef DeviceObjectBase<BaseInterface, BufferDesc, TBuffObjAllocator> TDeviceObjectBase;

    /// \param BuffObjAllocator - allocator that was used to allocate memory for this instance of the buffer object.
    /// \param BuffViewObjAllocator - allocator that is used to allocate memory for the buffer view instances.
    ///                               This parameter is only used for debug purposes.
	/// \param pDevice - pointer to the device.
	/// \param BuffDesc - buffer description.
	/// \param bIsDeviceInternal - flag indicating if the buffer is an internal device object and 
	///							   must not keep a strong reference to the device.
    BufferBase( TBuffObjAllocator &BuffObjAllocator, 
                TBuffViewObjAllocator &BuffViewObjAllocator, 
                IRenderDevice *pDevice, 
                const BufferDesc& BuffDesc, 
                bool bIsDeviceInternal = false ) :
        TDeviceObjectBase( BuffObjAllocator, pDevice, BuffDesc, nullptr, bIsDeviceInternal ),
#ifdef _DEBUG
        m_dbgBuffViewAllocator(BuffViewObjAllocator),
#endif
        m_pDefaultUAV(nullptr, STDDeleter<BufferViewImplType, TBuffViewObjAllocator>(BuffViewObjAllocator) ),
        m_pDefaultSRV(nullptr, STDDeleter<BufferViewImplType, TBuffViewObjAllocator>(BuffViewObjAllocator) )
    {
#define VERIFY_BUFFER(Expr, ...) VERIFY(Expr, "Buffer \"",  this->m_Desc.Name ? this->m_Desc.Name : "", "\": ", ##__VA_ARGS__)

#ifdef _DEBUG
        Uint32 AllowedBindFlags =
            BIND_VERTEX_BUFFER | BIND_INDEX_BUFFER | BIND_UNIFORM_BUFFER |
            BIND_SHADER_RESOURCE | BIND_STREAM_OUTPUT | BIND_UNORDERED_ACCESS |
            BIND_INDIRECT_DRAW_ARGS;
        const Char* strAllowedBindFlags =
            "BIND_VERTEX_BUFFER (1), BIND_INDEX_BUFFER (2), BIND_UNIFORM_BUFFER (4), "
            "BIND_SHADER_RESOURCE (8), BIND_STREAM_OUTPUT (16), BIND_UNORDERED_ACCESS (128), "
            "BIND_INDIRECT_DRAW_ARGS (256)";

        VERIFY_BUFFER( (BuffDesc.BindFlags & ~AllowedBindFlags) == 0, "Incorrect bind flags specified (", BuffDesc.BindFlags & ~AllowedBindFlags, "). Only the following flags are allowed:\n", strAllowedBindFlags );
#endif
        
        if( (this->m_Desc.BindFlags & BIND_UNORDERED_ACCESS) ||
            (this->m_Desc.BindFlags & BIND_SHADER_RESOURCE) )
        {
            VERIFY_BUFFER( this->m_Desc.Mode > BUFFER_MODE_UNDEFINED && this->m_Desc.Mode < BUFFER_MODE_NUM_MODES, "Buffer mode (", this->m_Desc.Mode, ") is not correct" );
            if( this->m_Desc.Mode == BUFFER_MODE_STRUCTURED )
            {
                VERIFY_BUFFER( this->m_Desc.ElementByteStride != 0, "Element stride cannot be zero for structured buffer" );
            }

            if( this->m_Desc.Mode == BUFFER_MODE_FORMATED )
            {
                VERIFY_BUFFER( this->m_Desc.Format.ValueType != VT_UNDEFINED, "Value type is not specified for a formated buffer" );
                VERIFY_BUFFER( this->m_Desc.Format.NumComponents != 0, "Num components cannot be zero in a formated buffer" );
                if( this->m_Desc.ElementByteStride == 0 )
                    this->m_Desc.ElementByteStride = static_cast<Uint32>(GetValueSize( this->m_Desc.Format.ValueType )) * this->m_Desc.Format.NumComponents;
            }
        }
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_Buffer, TDeviceObjectBase )

    /// Base implementation of IBuffer::UpdateData(); validates input parameters.
    virtual void UpdateData( IDeviceContext *pContext, Uint32 Offset, Uint32 Size, const PVoid pData )override = 0;

    /// Base implementation of IBuffer::CopyData(); validates input parameters.
    virtual void CopyData( IDeviceContext *pContext, IBuffer *pSrcBuffer, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size )override = 0;

    /// Base implementation of IBuffer::Map(); validates input parameters.
    virtual void Map( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData )override;

    /// Base implementation of IBuffer::Unmap()
    virtual void Unmap( IDeviceContext *pContext, MAP_TYPE MapType )override = 0;

    /// Implementation of IBuffer::CreateView(); calls CreateViewInternal() virtual function
    /// that creates buffer view for the specific engine implementation.
    virtual void CreateView( const struct BufferViewDesc &ViewDesc, IBufferView **ppView )override;

    /// Implementation of IBuffer::GetDefaultView().
    virtual IBufferView* GetDefaultView( BUFFER_VIEW_TYPE ViewType )override;

    /// Creates default buffer views.

    /// 
    /// - Creates default shader resource view addressing the entire buffer if Diligent::BIND_SHADER_RESOURCE flag is set
    /// - Creates default unordered access view addressing the entire buffer if Diligent::BIND_UNORDERED_ACCESS flag is set 
    ///
    /// The function calls CreateViewInternal().
    void CreateDefaultViews();

protected:

    /// Pure virtual function that creates buffer view for the specific engine implementation.
    virtual void CreateViewInternal( const struct BufferViewDesc &ViewDesc, IBufferView **ppView, bool bIsDefaultView ) = 0;

    /// Corrects buffer view description and validates view parameters.
    void CorrectBufferViewDesc( struct BufferViewDesc &ViewDesc );

#ifdef _DEBUG
    TBuffViewObjAllocator &m_dbgBuffViewAllocator;
#endif

    /// Default UAV addressing the entire buffer
    std::unique_ptr<BufferViewImplType, STDDeleter<BufferViewImplType, TBuffViewObjAllocator> > m_pDefaultUAV;

    /// Default SRV addressing the entire buffer
    std::unique_ptr<BufferViewImplType, STDDeleter<BufferViewImplType, TBuffViewObjAllocator> > m_pDefaultSRV;
};

template<class BaseInterface, class BufferViewImplType, class TBuffObjAllocator, class TBuffViewObjAllocator>
void BufferBase<BaseInterface, BufferViewImplType, TBuffObjAllocator, TBuffViewObjAllocator> :: UpdateData( IDeviceContext *pContext, Uint32 Offset, Uint32 Size, const PVoid pData )
{
    VERIFY_BUFFER( this->m_Desc.Usage == USAGE_DEFAULT, "Only default usage buffers can be updated with UpdateData()" );
    VERIFY_BUFFER( Offset < this->m_Desc.uiSizeInBytes, "Offset (", Offset, ") exceeds the buffer size (", this->m_Desc.uiSizeInBytes, ")" );
    VERIFY_BUFFER( Size + Offset <= this->m_Desc.uiSizeInBytes, "Update region [", Offset, ",", Size + Offset, ") is out of buffer bounds [0,",this->m_Desc.uiSizeInBytes,")" );
}

template<class BaseInterface, class BufferViewImplType, class TBuffObjAllocator, class TBuffViewObjAllocator>
void BufferBase<BaseInterface, BufferViewImplType, TBuffObjAllocator, TBuffViewObjAllocator> :: CopyData( IDeviceContext *pContext, IBuffer *pSrcBuffer, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size )
{
    VERIFY_BUFFER( DstOffset + Size <= this->m_Desc.uiSizeInBytes, "Destination range [", DstOffset, ",", DstOffset + Size, ") is out of buffer bounds [0,",this->m_Desc.uiSizeInBytes,")" );
    VERIFY_BUFFER( SrcOffset + Size <= pSrcBuffer->GetDesc().uiSizeInBytes, "Source range [", SrcOffset, ",", SrcOffset + Size, ") is out of buffer bounds [0,",this->m_Desc.uiSizeInBytes,")" );
}


template<class BaseInterface, class BufferViewImplType, class TBuffObjAllocator, class TBuffViewObjAllocator>
void BufferBase<BaseInterface, BufferViewImplType, TBuffObjAllocator, TBuffViewObjAllocator> :: Map( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData )
{
    switch( MapType )
    {
    case MAP_READ:
        VERIFY_BUFFER( this->m_Desc.Usage == USAGE_CPU_ACCESSIBLE,      "Only buffers with usage USAGE_CPU_ACCESSIBLE can be read from" );
        VERIFY_BUFFER( (this->m_Desc.CPUAccessFlags & CPU_ACCESS_READ), "Buffer being mapped for reading was not created with CPU_ACCESS_READ flag" );
        break;

    case MAP_WRITE:
        VERIFY_BUFFER( this->m_Desc.Usage == USAGE_CPU_ACCESSIBLE,       "Only buffers with usage USAGE_CPU_ACCESSIBLE can be written to" );
        VERIFY_BUFFER( (this->m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE), "Buffer being mapped for writing was not created with CPU_ACCESS_WRITE flag" );
        break;

    case MAP_READ_WRITE:
        VERIFY_BUFFER( this->m_Desc.Usage == USAGE_CPU_ACCESSIBLE,       "Only buffers with usage USAGE_CPU_ACCESSIBLE can be read and written" );
        VERIFY_BUFFER( (this->m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE), "Buffer being mapped for reading & writing was not created with CPU_ACCESS_WRITE flag" );
        VERIFY_BUFFER( (this->m_Desc.CPUAccessFlags & CPU_ACCESS_READ),  "Buffer being mapped for reading & writing was not created with CPU_ACCESS_READ flag" );
        break;

    case MAP_WRITE_DISCARD:
        VERIFY_BUFFER( this->m_Desc.Usage == USAGE_DYNAMIC,              "Only dynamic buffers can be mapped with write discard flag" );
        VERIFY_BUFFER( (this->m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE), "Dynamic buffer must be created with CPU_ACCESS_WRITE flag" );
        break;

    case MAP_WRITE_NO_OVERWRITE:
        VERIFY_BUFFER( this->m_Desc.Usage == USAGE_DYNAMIC,              "Only dynamic buffers can be mapped with write no overwrite flag" );
        VERIFY_BUFFER( (this->m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE), "Dynamic buffer must be created with CPU_ACCESS_WRITE flag" );
        break;

    default: UNEXPECTED( "Unknown map type" );
    }
}

template<class BaseInterface, class BufferViewImplType, class TBuffObjAllocator, class TBuffViewObjAllocator>
void BufferBase<BaseInterface, BufferViewImplType, TBuffObjAllocator, TBuffViewObjAllocator> :: Unmap( IDeviceContext *pContext, MAP_TYPE MapType )
{

}


template<class BaseInterface, class BufferViewImplType, class TBuffObjAllocator, class TBuffViewObjAllocator>
void BufferBase<BaseInterface, BufferViewImplType, TBuffObjAllocator, TBuffViewObjAllocator> :: CreateView( const struct BufferViewDesc &ViewDesc, IBufferView **ppView )
{
    CreateViewInternal( ViewDesc, ppView, false );
}


template<class BaseInterface, class BufferViewImplType, class TBuffObjAllocator, class TBuffViewObjAllocator>
void BufferBase<BaseInterface, BufferViewImplType, TBuffObjAllocator, TBuffViewObjAllocator> :: CorrectBufferViewDesc( struct BufferViewDesc &ViewDesc )
{
    if( ViewDesc.ByteWidth == 0 )
        ViewDesc.ByteWidth = this->m_Desc.uiSizeInBytes;
    if( ViewDesc.ByteOffset + ViewDesc.ByteWidth > this->m_Desc.uiSizeInBytes )
        LOG_ERROR_AND_THROW( "Buffer view range [", ViewDesc.ByteOffset, ", ", ViewDesc.ByteOffset + ViewDesc.ByteWidth, ") is out of the buffer boundaries [0, ", this->m_Desc.uiSizeInBytes, ")." );
    if( (this->m_Desc.BindFlags & BIND_UNORDERED_ACCESS) ||
        (this->m_Desc.BindFlags & BIND_SHADER_RESOURCE) )
    {
        VERIFY( this->m_Desc.ElementByteStride != 0, "Element byte stride is zero" );
        if( (ViewDesc.ByteOffset % this->m_Desc.ElementByteStride) != 0 )
            LOG_ERROR_AND_THROW( "Buffer view byte offset (", ViewDesc.ByteOffset, ") is not multiple of element byte stride (", this->m_Desc.ElementByteStride, ")." );
        if( (ViewDesc.ByteWidth % this->m_Desc.ElementByteStride) != 0 )
            LOG_ERROR_AND_THROW( "Buffer view byte width (", ViewDesc.ByteWidth, ") is not multiple of element byte stride (", this->m_Desc.ElementByteStride, ")." );
    }
}

template<class BaseInterface, class BufferViewImplType, class TBuffObjAllocator, class TBuffViewObjAllocator>
IBufferView* BufferBase<BaseInterface, BufferViewImplType, TBuffObjAllocator, TBuffViewObjAllocator> ::GetDefaultView( BUFFER_VIEW_TYPE ViewType )
{
    switch( ViewType )
    {
        case BUFFER_VIEW_SHADER_RESOURCE:  return m_pDefaultSRV.get();
        case BUFFER_VIEW_UNORDERED_ACCESS: return m_pDefaultUAV.get();
        default: UNEXPECTED( "Unknown view type" ); return nullptr;
    }
}

template<class BaseInterface, class BufferViewImplType, class TBuffObjAllocator, class TBuffViewObjAllocator>
void BufferBase<BaseInterface, BufferViewImplType, TBuffObjAllocator, TBuffViewObjAllocator> :: CreateDefaultViews()
{
    if( this->m_Desc.BindFlags & BIND_UNORDERED_ACCESS )
    {
        BufferViewDesc ViewDesc;
        ViewDesc.ViewType = BUFFER_VIEW_UNORDERED_ACCESS;
        IBufferView *pUAV = nullptr;
        CreateViewInternal( ViewDesc, &pUAV, true );
        m_pDefaultUAV.reset( static_cast<BufferViewImplType*>(pUAV) );
        VERIFY( m_pDefaultUAV->GetDesc().ViewType == BUFFER_VIEW_UNORDERED_ACCESS, "Unexpected view type" );
    }

    if( this->m_Desc.BindFlags & BIND_SHADER_RESOURCE )
    {
        BufferViewDesc ViewDesc;
        ViewDesc.ViewType = BUFFER_VIEW_SHADER_RESOURCE;
        IBufferView* pSRV = nullptr;
        CreateViewInternal( ViewDesc, &pSRV, true );
        m_pDefaultSRV.reset( static_cast<BufferViewImplType*>(pSRV) );
        VERIFY( m_pDefaultSRV->GetDesc().ViewType == BUFFER_VIEW_SHADER_RESOURCE, "Unexpected view type"  );
    }
}

}
