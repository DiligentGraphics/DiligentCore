/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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
/// Declaration of Diligent::ShaderVariableManagerD3D11 class

#include "ShaderResources.hpp"
#include "ShaderBase.hpp"
#include "ShaderResourceVariableBase.hpp"
#include "ShaderVariableD3D.hpp"
#include "ShaderResourcesD3D11.hpp"
#include "ShaderResourceVariableD3D.h"
#include "PipelineResourceAttribsD3D11.hpp"
#include "ShaderResourceCacheD3D11.hpp"

namespace Diligent
{

/// Diligent::ShaderVariableManagerD3D11 class
// sizeof(ShaderVariableManagerD3D11) == AZ TODO (Release, x64)
class ShaderVariableManagerD3D11
{
public:
    ShaderVariableManagerD3D11(IObject&                  Owner,
                               ShaderResourceCacheD3D11& ResourceCache) noexcept :
        m_Owner{Owner},
        m_ResourceCache{ResourceCache}
    {
    }

    ~ShaderVariableManagerD3D11();

    void Destroy(IMemoryAllocator& Allocator);

    // clang-format off
    // No copies, only moves are allowed
    ShaderVariableManagerD3D11             (const ShaderVariableManagerD3D11&)  = delete;
    ShaderVariableManagerD3D11& operator = (const ShaderVariableManagerD3D11&)  = delete;
    ShaderVariableManagerD3D11             (      ShaderVariableManagerD3D11&&) = default;
    ShaderVariableManagerD3D11& operator = (      ShaderVariableManagerD3D11&&) = delete;
    // clang-format on

    void Initialize(const PipelineResourceSignatureD3D11Impl& Signature,
                    IMemoryAllocator&                         Allocator,
                    const SHADER_RESOURCE_VARIABLE_TYPE*      AllowedVarTypes,
                    Uint32                                    NumAllowedTypes,
                    SHADER_TYPE                               ShaderType);

    static size_t GetRequiredMemorySize(const PipelineResourceSignatureD3D11Impl& Signature,
                                        const SHADER_RESOURCE_VARIABLE_TYPE*      AllowedVarTypes,
                                        Uint32                                    NumAllowedTypes,
                                        SHADER_TYPE                               ShaderType);

    using ResourceAttribs = PipelineResourceAttribsD3D11;

    const PipelineResourceDesc& GetResourceDesc(Uint32 Index) const;
    const ResourceAttribs&      GetAttribs(Uint32 Index) const;


    struct ShaderVariableD3D11Base : ShaderVariableBase<ShaderVariableManagerD3D11, IShaderResourceVariableD3D>
    {
    public:
        ShaderVariableD3D11Base(ShaderVariableManagerD3D11& ParentLayout, Uint32 ResIndex) :
            ShaderVariableBase<ShaderVariableManagerD3D11, IShaderResourceVariableD3D>{ParentLayout},
            m_ResIndex{ResIndex}
        {}

        // clang-format off
        ShaderVariableD3D11Base            (const ShaderVariableD3D11Base&)  = delete;
        ShaderVariableD3D11Base            (      ShaderVariableD3D11Base&&) = delete;
        ShaderVariableD3D11Base& operator= (const ShaderVariableD3D11Base&)  = delete;
        ShaderVariableD3D11Base& operator= (      ShaderVariableD3D11Base&&) = delete;
        // clang-format on

        const PipelineResourceDesc& GetDesc() const { return m_ParentManager.GetResourceDesc(m_ResIndex); }
        const ResourceAttribs&      GetAttribs() const { return m_ParentManager.GetAttribs(m_ResIndex); }

        virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final
        {
            if (ppInterface == nullptr)
                return;

            *ppInterface = nullptr;
            if (IID == IID_ShaderResourceVariableD3D || IID == IID_ShaderResourceVariable || IID == IID_Unknown)
            {
                *ppInterface = this;
                (*ppInterface)->AddRef();
            }
        }

        virtual SHADER_RESOURCE_VARIABLE_TYPE DILIGENT_CALL_TYPE GetType() const override final
        {
            return GetDesc().VarType;
        }

