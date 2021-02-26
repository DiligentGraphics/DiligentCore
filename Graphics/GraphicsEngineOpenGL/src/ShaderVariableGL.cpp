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

#include "pch.h"
#include <array>
#include "ShaderVariableGL.hpp"
#include "PipelineResourceSignatureGLImpl.hpp"
#include "Align.hpp"
#include "PlatformMisc.hpp"
#include "ShaderBase.hpp"

namespace Diligent
{

void ShaderVariableGL::CountResources(const PipelineResourceSignatureGLImpl& Signature,
                                      const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                      Uint32                                 NumAllowedTypes,
                                      const SHADER_TYPE                      ShaderType,
                                      ResourceCounters&                      Counters)
{
    ProcessSignatureResources(
        Signature, AllowedVarTypes, NumAllowedTypes, ShaderType,
        [&](Uint32 Index) {
            const auto& ResDesc = Signature.GetResourceDesc(Index);
            static_assert(BINDING_RANGE_COUNT == 4, "Please update the switch below to handle the new shader resource range");
            switch (PipelineResourceToBindingRange(ResDesc))
            {
                // clang-format off
                case BINDING_RANGE_UNIFORM_BUFFER: ++Counters.NumUBs;           break;
                case BINDING_RANGE_TEXTURE:        ++Counters.NumTextures;      break;
                case BINDING_RANGE_IMAGE:          ++Counters.NumImages;        break;
                case BINDING_RANGE_STORAGE_BUFFER: ++Counters.NumStorageBlocks; break;
                    // clang-format on
                default:
                    UNEXPECTED("Unsupported resource type.");
            }
        });
}

template <typename HandlerType>
void ShaderVariableGL::ProcessSignatureResources(const PipelineResourceSignatureGLImpl& Signature,
                                                 const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                                 Uint32                                 NumAllowedTypes,
                                                 SHADER_TYPE                            ShaderType,
                                                 HandlerType                            Handler)
{
    const Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);

    for (Uint32 var_type = 0; var_type < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; ++var_type)
    {
        const auto VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(var_type);
        if (IsAllowedType(VarType, AllowedTypeBits))
        {
            const auto ResIdxRange = Signature.GetResourceIndexRange(VarType);
            for (Uint32 r = ResIdxRange.first; r < ResIdxRange.second; ++r)
            {
                const auto& Res = Signature.GetResourceDesc(r);
                VERIFY_EXPR(Res.VarType == VarType);

                if ((Res.ShaderStages & ShaderType) == 0)
                    continue;

                if (Res.ResourceType == SHADER_RESOURCE_TYPE_SAMPLER)
                    continue;

                Handler(r);
            }
        }
    }
}

size_t ShaderVariableGL::GetRequiredMemorySize(const PipelineResourceSignatureGLImpl& Signature,
                                               const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                               Uint32                                 NumAllowedTypes,
                                               SHADER_TYPE                            ShaderType)
{
    ResourceCounters Counters;
    CountResources(Signature, AllowedVarTypes, NumAllowedTypes, ShaderType, Counters);

    // clang-format off
    size_t RequiredSize = Counters.NumUBs           * sizeof(UniformBuffBindInfo)   + 
                          Counters.NumTextures      * sizeof(SamplerBindInfo)       +
                          Counters.NumImages        * sizeof(ImageBindInfo)         +
                          Counters.NumStorageBlocks * sizeof(StorageBufferBindInfo);
    // clang-format on
    return RequiredSize;
}

