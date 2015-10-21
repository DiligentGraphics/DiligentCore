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

#pragma once

/// \file
/// Implementation of the Diligent::BufferBase template class

#include "Buffer.h"
#include "DeviceObjectBase.h"
#include "GraphicsUtilities.h"
#include <memory>

namespace Diligent
{

/// Template class implementing base functionality for a buffer object

/// \tparam BaseInterface - base interface that this class will inheret 
///                         (Diligent::IBufferD3D11 or Diligent::IBufferGL).
/// \tparam BufferViewImplType - type of the buffer view implementation
///                              (Diligent::BufferViewD3D11Impl or Diligent::BufferVeiwGLImpl)
template<class BaseInterface, class BufferViewImplType>
class BufferBase : public DeviceObjectBase < BaseInterface, BufferDesc >
{
public:
    typedef DeviceObjectBase<BaseInterface, BufferDesc> TDeviceObjectBase;

	/// \param pDevice - pointer to the device.
	/// \param BuffDesc - buffer description.
	/// \param bIsDeviceInternal - flag indicating if the buffer is an internal device object and 
	///							   must not keep a strong reference to the device.
    BufferBase( IRenderDevice *pDevice, const BufferDesc& BuffDesc, bool bIsDeviceInternal = false ) :
        TDeviceObjectBase( pDevice, BuffDesc, nullptr, bIsDeviceInternal )
    {
        Uint32 AllowedBindFlags =
            BIND_VERTEX_BUFFER | BIND_INDEX_BUFFER | BIND_UNIFORM_BUFFER |
            BIND_SHADER_RESOURCE | BIND_STREAM_OUTPUT | BIND_UNORDERED_ACCESS |
            BIND_INDIRECT_DRAW_ARGS;
        const Char* strAllowedBindFlags =
            "BIND_VERTEX_BUFFER (1), BIND_INDEX_BUFFER (2), BIND_UNIFORM_BUFFER (4), "
            "BIND_SHADER_RESOURCE (8), BIND_STREAM_OUTPUT (16), BIND_UNORDERED_ACCESS (128), "
            "BIND_INDIRECT_DRAW_ARGS (256)";

#define VERIFY_BUFFER(Expr, ...) VERIFY(Expr, "Buffer \"",  m_Desc.Name ? m_Desc.Name : "", "\": ", ##__VA_ARGS__)

        VERIFY_BUFFER( (BuffDesc.BindFlags & ~AllowedBindFlags) == 0, "Incorrect bind flags specified (", BuffDesc.BindFlags & ~AllowedBindFlags, "). Only the following flags are allowed:\n", strAllowedBindFlags );

        m_Desc = BuffDesc;
        if( (m_Desc.BindFlags & BIND_UNORDERED_ACCESS) ||
            (m_Desc.BindFlags & BIND_SHADER_RESOURCE) )
        {
            VERIFY_BUFFER( m_Desc.Mode > BUFFER_MODE_UNDEFINED && m_Desc.Mode < BUFFER_MODE_NUM_MODES, "Buffer mode (", m_Desc.Mode, ") is not correct" );
            if( m_Desc.Mode == BUFFER_MODE_STRUCTURED )
            {
                VERIFY_BUFFER( m_Desc.ElementByteStride != 0, "Element stride cannot be zero for structured buffer" );
            }

            if( m_Desc.Mode == BUFFER_MODE_FORMATED )
            {
                VERIFY_BUFFER( m_Desc.Format.ValueType != VT_UNDEFINED, "Value type is not specified for a formated buffer" );
                VERIFY_BUFFER( m_Desc.Format.NumComponents != 0, "Num components cannot be zero in a formated buffer" );
                if( m_Desc.ElementByteStride == 0 )
                    m_Desc.ElementByteStride = static_cast<Uint32>(GetValueSize( m_Desc.Format.ValueType )) * m_Desc.Format.NumComponents;
            }
        }
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_Buffer, TDeviceObjectBase )

    virtual void UpdateData( IDeviceContext *pContext, Uint32 Offset, Uint32 Size, const PVoid pData )override = 0;

