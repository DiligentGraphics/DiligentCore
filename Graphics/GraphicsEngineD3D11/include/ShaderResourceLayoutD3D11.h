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
/// Declaration of Diligent::ShaderResourceLayoutD3D11 class

// Set this define to 1 to use unordered_map to store shader variables. 
// Note that sizeof(m_VariableHash)==128 (release mode, MS compiler, x64).
#define USE_VARIABLE_HASH_MAP 0


#include "ShaderResources.h"
#include "ShaderBase.h"
#include "ShaderResourceCacheD3D11.h"
#include "EngineD3D11Defines.h"
#include "STDAllocator.h"
#include "ShaderVariableD3DBase.h"
#include "ShaderResourcesD3D11.h"

namespace Diligent
{

class IMemoryAllocator;

/// Diligent::ShaderResourceLayoutD3D11 class
/// http://diligentgraphics.com/diligent-engine/architecture/d3d11/shader-resource-layout/
class ShaderResourceLayoutD3D11
{
public:
    ShaderResourceLayoutD3D11(IObject &Owner, IMemoryAllocator& ResLayoutDataAllocator);
    ~ShaderResourceLayoutD3D11();

    // No copies or moves
    ShaderResourceLayoutD3D11(const ShaderResourceLayoutD3D11&) = delete;
    ShaderResourceLayoutD3D11& operator = (const ShaderResourceLayoutD3D11&) = delete;
    ShaderResourceLayoutD3D11(ShaderResourceLayoutD3D11&&) = default;
    ShaderResourceLayoutD3D11& operator = (ShaderResourceLayoutD3D11&&) = delete;

    void Initialize(const std::shared_ptr<const ShaderResourcesD3D11>& pSrcResources,
                    const SHADER_VARIABLE_TYPE *VarTypes, 
                    Uint32 NumVarTypes, 
                    ShaderResourceCacheD3D11& ResourceCache,
                    IMemoryAllocator& ResCacheDataAllocator,
                    IMemoryAllocator& ResLayoutDataAllocator);

    void CopyResources(ShaderResourceCacheD3D11 &DstCache);

    using ShaderVariableD3D11Base = ShaderVariableD3DBase<ShaderResourceLayoutD3D11>;

    struct ConstBuffBindInfo : ShaderVariableD3D11Base
    {
        ConstBuffBindInfo( const D3DShaderResourceAttribs& ResourceAttribs,
                           ShaderResourceLayoutD3D11 &ParentResLayout ) :
            ShaderVariableD3D11Base(ParentResLayout, ResourceAttribs)
        {}
        // Non-virtual function
        __forceinline void BindResource(IDeviceObject *pObject, Uint32 ArrayIndex, const ShaderResourceLayoutD3D11 *dbgResLayout);
        virtual void Set(IDeviceObject *pObject)override final{ BindResource(pObject, 0, nullptr); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 elem=0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement+elem, nullptr);
        }

        __forceinline bool IsBound(Uint32 ArrayIndex);
    };
    
    struct TexAndSamplerBindInfo : ShaderVariableD3D11Base
    {
        TexAndSamplerBindInfo( const D3DShaderResourceAttribs& _TextureAttribs, 
                               const D3DShaderResourceAttribs& _SamplerAttribs,
                               ShaderResourceLayoutD3D11 &ParentResLayout) :
            ShaderVariableD3D11Base(ParentResLayout, _TextureAttribs),
            SamplerAttribs(_SamplerAttribs)
        {}

        // Non-virtual function
        __forceinline void BindResource(IDeviceObject *pObject, Uint32 ArrayIndex, const ShaderResourceLayoutD3D11 *dbgResLayout);
        virtual void Set(IDeviceObject *pObject)override final{ BindResource(pObject, 0, nullptr); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 elem=0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement+elem, nullptr);
        }

        __forceinline bool IsBound(Uint32 ArrayIndex);
        
        static const D3DShaderResourceAttribs InvalidSamplerAttribs;