        virtual void DILIGENT_CALL_TYPE GetResourceDesc(ShaderResourceDesc& ResourceDesc) const override final
        {
            const auto& Desc       = GetDesc();
            ResourceDesc.Name      = Desc.Name;
            ResourceDesc.Type      = Desc.ResourceType;
            ResourceDesc.ArraySize = Desc.ArraySize;
        }

        virtual Uint32 DILIGENT_CALL_TYPE GetIndex() const override final
        {
            return m_ParentManager.GetVariableIndex(*this);
        }

        virtual void DILIGENT_CALL_TYPE GetHLSLResourceDesc(HLSLShaderResourceDesc& HLSLResDesc) const override final
        {
            // AZ TODO
        }

    private:
        const Uint32 m_ResIndex;
    };

    struct ConstBuffBindInfo final : ShaderVariableD3D11Base
    {
        ConstBuffBindInfo(ShaderVariableManagerD3D11& ParentLayout, Uint32 ResIndex) :
            ShaderVariableD3D11Base{ParentLayout, ResIndex}
        {}

        // Non-virtual function
        __forceinline void BindResource(IDeviceObject* pObj, Uint32 ArrayIndex);

        virtual void DILIGENT_CALL_TYPE Set(IDeviceObject* pObject) override final
        {
            BindResource(pObject, 0);
        }

        virtual void DILIGENT_CALL_TYPE SetArray(IDeviceObject* const* ppObjects,
                                                 Uint32                FirstElement,
                                                 Uint32                NumElements) override final
        {
            const auto& Desc = GetDesc();
            VerifyAndCorrectSetArrayArguments(Desc.Name, Desc.ArraySize, FirstElement, NumElements);
            for (Uint32 elem = 0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement + elem);
        }

        virtual bool DILIGENT_CALL_TYPE IsBound(Uint32 ArrayIndex) const override final
        {
            VERIFY_EXPR(ArrayIndex < GetDesc().ArraySize);
            return m_ParentManager.m_ResourceCache.IsCBBound(GetAttribs().CacheOffset + ArrayIndex);
        }
    };

    struct TexSRVBindInfo final : ShaderVariableD3D11Base
    {
        TexSRVBindInfo(ShaderVariableManagerD3D11& ParentLayout, Uint32 ResIndex) :
            ShaderVariableD3D11Base{ParentLayout, ResIndex}
        {}

        // Non-virtual function
        __forceinline void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);

        virtual void DILIGENT_CALL_TYPE Set(IDeviceObject* pObject) override final
        {
            BindResource(pObject, 0);
        }

        virtual void DILIGENT_CALL_TYPE SetArray(IDeviceObject* const* ppObjects,
                                                 Uint32                FirstElement,
                                                 Uint32                NumElements) override final
        {
            const auto& Desc = GetDesc();
            VerifyAndCorrectSetArrayArguments(Desc.Name, Desc.ArraySize, FirstElement, NumElements);
            for (Uint32 elem = 0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement + elem);
        }

        virtual bool DILIGENT_CALL_TYPE IsBound(Uint32 ArrayIndex) const override final
        {
            VERIFY_EXPR(ArrayIndex < GetDesc().ArraySize);
            return m_ParentManager.m_ResourceCache.IsSRVBound(GetAttribs().CacheOffset + ArrayIndex, true);
        }
    };

    struct TexUAVBindInfo final : ShaderVariableD3D11Base
    {
        TexUAVBindInfo(ShaderVariableManagerD3D11& ParentLayout, Uint32 ResIndex) :
            ShaderVariableD3D11Base{ParentLayout, ResIndex}
        {}

        // Provide non-virtual function
        __forceinline void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);

        virtual void DILIGENT_CALL_TYPE Set(IDeviceObject* pObject) override final
        {
            BindResource(pObject, 0);
        }

        virtual void DILIGENT_CALL_TYPE SetArray(IDeviceObject* const* ppObjects,
                                                 Uint32                FirstElement,
                                                 Uint32                NumElements) override final
        {
            const auto& Desc = GetDesc();
            VerifyAndCorrectSetArrayArguments(Desc.Name, Desc.ArraySize, FirstElement, NumElements);
            for (Uint32 elem = 0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement + elem);
        }