void ShaderVariableGL::Initialize(const PipelineResourceSignatureGLImpl& Signature,
                                  const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                  Uint32                                 NumAllowedTypes,
                                  SHADER_TYPE                            ShaderType)
{
    ResourceCounters Counters;
    CountResources(Signature, AllowedVarTypes, NumAllowedTypes, ShaderType, Counters);

    m_pSignature = &Signature;

    // Initialize offsets
    size_t CurrentOffset = 0;

    auto AdvanceOffset = [&CurrentOffset](size_t NumBytes) //
    {
        constexpr size_t MaxOffset = std::numeric_limits<OffsetType>::max();
        VERIFY(CurrentOffset <= MaxOffset, "Current offser (", CurrentOffset, ") exceeds max allowed value (", MaxOffset, ")");
        (void)MaxOffset;
        auto Offset = static_cast<OffsetType>(CurrentOffset);
        CurrentOffset += NumBytes;
        return Offset;
    };

    // clang-format off
    auto UBOffset         = AdvanceOffset(Counters.NumUBs           * sizeof(UniformBuffBindInfo)  ); (void)UBOffset; // To suppress warning
    m_TextureOffset       = AdvanceOffset(Counters.NumTextures      * sizeof(SamplerBindInfo)      );
    m_ImageOffset         = AdvanceOffset(Counters.NumImages        * sizeof(ImageBindInfo)        );
    m_StorageBufferOffset = AdvanceOffset(Counters.NumStorageBlocks * sizeof(StorageBufferBindInfo));
    m_VariableEndOffset   = AdvanceOffset(0);
    // clang-format off
    auto TotalMemorySize = m_VariableEndOffset;
    VERIFY_EXPR(TotalMemorySize == GetRequiredMemorySize(Signature, AllowedVarTypes, NumAllowedTypes, ShaderType));

    auto& ResLayoutDataAllocator = GetRawAllocator();
    if (TotalMemorySize)
    {
        auto* pRawMem = ALLOCATE_RAW(ResLayoutDataAllocator, "Raw memory buffer for shader resource layout resources", TotalMemorySize);
        m_ResourceBuffer = std::unique_ptr<void, STDDeleterRawMem<void> >(pRawMem, ResLayoutDataAllocator);
    }

    // clang-format off
    VERIFY_EXPR(Counters.NumUBs           == GetNumUBs()           );
    VERIFY_EXPR(Counters.NumTextures      == GetNumTextures()      );
    VERIFY_EXPR(Counters.NumImages        == GetNumImages()        );
    VERIFY_EXPR(Counters.NumStorageBlocks == GetNumStorageBuffers());
    // clang-format on

    // Current resource index for every resource type
    ResourceCounters VarCounters = {};

    ProcessSignatureResources(
        Signature, AllowedVarTypes, NumAllowedTypes, ShaderType,
        [&](Uint32 Index) {
            const auto& ResDesc = Signature.GetResourceDesc(Index);
            static_assert(BINDING_RANGE_COUNT == 4, "Please update the switch below to handle the new shader resource range");
            switch (PipelineResourceToBindingRange(ResDesc))
            {
                case BINDING_RANGE_UNIFORM_BUFFER:
                    new (&GetResource<UniformBuffBindInfo>(VarCounters.NumUBs++)) UniformBuffBindInfo{*this, Index};
                    break;
                case BINDING_RANGE_TEXTURE:
                    new (&GetResource<SamplerBindInfo>(VarCounters.NumTextures++)) SamplerBindInfo{*this, Index};
                    break;
                case BINDING_RANGE_IMAGE:
                    new (&GetResource<ImageBindInfo>(VarCounters.NumImages++)) ImageBindInfo{*this, Index};
                    break;
                case BINDING_RANGE_STORAGE_BUFFER:
                    new (&GetResource<StorageBufferBindInfo>(VarCounters.NumStorageBlocks++)) StorageBufferBindInfo{*this, Index};
                    break;
                default:
                    UNEXPECTED("Unsupported resource type.");
            }
        });

    // clang-format off
    VERIFY(VarCounters.NumUBs           == GetNumUBs(),             "Not all UBs are initialized which will cause a crash when dtor is called");
    VERIFY(VarCounters.NumTextures      == GetNumTextures(),        "Not all Samplers are initialized which will cause a crash when dtor is called");
    VERIFY(VarCounters.NumImages        == GetNumImages(),          "Not all Images are initialized which will cause a crash when dtor is called");
    VERIFY(VarCounters.NumStorageBlocks == GetNumStorageBuffers(),  "Not all SSBOs are initialized which will cause a crash when dtor is called");
    // clang-format on
}

ShaderVariableGL::~ShaderVariableGL()
{
    // clang-format off
    HandleResources(
        [&](UniformBuffBindInfo& ub)
        {
            ub.~UniformBuffBindInfo();
        },

        [&](SamplerBindInfo& sam)
        {
            sam.~SamplerBindInfo();
        },

        [&](ImageBindInfo& img)
        {
            img.~ImageBindInfo();
        },

        [&](StorageBufferBindInfo& ssbo)
        {
            ssbo.~StorageBufferBindInfo();
        }
    );
    // clang-format on
}

