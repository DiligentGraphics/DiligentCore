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
#include "ResourceMappingImpl.h"
#include "DeviceObjectBase.h"

using namespace std;

namespace Diligent
{
    ResourceMappingImpl::~ResourceMappingImpl()
    {
    }

    IMPLEMENT_QUERY_INTERFACE( ResourceMappingImpl, IID_ResourceMapping, ObjectBase<IResourceMapping> )

    ThreadingTools::LockHelper ResourceMappingImpl::Lock()
    {
        return std::move( ThreadingTools::LockHelper( m_LockFlag ) );
    }

    void ResourceMappingImpl::AddResource( const Char *Name, IDeviceObject *pObject, bool bIsUnique )
    {
        if( Name == nullptr || *Name == 0 )
            return;

        auto LockHelper = Lock();
        // Try to construct new element in place
        auto Elems = 
            m_HashTable.emplace( 
                                make_pair( Diligent::HashMapStringKey(Name, true), // Make a copy of the source string
                                           Diligent::RefCntAutoPtr<IDeviceObject>(pObject) 
                                          ) 
                                );
        // If there is already element with the same name, replace it
        if( !Elems.second && Elems.first->second != pObject )
        {
            if( bIsUnique )
            {
                UNEXPECTED( "Resource with the same name already exists" );
                LOG_WARNING_MESSAGE(
                    "Resource with name ", Name, " marked is unique, but already present in the hash.\n"
                    "New resource will be used\n." );
            }
            Elems.first->second = pObject;
        }
    }

    void ResourceMappingImpl::RemoveResourceByName( const Char *Name )
    {
        if( *Name == 0 )
            return;

        auto LockHelper = Lock();
        // Remove object with the given name
        // Name will be implicitly converted to HashMapStringKey without making a copy
        auto It = m_HashTable.erase( Name );
    }

    void ResourceMappingImpl::RemoveResource( IDeviceObject *pObject )
    {
        const auto *Name = pObject->GetDesc().Name;
        VERIFY( Name, "Name is null" );
        if( *Name == 0 )
            return;

        auto LockHelper = Lock();

        // Find active object with the same name
        // Name will be implicitly converted to HashMapStringKey without making a copy
        auto It = m_HashTable.find( Name );
        // Check if the active object is in fact the object being removed
        if( It != m_HashTable.end() && It->second == pObject )
        {
            m_HashTable.erase( It );
        }
    }

    void ResourceMappingImpl::GetResource( const Char *Name, IDeviceObject **ppResource )
    {
        VERIFY(Name, "Name is null")
        if( *Name == 0 )
            return;

        VERIFY( ppResource, "Null pointer provided" );
        if(!ppResource)
            return;

        VERIFY( *ppResource == nullptr, "Overwriting reference to existing object may cause memory leaks" );
        *ppResource = nullptr;

        auto LockHelper = Lock();

        // Find an object with the requested name
        // Name will be implicitly converted to HashMapStringKey without making a copy
        auto It = m_HashTable.find( Name );
        if( It != m_HashTable.end() )
        {
            *ppResource = It->second.RawPtr();
            if(*ppResource)
                (*ppResource)->AddRef();
        }
    }

    size_t ResourceMappingImpl::GetSize()
    {
        return m_HashTable.size();
    }
}
