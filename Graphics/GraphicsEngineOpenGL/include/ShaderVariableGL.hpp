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

// ShaderVariableGL class manages resource bindings for all stages in a pipeline

//
//
//                                                      To            program              resource                  cache
//
//                                               A          A                  A        A              A           A              A            A
//                                               |          |                  |        |              |           |              |            |
//                                            Binding    Binding            Binding   Binding       Binding     Binding        Binding      Binding
//      ___________________                  ____|__________|__________________|________|______________|___________|______________|____________|____________
//     |                   |                |          |          |       |        |        |       |        |        |       |          |          |       |
//     | ShaderResourcesGL |--------------->|   UB[0]  |   UB[1]  |  ...  | Sam[0] | Sam[1] |  ...  | Img[0] | Img[1] |  ...  | SSBOs[0] | SSBOs[1] |  ...  |
//     |___________________|                |__________|__________|_______|________|________|_______|________|________|_______|__________|__________|_______|
//                                                A                                    A                        A                            A
//                                                |                                    |                        |                            |
//                                               Ref                                  Ref                      Ref                          Ref
//    .-==========================-.         _____|____________________________________|________________________|____________________________|______________
//    ||                          ||        |           |           |       |            |            |       |            |         |           |          |
//  __||     ShaderVariableGL     ||------->| UBInfo[0] | UBInfo[1] |  ...  | SamInfo[0] | SamInfo[1] |  ...  | ImgInfo[0] |   ...   |  SSBO[0]  |   ...    |
// |  ||                          ||        |___________|___________|_______|____________|____________|_______|____________|_________|___________|__________|
// |  '-==========================-'                     /                                         \
// |                                                   Ref                                         Ref
// |                                                  /                                              \
// |    ___________________                  ________V________________________________________________V_____________________________________________________
// |   |                   |                |          |          |       |        |        |       |        |        |       |          |          |       |
// |   | ShaderResourcesGL |--------------->|   UB[0]  |   UB[1]  |  ...  | Sam[0] | Sam[1] |  ...  | Img[0] | Img[1] |  ...  | SSBOs[0] | SSBOs[1] |  ...  |
// |   |___________________|                |__________|__________|_______|________|________|_______|________|________|_______|__________|__________|_______|
// |                                             |           |                |         |                |        |                |           |
// |                                          Binding     Binding          Binding    Binding          Binding  Binding         Binding      Binding
// |                                             |           |                |         |                |        |                |           |
// |    _______________________              ____V___________V________________V_________V________________V________V________________V___________V_____________
// |   |                       |            |                           |                           |                           |                           |
// '-->| ShaderResourceCacheGL |----------->|      Uinform Buffers      |          Textures         |          Images           |       Storge Buffers      |
//     |_______________________|            |___________________________|___________________________|___________________________|___________________________|
//
//
// Note that ShaderResourcesGL are kept by PipelineStateGLImpl. ShaderVariableGL class is either part of the same PSO class,
// or part of ShaderResourceBindingGLImpl object that keeps a strong reference to the pipeline. So all references from GLVariableBase
// are always valid.

#include <array>

#include "Object.h"
#include "ShaderResourceVariableBase.hpp"
#include "ShaderResourceCacheGL.hpp"
#include "PipelineResourceSignatureGLImpl.hpp"

namespace Diligent
{

// sizeof(ShaderVariableGL) == 48 (x64, msvc, Release)
class ShaderVariableGL
{
public:
    ShaderVariableGL(IObject& Owner, ShaderResourceCacheGL& ResourceCache) noexcept :
        m_Owner(Owner),
        m_ResourceCache{ResourceCache}
    {}

    ~ShaderVariableGL();

    // No copies, only moves are allowed
    // clang-format off
    ShaderVariableGL             (const ShaderVariableGL&)  = delete;
    ShaderVariableGL& operator = (const ShaderVariableGL&)  = delete;
    ShaderVariableGL             (      ShaderVariableGL&&) = default;
    ShaderVariableGL& operator = (      ShaderVariableGL&&) = delete;
    // clang-format on

    void Initialize(const PipelineResourceSignatureGLImpl& Signature,
                    const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                    Uint32                                 NumAllowedTypes,
                    SHADER_TYPE                            ShaderType);

    static size_t GetRequiredMemorySize(const PipelineResourceSignatureGLImpl& Signature,
                                        const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                        Uint32                                 NumAllowedTypes,
                                        SHADER_TYPE                            ShaderType);

    using ResourceAttribs = PipelineResourceSignatureGLImpl::ResourceAttribs;

    const PipelineResourceDesc& GetResourceDesc(Uint32 Index) const
    {
        VERIFY_EXPR(m_pSignature);
        return m_pSignature->GetResourceDesc(Index);
    }
    const ResourceAttribs& GetAttribs(Uint32 Index) const
    {
        VERIFY_EXPR(m_pSignature);
        return m_pSignature->GetResourceAttribs(Index);
    }


