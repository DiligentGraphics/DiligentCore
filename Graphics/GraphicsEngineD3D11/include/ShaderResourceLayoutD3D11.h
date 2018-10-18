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
    ShaderResourceLayoutD3D11(IObject& Owner);
    ~ShaderResourceLayoutD3D11();

    // No copies or moves
    ShaderResourceLayoutD3D11             (const ShaderResourceLayoutD3D11&)  = delete;
    ShaderResourceLayoutD3D11& operator = (const ShaderResourceLayoutD3D11&)  = delete;
    ShaderResourceLayoutD3D11             (      ShaderResourceLayoutD3D11&&) = default;
    ShaderResourceLayoutD3D11& operator = (      ShaderResourceLayoutD3D11&&) = delete;

    static size_t GetRequiredMemorySize(const ShaderResourcesD3D11& SrcResources, 
                                        const SHADER_VARIABLE_TYPE* VarTypes, 
                                        Uint32                      NumVarTypes);

    void Initialize(const std::shared_ptr<const ShaderResourcesD3D11>& pSrcResources,
                    const SHADER_VARIABLE_TYPE*                        VarTypes, 
                    Uint32                                             NumVarTypes, 
                    ShaderResourceCacheD3D11&                          ResourceCache,
                    IMemoryAllocator&                                  ResCacheDataAllocator,
                    IMemoryAllocator&                                  ResLayoutDataAllocator);

    void CopyResources(ShaderResourceCacheD3D11& DstCache);

    using ShaderVariableD3D11Base = ShaderVariableD3DBase<ShaderResourceLayoutD3D11>;

    struct ConstBuffBindInfo final : ShaderVariableD3D11Base
    {
        ConstBuffBindInfo( const D3DShaderResourceAttribs& ResourceAttribs,
                           ShaderResourceLayoutD3D11&      ParentResLayout ) :
            ShaderVariableD3D11Base(ParentResLayout, ResourceAttribs)
        {}
        // Non-virtual function
        __forceinline void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);
        virtual void Set(IDeviceObject* pObject)override final{ BindResource(pObject, 0); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 elem=0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement+elem);
        }

        __forceinline bool IsBound(Uint32 ArrayIndex);
    };
    
    struct TexSRVBindInfo final : ShaderVariableD3D11Base
    {
        TexSRVBindInfo( const D3DShaderResourceAttribs& _TextureAttribs, 
                        Uint32                          _SamplerIndex,
                        ShaderResourceLayoutD3D11&      ParentResLayout) :
            ShaderVariableD3D11Base(ParentResLayout, _TextureAttribs),
            SamplerIndex(_SamplerIndex)
        {}

        // Non-virtual function
        __forceinline void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);
        virtual void Set(IDeviceObject* pObject)override final{ BindResource(pObject, 0); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 elem=0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement+elem);
        }

        __forceinline bool IsBound(Uint32 ArrayIndex)const;
        
        bool ValidSamplerAssigned() const {return SamplerIndex != InvalidSamplerIndex;}

        static constexpr Uint32 InvalidSamplerIndex = static_cast<Uint32>(-1);
        const Uint32 SamplerIndex;
    };

    struct TexUAVBindInfo final : ShaderVariableD3D11Base
    {
        TexUAVBindInfo( const D3DShaderResourceAttribs& ResourceAttribs,
                        ShaderResourceLayoutD3D11&      ParentResLayout ) :
            ShaderVariableD3D11Base(ParentResLayout, ResourceAttribs)
        {}

        // Provide non-virtual function
        __forceinline void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);
        virtual void Set(IDeviceObject* pObject)override final{ BindResource(pObject, 0); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 elem=0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement+elem);
        }

        __forceinline bool IsBound(Uint32 ArrayIndex)const;
    };

    struct BuffUAVBindInfo final : ShaderVariableD3D11Base
    {
        BuffUAVBindInfo( const D3DShaderResourceAttribs& ResourceAttribs,
                         ShaderResourceLayoutD3D11&      ParentResLayout ) :
            ShaderVariableD3D11Base(ParentResLayout, ResourceAttribs)
        {}

        // Non-virtual function
        __forceinline void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);
        virtual void Set(IDeviceObject* pObject)override final{ BindResource(pObject, 0); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 elem=0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement+elem);
        }

        __forceinline bool IsBound(Uint32 ArrayIndex)const;
    };

    struct BuffSRVBindInfo final : ShaderVariableD3D11Base
    {
        BuffSRVBindInfo( const D3DShaderResourceAttribs& ResourceAttribs,
                         ShaderResourceLayoutD3D11&      ParentResLayout ) :
            ShaderVariableD3D11Base(ParentResLayout, ResourceAttribs)
        {}

        // Non-virtual function
        __forceinline void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);
        virtual void Set(IDeviceObject* pObject)override final{ BindResource(pObject, 0); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 elem=0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement+elem);
        }

        __forceinline bool IsBound(Uint32 ArrayIndex)const;
    };

    struct SamplerBindInfo final : ShaderVariableD3D11Base
    {
        SamplerBindInfo( const D3DShaderResourceAttribs& ResourceAttribs,
                         ShaderResourceLayoutD3D11&      ParentResLayout ) :
            ShaderVariableD3D11Base(ParentResLayout, ResourceAttribs)
        {}

        // Non-virtual function
        __forceinline void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);
        virtual void Set(IDeviceObject* pObject)override final{ BindResource(pObject, 0); }

        virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
        {
            for(Uint32 elem=0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement+elem);
        }

        __forceinline bool IsBound(Uint32 ArrayIndex)const;
    };

    // dbgResourceCache is only used for sanity check and as a remainder that the resource cache must be alive
    // while Layout is alive
    void BindResources( IResourceMapping* pResourceMapping, Uint32 Flags, const ShaderResourceCacheD3D11& dbgResourceCache );