void ShaderVariableGL::UniformBuffBindInfo::BindResource(IDeviceObject* pBuffer,
                                                         Uint32         ArrayIndex)
{
    const auto& Desc = GetDesc();
    const auto& Attr = GetAttribs();

    DEV_CHECK_ERR(ArrayIndex < Desc.ArraySize, "Array index (", ArrayIndex, ") is out of range for variable '", Desc.Name, "'. Max allowed index: ", Desc.ArraySize - 1);
    auto& ResourceCache = m_ParentManager.m_ResourceCache;

    VERIFY_EXPR(Desc.ResourceType == SHADER_RESOURCE_TYPE_CONSTANT_BUFFER);

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<BufferGLImpl> pBuffGLImpl(pBuffer, IID_BufferGL);
#ifdef DILIGENT_DEVELOPMENT
    {
        const auto& CachedUB = ResourceCache.GetConstUB(Attr.CacheOffset + ArrayIndex);
        VerifyConstantBufferBinding(Desc.Name, Desc.ArraySize, Desc.VarType, Desc.Flags, ArrayIndex, pBuffer, pBuffGLImpl.RawPtr(), CachedUB.pBuffer.RawPtr());
    }
#endif

    ResourceCache.SetUniformBuffer(Attr.CacheOffset + ArrayIndex, std::move(pBuffGLImpl));
}



void ShaderVariableGL::SamplerBindInfo::BindResource(IDeviceObject* pView,
                                                     Uint32         ArrayIndex)
{
    const auto& Desc = GetDesc();
    const auto& Attr = GetAttribs();

    DEV_CHECK_ERR(ArrayIndex < Desc.ArraySize, "Array index (", ArrayIndex, ") is out of range for variable '", Desc.Name, "'. Max allowed index: ", Desc.ArraySize - 1);
    auto& ResourceCache = m_ParentManager.m_ResourceCache;

    VERIFY_EXPR(Desc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV ||
                Desc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV);

    if (Desc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV)
    {
        // We cannot use ValidatedCast<> here as the resource retrieved from the
        // resource mapping can be of wrong type
        RefCntAutoPtr<TextureViewGLImpl> pViewGL(pView, IID_TextureViewGL);
#ifdef DILIGENT_DEVELOPMENT
        {
            auto& CachedTexSampler = ResourceCache.GetConstTexture(Attr.CacheOffset + ArrayIndex);
            VerifyResourceViewBinding(Desc.Name, Desc.ArraySize, Desc.VarType, ArrayIndex,
                                      pView, pViewGL.RawPtr(), {TEXTURE_VIEW_SHADER_RESOURCE},
                                      RESOURCE_DIM_UNDEFINED, false, CachedTexSampler.pView.RawPtr());
            if (Attr.IsImmutableSamplerAssigned() && ResourceCache.StaticResourcesInitialized())
            {
                VERIFY(CachedTexSampler.pSampler != nullptr, "Immutable samplers must be initialized by PipelineStateGLImpl::InitializeSRBResourceCache!");
            }
        }
#endif
        ResourceCache.SetTexture(Attr.CacheOffset + ArrayIndex, std::move(pViewGL), !Attr.IsImmutableSamplerAssigned());
    }
    else if (Desc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV)
    {
        // We cannot use ValidatedCast<> here as the resource retrieved from the
        // resource mapping can be of wrong type
        RefCntAutoPtr<BufferViewGLImpl> pViewGL(pView, IID_BufferViewGL);
#ifdef DILIGENT_DEVELOPMENT
        {
            auto& CachedBuffSampler = ResourceCache.GetConstTexture(Attr.CacheOffset + ArrayIndex);
            VerifyResourceViewBinding(Desc.Name, Desc.ArraySize, Desc.VarType, ArrayIndex,
                                      pView, pViewGL.RawPtr(), {BUFFER_VIEW_SHADER_RESOURCE},
                                      RESOURCE_DIM_BUFFER, false, CachedBuffSampler.pView.RawPtr());
            if (pViewGL != nullptr)
            {
                const auto& ViewDesc = pViewGL->GetDesc();
                const auto& BuffDesc = pViewGL->GetBuffer()->GetDesc();
                if (!((BuffDesc.Mode == BUFFER_MODE_FORMATTED && ViewDesc.Format.ValueType != VT_UNDEFINED) || BuffDesc.Mode == BUFFER_MODE_RAW))
                {
                    LOG_ERROR_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                      Desc.Name, ": formatted buffer view is expected.");
                }
            }
        }
#endif
        VERIFY_EXPR((Desc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != 0);
        ResourceCache.SetTexelBuffer(Attr.CacheOffset + ArrayIndex, std::move(pViewGL));
    }
    else
    {
        UNEXPECTED("Unexpected resource type ", GetShaderResourceTypeLiteralName(Desc.ResourceType), ". Texture SRV or buffer SRV is expected.");
    }
}


