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
/// Implementation of the Diligent::DeviceObjectBase template class

#include "ObjectBase.h"
#include "UniqueIdentifier.h"

namespace Diligent
{
   
template<typename BaseInterface>
class RenderDeviceBase;

/// Template class implementing base functionality for a device object
template<class BaseInterface, typename ObjectDescType, typename TObjectAllocator>
class DeviceObjectBase : public ObjectBase<BaseInterface, TObjectAllocator>
{
public:
    typedef ObjectBase<BaseInterface, TObjectAllocator> TBase;

    /// \param ObjAllocator - allocator that was used to allocate memory for this instance
	/// \param pDevice - pointer to the render device.
	/// \param ObjDesc - object description.
	/// \param pOwner - owner object.
	/// \param bIsDeviceInternal - flag indicating if the object is an internal device object
	///							   and must not keep a strong reference to the device.
    DeviceObjectBase( TObjectAllocator &ObjAllocator,
                      class IRenderDevice *pDevice,
					  const ObjectDescType &ObjDesc,
					  IObject *pOwner = nullptr,
				      bool bIsDeviceInternal = false) :
        TBase(pOwner, &ObjAllocator),
        m_pDevice( pDevice ),
		// Do not keep strong reference to the device if the object is an internal device object
		m_spDevice( bIsDeviceInternal ? nullptr : pDevice ),
        m_ObjectNameCopy(ObjDesc.Name ? ObjDesc.Name : ""),
        m_Desc( ObjDesc )
    {
        m_Desc.Name = m_ObjectNameCopy.c_str();

        //                        !!!WARNING!!!
        // We cannot add resource to the hash table from here, because the object
        // has not been completely created yet and the reference counters object
        // is not initialized!
        //m_pDevice->AddResourceToHash( this ); - ERROR!
    }

    virtual ~DeviceObjectBase()
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_DeviceObject, TBase )

    virtual const ObjectDescType& GetDesc()const override final
    {
        return m_Desc;
    }

	/// Returns unique identifier
    UniqueIdentifier GetUniqueID()const
    {
		/// \note
        /// This unique ID is used to unambiguously identify device object for
        /// tracking purposes.
        /// Niether GL handle nor pointer could be safely used for this purpose
        /// as both GL reuses released handles and the OS reuses released pointers
        return m_UniqueID.GetID();
    }

	IRenderDevice *GetDevice()const{return m_pDevice;}

protected:
	
    /// Copy of a device object name.

    /// When new object is created, its description structure is copied
    /// to m_Desc, the name is copied to m_ObjectNameCopy, and 
    /// m_Desc.Name pointer is set to m_ObjectNameCopy.c_str().
    const String m_ObjectNameCopy;

	/// Object description
    ObjectDescType m_Desc;
    
    // Template argument is only used to separate counters for 
    // different groups of objects
    UniqueIdHelper<BaseInterface> m_UniqueID;

private:
    /// Pointer to the device
    IRenderDevice *m_pDevice;
	/// Strong reference to the device
	RefCntAutoPtr<IRenderDevice> m_spDevice;

    DeviceObjectBase( const DeviceObjectBase& );
    DeviceObjectBase( DeviceObjectBase&& );
    const DeviceObjectBase& operator = ( const DeviceObjectBase& );
    const DeviceObjectBase& operator = ( DeviceObjectBase&& );
};

}
