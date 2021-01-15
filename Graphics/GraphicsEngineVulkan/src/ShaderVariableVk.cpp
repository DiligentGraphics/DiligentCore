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

#include "ShaderVariableVk.hpp"
#include "ShaderResourceVariableBase.hpp"
#include "PipelineResourceSignatureVkImpl.hpp"
#include "SamplerVkImpl.hpp"
#include "TextureViewVkImpl.hpp"
#include "TopLevelASVkImpl.hpp"

namespace Diligent
{

size_t ShaderVariableManagerVk::GetRequiredMemorySize(const PipelineResourceSignatureVkImpl& Layout,
                                                      const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                                      Uint32                                 NumAllowedTypes,
                                                      Uint32&                                NumVariables)
{
    NumVariables                       = 0;
    const Uint32 AllowedTypeBits       = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    const bool   UsingSeparateSamplers = true; //Layout.IsUsingSeparateSamplers();
    const auto&  Desc                  = Layout.GetDesc();

    for (Uint32 r = 0; r < Desc.NumResources; ++r)
    {
        const auto& SrcRes = Desc.Resources[r];

        if (!IsAllowedType(SrcRes.VarType, AllowedTypeBits))
            continue;

        // AZ TODO
        /*
        // When using HLSL-style combined image samplers, we need to skip separate samplers.
        // Also always skip immutable separate samplers.
        if (SrcRes.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler && // AZ TODO
            (!UsingSeparateSamplers || SrcRes.IsImmutableSamplerAssigned()))
            continue;
        */
        ++NumVariables;
    }

    return NumVariables * sizeof(ShaderVariableVkImpl);
}

// Creates shader variable for every resource from SrcLayout whose type is one AllowedVarTypes
void ShaderVariableManagerVk::Initialize(const PipelineResourceSignatureVkImpl& SrcLayout,
                                         IMemoryAllocator&                      Allocator,
                                         const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes,
                                         Uint32                                 NumAllowedTypes)
{
#ifdef DILIGENT_DEBUG
    m_pDbgAllocator = &Allocator;
#endif

    VERIFY_EXPR(m_pSignature == nullptr);

    const Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    VERIFY_EXPR(m_NumVariables == 0);
    auto MemSize = GetRequiredMemorySize(SrcLayout, AllowedVarTypes, NumAllowedTypes, m_NumVariables);

    if (m_NumVariables == 0)
        return;

    auto* pRawMem = ALLOCATE_RAW(Allocator, "Raw memory buffer for shader variables", MemSize);
    m_pVariables  = reinterpret_cast<ShaderVariableVkImpl*>(pRawMem);

    Uint32      VarInd                = 0;
    const bool  UsingSeparateSamplers = false; //SrcLayout.IsUsingSeparateSamplers();
    const auto& Desc                  = SrcLayout.GetDesc();

    for (Uint32 r = 0; r < Desc.NumResources; ++r)
    {
        const auto& SrcRes = Desc.Resources[r];

        if (!IsAllowedType(SrcRes.VarType, AllowedTypeBits))
            continue;

        // AZ TODO
        /*
        // Skip separate samplers when using combined HLSL-style image samplers. Also always skip immutable separate samplers.
        if (SrcRes.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler &&
            (!UsingSeparateSamplers || SrcRes.IsImmutableSamplerAssigned()))
            continue;
        */
        ::new (m_pVariables + VarInd) ShaderVariableVkImpl(*this);
        ++VarInd;
    }
    VERIFY_EXPR(VarInd == m_NumVariables);

    m_pSignature = &SrcLayout;
}

ShaderVariableManagerVk::~ShaderVariableManagerVk()
{
    VERIFY(m_pVariables == nullptr, "DestroyVariables() has not been called");
}

void ShaderVariableManagerVk::DestroyVariables(IMemoryAllocator& Allocator)
{
    if (m_pVariables != nullptr)
    {
        VERIFY(m_pDbgAllocator == &Allocator, "Incosistent alloctor");

        for (Uint32 v = 0; v < m_NumVariables; ++v)
            m_pVariables[v].~ShaderVariableVkImpl();
        Allocator.Free(m_pVariables);
        m_pVariables = nullptr;
    }
}

ShaderVariableVkImpl* ShaderVariableManagerVk::GetVariable(const Char* Name) const
{
    ShaderVariableVkImpl* pVar = nullptr;
    for (Uint32 v = 0; v < m_NumVariables; ++v)
    {
        auto&       Var = m_pVariables[v];
        const auto& Res = Var.GetDesc();
        if (strcmp(Res.Name, Name) == 0)
        {
            pVar = &Var;
            break;
        }
    }
    return pVar;
}


ShaderVariableVkImpl* ShaderVariableManagerVk::GetVariable(Uint32 Index) const
{
    if (Index >= m_NumVariables)
    {
        LOG_ERROR("Index ", Index, " is out of range");
        return nullptr;
    }

    return m_pVariables + Index;
}

Uint32 ShaderVariableManagerVk::GetVariableIndex(const ShaderVariableVkImpl& Variable)
{
    if (m_pVariables == nullptr)
    {
        LOG_ERROR("This shader variable manager has no variables");
        return static_cast<Uint32>(-1);
    }

    auto Offset = reinterpret_cast<const Uint8*>(&Variable) - reinterpret_cast<Uint8*>(m_pVariables);
    VERIFY(Offset % sizeof(ShaderVariableVkImpl) == 0, "Offset is not multiple of ShaderVariableVkImpl class size");
    auto Index = static_cast<Uint32>(Offset / sizeof(ShaderVariableVkImpl));
    if (Index < m_NumVariables)
        return Index;
    else
    {
        LOG_ERROR("Failed to get variable index. The variable ", &Variable, " does not belong to this shader variable manager");
        return static_cast<Uint32>(-1);
    }
}

void ShaderVariableManagerVk::BindResources(IResourceMapping* pResourceMapping, Uint32 Flags) const
{
    if (!pResourceMapping)
    {
        LOG_ERROR_MESSAGE("Failed to bind resources: resource mapping is null");
        return;
    }

    if ((Flags & BIND_SHADER_RESOURCES_UPDATE_ALL) == 0)
        Flags |= BIND_SHADER_RESOURCES_UPDATE_ALL;
    /*
    for (Uint32 v = 0; v < m_NumVariables; ++v)
    {
        auto&       Var = m_pVariables[v];
        const auto& Res = Var.m_Resource;

        // There should be no immutable separate samplers
        VERIFY(Res.Type != SPIRVShaderResourceAttribs::ResourceType::SeparateSampler || !Res.IsImmutableSamplerAssigned(),
               "There must be no shader resource variables for immutable separate samplers");

        if ((Flags & (1 << Res.GetVariableType())) == 0)
            continue;

        for (Uint32 ArrInd = 0; ArrInd < Res.ArraySize; ++ArrInd)
        {
            if ((Flags & BIND_SHADER_RESOURCES_KEEP_EXISTING) && Res.IsBound(ArrInd, m_ResourceCache))
                continue;

            const auto*                  VarName = Res.Name;
            RefCntAutoPtr<IDeviceObject> pObj;
            pResourceMapping->GetResource(VarName, &pObj, ArrInd);
            if (pObj)
            {
                Res.BindResource(pObj, ArrInd, m_ResourceCache);
            }
            else
            {
                if ((Flags & BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED) && !Res.IsBound(ArrInd, m_ResourceCache))
                {
                    LOG_ERROR_MESSAGE("Unable to bind resource to shader variable '", Res.GetPrintName(ArrInd),
                                      "': resource is not found in the resource mapping. "
                                      "Do not use BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED flag to suppress the message if this is not an issue.");
                }
            }
        }
    }*/
}

void ShaderVariableVkImpl::Set(IDeviceObject* pObject)
{
    BindResource(pObject, 0);
}

void ShaderVariableVkImpl::SetArray(IDeviceObject* const* ppObjects,
                                    Uint32                FirstElement,
                                    Uint32                NumElements)
{
    const auto& ResDesc = GetDesc();
    VerifyAndCorrectSetArrayArguments(ResDesc.Name, ResDesc.ArraySize, FirstElement, NumElements);

    for (Uint32 Elem = 0; Elem < NumElements; ++Elem)
        BindResource(ppObjects[Elem], FirstElement + Elem);
}

bool ShaderVariableVkImpl::IsBound(Uint32 ArrayIndex) const
{
    auto&       ResourceCache = m_ParentManager.m_ResourceCache;
    const auto& ResDesc       = GetDesc();
    const auto& Binding       = GetBinding();
    Uint32      CacheOffset   = GetIndex();

    VERIFY_EXPR(ArrayIndex < ResDesc.ArraySize);

    if (Binding.DescSet < ResourceCache.GetNumDescriptorSets())
    {
        const auto& Set = ResourceCache.GetDescriptorSet(Binding.DescSet);
        if (CacheOffset + ArrayIndex < Set.GetSize())
        {
            const auto& CachedRes = Set.GetResource(CacheOffset + ArrayIndex);
            return CachedRes.pObject != nullptr;
        }
    }

    return false;
}

void ShaderVariableVkImpl::BindResource(IDeviceObject* pObj, Uint32 ArrayIndex) const
{
    auto  CacheOffset   = GetIndex();
    auto& ResDesc       = GetDesc();
    auto& Bindings      = GetBinding();
    auto& ResourceCache = m_ParentManager.m_ResourceCache;
    auto& DstDescrSet   = ResourceCache.GetDescriptorSet(Bindings.DescSet);

    VERIFY_EXPR(ArrayIndex < ResDesc.ArraySize);

    UpdateInfo Info{
        DstDescrSet.GetResource(CacheOffset + ArrayIndex),
        DstDescrSet.GetVkDescriptorSet(),
        ArrayIndex,
        ResDesc.VarType,
        Bindings.Binding,
        Bindings.SamplerInd,
        ResDesc.Name};

#ifdef DILIGENT_DEBUG
    if (ResourceCache.DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::SRBResources)
    {
        if (Info.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC || Info.VarType == SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
        {
            VERIFY(Info.vkDescrSet != VK_NULL_HANDLE, "Static and mutable variables must have valid vulkan descriptor set assigned");
            // Dynamic variables do not have vulkan descriptor set only until they are assigned one the first time
        }
    }
    else if (ResourceCache.DbgGetContentType() == ShaderResourceCacheVk::DbgCacheContentType::StaticShaderResources)
    {
        VERIFY(Info.vkDescrSet == VK_NULL_HANDLE, "Static shader resource cache should not have vulkan descriptor set allocation");
    }
    else
    {
        UNEXPECTED("Unexpected shader resource cache content type");
    }
#endif

    VERIFY(Info.DstRes.Type == PipelineResourceSignatureVkImpl::GetVkDescriptorType(ResDesc), "Inconsistent types");

    if (pObj)
    {
        switch (Info.DstRes.Type)
        {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                CacheUniformBuffer(pObj, Info, ResourceCache.GetDynamicBuffersCounter());
                break;

            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                CacheStorageBuffer(pObj, Info, ResourceCache.GetDynamicBuffersCounter());
                break;

            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                CacheTexelBuffer(pObj, Info, ResourceCache.GetDynamicBuffersCounter());
                break;

            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                CacheImage(pObj, Info,
                           [&](const ShaderVariableVkImpl& SeparateSampler, ISampler* pSampler) {
                               VERIFY(!SeparateSampler.IsImmutableSamplerAssigned(), "Separate sampler '", SeparateSampler.GetDesc().Name, "' is assigned an immutable sampler");
                               VERIFY_EXPR(Info.DstRes.Type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
                               DEV_CHECK_ERR(SeparateSampler.GetDesc().ArraySize == 1 || SeparateSampler.GetDesc().ArraySize == ResDesc.ArraySize,
                                             "Array size (", SeparateSampler.GetDesc().ArraySize,
                                             ") of separate sampler variable '",
                                             SeparateSampler.GetDesc().Name,
                                             "' must be one or the same as the array size (", ResDesc.ArraySize,
                                             ") of separate image variable '", ResDesc.Name, "' it is assigned to");
                               Uint32 SamplerArrInd = SeparateSampler.GetDesc().ArraySize == 1 ? 0 : ArrayIndex;
                               SeparateSampler.BindResource(pSampler, SamplerArrInd);
                           });
                break;

            case VK_DESCRIPTOR_TYPE_SAMPLER:
                if (!IsImmutableSamplerAssigned())
                {
                    CacheSeparateSampler(pObj, Info);
                }
                else
                {
                    // Immutable samplers are permanently bound into the set layout; later binding a sampler
                    // into an immutable sampler slot in a descriptor set is not allowed (13.2.1)
                    LOG_ERROR_MESSAGE("Attempting to assign a sampler to an immutable sampler '", ResDesc.Name, '\'');
                }
                break;

            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                CacheInputAttachment(pObj, Info);
                break;

            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                CacheAccelerationStructure(pObj, Info);
                break;

            default: UNEXPECTED("Unknown resource type ", static_cast<Int32>(Info.DstRes.Type));
        }
    }
    else
    {
        if (Info.DstRes.pObject && Info.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            LOG_ERROR_MESSAGE("Shader variable '", ResDesc.Name, "' is not dynamic but being unbound. This is an error and may cause unpredicted behavior. "
                                                                 "Use another shader resource binding instance or label shader variable as dynamic if you need to bind another resource.");
        }

        Info.DstRes.pObject.Release();
    }
}

template <typename ObjectType, typename TPreUpdateObject>
bool ShaderVariableVkImpl::UpdateCachedResource(UpdateInfo&                 Info,
                                                RefCntAutoPtr<ObjectType>&& pObject,
                                                TPreUpdateObject            PreUpdateObject) const
{
    // We cannot use ValidatedCast<> here as the resource retrieved from the
    // resource mapping can be of wrong type
    if (pObject)
    {
        if (Info.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && Info.DstRes.pObject != nullptr)
        {
            // Do not update resource if one is already bound unless it is dynamic. This may be
            // dangerous as writing descriptors while they are used by the GPU is an undefined behavior
            return false;
        }

        PreUpdateObject(Info.DstRes.pObject.template RawPtr<const ObjectType>(), pObject.template RawPtr<const ObjectType>());
        Info.DstRes.pObject.Attach(pObject.Detach());
        return true;
    }
    else
    {
        return false;
    }
}

void ShaderVariableVkImpl::CacheUniformBuffer(IDeviceObject* pBuffer,
                                              UpdateInfo&    Info,
                                              Uint16&        DynamicBuffersCounter) const
{
    // clang-format off
    VERIFY(Info.DstRes.Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
           Info.DstRes.Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
           "Uniform buffer resource is expected");
    // clang-format on
    RefCntAutoPtr<BufferVkImpl> pBufferVk{pBuffer, IID_BufferVk};
#ifdef DILIGENT_DEVELOPMENT
    VerifyConstantBufferBinding(*this, Info.VarType, Info.ArrayIndex, pBuffer, pBufferVk.RawPtr(), Info.DstRes.pObject.RawPtr());

    /*if (pBufferVk->GetDesc().uiSizeInBytes < BufferStaticSize)
    {
        // It is OK if robustBufferAccess feature is enabled, otherwise access outside of buffer range may lead to crash or undefined behavior.
        LOG_WARNING_MESSAGE("Error binding uniform buffer '", pBufferVk->GetDesc().Name, "' to shader variable '",
                            GetDesc().Name, "': buffer size in the shader (",
                            BufferStaticSize, ") is incompatible with the actual buffer size (", pBufferVk->GetDesc().uiSizeInBytes, ").");
    }*/
#endif

    auto UpdateDynamicBuffersCounter = [&DynamicBuffersCounter](const BufferVkImpl* pOldBuffer, const BufferVkImpl* pNewBuffer) {
        if (pOldBuffer != nullptr && pOldBuffer->GetDesc().Usage == USAGE_DYNAMIC)
        {
            VERIFY(DynamicBuffersCounter > 0, "Dynamic buffers counter must be greater than zero when there is at least one dynamic buffer bound in the resource cache");
            --DynamicBuffersCounter;
        }
        if (pNewBuffer != nullptr && pNewBuffer->GetDesc().Usage == USAGE_DYNAMIC)
            ++DynamicBuffersCounter;
    };
    if (UpdateCachedResource(Info, std::move(pBufferVk), UpdateDynamicBuffersCounter))
    {
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC descriptor type require
        // buffer to be created with VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT

        // Do not update descriptor for a dynamic uniform buffer. All dynamic resource
        // descriptors are updated at once by CommitDynamicResources() when SRB is committed.
        if (Info.vkDescrSet != VK_NULL_HANDLE && Info.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorBufferInfo DescrBuffInfo = Info.DstRes.GetUniformBufferDescriptorWriteInfo();
            UpdateDescriptorHandle(Info, nullptr, &DescrBuffInfo, nullptr);
        }
    }
}

void ShaderVariableVkImpl::CacheStorageBuffer(IDeviceObject* pBufferView,
                                              UpdateInfo&    Info,
                                              Uint16&        DynamicBuffersCounter) const
{
    // clang-format off
    VERIFY(Info.DstRes.Type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || 
           Info.DstRes.Type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
           "Storage buffer resource is expected");
    // clang-format on

    RefCntAutoPtr<BufferViewVkImpl> pBufferViewVk{pBufferView, IID_BufferViewVk};
#ifdef DILIGENT_DEVELOPMENT
    {
        // HLSL buffer SRVs are mapped to storge buffers in GLSL
        auto RequiredViewType = BUFFER_VIEW_UNORDERED_ACCESS; // AZ TODO
        VerifyResourceViewBinding(*this, Info.VarType, Info.ArrayIndex, pBufferView, pBufferViewVk.RawPtr(), {RequiredViewType}, Info.DstRes.pObject.RawPtr());
        if (pBufferViewVk != nullptr)
        {
            const auto& ViewDesc = pBufferViewVk->GetDesc();
            const auto& BuffDesc = pBufferViewVk->GetBuffer()->GetDesc();
            if (BuffDesc.Mode != BUFFER_MODE_STRUCTURED && BuffDesc.Mode != BUFFER_MODE_RAW)
            {
                LOG_ERROR_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                  GetDesc().Name, "': structured buffer view is expected.");
            }

            /*if (BufferStride == 0 && ViewDesc.ByteWidth < BufferStaticSize)
            {
                // It is OK if robustBufferAccess feature is enabled, otherwise access outside of buffer range may lead to crash or undefined behavior.
                LOG_WARNING_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                    Name, "': buffer size in the shader (",
                                    BufferStaticSize, ") is incompatible with the actual buffer view size (", ViewDesc.ByteWidth, ").");
            }

            if (BufferStride > 0 && (ViewDesc.ByteWidth < BufferStaticSize || (ViewDesc.ByteWidth - BufferStaticSize) % BufferStride != 0))
            {
                // For buffers with dynamic arrays we know only static part size and array element stride.
                // Element stride in the shader may be differ than in the code. Here we check that the buffer size is exactly the same as the array with N elements.
                LOG_WARNING_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                    Name, "': static buffer size in the shader (",
                                    BufferStaticSize, ") and array element stride (", BufferStride, ") are incompatible with the actual buffer view size (", ViewDesc.ByteWidth, "),",
                                    " this may be the result of the array element size mismatch.");
            }*/
        }
    }
#endif

    auto UpdateDynamicBuffersCounter = [&DynamicBuffersCounter](const BufferViewVkImpl* pOldBufferView, const BufferViewVkImpl* pNewBufferView) {
        if (pOldBufferView != nullptr && pOldBufferView->GetBuffer<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC)
        {
            VERIFY(DynamicBuffersCounter > 0, "Dynamic buffers counter must be greater than zero when there is at least one dynamic buffer bound in the resource cache");
            --DynamicBuffersCounter;
        }
        if (pNewBufferView != nullptr && pNewBufferView->GetBuffer<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC)
            ++DynamicBuffersCounter;
    };

    if (UpdateCachedResource(Info, std::move(pBufferViewVk), UpdateDynamicBuffersCounter))
    {
        // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC descriptor type
        // require buffer to be created with VK_BUFFER_USAGE_STORAGE_BUFFER_BIT (13.2.4)

        // Do not update descriptor for a dynamic storage buffer. All dynamic resource
        // descriptors are updated at once by CommitDynamicResources() when SRB is committed.
        if (Info.vkDescrSet != VK_NULL_HANDLE && Info.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorBufferInfo DescrBuffInfo = Info.DstRes.GetStorageBufferDescriptorWriteInfo();
            UpdateDescriptorHandle(Info, nullptr, &DescrBuffInfo, nullptr);
        }
    }
}

void ShaderVariableVkImpl::CacheTexelBuffer(IDeviceObject* pBufferView,
                                            UpdateInfo&    Info,
                                            Uint16&        DynamicBuffersCounter) const
{
    // clang-format off
    VERIFY(Info.DstRes.Type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER || 
           Info.DstRes.Type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
           "Uniform or storage buffer resource is expected");
    // clang-format on

    RefCntAutoPtr<BufferViewVkImpl> pBufferViewVk{pBufferView, IID_BufferViewVk};
#ifdef DILIGENT_DEVELOPMENT
    {
        // HLSL buffer SRVs are mapped to storge buffers in GLSL
        auto RequiredViewType = BUFFER_VIEW_UNORDERED_ACCESS; // AZ TODO
        VerifyResourceViewBinding(*this, Info.VarType, Info.ArrayIndex, pBufferView, pBufferViewVk.RawPtr(), {RequiredViewType}, Info.DstRes.pObject.RawPtr());
        if (pBufferViewVk != nullptr)
        {
            const auto& ViewDesc = pBufferViewVk->GetDesc();
            const auto& BuffDesc = pBufferViewVk->GetBuffer()->GetDesc();
            if (!((BuffDesc.Mode == BUFFER_MODE_FORMATTED && ViewDesc.Format.ValueType != VT_UNDEFINED) || BuffDesc.Mode == BUFFER_MODE_RAW))
            {
                LOG_ERROR_MESSAGE("Error binding buffer view '", ViewDesc.Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                  GetDesc().Name, "': formatted buffer view is expected.");
            }
        }
    }
#endif

    auto UpdateDynamicBuffersCounter = [&DynamicBuffersCounter](const BufferViewVkImpl* pOldBufferView, const BufferViewVkImpl* pNewBufferView) {
        if (pOldBufferView != nullptr && pOldBufferView->GetBuffer<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC)
        {
            VERIFY(DynamicBuffersCounter > 0, "Dynamic buffers counter must be greater than zero when there is at least one dynamic buffer bound in the resource cache");
            --DynamicBuffersCounter;
        }
        if (pNewBufferView != nullptr && pNewBufferView->GetBuffer<const BufferVkImpl>()->GetDesc().Usage == USAGE_DYNAMIC)
            ++DynamicBuffersCounter;
    };

    if (UpdateCachedResource(Info, std::move(pBufferViewVk), UpdateDynamicBuffersCounter))
    {
        // The following bits must have been set at buffer creation time:
        //  * VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER  ->  VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
        //  * VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER  ->  VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT

        // Do not update descriptor for a dynamic texel buffer. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (Info.vkDescrSet != VK_NULL_HANDLE && Info.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkBufferView BuffView = Info.DstRes.pObject.RawPtr<BufferViewVkImpl>()->GetVkBufferView();
            UpdateDescriptorHandle(Info, nullptr, nullptr, &BuffView);
        }
    }
}

template <typename TCacheSampler>
void ShaderVariableVkImpl::CacheImage(IDeviceObject* pTexView,
                                      UpdateInfo&    Info,
                                      TCacheSampler  CacheSampler) const
{
    // clang-format off
    VERIFY(Info.DstRes.Type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || 
           Info.DstRes.Type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
           Info.DstRes.Type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
           "Storage image, separate image or sampled image resource is expected");
    // clang-format on
    RefCntAutoPtr<TextureViewVkImpl> pTexViewVk0{pTexView, IID_TextureViewVk};
#ifdef DILIGENT_DEVELOPMENT
    {
        // HLSL buffer SRVs are mapped to storge buffers in GLSL
        auto RequiredViewType = TEXTURE_VIEW_UNORDERED_ACCESS; // AZ TODO: read only
        VerifyResourceViewBinding(*this, Info.VarType, Info.ArrayIndex, pTexView, pTexViewVk0.RawPtr(), {RequiredViewType}, Info.DstRes.pObject.RawPtr());
    }
#endif
    if (UpdateCachedResource(Info, std::move(pTexViewVk0), [](const TextureViewVkImpl*, const TextureViewVkImpl*) {}))
    {
        // We can do RawPtr here safely since UpdateCachedResource() returned true
        auto* pTexViewVk = Info.DstRes.pObject.RawPtr<TextureViewVkImpl>();
#ifdef DILIGENT_DEVELOPMENT
        if (Info.DstRes.Type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && !IsImmutableSamplerAssigned())
        {
            if (pTexViewVk->GetSampler() == nullptr)
            {
                LOG_ERROR_MESSAGE("Error binding texture view '", pTexViewVk->GetDesc().Name, "' to variable '", GetPrintName(Info.ArrayIndex),
                                  "'. No sampler is assigned to the view");
            }
        }
#endif

        // Do not update descriptor for a dynamic image. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (Info.vkDescrSet != VK_NULL_HANDLE && Info.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorImageInfo DescrImgInfo = Info.DstRes.GetImageDescriptorWriteInfo(IsImmutableSamplerAssigned());
            UpdateDescriptorHandle(Info, &DescrImgInfo, nullptr, nullptr);
        }

        if (Info.SamplerInd != InvalidSamplerInd)
        {
            VERIFY(Info.DstRes.Type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                   "Only separate images can be assigned separate samplers when using HLSL-style combined samplers.");
            VERIFY(!IsImmutableSamplerAssigned(), "Separate image can't be assigned an immutable sampler.");
            const auto& SamplerAttribs = *m_ParentManager.GetVariable(Info.SamplerInd);
            VERIFY_EXPR(SamplerAttribs.GetDesc().ResourceType == SHADER_RESOURCE_TYPE_SAMPLER);
            if (!SamplerAttribs.IsImmutableSamplerAssigned())
            {
                auto* pSampler = pTexViewVk->GetSampler();
                if (pSampler != nullptr)
                {
                    CacheSampler(SamplerAttribs, pSampler);
                }
                else
                {
                    LOG_ERROR_MESSAGE("Failed to bind sampler to sampler variable '", SamplerAttribs.GetDesc().Name,
                                      "' assigned to separate image '", GetPrintName(Info.ArrayIndex),
                                      "': no sampler is set in texture view '", pTexViewVk->GetDesc().Name, '\'');
                }
            }
        }
    }
}

