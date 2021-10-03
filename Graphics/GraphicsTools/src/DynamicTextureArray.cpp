/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

namespace Diligent
{

namespace
{

bool VerifySparseTextureCompatibility(IRenderDevice* pDevice, const TextureDesc& TexDesc)
{
    VERIFY_EXPR(pDevice != nullptr);

    const auto& DeviceInfo = pDevice->GetDeviceInfo().Features;
    if (!DeviceInfo.SparseResources)
    {
        LOG_WARNING_MESSAGE("SparseResources device feature is not enabled.");
        return false;
    }

    const auto& SparseRes = pDevice->GetAdapterInfo().SparseResources;
    if ((SparseRes.CapFlags & SPARSE_RESOURCE_CAP_FLAG_TEXTURE_2D_ARRAY_MIP_TAIL) == 0)
    {
        LOG_WARNING_MESSAGE("This device does not support sparse texture 2D arrays with mip tails.");
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

    if (m_Desc.MipLevels == 0)
        m_Desc.MipLevels = ComputeMipLevelsCount(m_Desc.GetWidth(), m_Desc.GetHeight(), m_Desc.GetDepth());

    m_PendingSize    = m_Desc.ArraySize;
    m_Desc.ArraySize = 0; // Current array size
    if (pDevice != nullptr && (m_PendingSize > 0 || m_Desc.Usage == USAGE_SPARSE))
    {
        CreateResources(pDevice);
    }
}


void DynamicTextureArray::CreateSparseTexture(IRenderDevice* pDevice)
{
    VERIFY_EXPR(!m_pTexture && !m_pMemory);
    VERIFY_EXPR(pDevice != nullptr);
    VERIFY_EXPR(m_Desc.Usage == USAGE_SPARSE);

    if (!VerifySparseTextureCompatibility(pDevice, m_Desc))
    {
        LOG_WARNING_MESSAGE("This device does not support capabilities required for sparse texture 2D arrays. USAGE_DEFAULT texture will be used instead.");
        m_Desc.Usage = USAGE_DEFAULT;
        return;
    }

    const auto& AdapterInfo = pDevice->GetAdapterInfo();

    {
        auto TmpDesc = m_Desc;
        // Reserve the maximum available number of slices
        TmpDesc.ArraySize = AdapterInfo.Texture.MaxTexture2DArraySlices;
        pDevice->CreateTexture(TmpDesc, nullptr, &m_pTexture);
        DEV_CHECK_ERR(m_pTexture, "Failed to create sparse texture");
        if (!m_pTexture)
            return;

        // No slices are currently committed
        m_Desc.ArraySize = 0;
    }

    const auto& TexSparseProps = m_pTexture->GetSparseProperties();
    const auto  NumNormalMips  = std::min(m_Desc.MipLevels, TexSparseProps.FirstMipInTail);
    // Compute the total number of blocks in one slice
    Uint64 NumBlocksInSlice = 0;
    for (Uint32 Mip = 0; Mip < NumNormalMips; ++Mip)
    {
        const auto NumTilesInMip = GetNumSparseTilesInMipLevel(m_Desc, TexSparseProps, Mip);
        NumBlocksInSlice += Uint64{NumTilesInMip.x} * Uint64{NumTilesInMip.y} * Uint64{NumTilesInMip.z};
    }

    m_MemoryPageSize = NumBlocksInSlice * TexSparseProps.BlockSize;
    if (m_Desc.MipLevels > TexSparseProps.FirstMipInTail)
    {
        m_MemoryPageSize += TexSparseProps.MipTailSize;
    }

    m_MemoryPageSize *= m_NumSlicesInPage;

    // Create memory pool
    {
        DeviceMemoryCreateInfo MemCI;
        MemCI.Desc.Name     = "Sparse dynamic texture memory pool";
        MemCI.Desc.Type     = DEVICE_MEMORY_TYPE_SPARSE;
        MemCI.Desc.PageSize = m_MemoryPageSize;

        MemCI.InitialSize = m_MemoryPageSize;

        IDeviceObject* pCompatibleRes[]{m_pTexture};
        MemCI.ppCompatibleResources = pCompatibleRes;
        MemCI.NumResources          = _countof(pCompatibleRes);

        pDevice->CreateDeviceMemory(MemCI, &m_pMemory);
        DEV_CHECK_ERR(m_pMemory, "Failed to create device memory");
    }
}

void DynamicTextureArray::CreateResources(IRenderDevice* pDevice)
{
    VERIFY_EXPR(pDevice != nullptr);
    VERIFY(!m_pTexture, "The texture has already been initialized");
    VERIFY(!m_pMemory, "Memory has already been initialized");

    if (m_Desc.Usage == USAGE_SPARSE)
    {
        CreateSparseTexture(pDevice);
    }

    // NB: m_Desc.Usage may be changed by CreateSparseTexture()
    if (m_Desc.Usage == USAGE_DEFAULT && m_PendingSize > 0)
    {
        auto Desc      = m_Desc;
        Desc.ArraySize = m_PendingSize;
        pDevice->CreateTexture(Desc, nullptr, &m_pTexture);
        if (m_Desc.ArraySize == 0)
        {
            // The array was previously empty - nothing to copy
            m_Desc.ArraySize = m_PendingSize;
        }
    }
    DEV_CHECK_ERR(m_pTexture, "Failed to create texture for a dynamic texture array");

    m_Version.fetch_add(1);
}


void DynamicTextureArray::ResizeSparseTexture(IDeviceContext* pContext)
{
    VERIFY_EXPR(m_PendingSize != m_Desc.ArraySize);
    VERIFY_EXPR(m_pTexture && m_pMemory);

    m_PendingSize = AlignUp(m_PendingSize, m_NumSlicesInPage);

    const auto RequiredMemSize = (m_PendingSize / m_NumSlicesInPage) * m_MemoryPageSize;
    if (RequiredMemSize > m_pMemory->GetCapacity())
        m_pMemory->Resize(RequiredMemSize);

    const auto NumSlicesToBind = m_PendingSize > m_Desc.ArraySize ?
        m_PendingSize - m_Desc.ArraySize :
        m_Desc.ArraySize - m_PendingSize;

    auto CurrMemOffset = Uint64{(m_PendingSize > m_Desc.ArraySize ? m_Desc.ArraySize : m_PendingSize) / m_NumSlicesInPage} * m_MemoryPageSize;

    const auto& TexSparseProps = m_pTexture->GetSparseProperties();
    const auto  NumNormalMips  = std::min(m_Desc.MipLevels, TexSparseProps.FirstMipInTail);
    const auto  HasMipTail     = m_Desc.MipLevels > TexSparseProps.FirstMipInTail;

    std::vector<SparseTextureMemoryBindInfo> TexBinds;
    TexBinds.reserve(NumSlicesToBind * (HasMipTail ? 2 : 1));
    std::vector<SparseTextureMemoryBindRange> MipRanges(NumSlicesToBind * (NumNormalMips + (HasMipTail ? 1 : 0)));

    auto range_it   = MipRanges.begin();
    auto StartSlice = std::min(m_Desc.ArraySize, m_PendingSize);
    auto EndSlice   = std::max(m_Desc.ArraySize, m_PendingSize);
    for (auto Slice = StartSlice; Slice != EndSlice; ++Slice)
    {
        // Bind normal mip levels
        {
            SparseTextureMemoryBindInfo NormalMipBindInfo;
            NormalMipBindInfo.pTexture  = m_pTexture;
            NormalMipBindInfo.pRanges   = &*range_it;
            NormalMipBindInfo.NumRanges = NumNormalMips;
            for (Uint32 Mip = 0; Mip < NumNormalMips; ++Mip, ++range_it)
            {
                const auto MipProps = GetMipLevelProperties(m_Desc, Mip);

                range_it->ArraySlice = Slice;
                range_it->MipLevel   = Mip;
                range_it->Region     = Box{0, MipProps.StorageWidth, 0, MipProps.StorageHeight, 0, MipProps.Depth};

                if (Slice >= m_Desc.ArraySize)
                {
                    const auto NumTilesInMip = GetNumSparseTilesInBox(range_it->Region, TexSparseProps);
                    range_it->pMemory        = m_pMemory;
                    range_it->MemoryOffset   = CurrMemOffset;
                    range_it->MemorySize     = Uint64{NumTilesInMip.x} * NumTilesInMip.y * NumTilesInMip.z * TexSparseProps.BlockSize;

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

            if (Slice >= m_Desc.ArraySize)
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
    pContext->BindSparseResourceMemory(BindMemAttribs);

    if (RequiredMemSize < m_pMemory->GetCapacity())
        m_pMemory->Resize(RequiredMemSize);
}

void DynamicTextureArray::ResizeDefaultTexture(IDeviceContext* pContext)
{
    VERIFY_EXPR(m_PendingSize != m_Desc.ArraySize);
    VERIFY_EXPR(m_pTexture && m_pStaleTexture);
    const auto& SrcTexDesc = m_pStaleTexture->GetDesc();
    const auto& DstTexDesc = m_pTexture->GetDesc();
    VERIFY_EXPR(SrcTexDesc.MipLevels == DstTexDesc.MipLevels);

    CopyTextureAttribs CopyAttribs;
    CopyAttribs.pSrcTexture              = m_pStaleTexture;
    CopyAttribs.pDstTexture              = m_pTexture;
    CopyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    CopyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    const auto NumSlicesToCopy = std::min(SrcTexDesc.ArraySize, DstTexDesc.ArraySize);
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
        if (pDevice != nullptr)
            CreateResources(pDevice);
        else
            DEV_CHECK_ERR(AllowNull, "Sparse texture must be initialized, but pDevice is null");
    }

    if (m_pTexture && m_Desc.ArraySize != m_PendingSize)
    {
        if (pContext != nullptr)
        {
            if (m_Desc.Usage == USAGE_SPARSE)
                ResizeSparseTexture(pContext);
            else
                ResizeDefaultTexture(pContext);

            m_Desc.ArraySize = m_PendingSize;

            LOG_INFO_MESSAGE("Dynamic texture array: expanding texture '", m_Desc.Name,
                             "' (", m_Desc.Width, " x ", m_Desc.Height, " ", m_Desc.MipLevels, "-mip ",
                             GetTextureFormatAttribs(m_Desc.Format).Name, ") to ",
                             m_Desc.ArraySize, " slices. Version: ", GetVersion());
        }
        else
        {
            DEV_CHECK_ERR(AllowNull, "Dynamic texture must be resized, but pContext is null. Use PendingUpdate() to check if the Texture must be updated.");
        }
    }
}

ITexture* DynamicTextureArray::Resize(IRenderDevice*  pDevice,
                                      IDeviceContext* pContext,
                                      Uint32          NewArraySize,
                                      bool            DiscardContent)
{
    if (m_Desc.ArraySize != NewArraySize)
    {
        m_PendingSize = NewArraySize;

        if (m_Desc.Usage != USAGE_SPARSE)
        {
            if (!m_pStaleTexture)
                m_pStaleTexture = std::move(m_pTexture);
            else
            {
                DEV_CHECK_ERR(!m_pTexture || NewArraySize == 0,
                              "There is a non-null stale Texture. This likely indicates that "
                              "Resize() has been called multiple times with different sizes, "
                              "but copy has not been committed by providing non-null device "
                              "context to either Resize() or GetTexture()");
            }

            if (m_PendingSize == 0)
            {
                m_pStaleTexture.Release();
                m_pTexture.Release();
                m_Desc.ArraySize = 0;
            }

            if (DiscardContent)
                m_pStaleTexture.Release();
        }
    }

    CommitResize(pDevice, pContext, true /*AllowNull*/);

    return m_pTexture;
}

ITexture* DynamicTextureArray::GetTexture(IRenderDevice*  pDevice,
                                          IDeviceContext* pContext)
{
    CommitResize(pDevice, pContext, false /*AllowNull*/);
    return m_pTexture;
}

Uint64 DynamicTextureArray::GetMemoryUsage() const
{
    Uint64 MemUsage = 0;
    if (m_Desc.Usage == USAGE_SPARSE)
    {
        MemUsage = m_pMemory ? m_pMemory->GetCapacity() : 0;
    }
    else
    {
        for (Uint32 mip = 0; mip < m_Desc.MipLevels; ++mip)
            MemUsage += GetMipLevelProperties(m_Desc, mip).MipSize;

        MemUsage *= m_Desc.ArraySize;
    }
    return MemUsage;
}

} // namespace Diligent