    virtual void CopyData( IDeviceContext *pContext, IBuffer *pSrcBuffer, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size )override = 0;

    virtual void Map( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData )override;

    virtual void Unmap( IDeviceContext *pContext )override = 0;

    virtual void CreateView( const struct BufferViewDesc &ViewDesc, IBufferView **ppView )override;

    virtual IBufferView* GetDefaultView( BUFFER_VIEW_TYPE ViewType )override;

    void CreateDefaultViews();

protected:
    virtual void CreateViewInternal( const struct BufferViewDesc &ViewDesc, IBufferView **ppView, bool bIsDefaultView ) = 0;

    void CorrectBufferViewDesc( struct BufferViewDesc &ViewDesc );

    BufferDesc m_Desc;

    std::unique_ptr<BufferViewImplType> m_pDefaultUAV;
    std::unique_ptr<BufferViewImplType> m_pDefaultSRV;
};

template<class BaseInterface, class BufferViewImplType>
void BufferBase<BaseInterface, BufferViewImplType> :: UpdateData( IDeviceContext *pContext, Uint32 Offset, Uint32 Size, const PVoid pData )
{
    VERIFY_BUFFER( m_Desc.Usage == USAGE_DEFAULT, "Only default usage buffers can be updated with UpdateData()" );
    VERIFY_BUFFER( Offset < m_Desc.uiSizeInBytes, "Offset (", Offset, ") exceeds the buffer size (", m_Desc.uiSizeInBytes, ")" );
    VERIFY_BUFFER( Size + Offset <= m_Desc.uiSizeInBytes, "Update region [", Offset, ",", Size + Offset, ") is out of buffer bounds [0,",m_Desc.uiSizeInBytes,")" );
}

template<class BaseInterface, class BufferViewImplType>
void BufferBase<BaseInterface, BufferViewImplType> :: CopyData( IDeviceContext *pContext, IBuffer *pSrcBuffer, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size )
{
    VERIFY_BUFFER( DstOffset + Size <= m_Desc.uiSizeInBytes, "Destination range [", DstOffset, ",", DstOffset + Size, ") is out of buffer bounds [0,",m_Desc.uiSizeInBytes,")" );
    VERIFY_BUFFER( SrcOffset + Size <= pSrcBuffer->GetDesc().uiSizeInBytes, "Source range [", SrcOffset, ",", SrcOffset + Size, ") is out of buffer bounds [0,",m_Desc.uiSizeInBytes,")" );
}


template<class BaseInterface, class BufferViewImplType>
void BufferBase<BaseInterface, BufferViewImplType> :: Map( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData )
{
    switch( MapType )
    {
    case MAP_READ:
        VERIFY_BUFFER( m_Desc.Usage == USAGE_CPU_ACCESSIBLE,      "Only buffers with usage USAGE_CPU_ACCESSIBLE can be read from" );
        VERIFY_BUFFER( (m_Desc.CPUAccessFlags & CPU_ACCESS_READ), "Buffer being mapped for reading was not created with CPU_ACCESS_READ flag" );
        break;

    case MAP_WRITE:
        VERIFY_BUFFER( m_Desc.Usage == USAGE_CPU_ACCESSIBLE,       "Only buffers with usage USAGE_CPU_ACCESSIBLE can be written to" );
        VERIFY_BUFFER( (m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE), "Buffer being mapped for writing was not created with CPU_ACCESS_WRITE flag" );
        break;

    case MAP_READ_WRITE:
        VERIFY_BUFFER( m_Desc.Usage == USAGE_CPU_ACCESSIBLE,       "Only buffers with usage USAGE_CPU_ACCESSIBLE can be read and written" );
        VERIFY_BUFFER( (m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE), "Buffer being mapped for reading & writing was not created with CPU_ACCESS_WRITE flag" );
        VERIFY_BUFFER( (m_Desc.CPUAccessFlags & CPU_ACCESS_READ),  "Buffer being mapped for reading & writing was not created with CPU_ACCESS_READ flag" );
        break;

    case MAP_WRITE_DISCARD:
        VERIFY_BUFFER( m_Desc.Usage == USAGE_DYNAMIC,              "Only dynamic buffers can be mapped with write discard flag" );
        VERIFY_BUFFER( (m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE), "Dynamic buffer must be created with CPU_ACCESS_WRITE flag" );
        break;

    case MAP_WRITE_NO_OVERWRITE:
        VERIFY_BUFFER( m_Desc.Usage == USAGE_DYNAMIC,              "Only dynamic buffers can be mapped with write no overwrite flag" );
        VERIFY_BUFFER( (m_Desc.CPUAccessFlags & CPU_ACCESS_WRITE), "Dynamic buffer must be created with CPU_ACCESS_WRITE flag" );
        break;

    default: UNEXPECTED( "Unknown map type" );
    }
}

template<class BaseInterface, class BufferViewImplType>
void BufferBase<BaseInterface, BufferViewImplType> :: Unmap( IDeviceContext *pContext )
{

}


template<class BaseInterface, class BufferViewImplType>
void BufferBase<BaseInterface, BufferViewImplType> :: CreateView( const struct BufferViewDesc &ViewDesc, IBufferView **ppView )
{
    CreateViewInternal( ViewDesc, ppView, false );
}


template<class BaseInterface, class BufferViewImplType>
void BufferBase<BaseInterface, BufferViewImplType> :: CorrectBufferViewDesc( struct BufferViewDesc &ViewDesc )
{
    if( ViewDesc.ByteWidth == 0 )
        ViewDesc.ByteWidth = m_Desc.uiSizeInBytes;
    if( ViewDesc.ByteOffset + ViewDesc.ByteWidth > m_Desc.uiSizeInBytes )
        LOG_ERROR_AND_THROW( "Buffer view range [", ViewDesc.ByteOffset, ", ", ViewDesc.ByteOffset + ViewDesc.ByteWidth, ") is out of the buffer boundaries [0, ", m_Desc.uiSizeInBytes, ")." );
    if( (m_Desc.BindFlags & BIND_UNORDERED_ACCESS) ||
        (m_Desc.BindFlags & BIND_SHADER_RESOURCE) )
    {
        VERIFY( m_Desc.ElementByteStride != 0, "Element byte stride is zero" );
        if( (ViewDesc.ByteOffset % m_Desc.ElementByteStride) != 0 )
            LOG_ERROR_AND_THROW( "Buffer view byte offset (", ViewDesc.ByteOffset, ") is not multiple of element byte stride (", m_Desc.ElementByteStride, ")." );
        if( (ViewDesc.ByteWidth % m_Desc.ElementByteStride) != 0 )
            LOG_ERROR_AND_THROW( "Buffer view byte width (", ViewDesc.ByteWidth, ") is not multiple of element byte stride (", m_Desc.ElementByteStride, ")." );
    }
}

template<class BaseInterface, class BufferViewImplType>
IBufferView* BufferBase<BaseInterface, BufferViewImplType> ::GetDefaultView( BUFFER_VIEW_TYPE ViewType )
{
    switch( ViewType )
    {
        case BUFFER_VIEW_SHADER_RESOURCE:  return m_pDefaultSRV.get();
        case BUFFER_VIEW_UNORDERED_ACCESS: return m_pDefaultUAV.get();
        default: UNEXPECTED( "Unknown view type" ); return nullptr;
    }
}

template<class BaseInterface, class BufferViewImplType>
void BufferBase<BaseInterface, BufferViewImplType> :: CreateDefaultViews()
{
    if( m_Desc.BindFlags & BIND_UNORDERED_ACCESS )
    {
        BufferViewDesc ViewDesc;
        ViewDesc.ViewType = BUFFER_VIEW_UNORDERED_ACCESS;
        IBufferView *pUAV = nullptr;
        CreateViewInternal( ViewDesc, &pUAV, true );
        m_pDefaultUAV.reset( static_cast<BufferViewImplType*>(pUAV) );
        VERIFY( m_pDefaultUAV->GetDesc().ViewType == BUFFER_VIEW_UNORDERED_ACCESS, "Unexpected view type" );
    }

    if( m_Desc.BindFlags & BIND_SHADER_RESOURCE )
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
