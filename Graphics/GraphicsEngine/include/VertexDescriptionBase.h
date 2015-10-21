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
/// Implementation of the Diligent::VertexDescriptionBase template class

#include "VertexDescription.h"
#include "DeviceObjectBase.h"
#include "GraphicsUtilities.h"

namespace Diligent
{

/// Template class implementing base functionality for a vertex description object

/// \tparam BaseInterface - base interface that this class will inheret 
///                         (Diligent::IVertexDescriptionD3D11 or Diligent::IVertexDescriptionGL).
template<class BaseInterface = IVertexDescription>
class VertexDescriptionBase : public DeviceObjectBase<BaseInterface, LayoutDesc>
{
public:
    typedef DeviceObjectBase<BaseInterface, LayoutDesc> TDeviceObjectBase;

	/// \param pDevice - pointer to the device.
	/// \param LayoutDesc - layout description.
	/// \param bIsDeviceInternal - flag indicating if the vertex description is an internal device object and 
	///							   must not keep a strong reference to the device.
    VertexDescriptionBase( IRenderDevice *pDevice, const LayoutDesc &LayoutDesc, bool bIsDeviceInternal = false ) :
        TDeviceObjectBase( pDevice, LayoutDesc, nullptr, bIsDeviceInternal),
        m_TightStrides( MaxBufferSlots ), // The size of this array must be equal to the
                                          // maximum number of buffer slots, because a layout 
                                          // element can refer to any input slot
        m_LayoutElements( LayoutDesc.NumElements )
    {
        for( size_t Elem = 0; Elem < LayoutDesc.NumElements; ++Elem )
            m_LayoutElements[Elem] = LayoutDesc.LayoutElements[Elem];
        this->m_Desc.LayoutElements = m_LayoutElements.data();
        
        // Correct description and compute offsets and tight strides
        for( auto It = m_LayoutElements.begin(); It != m_LayoutElements.end(); ++It )
        {
            if( It->ValueType == VT_FLOAT32 || It->ValueType == VT_FLOAT16 )
                It->IsNormalized = false; // Floating point values cannot be normalized

            auto BuffSlot = It->BufferSlot;
            if( BuffSlot >= m_TightStrides.size() )
                m_TightStrides.resize( BuffSlot + 1 );

            auto &CurrStride = m_TightStrides[BuffSlot];
            if( It->RelativeOffset < CurrStride )
            {
                if( It->RelativeOffset == 0 )
                    It->RelativeOffset = CurrStride;
                else
                    UNEXPECTED( "Overlapping layout elements" );
            }

            CurrStride += It->NumComponents * GetValueSize( It->ValueType );
        }
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_VertexDescription, TDeviceObjectBase )

    virtual const Uint32* GetTightStrides()
    { 
        return m_TightStrides.data();
    }

protected:
    std::vector<LayoutElement> m_LayoutElements;
    std::vector<Uint32> m_TightStrides;
};

}