    struct GLVariableBase : public ShaderVariableBase<ShaderVariableGL>
    {
        using TBase = ShaderVariableBase<ShaderVariableGL>;
        GLVariableBase(ShaderVariableGL& ParentLayout, Uint32 ResIndex) :
            TBase{ParentLayout},
            m_ResIndex{ResIndex}
        {}

        const PipelineResourceDesc& GetDesc() const { return m_ParentManager.GetResourceDesc(m_ResIndex); }
        const ResourceAttribs&      GetAttribs() const { return m_ParentManager.GetAttribs(m_ResIndex); }

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

        const Uint32 m_ResIndex;
    };


    struct UniformBuffBindInfo final : GLVariableBase
    {
        UniformBuffBindInfo(ShaderVariableGL& ParentLayout, Uint32 ResIndex) :
            GLVariableBase{ParentLayout, ResIndex}
        {}

        // Non-virtual function
        void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);

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
            return m_ParentManager.m_ResourceCache.IsUBBound(GetAttribs().CacheOffset + ArrayIndex);
        }
    };


    struct SamplerBindInfo final : GLVariableBase
    {
        SamplerBindInfo(ShaderVariableGL& ParentLayout, Uint32 ResIndex) :
            GLVariableBase{ParentLayout, ResIndex}
        {}

        // Non-virtual function
        void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);

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
            return m_ParentManager.m_ResourceCache.IsTextureBound(GetAttribs().CacheOffset + ArrayIndex, GetDesc().ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV);
        }
    };


    struct ImageBindInfo final : GLVariableBase
    {
        ImageBindInfo(ShaderVariableGL& ParentLayout, Uint32 ResIndex) :
            GLVariableBase{ParentLayout, ResIndex}
        {}

        // Provide non-virtual function
        void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);

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
            return m_ParentManager.m_ResourceCache.IsImageBound(GetAttribs().CacheOffset + ArrayIndex, GetDesc().ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_UAV);
        }
    };


    struct StorageBufferBindInfo final : GLVariableBase
    {
        StorageBufferBindInfo(ShaderVariableGL& ParentLayout, Uint32 ResIndex) :
            GLVariableBase{ParentLayout, ResIndex}
        {}

        // Non-virtual function
        void BindResource(IDeviceObject* pObject, Uint32 ArrayIndex);

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
            return m_ParentManager.m_ResourceCache.IsSSBOBound(GetAttribs().CacheOffset + ArrayIndex);
        }
    };


    // dbgResourceCache is only used for sanity check and as a remainder that the resource cache must be alive
    // while Layout is alive
    void BindResources(IResourceMapping* pResourceMapping, Uint32 Flags);

#ifdef DILIGENT_DEVELOPMENT
    bool dvpVerifyBindings(const ShaderResourceCacheGL& ResourceCache) const;
#endif

    IShaderResourceVariable* GetVariable(const Char* Name) const;
    IShaderResourceVariable* GetVariable(Uint32 Index) const;

    IObject& GetOwner() { return m_Owner; }

    Uint32 GetVariableCount() const;

    // clang-format off
    Uint32 GetNumUBs()            const { return (m_TextureOffset       - m_UBOffset)            / sizeof(UniformBuffBindInfo);    }
    Uint32 GetNumTextures()       const { return (m_ImageOffset         - m_TextureOffset)       / sizeof(SamplerBindInfo);        }
    Uint32 GetNumImages()         const { return (m_StorageBufferOffset - m_ImageOffset)         / sizeof(ImageBindInfo) ;         }
    Uint32 GetNumStorageBuffers() const { return (m_VariableEndOffset   - m_StorageBufferOffset) / sizeof(StorageBufferBindInfo);  }
    // clang-format on

    template <typename ResourceType> Uint32 GetNumResources() const;

    template <typename ResourceType>
    const ResourceType& GetConstResource(Uint32 ResIndex) const
    {
        VERIFY(ResIndex < GetNumResources<ResourceType>(), "Resource index (", ResIndex, ") exceeds max allowed value (", GetNumResources<ResourceType>(), ")");
        auto Offset = GetResourceOffset<ResourceType>();
        return reinterpret_cast<const ResourceType*>(reinterpret_cast<const Uint8*>(m_ResourceBuffer.get()) + Offset)[ResIndex];
    }

    Uint32 GetVariableIndex(const GLVariableBase& Var) const;