void ShaderVariableGL::ImageBindInfo::BindResource(IDeviceObject* pView,
                                                   Uint32         ArrayIndex)
{
    const auto& Desc = GetDesc();
    const auto& Attr = GetAttribs();

    DEV_CHECK_ERR(ArrayIndex < Desc.ArraySize, "Array index (", ArrayIndex, ") is out of range for variable '", Desc.Name, "'. Max allowed index: ", Desc.ArraySize - 1);
    auto& ResourceCache = m_ParentManager.m_ResourceCache;

    if (Desc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_UAV)
    {
        // We cannot use ValidatedCast<> here as the resource retrieved from the
        // resource mapping can be of wrong type
        RefCntAutoPtr<TextureViewGLImpl> pViewGL(pView, IID_TextureViewGL);
#ifdef DILIGENT_DEVELOPMENT
        {
            auto& CachedUAV = ResourceCache.GetConstImage(Attr.CacheOffset + ArrayIndex);
            VerifyResourceViewBinding(Desc.Name, Desc.ArraySize, Desc.VarType, ArrayIndex,
                                      pView, pViewGL.RawPtr(), {TEXTURE_VIEW_UNORDERED_ACCESS},
                                      RESOURCE_DIM_UNDEFINED, false, CachedUAV.pView.RawPtr());
        }
#endif
        ResourceCache.SetTexImage(Attr.CacheOffset + ArrayIndex, std::move(pViewGL));
    }
    else if (Desc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV)
    {
        // We cannot use ValidatedCast<> here as the resource retrieved from the
        // resource mapping can be of wrong type
        RefCntAutoPtr<BufferViewGLImpl> pViewGL(pView, IID_BufferViewGL);
#ifdef DILIGENT_DEVELOPMENT
        {
            auto& CachedUAV = ResourceCache.GetConstImage(Attr.CacheOffset + ArrayIndex);
            VerifyResourceViewBinding(Desc.Name, Desc.ArraySize, Desc.VarType, ArrayIndex,
                                      pView, pViewGL.RawPtr(), {BUFFER_VIEW_UNORDERED_ACCESS},
                                      RESOURCE_DIM_BUFFER, false, CachedUAV.pView.RawPtr());
            if (pViewGL != nullptr)
            {
                const auto& ViewDesc = pViewGL->GetDesc();
                const auto& BuffDesc = pViewGL->GetBuffer()->GetDesc();
                if (!((BuffDesc.Mode == BUFFER_MODE_FORMATTED && ViewDesc.Format.ValueType != VT_UNDEFINED) || BuffDesc.Mode == BUFFER_MODE_RAW))
                {
                    LOG_ERROR_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                      Desc.Name, ": formatted buffer view is expected.");
                }
            }
        }
#endif
        VERIFY_EXPR((Desc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) != 0);
        ResourceCache.SetBufImage(Attr.CacheOffset + ArrayIndex, std::move(pViewGL));
    }
    else
    {
        UNEXPECTED("Unexpected resource type ", GetShaderResourceTypeLiteralName(Desc.ResourceType), ". Texture UAV or buffer UAV is expected.");
    }
}



