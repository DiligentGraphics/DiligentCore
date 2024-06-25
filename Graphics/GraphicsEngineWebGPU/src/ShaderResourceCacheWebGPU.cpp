/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "ShaderResourceCacheWebGPU.hpp"
#include "EngineMemory.h"
#include "RenderDeviceWebGPUImpl.hpp"
#include "BufferWebGPUImpl.hpp"
#include "TextureWebGPUImpl.hpp"
#include "SamplerWebGPUImpl.hpp"

namespace Diligent
{

size_t ShaderResourceCacheWebGPU::GetRequiredMemorySize(Uint32 NumGroups, const Uint32* GroupSizes)
{
    Uint32 TotalResources = 0;
    for (Uint32 t = 0; t < NumGroups; ++t)
        TotalResources += GroupSizes[t];
    size_t MemorySize = NumGroups * sizeof(BindGroup) + TotalResources * sizeof(Resource) + TotalResources * sizeof(WGPUBindGroupEntry);
    return MemorySize;
}

ShaderResourceCacheWebGPU::ShaderResourceCacheWebGPU(ResourceCacheContentType ContentType) noexcept :
    m_ContentType{static_cast<Uint32>(ContentType)}
{
}

ShaderResourceCacheWebGPU::~ShaderResourceCacheWebGPU()
{
    if (m_pMemory)
    {
        Resource* pResources = GetFirstResourcePtr();
        for (Uint32 res = 0; res < m_TotalResources; ++res)
            pResources[res].~Resource();
        for (Uint32 t = 0; t < m_NumBindGroups; ++t)
            GetBindGroup(t).~BindGroup();
    }
}

void ShaderResourceCacheWebGPU::InitializeGroups(IMemoryAllocator& MemAllocator, Uint32 NumGroups, const Uint32* GroupSizes)
{
    VERIFY(!m_pMemory, "Memory has already been allocated");

    // Memory layout:
    //
    //  m_pMemory
    //  |
    //  V
    // ||  BindGroup[0]  |   ....    |  BindGroup[Ng-1]  |  Res[0]  |  ... |  Res[n-1]  |   ....   | Res[0]  |  ... |  Res[m-1]  | wgpuEntry[0] | ... | wgpuEntry[n-1] |   ....   | wgpuEntry[0] | ... | wgpuEntry[m-1] ||
    //
    //
    //  Ng = m_NumBindGroups

    m_NumBindGroups = static_cast<Uint16>(NumGroups);
    VERIFY(m_NumBindGroups == NumGroups, "NumGroups (", NumGroups, ") exceeds maximum representable value");

    m_TotalResources = 0;
    for (Uint32 t = 0; t < NumGroups; ++t)
    {
        VERIFY_EXPR(GroupSizes[t] > 0);
        m_TotalResources += GroupSizes[t];
    }

    const size_t MemorySize = NumGroups * sizeof(BindGroup) + m_TotalResources * sizeof(Resource) + m_TotalResources * sizeof(WGPUBindGroupEntry);
    VERIFY_EXPR(MemorySize == GetRequiredMemorySize(NumGroups, GroupSizes));
#ifdef DILIGENT_DEBUG
    m_DbgInitializedResources.resize(m_NumBindGroups);
#endif

    if (MemorySize > 0)
    {
        m_pMemory = decltype(m_pMemory){
            ALLOCATE_RAW(MemAllocator, "Memory for shader resource cache data", MemorySize),
            STDDeleter<void, IMemoryAllocator>(MemAllocator),
        };

        BindGroup*          pGroups           = reinterpret_cast<BindGroup*>(m_pMemory.get());
        Resource*           pCurrResPtr       = reinterpret_cast<Resource*>(pGroups + m_NumBindGroups);
        WGPUBindGroupEntry* pCurrWGPUEntryPtr = reinterpret_cast<WGPUBindGroupEntry*>(pCurrResPtr + m_TotalResources);
        for (Uint32 t = 0; t < NumGroups; ++t)
        {
            new (&GetBindGroup(t)) BindGroup{
                GroupSizes[t],
                GroupSizes[t] > 0 ? pCurrResPtr : nullptr,
                GroupSizes[t] > 0 ? pCurrWGPUEntryPtr : nullptr,
            };
            pCurrResPtr += GroupSizes[t];
            pCurrWGPUEntryPtr += GroupSizes[t];

#ifdef DILIGENT_DEBUG
            m_DbgInitializedResources[t].resize(GroupSizes[t]);
#endif
        }
        VERIFY_EXPR((char*)pCurrResPtr == (char*)m_pMemory.get() + MemorySize);
    }
}

void ShaderResourceCacheWebGPU::Resource::SetUniformBuffer(RefCntAutoPtr<IDeviceObject>&& _pBuffer, Uint64 _BaseOffset, Uint64 _RangeSize)
{
    VERIFY_EXPR(Type == BindGroupEntryType::UniformBuffer);

    pObject = std::move(_pBuffer);

    const BufferWebGPUImpl* pBuffWGPU = pObject.ConstPtr<BufferWebGPUImpl>();
    VERIFY_EXPR(pBuffWGPU == nullptr || (pBuffWGPU->GetDesc().BindFlags & BIND_UNIFORM_BUFFER) != 0);

    VERIFY(_BaseOffset + _RangeSize <= (pBuffWGPU != nullptr ? pBuffWGPU->GetDesc().Size : 0), "Specified range is out of buffer bounds");
    BufferBaseOffset = _BaseOffset;
    BufferRangeSize  = _RangeSize;
    if (BufferRangeSize == 0)
        BufferRangeSize = pBuffWGPU != nullptr ? (pBuffWGPU->GetDesc().Size - BufferBaseOffset) : 0;

    // Reset dynamic offset
    BufferDynamicOffset = 0;
}

void ShaderResourceCacheWebGPU::Resource::SetStorageBuffer(RefCntAutoPtr<IDeviceObject>&& _pBufferView)
{
    VERIFY_EXPR(Type == BindGroupEntryType::StorageBuffer ||
                Type == BindGroupEntryType::StorageBuffer_ReadOnly);

    pObject = std::move(_pBufferView);

    BufferDynamicOffset = 0; // It is essential to reset dynamic offset
    BufferBaseOffset    = 0;
    BufferRangeSize     = 0;

    if (!pObject)
        return;

    const BufferViewWebGPUImpl* pBuffViewWGPU = pObject.ConstPtr<BufferViewWebGPUImpl>();
    const BufferViewDesc&       ViewDesc      = pBuffViewWGPU->GetDesc();

    BufferBaseOffset = ViewDesc.ByteOffset;
    BufferRangeSize  = ViewDesc.ByteWidth;

#ifdef DILIGENT_DEBUG
    {
        const BufferWebGPUImpl* pBuffWGPU = pBuffViewWGPU->GetBuffer<const BufferWebGPUImpl>();
        const BufferDesc&       BuffDesc  = pBuffWGPU->GetDesc();
        VERIFY(BufferBaseOffset + BufferRangeSize <= BuffDesc.Size, "Specified view range is out of buffer bounds");

        if (Type == BindGroupEntryType::StorageBuffer_ReadOnly)
        {
            VERIFY(ViewDesc.ViewType == BUFFER_VIEW_SHADER_RESOURCE, "Attempting to bind buffer view '", ViewDesc.Name,
                   "' as read-only storage buffer. Expected view type is BUFFER_VIEW_SHADER_RESOURCE. Actual type: ",
                   GetBufferViewTypeLiteralName(ViewDesc.ViewType));
            VERIFY((BuffDesc.BindFlags & BIND_SHADER_RESOURCE) != 0,
                   "Buffer '", BuffDesc.Name, "' being set as read-only storage buffer was not created with BIND_SHADER_RESOURCE flag");
        }
        else if (Type == BindGroupEntryType::StorageBuffer)
        {
            VERIFY(ViewDesc.ViewType == BUFFER_VIEW_UNORDERED_ACCESS, "Attempting to bind buffer view '", ViewDesc.Name,
                   "' as writable storage buffer. Expected view type is BUFFER_VIEW_UNORDERED_ACCESS. Actual type: ",
                   GetBufferViewTypeLiteralName(ViewDesc.ViewType));
            VERIFY((BuffDesc.BindFlags & BIND_UNORDERED_ACCESS) != 0,
                   "Buffer '", BuffDesc.Name, "' being set as writable storage buffer was not created with BIND_UNORDERED_ACCESS flag");
        }
        else
        {
            UNEXPECTED("Unexpected resource type");
        }
    }
#endif
}

void ShaderResourceCacheWebGPU::InitializeResources(Uint32 GroupIdx, Uint32 Offset, Uint32 ArraySize, BindGroupEntryType Type, bool HasImmutableSampler)
{
    BindGroup& Group = GetBindGroup(GroupIdx);
    for (Uint32 res = 0; res < ArraySize; ++res)
    {
        new (&Group.GetResource(Offset + res)) Resource{Type, HasImmutableSampler};
#ifdef DILIGENT_DEBUG
        m_DbgInitializedResources[GroupIdx][size_t{Offset} + res] = true;
#endif
    }
}

static bool IsDynamicBuffer(const ShaderResourceCacheWebGPU::Resource& Res)
{
    if (!Res.pObject)
        return false;

    const BufferWebGPUImpl* pBuffer = nullptr;
    static_assert(static_cast<Uint32>(BindGroupEntryType::Count) == 9, "Please update the switch below to handle the new bind group entry type");
    switch (Res.Type)
    {
        case BindGroupEntryType::UniformBuffer:
            pBuffer = Res.pObject.ConstPtr<BufferWebGPUImpl>();
            break;

        case BindGroupEntryType::StorageBuffer:
        case BindGroupEntryType::StorageBuffer_ReadOnly:
            pBuffer = Res.pObject ? Res.pObject.ConstPtr<const BufferViewWebGPUImpl>()->GetBuffer<const BufferWebGPUImpl>() : nullptr;
            break;

        default:
            VERIFY_EXPR(Res.BufferRangeSize == 0);
            // Do nothing
            break;
    }

    if (pBuffer == nullptr)
        return false;

    const BufferDesc& BuffDesc = pBuffer->GetDesc();

    bool IsDynamic = (BuffDesc.Usage == USAGE_DYNAMIC);
    return IsDynamic;
}

const ShaderResourceCacheWebGPU::Resource& ShaderResourceCacheWebGPU::SetResource(
    Uint32            BindGroupIdx,
    Uint32            CacheOffset,
    SetResourceInfo&& SrcRes)
{
    BindGroup& Group  = GetBindGroup(BindGroupIdx);
    Resource&  DstRes = Group.GetResource(CacheOffset);

    if (IsDynamicBuffer(DstRes))
    {
        VERIFY(m_NumDynamicBuffers > 0, "Dynamic buffers counter must be greater than zero when there is at least one dynamic buffer bound in the resource cache");
        --m_NumDynamicBuffers;
    }

    static_assert(static_cast<Uint32>(BindGroupEntryType::Count) == 9, "Please update the switch below to handle the new bind group entry type");
    switch (DstRes.Type)
    {
        case BindGroupEntryType::UniformBuffer:
            DstRes.SetUniformBuffer(std::move(SrcRes.pObject), SrcRes.BufferBaseOffset, SrcRes.BufferRangeSize);
            break;

        case BindGroupEntryType::StorageBuffer:
        case BindGroupEntryType::StorageBuffer_ReadOnly:
            DstRes.SetStorageBuffer(std::move(SrcRes.pObject));
            break;

        default:
            VERIFY(SrcRes.BufferBaseOffset == 0 && SrcRes.BufferRangeSize == 0, "Buffer range can only be specified for uniform buffers");
            DstRes.pObject = std::move(SrcRes.pObject);
    }

    if (IsDynamicBuffer(DstRes))
    {
        ++m_NumDynamicBuffers;
    }

    if (DstRes.pObject)
    {
        WGPUBindGroupEntry& wgpuEntry = Group.m_wgpuEntries[CacheOffset + SrcRes.ArrayIndex];
        VERIFY_EXPR(wgpuEntry.binding == CacheOffset + SrcRes.ArrayIndex);

        static_assert(static_cast<Uint32>(BindGroupEntryType::Count) == 9, "Please update the switch below to handle the new bind group entry type");
        switch (DstRes.Type)
        {
            case BindGroupEntryType::UniformBuffer:
            {
                const BufferWebGPUImpl* pBuffWGPU = DstRes.pObject.ConstPtr<BufferWebGPUImpl>();

                wgpuEntry.buffer = pBuffWGPU->GetWebGPUBuffer();
                VERIFY_EXPR(DstRes.BufferBaseOffset + DstRes.BufferRangeSize <= pBuffWGPU->GetDesc().Size);
                wgpuEntry.offset = DstRes.BufferBaseOffset;
                wgpuEntry.size   = DstRes.BufferRangeSize;
            }
            break;

            case BindGroupEntryType::StorageBuffer:
            case BindGroupEntryType::StorageBuffer_ReadOnly:
            {
                const BufferViewWebGPUImpl* pBuffViewWGPU = DstRes.pObject.ConstPtr<BufferViewWebGPUImpl>();
                const BufferWebGPUImpl*     pBuffWGPU     = pBuffViewWGPU->GetBuffer<const BufferWebGPUImpl>();

                wgpuEntry.buffer = pBuffWGPU->GetWebGPUBuffer();
                VERIFY_EXPR(DstRes.BufferBaseOffset + DstRes.BufferRangeSize <= pBuffWGPU->GetDesc().Size);
                wgpuEntry.offset = DstRes.BufferBaseOffset;
                wgpuEntry.size   = DstRes.BufferRangeSize;
            }
            break;

            case BindGroupEntryType::Texture:
            case BindGroupEntryType::StorageTexture_WriteOnly:
            case BindGroupEntryType::StorageTexture_ReadOnly:
            case BindGroupEntryType::StorageTexture_ReadWrite:
            {
                const TextureViewWebGPUImpl* pTexViewWGPU = DstRes.pObject.ConstPtr<TextureViewWebGPUImpl>();

                wgpuEntry.textureView = pTexViewWGPU->GetWebGPUTextureView();
            }
            break;

            case BindGroupEntryType::ExternalTexture:
            {
                UNSUPPORTED("External textures are not currently supported");
            }
            break;

            case BindGroupEntryType::Sampler:
            {
                const SamplerWebGPUImpl* pSamplerWGPU = DstRes.pObject.ConstPtr<SamplerWebGPUImpl>();

                wgpuEntry.sampler = pSamplerWGPU->GetWebGPUSampler();
            }
            break;

            default:
                UNEXPECTED("Unexpected resource type");
        }
    }

    UpdateRevision();

    return DstRes;
}


void ShaderResourceCacheWebGPU::SetDynamicBufferOffset(Uint32 DescrSetIndex,
                                                       Uint32 CacheOffset,
                                                       Uint32 DynamicBufferOffset)
{
    BindGroup& Group  = GetBindGroup(DescrSetIndex);
    Resource&  DstRes = Group.GetResource(CacheOffset);

    DEV_CHECK_ERR(DstRes.pObject, "Setting dynamic offset when no object is bound");
    const auto* pBufferWGPU = DstRes.Type == BindGroupEntryType::UniformBuffer ?
        DstRes.pObject.ConstPtr<BufferWebGPUImpl>() :
        DstRes.pObject.ConstPtr<BufferViewWebGPUImpl>()->GetBuffer<const BufferWebGPUImpl>();
    DEV_CHECK_ERR(DstRes.BufferBaseOffset + DstRes.BufferRangeSize + DynamicBufferOffset <= pBufferWGPU->GetDesc().Size,
                  "Specified offset is out of buffer bounds");

    DstRes.BufferDynamicOffset = DynamicBufferOffset;
}

void ShaderResourceCacheWebGPU::CommitBindGroup(WGPUDevice wgpuDevice, Uint32 GroupIndex, WGPUBindGroupLayout wgpuGroupLayout)
{
    BindGroup& Group = GetBindGroup(GroupIndex);
    if (!Group.m_wgpuBindGroup)
    {
        WGPUBindGroupDescriptor wgpuBindGroupDescriptor;
        wgpuBindGroupDescriptor.nextInChain = nullptr;
        wgpuBindGroupDescriptor.label       = nullptr;
        wgpuBindGroupDescriptor.layout      = wgpuGroupLayout;
        wgpuBindGroupDescriptor.entryCount  = Group.m_NumResources;
        wgpuBindGroupDescriptor.entries     = Group.m_wgpuEntries;

        Group.m_wgpuBindGroup.Reset(wgpuDeviceCreateBindGroup(wgpuDevice, &wgpuBindGroupDescriptor));
    }
}

#ifdef DILIGENT_DEBUG
void ShaderResourceCacheWebGPU::DbgVerifyResourceInitialization() const
{
    for (const auto& SetFlags : m_DbgInitializedResources)
    {
        for (auto ResInitialized : SetFlags)
            VERIFY(ResInitialized, "Not all resources in the cache have been initialized. This is a bug.");
    }
}
#endif

} // namespace Diligent