#ifdef DEVELOPMENT
    void dvpVerifyBindings()const;
#endif

    IShaderVariable* GetShaderVariable( const Char* Name );
    IShaderVariable* GetShaderVariable( Uint32 Index );
    __forceinline SHADER_TYPE GetShaderType()const{return m_pResources->GetShaderType();}

    IObject& GetOwner(){return m_Owner;}

    Uint32 GetVariableIndex(const ShaderVariableD3D11Base& Variable)const;
    Uint32 GetTotalResourceCount()const
    {
        auto ResourceCount = m_NumCBs + m_NumTexSRVs + m_NumTexUAVs + m_NumBufUAVs + m_NumBufSRVs;
        // Do not expose sampler variables when using combined texture samplers
        if (!m_pResources->IsUsingCombinedTextureSamplers())
            ResourceCount += m_NumSamplers;
        return ResourceCount;
    }

private:

    const Char* GetShaderName()const;

    // No need to use shared pointer, as the resource cache is either part of the same
    // ShaderD3D11Impl object, or ShaderResourceBindingD3D11Impl object
    ShaderResourceCacheD3D11* m_pResourceCache = nullptr;

    std::unique_ptr<void, STDDeleterRawMem<void> > m_ResourceBuffer;

    // Offsets in bytes
    Uint16 m_TexSRVsOffset  = 0;
    Uint16 m_TexUAVsOffset  = 0;
    Uint16 m_BuffUAVsOffset = 0;
    Uint16 m_BuffSRVsOffset = 0;
    Uint16 m_SamplerOffset  = 0;
    
    Uint8 m_NumCBs      = 0; // Max == 14
    Uint8 m_NumTexSRVs  = 0; // Max == 128
    Uint8 m_NumTexUAVs  = 0; // Max == 8
    Uint8 m_NumBufUAVs  = 0; // Max == 8
    Uint8 m_NumBufSRVs  = 0; // Max == 128
    Uint8 m_NumSamplers = 0; // Max == 16

    ConstBuffBindInfo& GetCB(Uint32 cb)
    {
        VERIFY_EXPR(cb<m_NumCBs);
        return reinterpret_cast<ConstBuffBindInfo*>(m_ResourceBuffer.get())[cb];
    }
    const ConstBuffBindInfo& GetCB(Uint32 cb)const
    {
        VERIFY_EXPR(cb<m_NumCBs);
        return reinterpret_cast<const ConstBuffBindInfo*>(m_ResourceBuffer.get())[cb];
    }

    TexSRVBindInfo& GetTexSRV(Uint32 t)
    {
        VERIFY_EXPR(t<m_NumTexSRVs);
        return reinterpret_cast<TexSRVBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_TexSRVsOffset)[t];
    }
    const TexSRVBindInfo& GetTexSRV(Uint32 t)const
    {
        VERIFY_EXPR(t<m_NumTexSRVs);
        return reinterpret_cast<const TexSRVBindInfo*>( reinterpret_cast<const Uint8*>(m_ResourceBuffer.get()) + m_TexSRVsOffset)[t];
    }

    TexUAVBindInfo& GetTexUAV(Uint32 u)
    {
        VERIFY_EXPR(u < m_NumTexUAVs);
        return reinterpret_cast<TexUAVBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_TexUAVsOffset)[u];
    }
    const TexUAVBindInfo& GetTexUAV(Uint32 u)const
    {
        VERIFY_EXPR(u < m_NumTexUAVs);
        return reinterpret_cast<const TexUAVBindInfo*>( reinterpret_cast<const Uint8*>(m_ResourceBuffer.get()) + m_TexUAVsOffset)[u];
    }

    BuffUAVBindInfo& GetBufUAV(Uint32 u)
    {
        VERIFY_EXPR(u < m_NumBufUAVs);
        return reinterpret_cast<BuffUAVBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_BuffUAVsOffset)[u];
    }
    const BuffUAVBindInfo& GetBufUAV(Uint32 u)const
    {
        VERIFY_EXPR(u < m_NumBufUAVs);
        return reinterpret_cast<const BuffUAVBindInfo*>( reinterpret_cast<const Uint8*>(m_ResourceBuffer.get()) + m_BuffUAVsOffset)[u];
    }

    BuffSRVBindInfo& GetBufSRV(Uint32 s)
    {
        VERIFY_EXPR(s < m_NumBufSRVs);
        return reinterpret_cast<BuffSRVBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_BuffSRVsOffset)[s];
    }
    const BuffSRVBindInfo& GetBufSRV(Uint32 s)const
    {
        VERIFY_EXPR(s < m_NumBufSRVs);
        return reinterpret_cast<const BuffSRVBindInfo*>( reinterpret_cast<const Uint8*>(m_ResourceBuffer.get()) + m_BuffSRVsOffset)[s];
    }

    SamplerBindInfo& GetSampler(Uint32 s)
    {
        VERIFY_EXPR(s < m_NumSamplers);
        return reinterpret_cast<SamplerBindInfo*>( reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + m_SamplerOffset)[s];
    }
    const SamplerBindInfo& GetSampler(Uint32 s)const
    {
        VERIFY_EXPR(s < m_NumSamplers);
        return reinterpret_cast<const SamplerBindInfo*>( reinterpret_cast<const Uint8*>(m_ResourceBuffer.get()) + m_SamplerOffset)[s];
    }

    template<typename THandleCB,
             typename THandleTexSRV,
             typename THandleTexUAV,
             typename THandleBufSRV,
             typename THandleBufUAV,
             typename THandleSampler>
    void HandleResources(THandleCB      HandleCB,
                         THandleTexSRV  HandleTexSRV,
                         THandleTexUAV  HandleTexUAV,
                         THandleBufSRV  HandleBufSRV,
                         THandleBufUAV  HandleBufUAV,
                         THandleSampler HandleSampler)
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
        for (Uint32 s = 0; s < m_NumSamplers; ++s)
            HandleSampler(GetSampler(s));
    }

    std::shared_ptr<const ShaderResourcesD3D11> m_pResources;
    IObject& m_Owner;
};

}