void ShaderVariableGL::StorageBufferBindInfo::BindResource(IDeviceObject* pView,
                                                           Uint32         ArrayIndex)
{
    const auto& Desc = GetDesc();
    const auto& Attr = GetAttribs();

    DEV_CHECK_ERR(ArrayIndex < Desc.ArraySize, "Array index (", ArrayIndex, ") is out of range for variable '", Desc.Name, "'. Max allowed index: ", Desc.ArraySize - 1);
    auto& ResourceCache = m_ParentManager.m_ResourceCache;
    VERIFY_EXPR(Desc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV ||
                Desc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV);
    VERIFY_EXPR((Desc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER) == 0);

    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    RefCntAutoPtr<BufferViewGLImpl> pViewGL(pView, IID_BufferViewGL);
#ifdef DILIGENT_DEVELOPMENT
    {
        auto& CachedSSBO = ResourceCache.GetConstSSBO(Attr.CacheOffset + ArrayIndex);
        // HLSL structured buffers are mapped to SSBOs in GLSL
        VerifyResourceViewBinding(Desc.Name, Desc.ArraySize, Desc.VarType, ArrayIndex,
                                  pView, pViewGL.RawPtr(), {BUFFER_VIEW_SHADER_RESOURCE, BUFFER_VIEW_UNORDERED_ACCESS},
                                  RESOURCE_DIM_BUFFER, false, CachedSSBO.pBufferView.RawPtr());
        if (pViewGL != nullptr)
        {
            const auto& ViewDesc = pViewGL->GetDesc();
            const auto& BuffDesc = pViewGL->GetBuffer()->GetDesc();
            if (BuffDesc.Mode != BUFFER_MODE_STRUCTURED && BuffDesc.Mode != BUFFER_MODE_RAW)
            {
                LOG_ERROR_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                  Desc.Name, ": structured buffer view is expected.");
            }
        }
    }
#endif
    ResourceCache.SetSSBO(Attr.CacheOffset + ArrayIndex, std::move(pViewGL));
}



// Helper template class that facilitates binding CBs, SRVs, and UAVs
class BindResourceHelper
{
public:
    BindResourceHelper(IResourceMapping& RM, Uint32 Fl) :
        // clang-format off
        ResourceMapping {RM},
        Flags           {Fl}
    // clang-format on
    {
    }

    template <typename ResourceType>
    void Bind(ResourceType& Res)
    {
        if ((Flags & (1 << Res.GetType())) == 0)
            return;

        const auto& ResDesc = Res.GetDesc();

        for (Uint16 elem = 0; elem < ResDesc.ArraySize; ++elem)
        {
            if ((Flags & BIND_SHADER_RESOURCES_KEEP_EXISTING) && Res.IsBound(elem))
                continue;

            const auto*                  VarName = ResDesc.Name;
            RefCntAutoPtr<IDeviceObject> pRes;
            ResourceMapping.GetResource(VarName, &pRes, elem);
            if (pRes)
            {
                //  Call non-virtual function
                Res.BindResource(pRes, elem);
            }
            else
            {
                if ((Flags & BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED) && !Res.IsBound(elem))
                    LOG_ERROR_MESSAGE("Unable to bind resource to shader variable '", VarName, "': resource is not found in the resource mapping");
            }
        }
    }

private:
    IResourceMapping& ResourceMapping;
    const Uint32      Flags;
};


void ShaderVariableGL::BindResources(IResourceMapping* pResourceMapping, Uint32 Flags)
{
    if (pResourceMapping == nullptr)
    {
        LOG_ERROR_MESSAGE("Failed to bind resources: resource mapping is null");
        return;
    }

    if ((Flags & BIND_SHADER_RESOURCES_UPDATE_ALL) == 0)
        Flags |= BIND_SHADER_RESOURCES_UPDATE_ALL;

    BindResourceHelper BindResHelper(*pResourceMapping, Flags);

    // clang-format off
    HandleResources(
        [&](UniformBuffBindInfo& ub)
        {
            BindResHelper.Bind(ub);
        },

        [&](SamplerBindInfo& sam)
        {
            BindResHelper.Bind(sam);
        },

        [&](ImageBindInfo& img)
        {
            BindResHelper.Bind(img);
        },

        [&](StorageBufferBindInfo& ssbo)
        {
            BindResHelper.Bind(ssbo);
        }
    );
    // clang-format on
}