void ShaderVariableVkImpl::CacheSeparateSampler(IDeviceObject* pSampler,
                                                UpdateInfo&    Info) const
{
    VERIFY(Info.DstRes.Type == VK_DESCRIPTOR_TYPE_SAMPLER, "Separate sampler resource is expected");
    VERIFY(!IsImmutableSamplerAssigned(), "This separate sampler is assigned an immutable sampler");

    RefCntAutoPtr<SamplerVkImpl> pSamplerVk{pSampler, IID_Sampler};
#ifdef DILIGENT_DEVELOPMENT
    if (pSampler != nullptr && pSamplerVk == nullptr)
    {
        LOG_ERROR_MESSAGE("Failed to bind object '", pSampler->GetDesc().Name, "' to variable '", GetPrintName(Info.ArrayIndex),
                          "'. Unexpected object type: sampler is expected");
    }
    if (Info.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && Info.DstRes.pObject != nullptr && Info.DstRes.pObject != pSamplerVk)
    {
        auto VarTypeStr = GetShaderVariableTypeLiteralName(Info.VarType);
        LOG_ERROR_MESSAGE("Non-null sampler is already bound to ", VarTypeStr, " shader variable '", GetPrintName(Info.ArrayIndex),
                          "'. Attempting to bind another sampler or null is an error and may "
                          "cause unpredicted behavior. Use another shader resource binding instance or label the variable as dynamic.");
    }
#endif
    if (UpdateCachedResource(Info, std::move(pSamplerVk), [](const SamplerVkImpl*, const SamplerVkImpl*) {}))
    {
        // Do not update descriptor for a dynamic sampler. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (Info.vkDescrSet != VK_NULL_HANDLE && Info.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorImageInfo DescrImgInfo = Info.DstRes.GetSamplerDescriptorWriteInfo();
            UpdateDescriptorHandle(Info, &DescrImgInfo, nullptr, nullptr);
        }
    }
}