        __forceinline virtual bool DILIGENT_CALL_TYPE IsBound(Uint32 ArrayIndex) const override final
        {
            VERIFY_EXPR(ArrayIndex < GetDesc().ArraySize);
            return m_ParentManager.m_ResourceCache.IsUAVBound(GetAttribs().CacheOffset + ArrayIndex, true);
        }
    };

    struct BuffUAVBindInfo final : ShaderVariableD3D11Base
    {
        BuffUAVBindInfo(ShaderVariableManagerD3D11& ParentLayout, Uint32 ResIndex) :
            ShaderVariableD3D11Base{ParentLayout, ResIndex}
        {}

        // Non-virtual function
        __forceinline void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);

        virtual void DILIGENT_CALL_TYPE Set(IDeviceObject* pObject) override final
        {
            BindResource(pObject, 0);
        }

        virtual void DILIGENT_CALL_TYPE SetArray(IDeviceObject* const* ppObjects,
                                                 Uint32                FirstElement,
                                                 Uint32                NumElements) override final
        {
            const auto& Desc = GetDesc();
            VerifyAndCorrectSetArrayArguments(Desc.Name, Desc.ArraySize, FirstElement, NumElements);
            for (Uint32 elem = 0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement + elem);
        }

        virtual bool DILIGENT_CALL_TYPE IsBound(Uint32 ArrayIndex) const override final
        {
            VERIFY_EXPR(ArrayIndex < GetDesc().ArraySize);
            return m_ParentManager.m_ResourceCache.IsUAVBound(GetAttribs().CacheOffset + ArrayIndex, false);
        }
    };

    struct BuffSRVBindInfo final : ShaderVariableD3D11Base
    {
        BuffSRVBindInfo(ShaderVariableManagerD3D11& ParentLayout, Uint32 ResIndex) :
            ShaderVariableD3D11Base{ParentLayout, ResIndex}
        {}

        // Non-virtual function
        __forceinline void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);

        virtual void DILIGENT_CALL_TYPE Set(IDeviceObject* pObject) override final
        {
            BindResource(pObject, 0);
        }

        virtual void DILIGENT_CALL_TYPE SetArray(IDeviceObject* const* ppObjects,
                                                 Uint32                FirstElement,
                                                 Uint32                NumElements) override final
        {
            const auto& Desc = GetDesc();
            VerifyAndCorrectSetArrayArguments(Desc.Name, Desc.ArraySize, FirstElement, NumElements);
            for (Uint32 elem = 0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement + elem);
        }

        virtual bool DILIGENT_CALL_TYPE IsBound(Uint32 ArrayIndex) const override final
        {
            VERIFY_EXPR(ArrayIndex < GetDesc().ArraySize);
            return m_ParentManager.m_ResourceCache.IsSRVBound(GetAttribs().CacheOffset + ArrayIndex, false);
        }
    };

    struct SamplerBindInfo final : ShaderVariableD3D11Base
    {
        SamplerBindInfo(ShaderVariableManagerD3D11& ParentLayout, Uint32 ResIndex) :
            ShaderVariableD3D11Base{ParentLayout, ResIndex}
        {}

        // Non-virtual function
        __forceinline void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);

        virtual void DILIGENT_CALL_TYPE Set(IDeviceObject* pObject) override final
        {
            BindResource(pObject, 0);
        }

        virtual void DILIGENT_CALL_TYPE SetArray(IDeviceObject* const* ppObjects,
                                                 Uint32                FirstElement,
                                                 Uint32                NumElements) override final
        {
            const auto& Desc = GetDesc();
            VerifyAndCorrectSetArrayArguments(Desc.Name, Desc.ArraySize, FirstElement, NumElements);
            for (Uint32 elem = 0; elem < NumElements; ++elem)
                BindResource(ppObjects[elem], FirstElement + elem);
        }

        virtual bool DILIGENT_CALL_TYPE IsBound(Uint32 ArrayIndex) const override final
        {
            VERIFY_EXPR(ArrayIndex < GetDesc().ArraySize);
            return m_ParentManager.m_ResourceCache.IsSamplerBound(GetAttribs().CacheOffset + ArrayIndex);
        }
    };

    void BindResources(IResourceMapping* pResourceMapping, Uint32 Flags);

#ifdef DILIGENT_DEVELOPMENT
    bool dvpVerifyBindings() const;
