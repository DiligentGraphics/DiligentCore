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
/// Definition of the Diligent::MapHelper helper template class

#include "DebugUtilities.h"

namespace Diligent
{

/// Facilitates resource mapping

/// \tparam DataType - type of the mapped data.
///
/// This class is designed to automate resource mapping/unmapping process.
/// The class automatically unmaps the resource when the class instance gets out of scope.\n
/// Usage example:
///
///     {
///         MapHelper<float> UniformData( pDeviceContext, pUniformBuff, MAP_WRITE_DISCARD, 0 );
///         UniformData[0] = UniformData[1] = UniformData[2] = UniformData[3] = 1;
///     }
template<typename DataType>
class MapHelper
{
public:

    /// Initializes the class member with null values
    MapHelper() : 
        m_pMappedData(nullptr)
    {
    }

    /// Initializes the object and maps the provided resource.
    /// See Map() for details.
    MapHelper( IDeviceContext *pContext, IBuffer *pBuffer, MAP_TYPE MapType, Uint32 MapFlags ) :
        m_pMappedData(nullptr)
    {
        Map(pContext, pBuffer, MapType, MapFlags);
    }

    /// Move constructor: takes over resource ownership from Helper
    MapHelper(MapHelper&& Helper) :
        m_pBuffer( std::move(Helper.m_pBuffer) ),
        m_pMappedData( std::move(Helper.m_pMappedData) ),
        m_pContext( std::move(Helper.m_pContext) )
    {
        Helper.m_pBuffer.Release();
        Helper.m_pContext.Release();
        Helper.m_pMappedData = nullptr;
    }

    /// Move-assignement operator: takes over resource ownership from Helper
    MapHelper& operator = (MapHelper&& Helper)
    {
        m_pBuffer = std::move(Helper.m_pBuffer);
        m_pMappedData = std::move(Helper.m_pMappedData);
        m_pContext = std::move( Helper.m_pContext );
        Helper.m_pBuffer.Release();
        Helper.m_pContext.Release();
        Helper.m_pMappedData = nullptr;
        return *this;
    }

    /// Maps the provided resource.

    /// \param pContext - Pointer to the device context to perform mapping with.
    /// \param pBuffer - Pointer to the buffer interface to map.
    /// \param MapType - Type of the map operation, see Diligent::MAP_TYPE for details.
    /// \param MapFlags - Additional map flags, see Diligent::MAP_FLAGS.
    void Map( IDeviceContext *pContext, IBuffer *pBuffer, MAP_TYPE MapType, Uint32 MapFlags )
    {
        VERIFY(!m_pBuffer && !m_pMappedData && !m_pContext, "Object already mapped");
        Unmap();
        m_pContext = pContext;
        m_pBuffer = pBuffer;
        m_pBuffer->Map(m_pContext, MapType, MapFlags, (PVoid&)m_pMappedData);
        VERIFY( m_pMappedData, "Map failed" );
    }

    /// Unamps the resource and resets the object state to default.
    void Unmap()
    {
        if( m_pBuffer )
        {
            m_pBuffer->Unmap(m_pContext);
            m_pBuffer.Release();
        }
        m_pContext.Release();
        m_pMappedData = nullptr;
    }
    
    /// Converts mapped data pointer to DataType*
    operator DataType* (){return m_pMappedData;}

    /// Converts mapped data pointer to const DataType*
    operator const DataType* ()const{return m_pMappedData;}

    /// Operator ->
    DataType* operator->() {return m_pMappedData;}

    /// Operator const ->
    const DataType* operator->() const{return m_pMappedData;}

    /// Unamps the resource
    ~MapHelper()
    {
        Unmap();
    }

private:
    MapHelper(const MapHelper&);
    MapHelper& operator=(const MapHelper&);

    /// Strong auto pointer to the resource
    RefCntAutoPtr<IBuffer> m_pBuffer;

    /// Strong auto pointer to the context
    RefCntAutoPtr<IDeviceContext> m_pContext;

    /// Pointer to the mapped data
    DataType *m_pMappedData;
};

}