private:
    struct ResourceCounters
    {
        Uint32 NumUBs           = 0;
        Uint32 NumTextures      = 0;
        Uint32 NumImages        = 0;
        Uint32 NumStorageBlocks = 0;
    };
    static void CountResources(const PipelineResourceSignatureGLImpl& Signature,
                               const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                               Uint32                                 NumAllowedTypes,
                               SHADER_TYPE                            ShaderType,
                               ResourceCounters&                      Counters);

    template <typename HandlerType>
    static void ProcessSignatureResources(const PipelineResourceSignatureGLImpl& Signature,
                                          const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                          Uint32                                 NumAllowedTypes,
                                          SHADER_TYPE                            ShaderType,
                                          HandlerType                            Handler);

    // Offsets in bytes
    using OffsetType = Uint16;

    template <typename ResourceType> OffsetType GetResourceOffset() const;

    template <typename ResourceType>
    ResourceType& GetResource(Uint32 ResIndex) const
    {
        VERIFY(ResIndex < GetNumResources<ResourceType>(), "Resource index (", ResIndex, ") exceeds max allowed value (", GetNumResources<ResourceType>() - 1, ")");
        auto Offset = GetResourceOffset<ResourceType>();
        return reinterpret_cast<ResourceType*>(reinterpret_cast<Uint8*>(m_ResourceBuffer.get()) + Offset)[ResIndex];
    }

    template <typename ResourceType>
    IShaderResourceVariable* GetResourceByName(const Char* Name) const;

    template <typename THandleUB,
              typename THandleSampler,
              typename THandleImage,
              typename THandleStorageBuffer>
    void HandleResources(THandleUB            HandleUB,
                         THandleSampler       HandleSampler,
                         THandleImage         HandleImage,
                         THandleStorageBuffer HandleStorageBuffer)
    {
        for (Uint32 ub = 0; ub < GetNumResources<UniformBuffBindInfo>(); ++ub)
            HandleUB(GetResource<UniformBuffBindInfo>(ub));

        for (Uint32 s = 0; s < GetNumResources<SamplerBindInfo>(); ++s)
            HandleSampler(GetResource<SamplerBindInfo>(s));

        for (Uint32 i = 0; i < GetNumResources<ImageBindInfo>(); ++i)
            HandleImage(GetResource<ImageBindInfo>(i));

        for (Uint32 s = 0; s < GetNumResources<StorageBufferBindInfo>(); ++s)
            HandleStorageBuffer(GetResource<StorageBufferBindInfo>(s));
    }

    template <typename THandleUB,
              typename THandleSampler,
              typename THandleImage,
              typename THandleStorageBuffer>
    void HandleConstResources(THandleUB            HandleUB,
                              THandleSampler       HandleSampler,
                              THandleImage         HandleImage,
                              THandleStorageBuffer HandleStorageBuffer) const
    {
        for (Uint32 ub = 0; ub < GetNumResources<UniformBuffBindInfo>(); ++ub)
            HandleUB(GetConstResource<UniformBuffBindInfo>(ub));

        for (Uint32 s = 0; s < GetNumResources<SamplerBindInfo>(); ++s)
            HandleSampler(GetConstResource<SamplerBindInfo>(s));

        for (Uint32 i = 0; i < GetNumResources<ImageBindInfo>(); ++i)
            HandleImage(GetConstResource<ImageBindInfo>(i));

        for (Uint32 s = 0; s < GetNumResources<StorageBufferBindInfo>(); ++s)
            HandleStorageBuffer(GetConstResource<StorageBufferBindInfo>(s));
    }

    friend class ShaderVariableIndexLocator;
    friend class ShaderVariableLocator;

private:
    PipelineResourceSignatureGLImpl const* m_pSignature = nullptr;

    IObject& m_Owner;
    // No need to use shared pointer, as the resource cache is either part of the same
    // ShaderGLImpl object, or ShaderResourceBindingGLImpl object
    ShaderResourceCacheGL&                        m_ResourceCache;
    std::unique_ptr<void, STDDeleterRawMem<void>> m_ResourceBuffer;

    static constexpr OffsetType m_UBOffset            = 0;
    OffsetType                  m_TextureOffset       = 0;
    OffsetType                  m_ImageOffset         = 0;
    OffsetType                  m_StorageBufferOffset = 0;
    OffsetType                  m_VariableEndOffset   = 0;
};


template <>
inline Uint32 ShaderVariableGL::GetNumResources<ShaderVariableGL::UniformBuffBindInfo>() const
{
    return GetNumUBs();
}

template <>
inline Uint32 ShaderVariableGL::GetNumResources<ShaderVariableGL::SamplerBindInfo>() const
{
    return GetNumTextures();
}

template <>
inline Uint32 ShaderVariableGL::GetNumResources<ShaderVariableGL::ImageBindInfo>() const
{
    return GetNumImages();
}

template <>
inline Uint32 ShaderVariableGL::GetNumResources<ShaderVariableGL::StorageBufferBindInfo>() const
{
    return GetNumStorageBuffers();
}



template <>
inline ShaderVariableGL::OffsetType ShaderVariableGL::
    GetResourceOffset<ShaderVariableGL::UniformBuffBindInfo>() const
{
    return m_UBOffset;
}

template <>
inline ShaderVariableGL::OffsetType ShaderVariableGL::
    GetResourceOffset<ShaderVariableGL::SamplerBindInfo>() const
{
    return m_TextureOffset;
}

template <>
inline ShaderVariableGL::OffsetType ShaderVariableGL::
    GetResourceOffset<ShaderVariableGL::ImageBindInfo>() const
{
    return m_ImageOffset;
}

template <>
inline ShaderVariableGL::OffsetType ShaderVariableGL::
    GetResourceOffset<ShaderVariableGL::StorageBufferBindInfo>() const
{
    return m_StorageBufferOffset;
}

} // namespace Diligent