#endif

    IShaderResourceVariable* GetVariable(const Char* Name) const;
    IShaderResourceVariable* GetVariable(Uint32 Index) const;

    IObject& GetOwner() { return m_Owner; }

    Uint32 GetVariableCount() const;

    Uint32 GetVariableIndex(const ShaderVariableD3D11Base& Variable) const;

    // clang-format off
    Uint32 GetNumCBs()      const { return (m_TexSRVsOffset  - 0               ) / sizeof(ConstBuffBindInfo);}
    Uint32 GetNumTexSRVs()  const { return (m_TexUAVsOffset  - m_TexSRVsOffset ) / sizeof(TexSRVBindInfo);   }
    Uint32 GetNumTexUAVs()  const { return (m_BuffSRVsOffset - m_TexUAVsOffset ) / sizeof(TexUAVBindInfo) ;  }
    Uint32 GetNumBufSRVs()  const { return (m_BuffUAVsOffset - m_BuffSRVsOffset) / sizeof(BuffSRVBindInfo);  }
    Uint32 GetNumBufUAVs()  const { return (m_SamplerOffset  - m_BuffUAVsOffset) / sizeof(BuffUAVBindInfo);  }
    Uint32 GetNumSamplers() const { return (m_MemorySize     - m_SamplerOffset ) / sizeof(SamplerBindInfo);  }

    template<typename ResourceType> Uint32 GetNumResources()const;
    template<> Uint32 GetNumResources<ConstBuffBindInfo>() const { return GetNumCBs();      }
    template<> Uint32 GetNumResources<TexSRVBindInfo>   () const { return GetNumTexSRVs();  }
    template<> Uint32 GetNumResources<TexUAVBindInfo>   () const { return GetNumTexUAVs();  }
    template<> Uint32 GetNumResources<BuffSRVBindInfo>  () const { return GetNumBufSRVs();  }
    template<> Uint32 GetNumResources<BuffUAVBindInfo>  () const { return GetNumBufUAVs();  }
    template<> Uint32 GetNumResources<SamplerBindInfo>  () const { return GetNumSamplers(); }
    // clang-format on