void ShaderVariableVkImpl::CacheInputAttachment(IDeviceObject* pTexView,
                                                UpdateInfo&    Info) const
{
    VERIFY(Info.DstRes.Type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, "Input attachment resource is expected");
    RefCntAutoPtr<TextureViewVkImpl> pTexViewVk0{pTexView, IID_TextureViewVk};
#ifdef DILIGENT_DEVELOPMENT
    VerifyResourceViewBinding(*this, Info.VarType, Info.ArrayIndex, pTexView, pTexViewVk0.RawPtr(), {TEXTURE_VIEW_SHADER_RESOURCE}, Info.DstRes.pObject.RawPtr());
#endif
    if (UpdateCachedResource(Info, std::move(pTexViewVk0), [](const TextureViewVkImpl*, const TextureViewVkImpl*) {}))
    {
        // Do not update descriptor for a dynamic image. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (Info.vkDescrSet != VK_NULL_HANDLE && Info.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkDescriptorImageInfo DescrImgInfo = Info.DstRes.GetInputAttachmentDescriptorWriteInfo();
            UpdateDescriptorHandle(Info, &DescrImgInfo, nullptr, nullptr);
        }
        //
    }
}

void ShaderVariableVkImpl::CacheAccelerationStructure(IDeviceObject* pTLAS,
                                                      UpdateInfo&    Info) const
{
    //VERIFY(Type == SPIRVShaderResourceAttribs::ResourceType::AccelerationStructure, "Acceleration Structure resource is expected");
    RefCntAutoPtr<TopLevelASVkImpl> pTLASVk{pTLAS, IID_TopLevelASVk};
#ifdef DILIGENT_DEVELOPMENT
    VerifyTLASResourceBinding(*this, Info.VarType, Info.ArrayIndex, pTLASVk.RawPtr(), Info.DstRes.pObject.RawPtr());
#endif
    if (UpdateCachedResource(Info, std::move(pTLASVk), [](const TopLevelASVkImpl*, const TopLevelASVkImpl*) {}))
    {
        // Do not update descriptor for a dynamic TLAS. All dynamic resource descriptors
        // are updated at once by CommitDynamicResources() when SRB is committed.
        if (Info.vkDescrSet != VK_NULL_HANDLE && Info.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
        {
            VkWriteDescriptorSetAccelerationStructureKHR DescrASInfo = Info.DstRes.GetAccelerationStructureWriteInfo();
            UpdateDescriptorHandle(Info, nullptr, nullptr, nullptr, &DescrASInfo);
        }
        //
    }
}

bool ShaderVariableVkImpl::IsImmutableSamplerAssigned() const
{
    return true; // AZ TODO
}

void ShaderVariableVkImpl::UpdateDescriptorHandle(UpdateInfo&                                         Info,
                                                  const VkDescriptorImageInfo*                        pImageInfo,
                                                  const VkDescriptorBufferInfo*                       pBufferInfo,
                                                  const VkBufferView*                                 pTexelBufferView,
                                                  const VkWriteDescriptorSetAccelerationStructureKHR* pAccelStructInfo) const
{
    VERIFY_EXPR(Info.vkDescrSet != VK_NULL_HANDLE);

    VkWriteDescriptorSet WriteDescrSet;
    WriteDescrSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    WriteDescrSet.pNext           = pAccelStructInfo;
    WriteDescrSet.dstSet          = Info.vkDescrSet;
    WriteDescrSet.dstBinding      = GetBinding().Binding;
    WriteDescrSet.dstArrayElement = Info.ArrayIndex;
    WriteDescrSet.descriptorCount = 1;
    // descriptorType must be the same type as that specified in VkDescriptorSetLayoutBinding for dstSet at dstBinding.
    // The type of the descriptor also controls which array the descriptors are taken from. (13.2.4)
    WriteDescrSet.descriptorType   = PipelineResourceSignatureVkImpl::GetVkDescriptorType(GetDesc());
    WriteDescrSet.pImageInfo       = pImageInfo;
    WriteDescrSet.pBufferInfo      = pBufferInfo;
    WriteDescrSet.pTexelBufferView = pTexelBufferView;

    m_ParentManager.m_pSignature->GetDevice()->GetLogicalDevice().UpdateDescriptorSets(1, &WriteDescrSet, 0, nullptr);
}

String ShaderVariableVkImpl::GetPrintName(Uint32 ArrayInd) const
{
    auto& ResDesc = GetDesc();
    VERIFY_EXPR(ArrayInd < ResDesc.ArraySize);

    if (ResDesc.ArraySize > 1)
    {
        std::stringstream ss;
        ss << ResDesc.Name << '[' << ArrayInd << ']';
        return ss.str();
    }
    else
        return ResDesc.Name;
}

RESOURCE_DIMENSION ShaderVariableVkImpl::GetResourceDimension() const
{
    return GetDesc().ResourceDim;
}

bool ShaderVariableVkImpl::IsMultisample() const
{
    return false; // AZ TODO
}

} // namespace Diligent
