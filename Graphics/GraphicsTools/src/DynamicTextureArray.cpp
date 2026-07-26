/*
 *  Copyright 2019-2026 Diligent Graphics LLC
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

#include "DynamicTextureArray.hpp"

#include <algorithm>
#include <vector>

#include "DebugUtilities.hpp"
#include "GraphicsAccessories.hpp"
#include "Align.hpp"
#include "GraphicsUtilities.h"

namespace Diligent
{

namespace
{

bool VerifySparseTextureCompatibility(IRenderDevice* pDevice, const TextureDesc& Desc)
{
    VERIFY_EXPR(pDevice != nullptr);

    const DeviceFeatures& DeviceInfo = pDevice->GetDeviceInfo().Features;
    if (!DeviceInfo.SparseResources)
    {
        LOG_WARNING_MESSAGE("SparseResources device feature is not enabled.");
        return false;
    }

    const SparseResourceProperties& SparseRes = pDevice->GetAdapterInfo().SparseResources;
    if ((SparseRes.CapFlags & SPARSE_RESOURCE_CAP_FLAG_TEXTURE_2D_ARRAY_MIP_TAIL) == 0)
    {
        LOG_WARNING_MESSAGE("This device does not support sparse texture 2D arrays with mip tails.");
        return false;
    }

    SparseTextureFormatInfo SparseInfo;
    if (!pDevice->GetSparseTextureFormatInfo(Desc.Format, Desc.Type, Desc.SampleCount, SparseInfo))
    {
        LOG_WARNING_MESSAGE("Failed to retrieve sparse texture format info.");
        return false;
    }
    if ((SparseInfo.BindFlags & Desc.BindFlags) != Desc.BindFlags)
    {
        LOG_WARNING_MESSAGE("The following bind flags requested for the sparse dynamic texture array are not supported by device: ", GetBindFlagsString(Desc.BindFlags & ~SparseInfo.BindFlags, ", "));
        return false;
    }

    return true;
}

} // namespace

DynamicTextureArray::DynamicTextureArray(IRenderDevice* pDevice, const DynamicTextureArrayCreateInfo& CreateInfo) :
    m_Name{CreateInfo.Desc.Name != nullptr ? CreateInfo.Desc.Name : "Dynamic Texture"},
    m_Desc{CreateInfo.Desc},
    m_NumSlicesInPage{std::max(CreateInfo.NumSlicesInMemoryPage, 1u)}
{
    m_Desc.Name = m_Name.c_str();

    if (m_Desc.Type != RESOURCE_DIM_TEX_2D_ARRAY)
        LOG_ERROR_AND_THROW(GetResourceDimString(m_Desc.Type), " is not a valid resource dimension. Only 2D array textures are allowed");

    if (m_Desc.Format == TEX_FORMAT_UNKNOWN)
        LOG_ERROR_AND_THROW("Texture format must not be UNKNOWN");

    if (m_Desc.Width == 0)
        LOG_ERROR_AND_THROW("Texture width must not be zero");

    if (m_Desc.Height == 0)
        LOG_ERROR_AND_THROW("Texture height must not be zero");

    if (m_Desc.Usage != USAGE_DEFAULT && m_Desc.Usage != USAGE_SPARSE)
        LOG_ERROR_AND_THROW("DynamicTextureArray only supports USAGE_DEFAULT and USAGE_SPARSE");

    if (m_Desc.Usage == USAGE_SPARSE && !IsPowerOfTwo(m_Desc.ImmediateContextMask))
        LOG_ERROR_AND_THROW("Sparse DynamicTextureArray requires exactly one immediate context");

    if (m_Desc.MipLevels == 0)
        m_Desc.MipLevels = ComputeMipLevelsCount(m_Desc.GetWidth(), m_Desc.GetHeight(), m_Desc.GetDepth());

    StoreUsage(m_Desc.Usage);
    m_PendingSize = m_Desc.ArraySize;
    // Current array size. Keep it separate from m_Desc so GetDesc() can return
    // a thread-safe snapshot while the render thread commits resizes.
    StoreArraySize(0);
    m_Desc.ArraySize = 0;
    if (pDevice != nullptr && (m_PendingSize > 0 || GetUsage() == USAGE_SPARSE))
    {
        if (!CreateResources(pDevice))
            LOG_ERROR_AND_THROW("Failed to create texture for a dynamic texture array");
    }
}

void DynamicTextureArray::FallbackToDefaultTexture() noexcept
{
    ReleaseTextureViews();
    m_pTexture.Release();
    m_pMemory.Release();
    m_pBeforeResizeFence.Release();
    m_pAfterResizeFence.Release();

    m_MemoryPageSize = 0;
    m_SparseMemoryUsage.store(0, std::memory_order_release);
    m_NextBeforeResizeFenceValue = 1;
    m_NextAfterResizeFenceValue  = 1;

    StoreArraySize(0);
    StoreUsage(USAGE_DEFAULT);
}


void DynamicTextureArray::CreateSparseTexture(IRenderDevice* pDevice)
{
    VERIFY_EXPR(!m_pTexture && !m_pMemory);
    VERIFY_EXPR(pDevice != nullptr);
    VERIFY_EXPR(GetUsage() == USAGE_SPARSE);

    class SparseCreationGuard
    {
    public:
        explicit SparseCreationGuard(DynamicTextureArray& TextureArray) noexcept :
            m_TextureArray{TextureArray}
        {}

        ~SparseCreationGuard() noexcept
        {
            if (m_IsArmed)
                m_TextureArray.FallbackToDefaultTexture();
        }

        void Disarm() noexcept
        {
            m_IsArmed = false;
        }

    private:
        DynamicTextureArray& m_TextureArray;
        bool                 m_IsArmed = true;
    };

    SparseCreationGuard CreationGuard{*this};

    if (!VerifySparseTextureCompatibility(pDevice, m_Desc))
    {
        LOG_WARNING_MESSAGE("This device does not support capabilities required for sparse texture 2D arrays. USAGE_DEFAULT texture will be used instead.");
        return;
    }

    const GraphicsAdapterInfo& AdapterInfo = pDevice->GetAdapterInfo();
    const RenderDeviceInfo&    DeviceInfo  = pDevice->GetDeviceInfo();

    {
        // Some implementations may return UINT64_MAX, so limit the maximum memory size per resource.
        // Some implementations will fail to create texture even if size is less than ResourceSpaceSize.
        const Uint64 MaxMemorySize = std::min(Uint64{1} << 40, AdapterInfo.SparseResources.ResourceSpaceSize) >> 1;

        // Estimate the address space occupied by one array slice from the
        // requested mip chain.
        Uint64 EstimatedSliceSize = 0;
        for (Uint32 Mip = 0; Mip < m_Desc.MipLevels; ++Mip)
            EstimatedSliceSize += GetMipLevelProperties(m_Desc, Mip).MipSize;

        if (EstimatedSliceSize > MaxMemorySize)
        {
            LOG_ERROR_MESSAGE("Sparse texture address space is insufficient for one dynamic texture array slice. USAGE_DEFAULT texture will be used instead.");
            return;
        }

        const Uint64 MaxSlicesInAddressSpace = MaxMemorySize / std::max(EstimatedSliceSize, Uint64{1});

        const Uint64 MaxArraySize = std::min(Uint64{AdapterInfo.Texture.MaxTexture2DArraySlices},
                                             MaxSlicesInAddressSpace);

        TextureDesc TmpDesc = m_Desc;
        TmpDesc.ArraySize   = StaticCast<Uint32>(MaxArraySize);

        if (DeviceInfo.IsMetalDevice())
        {
            // Metal sparse texture requires memory object at initialization
            DeviceMemoryCreateInfo MemCI;
            MemCI.Desc.Name                 = "Sparse dynamic texture memory pool";
            MemCI.Desc.Type                 = DEVICE_MEMORY_TYPE_SPARSE;
            MemCI.Desc.PageSize             = 65536; // Page size is not relevant in Metal
            MemCI.Desc.ImmediateContextMask = m_Desc.ImmediateContextMask;
            // TODO: properly set the heap size.
            MemCI.InitialSize = Uint64{512} << Uint64{20};

            pDevice->CreateDeviceMemory(MemCI, &m_pMemory);
            if (!m_pMemory)
            {
                LOG_ERROR_MESSAGE("Failed to create sparse dynamic texture memory. USAGE_DEFAULT texture will be used instead.");
                return;
            }

            CreateSparseTextureMtl(pDevice, TmpDesc, m_pMemory, &m_pTexture);
        }
        else
        {
            pDevice->CreateTexture(TmpDesc, nullptr, &m_pTexture);
        }
        if (!m_pTexture)
        {
            LOG_ERROR_MESSAGE("Failed to create sparse dynamic texture. USAGE_DEFAULT texture will be used instead.");
            return;
        }
        // No slices are currently committed
        StoreArraySize(0);
    }

    const SparseTextureProperties& TexSparseProps = m_pTexture->GetSparseProperties();
    if ((TexSparseProps.Flags & SPARSE_TEXTURE_FLAG_SINGLE_MIPTAIL) != 0)
    {
        LOG_ERROR_MESSAGE("This device requires single mip tail for the sparse texture 2D array, which is not suitable for the dynamic array.");
        return;
    }

    const Uint32 NumNormalMips = std::min(m_Desc.MipLevels, TexSparseProps.FirstMipInTail);
    // Compute the total number of blocks in one slice
    Uint64 NumBlocksInSlice = 0;
    for (Uint32 Mip = 0; Mip < NumNormalMips; ++Mip)
    {
        const uint3 NumTilesInMip = GetNumSparseTilesInMipLevel(m_Desc, TexSparseProps.TileSize, Mip);
        NumBlocksInSlice += Uint64{NumTilesInMip.x} * Uint64{NumTilesInMip.y} * Uint64{NumTilesInMip.z};
    }

    m_MemoryPageSize = NumBlocksInSlice * TexSparseProps.BlockSize;
    if (m_Desc.MipLevels > TexSparseProps.FirstMipInTail)
    {
        m_MemoryPageSize += TexSparseProps.MipTailSize;
    }

    m_MemoryPageSize *= m_NumSlicesInPage;

    // Create memory pool
    if (!m_pMemory)
    {
        DeviceMemoryCreateInfo MemCI;
        MemCI.Desc.Name                 = "Sparse dynamic texture memory pool";
        MemCI.Desc.Type                 = DEVICE_MEMORY_TYPE_SPARSE;
        MemCI.Desc.PageSize             = m_MemoryPageSize;
        MemCI.Desc.ImmediateContextMask = m_Desc.ImmediateContextMask;

        MemCI.InitialSize = m_MemoryPageSize;

        IDeviceObject* pCompatibleRes[]{m_pTexture};
        MemCI.ppCompatibleResources = pCompatibleRes;
        MemCI.NumResources          = _countof(pCompatibleRes);

        pDevice->CreateDeviceMemory(MemCI, &m_pMemory);
    }
    else
    {
        VERIFY_EXPR(DeviceInfo.IsMetalDevice());
        if (!m_pMemory->Resize(m_MemoryPageSize))
        {
            LOG_ERROR_MESSAGE("Failed to resize sparse dynamic texture memory. USAGE_DEFAULT texture will be used instead.");
            return;
        }
    }
    if (!m_pMemory || m_pMemory->GetCapacity() < m_MemoryPageSize)
    {
        LOG_ERROR_MESSAGE("Failed to allocate sparse dynamic texture memory. USAGE_DEFAULT texture will be used instead.");
        return;
    }
    m_SparseMemoryUsage.store(m_pMemory->GetCapacity(), std::memory_order_release);

    // Create fences
    // Note: D3D11 does not support general fences
    if (pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_D3D11 && pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_WEBGPU)
    {
        FenceDesc Desc;
        Desc.Type = FENCE_TYPE_GENERAL;

        Desc.Name = "Dynamic texture array before-resize fence";
        pDevice->CreateFence(Desc, &m_pBeforeResizeFence);
        Desc.Name = "Dynamic texture array after-resize fence";
        pDevice->CreateFence(Desc, &m_pAfterResizeFence);

        if (!m_pBeforeResizeFence || !m_pAfterResizeFence)
        {
            LOG_ERROR_MESSAGE("Failed to create sparse dynamic texture synchronization fences. USAGE_DEFAULT texture will be used instead.");
            return;
        }
    }

    CreationGuard.Disarm();

    m_Version.fetch_add(1);
    UpdateTextureViews(pDevice);
}

bool DynamicTextureArray::CreateResources(IRenderDevice* pDevice)
{
    VERIFY_EXPR(pDevice != nullptr);
    VERIFY(!m_pTexture, "The texture has already been initialized");
    VERIFY(!m_pMemory, "Memory has already been initialized");

    if (GetUsage() == USAGE_SPARSE)
    {
        CreateSparseTexture(pDevice);

        if (GetUsage() == USAGE_SPARSE)
            return m_pTexture != nullptr;
    }

    // Sparse creation may fall back to a default texture.
    VERIFY_EXPR(GetUsage() == USAGE_DEFAULT);
    return m_PendingSize == 0 || CreateDefaultTexture(pDevice);
}

bool DynamicTextureArray::CreateDefaultTexture(IRenderDevice* pDevice)
{
    VERIFY_EXPR(pDevice != nullptr);
    VERIFY_EXPR(GetUsage() == USAGE_DEFAULT);
    VERIFY_EXPR(m_PendingSize > 0);
    VERIFY_EXPR(!m_pStaleTexture);

    TextureDesc Desc = GetDesc();
    Desc.ArraySize   = m_PendingSize;

    // Keep the committed texture and its views usable unless replacement
    // allocation succeeds.
    RefCntAutoPtr<ITexture> pNewTexture;
    pDevice->CreateTexture(Desc, nullptr, &pNewTexture);
    if (!pNewTexture)
        return false;

    ReleaseTextureViews();

    const Uint32 CurrArraySize = GetArraySize();
    if (CurrArraySize != 0)
    {
        VERIFY_EXPR(m_pTexture != nullptr);
        m_pStaleTexture = std::move(m_pTexture);
    }

    m_pTexture = std::move(pNewTexture);
    m_Version.fetch_add(1);
    UpdateTextureViews(pDevice);

    if (CurrArraySize == 0)
    {
        // There is no previous texture to copy.
        StoreArraySize(m_PendingSize);
    }

    return true;
}

void DynamicTextureArray::ReleaseTextureViews() noexcept
{
    m_SrgbSRV.Release();
}

void DynamicTextureArray::UpdateTextureViews(IRenderDevice* pDevice)
{
    VERIFY_EXPR(pDevice != nullptr);

    if (m_SrgbSRV != nullptr || m_pTexture == nullptr ||
        pDevice->GetDeviceInfo().Features.TextureSubresourceViews != DEVICE_FEATURE_STATE_ENABLED)
        return;

    const TEXTURE_FORMAT StorageFormat = m_pTexture->GetDesc().Format;
    if ((m_pTexture->GetDesc().BindFlags & BIND_SHADER_RESOURCE) == 0 ||
        !GetTextureFormatAttribs(StorageFormat).IsTypeless)
        return;

    ITextureView* const pDefaultSRV = m_pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (pDefaultSRV == nullptr)
    {
        UNEXPECTED("Failed to get default shader resource view of the texture");
        return;
    }

    const TEXTURE_FORMAT DefaultViewFormat = pDefaultSRV->GetDesc().Format;
    if (IsSRGBFormat(DefaultViewFormat))
    {
        UNEXPECTED("Default shader resource view format (", GetTextureFormatAttribs(DefaultViewFormat).Name,
                   ") is already sRGB. This is unexpected for a typeless texture.");
        return;
    }

    const TEXTURE_FORMAT SRGBViewFormat = UnormFormatToSRGB(DefaultViewFormat);
    if (SRGBViewFormat != DefaultViewFormat)
    {
        TextureViewDesc ViewDesc;
        ViewDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
        ViewDesc.Format   = SRGBViewFormat;
        m_pTexture->CreateView(ViewDesc, m_SrgbSRV.GetAddressOfEmpty());
    }
}

ITextureView* DynamicTextureArray::GetTextureSRV(TEXTURE_FORMAT ViewFormat) const
{
    ITextureView* const pDefaultSRV = m_pTexture != nullptr ? m_pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE) : nullptr;
    if (pDefaultSRV != nullptr && pDefaultSRV->GetDesc().Format == ViewFormat)
        return pDefaultSRV;

    if (m_SrgbSRV != nullptr && m_SrgbSRV->GetDesc().Format == ViewFormat)
        return m_SrgbSRV;

    return nullptr;
}


bool DynamicTextureArray::PrepareSparseResize()
{
    VERIFY_EXPR(m_pTexture && m_pMemory);

    const Uint64 ResidentSize = AlignUpNonPw2(Uint64{m_PendingSize}, Uint64{m_NumSlicesInPage});
    if (ResidentSize > m_pTexture->GetDesc().ArraySize)
    {
        LOG_ERROR_MESSAGE("Requested sparse dynamic texture array size exceeds the texture capacity after page alignment");
        return false;
    }

    m_PendingSize                = StaticCast<Uint32>(ResidentSize);
    const Uint64 RequiredMemSize = (m_PendingSize / m_NumSlicesInPage) * m_MemoryPageSize;
    if (RequiredMemSize > m_pMemory->GetCapacity())
    {
        if (!m_pMemory->Resize(RequiredMemSize))
        {
            LOG_ERROR_MESSAGE("Failed to resize sparse dynamic texture memory pool to ", RequiredMemSize, " bytes");
            return false;
        }
        const Uint64 MemoryCapacity = m_pMemory->GetCapacity();
        m_SparseMemoryUsage.store(MemoryCapacity, std::memory_order_release);
        if (MemoryCapacity < RequiredMemSize)
        {
            LOG_ERROR_MESSAGE("Failed to resize sparse dynamic texture memory pool to ", RequiredMemSize, " bytes. Actual capacity is ", MemoryCapacity, " bytes");
            return false;
        }
    }

    return true;
}

bool DynamicTextureArray::ValidateSparseContext(IDeviceContext* pContext) const
{
    VERIFY_EXPR(pContext != nullptr);

    const DeviceContextDesc& CtxDesc    = pContext->GetDesc();
    const bool               IsValidId  = CtxDesc.ContextId < sizeof(Uint64) * 8;
    const Uint64             ContextBit = IsValidId ? Uint64{1} << CtxDesc.ContextId : 0;
    const bool               SupportsSparse =
        (CtxDesc.QueueType & COMMAND_QUEUE_TYPE_SPARSE_BINDING) == COMMAND_QUEUE_TYPE_SPARSE_BINDING;

    if (CtxDesc.IsDeferred ||
        (m_Desc.ImmediateContextMask & ContextBit) == 0 ||
        !SupportsSparse)
    {
        LOG_ERROR_MESSAGE("Invalid context for sparse DynamicTextureArray resize");
        return false;
    }

    return true;
}

bool DynamicTextureArray::ResizeSparseTexture(IDeviceContext* pContext)
{
    const Uint32 CurrResidentSize = GetArraySize();
    VERIFY_EXPR(m_PendingSize != CurrResidentSize);
    VERIFY_EXPR(m_pTexture && m_pMemory);
    VERIFY_EXPR(m_PendingSize % m_NumSlicesInPage == 0);

    const Uint64 RequiredMemSize = (m_PendingSize / m_NumSlicesInPage) * m_MemoryPageSize;
    VERIFY_EXPR(RequiredMemSize <= m_pMemory->GetCapacity());
    const Uint32 NumSlicesToBind = m_PendingSize > CurrResidentSize ?
        m_PendingSize - CurrResidentSize :
        CurrResidentSize - m_PendingSize;

    Uint64 CurrMemOffset = Uint64{(m_PendingSize > CurrResidentSize ? CurrResidentSize : m_PendingSize) / m_NumSlicesInPage} * m_MemoryPageSize;

    const SparseTextureProperties& TexSparseProps = m_pTexture->GetSparseProperties();
    const Uint32                   NumNormalMips  = std::min(m_Desc.MipLevels, TexSparseProps.FirstMipInTail);
    const bool                     HasMipTail     = m_Desc.MipLevels > TexSparseProps.FirstMipInTail;

    std::vector<SparseTextureMemoryBindInfo> TexBinds;
    TexBinds.reserve(size_t{NumSlicesToBind} * (HasMipTail ? 2 : 1));
    std::vector<SparseTextureMemoryBindRange> MipRanges(size_t{NumSlicesToBind} * (size_t{NumNormalMips} + (HasMipTail ? 1 : 0)));

    auto   range_it   = MipRanges.begin();
    Uint32 StartSlice = std::min(CurrResidentSize, m_PendingSize);
    Uint32 EndSlice   = std::max(CurrResidentSize, m_PendingSize);
    for (Uint32 Slice = StartSlice; Slice != EndSlice; ++Slice)
    {
        // Bind normal mip levels, if any.
        if (NumNormalMips > 0)
        {
            SparseTextureMemoryBindInfo NormalMipBindInfo;
            NormalMipBindInfo.pTexture  = m_pTexture;
            NormalMipBindInfo.pRanges   = &*range_it;
            NormalMipBindInfo.NumRanges = NumNormalMips;
            for (Uint32 Mip = 0; Mip < NumNormalMips; ++Mip, ++range_it)
            {
                const MipLevelProperties MipProps = GetMipLevelProperties(m_Desc, Mip);

                range_it->ArraySlice = Slice;
                range_it->MipLevel   = Mip;
                range_it->Region     = Box{0, MipProps.StorageWidth, 0, MipProps.StorageHeight, 0, MipProps.Depth};

                if (Slice >= CurrResidentSize)
                {
                    const uint3 NumTilesInMip = GetNumSparseTilesInBox(range_it->Region, TexSparseProps.TileSize);
                    range_it->pMemory         = m_pMemory;
                    range_it->MemoryOffset    = CurrMemOffset;
                    range_it->MemorySize      = Uint64{NumTilesInMip.x} * NumTilesInMip.y * NumTilesInMip.z * TexSparseProps.BlockSize;

                    CurrMemOffset += range_it->MemorySize;
                }
                else
                {
                    // Unbind tile
                    range_it->pMemory = nullptr;
                }
            }
            TexBinds.push_back(NormalMipBindInfo);
        }

        // Bind mip tail
        if (HasMipTail)
        {
            SparseTextureMemoryBindInfo MipTailBindInfo;
            MipTailBindInfo.pTexture  = m_pTexture;
            MipTailBindInfo.pRanges   = &*range_it;
            MipTailBindInfo.NumRanges = 1;

            range_it->ArraySlice = Slice;
            range_it->MipLevel   = TexSparseProps.FirstMipInTail;
            range_it->MemorySize = TexSparseProps.MipTailSize;

            if (Slice >= CurrResidentSize)
            {
                range_it->pMemory      = m_pMemory;
                range_it->MemoryOffset = CurrMemOffset;

                CurrMemOffset += range_it->MemorySize;
            }
            else
            {
                // Unbind tile
                range_it->pMemory = nullptr;
            }
            ++range_it;

            TexBinds.push_back(MipTailBindInfo);
        }
    }
    VERIFY_EXPR(range_it == MipRanges.end());
    VERIFY_EXPR(CurrMemOffset == RequiredMemSize);

    BindSparseResourceMemoryAttribs BindMemAttribs;
    BindMemAttribs.NumTextureBinds = StaticCast<Uint32>(TexBinds.size());
    BindMemAttribs.pTextureBinds   = TexBinds.data();

    Uint64  WaitFenceValue = 0;
    IFence* pWaitFence     = nullptr;
    if (m_pBeforeResizeFence)
    {
        WaitFenceValue = m_NextBeforeResizeFenceValue++;
        pWaitFence     = m_pBeforeResizeFence;

        BindMemAttribs.NumWaitFences    = 1;
        BindMemAttribs.pWaitFenceValues = &WaitFenceValue;
        BindMemAttribs.ppWaitFences     = &pWaitFence;

        pContext->EnqueueSignal(m_pBeforeResizeFence, WaitFenceValue);
    }

    Uint64  SignalFenceValue = 0;
    IFence* pSignalFence     = nullptr;
    if (m_pAfterResizeFence)
    {
        SignalFenceValue = m_NextAfterResizeFenceValue++;
        pSignalFence     = m_pAfterResizeFence;

        BindMemAttribs.NumSignalFences    = 1;
        BindMemAttribs.pSignalFenceValues = &SignalFenceValue;
        BindMemAttribs.ppSignalFences     = &pSignalFence;
    }

    pContext->BindSparseResourceMemory(BindMemAttribs);
    if (pSignalFence != nullptr)
    {
        // Order subsequent commands submitted through this context after the
        // sparse mapping operation before publishing the new resident size.
        pContext->DeviceWaitForFence(pSignalFence, SignalFenceValue);
    }

    if (RequiredMemSize < m_pMemory->GetCapacity())
        m_pMemory->Resize(RequiredMemSize); // Release unused memory

    m_SparseMemoryUsage.store(m_pMemory->GetCapacity(), std::memory_order_release);
    return true;
}

void DynamicTextureArray::CopyStaleTextureContents(IDeviceContext* pContext)
{
    VERIFY_EXPR(m_PendingSize != GetArraySize());
    VERIFY_EXPR(pContext != nullptr);
    VERIFY_EXPR(m_pTexture && m_pStaleTexture);
    const TextureDesc& SrcTexDesc = m_pStaleTexture->GetDesc();
    const TextureDesc& DstTexDesc = m_pTexture->GetDesc();
    VERIFY_EXPR(SrcTexDesc.MipLevels == DstTexDesc.MipLevels);

    CopyTextureAttribs CopyAttribs;
    CopyAttribs.pSrcTexture              = m_pStaleTexture;
    CopyAttribs.pDstTexture              = m_pTexture;
    CopyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    CopyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    const Uint32 NumSlicesToCopy = std::min(SrcTexDesc.ArraySize, DstTexDesc.ArraySize);
    for (Uint32 slice = 0; slice < NumSlicesToCopy; ++slice)
    {
        for (Uint32 mip = 0; mip < SrcTexDesc.MipLevels; ++mip)
        {
            CopyAttribs.SrcSlice    = slice;
            CopyAttribs.DstSlice    = slice;
            CopyAttribs.SrcMipLevel = mip;
            CopyAttribs.DstMipLevel = mip;
            pContext->CopyTexture(CopyAttribs);
        }
    }
    m_pStaleTexture.Release();
}

void DynamicTextureArray::CommitResize(IRenderDevice*  pDevice,
                                       IDeviceContext* pContext,
                                       bool            AllowNull)
{
    if (!m_pTexture && m_PendingSize > 0)
    {
        if (pDevice == nullptr)
        {
            DEV_CHECK_ERR(AllowNull, "Dynamic texture array must be initialized, but pDevice is null");
            return;
        }

        if (!CreateResources(pDevice))
        {
            LOG_ERROR_MESSAGE("Failed to create texture for a dynamic texture array");
            return;
        }
    }
    else if (GetUsage() == USAGE_DEFAULT &&
             m_PendingSize > 0 &&
             m_pTexture->GetDesc().ArraySize != m_PendingSize)
    {
        if (pDevice == nullptr)
        {
            DEV_CHECK_ERR(AllowNull, "Dynamic texture array replacement must be created, but pDevice is null");
            return;
        }

        if (!CreateDefaultTexture(pDevice))
        {
            LOG_ERROR_MESSAGE("Failed to create texture for a dynamic texture array");
            return;
        }
    }

    const Uint32 UpdatedArraySize = GetArraySize();
    if (m_pTexture && UpdatedArraySize != m_PendingSize)
    {
        if (GetUsage() == USAGE_DEFAULT && m_pTexture->GetDesc().ArraySize != m_PendingSize)
        {
            LOG_ERROR_MESSAGE("Pending texture size does not match the requested array size");
            return;
        }

        bool ResizeCommitted = false;
        if (GetUsage() == USAGE_SPARSE)
        {
            if (pContext != nullptr && !ValidateSparseContext(pContext))
                return;

            if ((pDevice != nullptr || pContext != nullptr) && !PrepareSparseResize())
                return;

            if (m_PendingSize == UpdatedArraySize)
            {
                // The requested logical size fits in the currently resident
                // page range, so no sparse bindings need to change.
                ResizeCommitted = true;
            }
            else if (pContext != nullptr)
            {
                ResizeCommitted = ResizeSparseTexture(pContext);
            }
            else
            {
                DEV_CHECK_ERR(AllowNull, "Dynamic texture must be resized, but pContext is null. Use PendingUpdate() to check if the Texture must be updated.");
            }
        }
        else if (m_pStaleTexture)
        {
            if (pContext != nullptr)
            {
                CopyStaleTextureContents(pContext);
                ResizeCommitted = true;
            }
            else
            {
                DEV_CHECK_ERR(AllowNull, "Dynamic texture must be resized, but pContext is null. Use PendingUpdate() to check if the Texture must be updated.");
            }
        }
        else
        {
            // The old contents were discarded, or there was no previous texture.
            // No GPU copy is required, so the resize can be committed without a context.
            ResizeCommitted = true;
        }

        if (ResizeCommitted && m_PendingSize != UpdatedArraySize)
        {
            StoreArraySize(m_PendingSize);

            LOG_INFO_MESSAGE("Dynamic texture array: expanding texture '", m_Desc.Name,
                             "' (", m_Desc.Width, " x ", m_Desc.Height, " ", m_Desc.MipLevels, "-mip ",
                             GetTextureFormatAttribs(m_Desc.Format).Name, ") to ",
                             m_PendingSize, " slices. Version: ", GetVersion());
        }
    }
}

ITexture* DynamicTextureArray::Resize(IRenderDevice*  pDevice,
                                      IDeviceContext* pContext,
                                      Uint32          NewArraySize,
                                      bool            DiscardContent)
{
    if (GetUsage() == USAGE_DEFAULT &&
        m_pStaleTexture != nullptr &&
        NewArraySize != m_PendingSize)
    {
        LOG_ERROR_MESSAGE("A default texture resize is already pending. Commit it before requesting another size.");
        return m_pTexture;
    }

    if (m_PendingSize != NewArraySize || DiscardContent)
    {
        m_PendingSize = NewArraySize;

        if (GetUsage() == USAGE_DEFAULT)
        {
            if (m_PendingSize == 0)
            {
                ReleaseTextureViews();
                m_pStaleTexture.Release();
                m_pTexture.Release();
                StoreArraySize(0);
            }
            else if (DiscardContent)
            {
                // If the replacement already exists, dropping the copy source lets
                // CommitResize() publish it without a context. Otherwise, explicitly
                // discard the committed texture so a retry can allocate under lower
                // memory pressure.
                m_pStaleTexture.Release();
                if (m_pTexture == nullptr || m_pTexture->GetDesc().ArraySize != m_PendingSize)
                {
                    ReleaseTextureViews();
                    m_pTexture.Release();
                    StoreArraySize(0);
                }
            }
        }
    }

    CommitResize(pDevice, pContext, true /*AllowNull*/);

    return m_pTexture;
}

ITexture* DynamicTextureArray::Update(IRenderDevice*  pDevice,
                                      IDeviceContext* pContext)
{
    CommitResize(pDevice, pContext, false /*AllowNull*/);
    return m_pTexture;
}

Uint64 DynamicTextureArray::GetMemoryUsage() const
{
    Uint64 MemUsage = 0;
    if (GetUsage() == USAGE_SPARSE)
    {
        MemUsage = m_SparseMemoryUsage.load(std::memory_order_acquire);
    }
    else
    {
        for (Uint32 mip = 0; mip < m_Desc.MipLevels; ++mip)
            MemUsage += GetMipLevelProperties(m_Desc, mip).MipSize;

        MemUsage *= GetArraySize();
    }
    return MemUsage;
}

} // namespace Diligent