        const D3DShaderResourceAttribs &SamplerAttribs;
    };

    struct TexUAVBindInfo : ShaderVariableD3D11Base
    {
        TexUAVBindInfo( const D3DShaderResourceAttribs& ResourceAttribs,
                        ShaderResourceLayoutD3D11 &ParentResLayout ) :
            ShaderVariableD3D11Base(ParentResLayout, ResourceAttribs)
        {}

        // Provide non-virtual function
        __forceinline void BindResource(IDeviceObject *pObject, Uint32 ArrayIndex, const ShaderResourceLayoutD3D11 *dbgResLayout);
        virtual void Set(IDeviceObject *pObject)override final{ BindResource(pObject, 0, nullptr); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 elem=0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement+elem, nullptr);
        }

        __forceinline bool IsBound(Uint32 ArrayIndex);
    };

    struct BuffUAVBindInfo : ShaderVariableD3D11Base
    {
        BuffUAVBindInfo( const D3DShaderResourceAttribs& ResourceAttribs,
                         ShaderResourceLayoutD3D11 &ParentResLayout ) :
            ShaderVariableD3D11Base(ParentResLayout, ResourceAttribs)
        {}

        // Non-virtual function
        __forceinline void BindResource(IDeviceObject *pObject, Uint32 ArrayIndex, const ShaderResourceLayoutD3D11 *dbgResLayout);
        virtual void Set(IDeviceObject *pObject)override final{ BindResource(pObject, 0, nullptr); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 elem=0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement+elem, nullptr);
        }

        __forceinline bool IsBound(Uint32 ArrayIndex);
    };

    struct BuffSRVBindInfo : ShaderVariableD3D11Base
    {
        BuffSRVBindInfo( const D3DShaderResourceAttribs& ResourceAttribs,
                         ShaderResourceLayoutD3D11 &ParentResLayout ) :
            ShaderVariableD3D11Base(ParentResLayout, ResourceAttribs)
        {}

        // Non-virtual function
        __forceinline void BindResource(IDeviceObject *pObject, Uint32 ArrayIndex, const ShaderResourceLayoutD3D11 *dbgResLayout);
        virtual void Set(IDeviceObject *pObject)override final{ BindResource(pObject, 0, nullptr); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 elem=0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement+elem, nullptr);
        }

        __forceinline bool IsBound(Uint32 ArrayIndex);
    };

    // dbgResourceCache is only used for sanity check and as a remainder that the resource cache must be alive
    // while Layout is alive
    void BindResources( IResourceMapping* pResourceMapping, Uint32 Flags, const ShaderResourceCacheD3D11 &dbgResourceCache );

#ifdef VERIFY_SHADER_BINDINGS
    void dbgVerifyBindings()const;
#endif

    IShaderVariable* GetShaderVariable( const Char* Name );
    __forceinline SHADER_TYPE GetShaderType()const{return m_pResources->GetShaderType();}

    IObject& GetOwner(){return m_Owner;}
