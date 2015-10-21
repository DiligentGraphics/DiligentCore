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
/// Declaration of the Diligent::ResourceMappingImpl class

#include "ResourceMapping.h"
#include "ObjectBase.h"
#include <unordered_map>
#include "HashUtils.h"

namespace Diligent
{
    /// Implementation of the resource mapping
    class ResourceMappingImpl : public ObjectBase<IResourceMapping>
    {
    public:
        ~ResourceMappingImpl();

        virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface );

        virtual void AddResource( const Char *Name, IDeviceObject *pObject, bool bIsUnique );
        virtual void RemoveResource( IDeviceObject *pObject );
        virtual void RemoveResourceByName( const Char *Name );
        
        /// Finds resource in the mapping. If no resource is found, nullptr is returned
        virtual void GetResource( const Char *Name, IDeviceObject **ppResource );
        virtual size_t GetSize();

    private:
        ThreadingTools::LockHelper Lock();

        ThreadingTools::LockFlag m_LockFlag;
        std::unordered_map< Diligent::HashMapStringKey, Diligent::RefCntAutoPtr<IDeviceObject> > m_HashTable;
    };
}