template <typename ResourceType>
IShaderResourceVariable* ShaderVariableGL::GetResourceByName(const Char* Name) const
{
    auto NumResources = GetNumResources<ResourceType>();
    for (Uint32 res = 0; res < NumResources; ++res)
    {
        auto&       Resource = GetResource<ResourceType>(res);
        const auto& ResDesc  = Resource.GetDesc();
        if (strcmp(ResDesc.Name, Name) == 0)
            return &Resource;
    }

    return nullptr;
}


IShaderResourceVariable* ShaderVariableGL::GetVariable(const Char* Name) const
{
    if (auto* pUB = GetResourceByName<UniformBuffBindInfo>(Name))
        return pUB;

    if (auto* pSampler = GetResourceByName<SamplerBindInfo>(Name))
        return pSampler;

    if (auto* pImage = GetResourceByName<ImageBindInfo>(Name))
        return pImage;

    if (auto* pSSBO = GetResourceByName<StorageBufferBindInfo>(Name))
        return pSSBO;

    return nullptr;
}

Uint32 ShaderVariableGL::GetVariableCount() const
{
    return GetNumUBs() + GetNumTextures() + GetNumImages() + GetNumStorageBuffers();
}

class ShaderVariableLocator
{
public:
    ShaderVariableLocator(const ShaderVariableGL& _Layout,
                          Uint32                  _Index) :
        // clang-format off
        Layout {_Layout},
        Index  {_Index}
    // clang-format on
    {
    }

    template <typename ResourceType>
    IShaderResourceVariable* TryResource(Uint32 NumResources)
    {
        if (Index < NumResources)
            return &Layout.GetResource<ResourceType>(Index);
        else
        {
            Index -= NumResources;
            return nullptr;
        }
    }

private:
    ShaderVariableGL const& Layout;
    Uint32                  Index;
};


IShaderResourceVariable* ShaderVariableGL::GetVariable(Uint32 Index) const
{
    ShaderVariableLocator VarLocator(*this, Index);

    if (auto* pUB = VarLocator.TryResource<UniformBuffBindInfo>(GetNumUBs()))
        return pUB;

    if (auto* pSampler = VarLocator.TryResource<SamplerBindInfo>(GetNumTextures()))
        return pSampler;

    if (auto* pImage = VarLocator.TryResource<ImageBindInfo>(GetNumImages()))
        return pImage;

    if (auto* pSSBO = VarLocator.TryResource<StorageBufferBindInfo>(GetNumStorageBuffers()))
        return pSSBO;

    LOG_ERROR(Index, " is not a valid variable index.");
    return nullptr;
}



class ShaderVariableIndexLocator
{
public:
    ShaderVariableIndexLocator(const ShaderVariableGL& _Layout, const ShaderVariableGL::GLVariableBase& Variable) :
        // clang-format off
        Layout   {_Layout},
        VarOffset(reinterpret_cast<const Uint8*>(&Variable) - reinterpret_cast<const Uint8*>(_Layout.m_ResourceBuffer.get()))
    // clang-format on
    {}

    template <typename ResourceType>
    bool TryResource(Uint32 NextResourceTypeOffset, Uint32 VarCount)
    {
        if (VarOffset < NextResourceTypeOffset)
        {
            auto RelativeOffset = VarOffset - Layout.GetResourceOffset<ResourceType>();
            DEV_CHECK_ERR(RelativeOffset % sizeof(ResourceType) == 0, "Offset is not multiple of resource type (", sizeof(ResourceType), ")");
            RelativeOffset /= sizeof(ResourceType);
            VERIFY(RelativeOffset >= 0 && RelativeOffset < VarCount,
                   "Relative offset is out of bounds which either means the variable does not belong to this SRB or "
                   "there is a bug in varaible offsets");
            Index += static_cast<Uint32>(RelativeOffset);
            return true;
        }
        else
        {
            Index += VarCount;
            return false;
        }
    }

    Uint32 GetIndex() const { return Index; }

private:
    const ShaderVariableGL& Layout;
    const size_t            VarOffset;
    Uint32                  Index = 0;
};