private:

    void InitVariablesHashMap();
    
    const Char* GetShaderName()const;

    // No need to use shared pointer, as the resource cache is either part of the same
    // ShaderD3D11Impl object, or ShaderResourceBindingD3D11Impl object
    ShaderResourceCacheD3D11 *m_pResourceCache = nullptr;

    std::unique_ptr<void, STDDeleterRawMem<void> > m_ResourceBuffer;

    // Offsets in bytes
    Uint16 m_TexAndSamplersOffset = 0;
    Uint16 m_TexUAVsOffset  = 0;
    Uint16 m_BuffUAVsOffset = 0;
    Uint16 m_BuffSRVsOffset = 0;
    
    Uint8 m_NumCBs     = 0; // Max == 14
    Uint8 m_NumTexSRVs = 0; // Max == 128
    Uint8 m_NumTexUAVs = 0; // Max == 8
    Uint8 m_NumBufUAVs = 0; // Max == 8
    Uint8 m_NumBufSRVs = 0; // Max == 128

    ConstBuffBindInfo& GetCB(Uint32 cb)
    {
        VERIFY_EXPR(cb<m_NumCBs);
        return reinterpret_cast<ConstBuffBindInfo*>(m_ResourceBuffer.get())[cb];
    }
    const ConstBuffBindInfo& GetCB(Uint32 cb)const
    {
        VERIFY_EXPR(cb<m_NumCBs);
        return reinterpret_cast<ConstBuffBindInfo*>(m_ResourceBuffer.get())[cb];
    }

    TexAndSamplerBindInfo& GetTexSRV(Uint32 t)
    {
        VERIFY_EXPR(t<m_NumTexSRVs);
        return reinterpret_cast<TexAndSamplerBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_TexAndSamplersOffset )[t];
    }
    const TexAndSamplerBindInfo& GetTexSRV(Uint32 t)const
    {
        VERIFY_EXPR(t<m_NumTexSRVs);
        return reinterpret_cast<TexAndSamplerBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_TexAndSamplersOffset )[t];
    }

    TexUAVBindInfo& GetTexUAV(Uint32 u)
    {
        VERIFY_EXPR(u < m_NumTexUAVs);
        return reinterpret_cast<TexUAVBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_TexUAVsOffset)[u];
    }
    const TexUAVBindInfo& GetTexUAV(Uint32 u)const
    {
        VERIFY_EXPR(u < m_NumTexUAVs);
        return reinterpret_cast<TexUAVBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_TexUAVsOffset)[u];
    }

    BuffUAVBindInfo& GetBufUAV(Uint32 u)
    {
        VERIFY_EXPR(u < m_NumBufUAVs);
        return reinterpret_cast<BuffUAVBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_BuffUAVsOffset)[u];
    }
    const BuffUAVBindInfo& GetBufUAV(Uint32 u)const
    {
        VERIFY_EXPR(u < m_NumBufUAVs);
        return reinterpret_cast<BuffUAVBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_BuffUAVsOffset)[u];
    }

    BuffSRVBindInfo& GetBufSRV(Uint32 s)
    {
        VERIFY_EXPR(s < m_NumBufSRVs);
        return reinterpret_cast<BuffSRVBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_BuffSRVsOffset)[s];
    }
    const BuffSRVBindInfo& GetBufSRV(Uint32 s)const
    {
        VERIFY_EXPR(s < m_NumBufSRVs);
        return reinterpret_cast<BuffSRVBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_BuffSRVsOffset)[s];
    }

    template<typename THandleCB,
             typename THandleTexSRV,
             typename THandleTexUAV,
             typename THandleBufSRV,
             typename THandleBufUAV>
    void HandleResources(THandleCB HandleCB,
                         THandleTexSRV HandleTexSRV,
                         THandleTexUAV HandleTexUAV,
                         THandleBufSRV HandleBufSRV,
                         THandleBufUAV HandleBufUAV)
    {
        for (Uint32 cb = 0; cb < m_NumCBs; ++cb)
            HandleCB(GetCB(cb));
        for (Uint32 t = 0; t < m_NumTexSRVs; ++t)
            HandleTexSRV(GetTexSRV(t));
        for (Uint32 u = 0; u < m_NumTexUAVs; ++u)
            HandleTexUAV(GetTexUAV(u));
        for (Uint32 s = 0; s < m_NumBufSRVs; ++s)
            HandleBufSRV(GetBufSRV(s));
        for (Uint32 u = 0; u < m_NumBufUAVs; ++u)
            HandleBufUAV(GetBufUAV(u));
    }

#if USE_VARIABLE_HASH_MAP
    // Hash map to look up shader variables by name.
    // Note that sizeof(m_VariableHash)==128 (release mode, MS compiler, x64).
    typedef std::pair<HashMapStringKey, IShaderVariable*> VariableHashData;
    std::unordered_map<HashMapStringKey, IShaderVariable*, std::hash<HashMapStringKey>, std::equal_to<HashMapStringKey>, STDAllocatorRawMem<VariableHashData> > m_VariableHash;
#endif

    std::shared_ptr<const ShaderResourcesD3D11> m_pResources;
    IObject &m_Owner;
};

}
