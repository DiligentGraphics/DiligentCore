/*     Copyright 2015-2018 Egor Yusov
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
#include "STDAllocator.h"

namespace Diligent
{
    struct ResMappingHashKey
    {
        ResMappingHashKey(const Char* Str, bool bMakeCopy, Uint32 ArrInd):
            StrKey(Str, bMakeCopy),
            ArrayIndex(ArrInd)
        {
        }

        ResMappingHashKey(ResMappingHashKey &&rhs) : 
            StrKey(std::move(rhs.StrKey)),
            ArrayIndex(rhs.ArrayIndex)
        {}

        bool operator == (const ResMappingHashKey& RHS)const
        {
            return StrKey == RHS.StrKey && ArrayIndex == RHS.ArrayIndex;
        }

        size_t GetHash()const
        {
            if( Hash == 0 )
            {
                Hash = ComputeHash( StrKey, ArrayIndex );
            }

            return Hash;
        }


        ResMappingHashKey( const ResMappingHashKey& )              = delete;
        ResMappingHashKey& operator = ( const ResMappingHashKey& ) = delete;
        ResMappingHashKey& operator = ( ResMappingHashKey&& )      = delete;

        HashMapStringKey StrKey;
        Uint32 ArrayIndex;
        mutable size_t Hash = 0;
    };
}

namespace std
{
    template<>
    struct hash<Diligent::ResMappingHashKey>
    {
        size_t operator()( const Diligent::ResMappingHashKey &Key ) const
        {
            return Key.GetHash();
        }                  
    };
}

namespace Diligent
{
    class FixedBlockMemoryAllocator;

    /// Implementation of the resource mapping
    class ResourceMappingImpl : public ObjectBase<IResourceMapping>
    {
    public:
        typedef ObjectBase<IResourceMapping> TObjectBase;

        /// \param pRefCounters - reference counters object that controls the lifetime of this resource mapping
        /// \param RawMemAllocator - raw memory allocator that is used by the m_HashTable member
        ResourceMappingImpl(IReferenceCounters *pRefCounters, IMemoryAllocator &RawMemAllocator) : 
            TObjectBase(pRefCounters),
            m_HashTable(STD_ALLOCATOR_RAW_MEM(HashTableElem, RawMemAllocator, "Allocator for unordered_map< ResMappingHashKey, RefCntAutoPtr<IDeviceObject> >") )
        {}

        ~ResourceMappingImpl();

        virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override final;

        /// Implementation of IResourceMapping::AddResource()
        virtual void AddResource( const Char *Name, IDeviceObject *pObject, bool bIsUnique )override final;

        /// Implementation of IResourceMapping::AddResourceArray()
        virtual void AddResourceArray( const Char *Name, Uint32 StartIndex, IDeviceObject * const* ppObjects, Uint32 NumElements, bool bIsUnique )override final;

        /// Implementation of IResourceMapping::RemoveResourceByName()
        virtual void RemoveResourceByName( const Char *Name, Uint32 ArrayIndex )override final;
        
        /// Implementation of IResourceMapping::GetResource()
        virtual void GetResource( const Char *Name, IDeviceObject **ppResource, Uint32 ArrayIndex )override final;

        /// Returns number of resources in the resource mapping.
        virtual size_t GetSize()override final;

    private:

        ThreadingTools::LockHelper Lock();

        ThreadingTools::LockFlag m_LockFlag;
        typedef std::pair<const ResMappingHashKey, RefCntAutoPtr<IDeviceObject> > HashTableElem;
        std::unordered_map< ResMappingHashKey, RefCntAutoPtr<IDeviceObject>, std::hash<ResMappingHashKey>, std::equal_to<ResMappingHashKey>, STDAllocatorRawMem<HashTableElem>  > m_HashTable;
    };
}
