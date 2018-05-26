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

#include <memory>
#include <unordered_map>

// Set this define to 1 to use unordered_map to store shader variables. 
// Note that sizeof(m_VariableHash)==128 (release mode, MS compiler, x64).
#define USE_VARIABLE_HASH_MAP 0

#include "ShaderResourceLayoutVk.h"

namespace Diligent
{

class ShaderVariableVkImpl;

class ShaderVariableManagerVk
{
public:
    ShaderVariableManagerVk(IObject &Owner) :
        m_Owner(Owner)
    {}
    ~ShaderVariableManagerVk();

    void Initialize(const ShaderResourceLayoutVk& Layout, 
                    IMemoryAllocator&             Allocator,
                    const SHADER_VARIABLE_TYPE*   AllowedVarTypes, 
                    Uint32                        NumAllowedTypes, 
                    ShaderResourceCacheVk&        ResourceCache);
    void Destroy(IMemoryAllocator& Allocator);

    ShaderVariableVkImpl* GetVariable(const Char* Name);

    void BindResources( IResourceMapping* pResourceMapping, Uint32 Flags);

private:
    friend ShaderVariableVkImpl;

#if USE_VARIABLE_HASH_MAP
    void InitVariablesHashMap();

    // Hash map to look up shader variables by name.
    // Note that sizeof(m_VariableHash)==128 (release mode, MS compiler, x64).
    typedef std::pair<HashMapStringKey, IShaderVariable*> VariableHashElemType;
    std::unordered_map<HashMapStringKey, IShaderVariable*, std::hash<HashMapStringKey>, std::equal_to<HashMapStringKey>, STDAllocatorRawMem<VariableHashElemType> > m_VariableHash;
#endif

    IObject&                      m_Owner;
    const ShaderResourceLayoutVk* m_pResourceLayout= nullptr;
    ShaderResourceCacheVk*        m_pResourceCache = nullptr;
    ShaderVariableVkImpl*         m_pVariables     = nullptr;
    Uint32                        m_NumVariables = 0;
#ifdef _DEBUG
    IMemoryAllocator*             m_pDbgAllocator = nullptr;
#endif
};

// sizeof(ShaderVariableVkImpl) == 24 (x64)
class ShaderVariableVkImpl : public IShaderVariable
{
public:
    ShaderVariableVkImpl(ShaderVariableManagerVk& ParentManager,
                         const ShaderResourceLayoutVk::VkResource& Resource) :
        m_ParentManager(ParentManager),
        m_Resource(Resource)
    {}

    ShaderVariableVkImpl(const ShaderVariableVkImpl&) = delete;
    ShaderVariableVkImpl(ShaderVariableVkImpl&&) = delete;
    ShaderVariableVkImpl& operator= (const ShaderVariableVkImpl&) = delete;
    ShaderVariableVkImpl& operator= (ShaderVariableVkImpl&&) = delete;


    virtual IReferenceCounters* GetReferenceCounters()const override final
    {
        return m_ParentManager.m_Owner.GetReferenceCounters();
    }

    virtual Atomics::Long AddRef()override final
    {
        return m_ParentManager.m_Owner.AddRef();
    }

    virtual Atomics::Long Release()override final
    {
        return m_ParentManager.m_Owner.Release();
    }

    void QueryInterface(const INTERFACE_ID &IID, IObject **ppInterface)override final
    {
        if (ppInterface == nullptr)
            return;

        *ppInterface = nullptr;
        if (IID == IID_ShaderVariable || IID == IID_Unknown)
        {
            *ppInterface = this;
            (*ppInterface)->AddRef();
        }
    }

    virtual void Set(IDeviceObject *pObject)override final 
    {
        VERIFY_EXPR(m_ParentManager.m_pResourceCache != nullptr);
        m_Resource.BindResource(pObject, 0, *m_ParentManager.m_pResourceCache); 
    }

    virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
    {
        VERIFY_EXPR(m_ParentManager.m_pResourceCache != nullptr);
        for (Uint32 Elem = 0; Elem < NumElements; ++Elem)
            m_Resource.BindResource(ppObjects[Elem], FirstElement + Elem, *m_ParentManager.m_pResourceCache);
    }

    const ShaderResourceLayoutVk::VkResource& GetResource()const
    {
        return m_Resource;
    }

private:
    friend ShaderVariableManagerVk;

    ShaderVariableManagerVk&                  m_ParentManager;
    const ShaderResourceLayoutVk::VkResource& m_Resource;
};

}