Uint32 ShaderVariableGL::GetVariableIndex(const GLVariableBase& Var) const
{
    if (!m_ResourceBuffer)
    {
        LOG_ERROR("This shader resource layout does not have resources");
        return static_cast<Uint32>(-1);
    }

    ShaderVariableIndexLocator IdxLocator(*this, Var);

    if (IdxLocator.TryResource<UniformBuffBindInfo>(m_TextureOffset, GetNumUBs()))
        return IdxLocator.GetIndex();

    if (IdxLocator.TryResource<SamplerBindInfo>(m_ImageOffset, GetNumTextures()))
        return IdxLocator.GetIndex();

    if (IdxLocator.TryResource<ImageBindInfo>(m_StorageBufferOffset, GetNumImages()))
        return IdxLocator.GetIndex();

    if (IdxLocator.TryResource<StorageBufferBindInfo>(m_VariableEndOffset, GetNumStorageBuffers()))
        return IdxLocator.GetIndex();

    LOG_ERROR("Failed to get variable index. The variable ", &Var, " does not belong to this shader resource layout");
    return ~0u;
}

#ifdef DILIGENT_DEVELOPMENT
bool ShaderVariableGL::dvpVerifyBindings(const ShaderResourceCacheGL& ResourceCache) const
{
#    define LOG_MISSING_BINDING(VarType, BindInfo, ArrIndex) LOG_ERROR_MESSAGE("No resource is bound to ", VarType, " variable '", GetShaderResourcePrintName(Desc, ArrIndex), "'")

    bool BindingsOK = true;
    HandleConstResources(
        [&](const UniformBuffBindInfo& ub) //
        {
            const auto& Desc = GetResourceDesc(ub.m_ResIndex);
            const auto& Attr = GetAttribs(ub.m_ResIndex);
            for (Uint32 ArrInd = 0; ArrInd < Desc.ArraySize; ++ArrInd)
            {
                if (!ResourceCache.IsUBBound(Attr.CacheOffset + ArrInd))
                {
                    LOG_MISSING_BINDING("constant buffer", ub, ArrInd);
                    BindingsOK = false;
                }
            }
        },

        [&](const SamplerBindInfo& sam) //
        {
            const auto& Desc = GetResourceDesc(sam.m_ResIndex);
            const auto& Attr = GetAttribs(sam.m_ResIndex);
            for (Uint32 ArrInd = 0; ArrInd < Desc.ArraySize; ++ArrInd)
            {
                VERIFY_EXPR(Desc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV ||
                            Desc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_SRV);
                if (!ResourceCache.IsTextureBound(Attr.CacheOffset + ArrInd, Desc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV))
                {
                    LOG_MISSING_BINDING("texture", sam, ArrInd);
                    BindingsOK = false;
                }
                else
                {
                    const auto& CachedSampler = ResourceCache.GetConstTexture(Attr.CacheOffset + ArrInd);
                    if (Attr.IsImmutableSamplerAssigned() && CachedSampler.pSampler == nullptr)
                    {
                        LOG_ERROR_MESSAGE("Immutable sampler is not initialized for texture '", Desc.Name, "'");
                        BindingsOK = false;
                    }
                }
            }
        },

        [&](const ImageBindInfo& img) //
        {
            const auto& Desc = GetResourceDesc(img.m_ResIndex);
            const auto& Attr = GetAttribs(img.m_ResIndex);
            for (Uint32 ArrInd = 0; ArrInd < Desc.ArraySize; ++ArrInd)
            {
                VERIFY_EXPR(Desc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_UAV ||
                            Desc.ResourceType == SHADER_RESOURCE_TYPE_BUFFER_UAV);
                if (!ResourceCache.IsImageBound(Attr.CacheOffset + ArrInd, Desc.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_UAV))
                {
                    LOG_MISSING_BINDING("texture UAV", img, ArrInd);
                    BindingsOK = false;
                }
            }
        },

        [&](const StorageBufferBindInfo& ssbo) //
        {
            const auto& Desc = GetResourceDesc(ssbo.m_ResIndex);
            const auto& Attr = GetAttribs(ssbo.m_ResIndex);
            for (Uint32 ArrInd = 0; ArrInd < Desc.ArraySize; ++ArrInd)
            {
                if (!ResourceCache.IsSSBOBound(Attr.CacheOffset + ArrInd))
                {
                    LOG_MISSING_BINDING("buffer", ssbo, ArrInd);
                    BindingsOK = false;
                }
            }
        });
#    undef LOG_MISSING_BINDING

    return BindingsOK;
}

#endif // DILIGENT_DEVELOPMENT

} // namespace Diligent