private:
    static void CountResources(const PipelineResourceSignatureD3D11Impl& Signature,
                               const SHADER_RESOURCE_VARIABLE_TYPE*      AllowedVarTypes,
                               Uint32                                    NumAllowedTypes,
                               SHADER_TYPE                               ShaderType,
                               D3DShaderResourceCounters&                Counters);

    template <typename HandlerType>
    static void ProcessSignatureResources(const PipelineResourceSignatureD3D11Impl& Signature,
                                          const SHADER_RESOURCE_VARIABLE_TYPE*      AllowedVarTypes,
                                          Uint32                                    NumAllowedTypes,
                                          SHADER_TYPE                               ShaderType,
                                          HandlerType                               Handler);

    // clang-format off
    using OffsetType = Uint16;
    template<typename ResourceType> OffsetType GetResourceOffset()const;
    template<> OffsetType GetResourceOffset<ConstBuffBindInfo>() const { return 0;                }
    template<> OffsetType GetResourceOffset<TexSRVBindInfo>   () const { return m_TexSRVsOffset;  }
    template<> OffsetType GetResourceOffset<TexUAVBindInfo>   () const { return m_TexUAVsOffset;  }
    template<> OffsetType GetResourceOffset<BuffSRVBindInfo>  () const { return m_BuffSRVsOffset; }
    template<> OffsetType GetResourceOffset<BuffUAVBindInfo>  () const { return m_BuffUAVsOffset; }
    template<> OffsetType GetResourceOffset<SamplerBindInfo>  () const { return m_SamplerOffset;  }
    // clang-format on

    template <typename ResourceType>
    ResourceType& GetResource(Uint32 ResIndex) const
    {
        VERIFY(ResIndex < GetNumResources<ResourceType>(), "Resource index (", ResIndex, ") must be less than (", GetNumResources<ResourceType>(), ")");
        auto Offset = GetResourceOffset<ResourceType>();
        return reinterpret_cast<ResourceType*>(reinterpret_cast<Uint8*>(m_ResourceBuffer) + Offset)[ResIndex];
    }

    template <typename ResourceType>
    const ResourceType& GetConstResource(Uint32 ResIndex) const
    {
        VERIFY(ResIndex < GetNumResources<ResourceType>(), "Resource index (", ResIndex, ") must be less than (", GetNumResources<ResourceType>(), ")");
        auto Offset = GetResourceOffset<ResourceType>();
        return reinterpret_cast<const ResourceType*>(reinterpret_cast<const Uint8*>(m_ResourceBuffer) + Offset)[ResIndex];
    }

    template <typename ResourceType>
    IShaderResourceVariable* GetResourceByName(const Char* Name) const;

    template <typename THandleCB,
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
        for (Uint32 cb = 0; cb < GetNumResources<ConstBuffBindInfo>(); ++cb)
            HandleCB(GetResource<ConstBuffBindInfo>(cb));

        for (Uint32 t = 0; t < GetNumResources<TexSRVBindInfo>(); ++t)
            HandleTexSRV(GetResource<TexSRVBindInfo>(t));

        for (Uint32 u = 0; u < GetNumResources<TexUAVBindInfo>(); ++u)
            HandleTexUAV(GetResource<TexUAVBindInfo>(u));

        for (Uint32 s = 0; s < GetNumResources<BuffSRVBindInfo>(); ++s)
            HandleBufSRV(GetResource<BuffSRVBindInfo>(s));

        for (Uint32 u = 0; u < GetNumResources<BuffUAVBindInfo>(); ++u)
            HandleBufUAV(GetResource<BuffUAVBindInfo>(u));

        for (Uint32 s = 0; s < GetNumResources<SamplerBindInfo>(); ++s)
            HandleSampler(GetResource<SamplerBindInfo>(s));
    }

    template <typename THandleCB,
              typename THandleTexSRV,
              typename THandleTexUAV,
              typename THandleBufSRV,
              typename THandleBufUAV,
              typename THandleSampler>
    void HandleConstResources(THandleCB      HandleCB,
                              THandleTexSRV  HandleTexSRV,
                              THandleTexUAV  HandleTexUAV,
                              THandleBufSRV  HandleBufSRV,
                              THandleBufUAV  HandleBufUAV,
                              THandleSampler HandleSampler) const
    {
        for (Uint32 cb = 0; cb < GetNumResources<ConstBuffBindInfo>(); ++cb)
            HandleCB(GetConstResource<ConstBuffBindInfo>(cb));

        for (Uint32 t = 0; t < GetNumResources<TexSRVBindInfo>(); ++t)
            HandleTexSRV(GetConstResource<TexSRVBindInfo>(t));

        for (Uint32 u = 0; u < GetNumResources<TexUAVBindInfo>(); ++u)
            HandleTexUAV(GetConstResource<TexUAVBindInfo>(u));

        for (Uint32 s = 0; s < GetNumResources<BuffSRVBindInfo>(); ++s)
            HandleBufSRV(GetConstResource<BuffSRVBindInfo>(s));

        for (Uint32 u = 0; u < GetNumResources<BuffUAVBindInfo>(); ++u)
            HandleBufUAV(GetConstResource<BuffUAVBindInfo>(u));

        for (Uint32 s = 0; s < GetNumResources<SamplerBindInfo>(); ++s)
            HandleSampler(GetConstResource<SamplerBindInfo>(s));
    }

    friend class ShaderVariableIndexLocator;
    friend class ShaderVariableLocator;

private:
    PipelineResourceSignatureD3D11Impl const* m_pSignature = nullptr;

    IObject& m_Owner;

    // No need to use shared pointer, as the resource cache is either part of the same
    // ShaderD3D11Impl object, or ShaderResourceBindingD3D11Impl object
    ShaderResourceCacheD3D11& m_ResourceCache;
    void*                     m_ResourceBuffer = nullptr;

    // Offsets in bytes
    OffsetType m_TexSRVsOffset  = 0;
    OffsetType m_TexUAVsOffset  = 0;
    OffsetType m_BuffSRVsOffset = 0;
    OffsetType m_BuffUAVsOffset = 0;
    OffsetType m_SamplerOffset  = 0;
    OffsetType m_MemorySize     = 0;

#ifdef DILIGENT_DEBUG
    IMemoryAllocator* m_pDbgAllocator = nullptr;
#endif
};

} // namespace Diligent
