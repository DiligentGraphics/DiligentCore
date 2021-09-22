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

#include "Vulkan/TestingEnvironmentVk.hpp"
#include "Vulkan/TestingSwapChainVk.hpp"

#include "DeviceContextVk.h"
#include "RenderDeviceVk.h"
#include "TextureVk.h"
#include "BufferVk.h"

#include "volk/volk.h"

#include "SparseMemoryTest.hpp"

namespace Diligent
{

namespace Testing
{

namespace
{

constexpr Uint32 StandardBlockSize = 64u << 10; // from specs

struct BufferWrap
{
    VkBuffer Handle = VK_NULL_HANDLE;

    BufferWrap() {}

    BufferWrap(BufferWrap&& Other) :
        Handle{Other.Handle}
    {
        Other.Handle = VK_NULL_HANDLE;
    }

    ~BufferWrap()
    {
        if (Handle != VK_NULL_HANDLE)
        {
            auto vkDevice = TestingEnvironmentVk::GetInstance()->GetVkDevice();
            vkDestroyBuffer(vkDevice, Handle, nullptr);
        }
    }

    explicit operator bool() const { return Handle != VK_NULL_HANDLE; }
};

struct ImageWrap
{
    VkImage Handle = VK_NULL_HANDLE;

    ImageWrap() {}

    ImageWrap(ImageWrap&& Other) :
        Handle{Other.Handle}
    {
        Other.Handle = VK_NULL_HANDLE;
    }

    ~ImageWrap()
    {
        if (Handle != VK_NULL_HANDLE)
        {
            auto vkDevice = TestingEnvironmentVk::GetInstance()->GetVkDevice();
            vkDestroyImage(vkDevice, Handle, nullptr);
        }
    }

    explicit operator bool() const { return Handle != VK_NULL_HANDLE; }
};

struct DeviceMemoryWrap
{
    VkDeviceMemory Handle = VK_NULL_HANDLE;

    DeviceMemoryWrap() {}

    DeviceMemoryWrap(DeviceMemoryWrap&& Other) :
        Handle{Other.Handle}
    {
        Other.Handle = VK_NULL_HANDLE;
    }

    ~DeviceMemoryWrap()
    {
        if (Handle != VK_NULL_HANDLE)
        {
            auto vkDevice = TestingEnvironmentVk::GetInstance()->GetVkDevice();
            vkFreeMemory(vkDevice, Handle, nullptr);
        }
    }

    explicit operator bool() const { return Handle != VK_NULL_HANDLE; }
};


BufferWrap CreateSparseBuffer(VkDeviceSize Size, VkBufferCreateFlags Flags = 0)
{
    auto vkDevice = TestingEnvironmentVk::GetInstance()->GetVkDevice();

    VkBufferCreateInfo BuffCI{};
    BuffCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BuffCI.size  = Size;
    BuffCI.flags = VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT | Flags;
    BuffCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    BufferWrap Result;
    vkCreateBuffer(vkDevice, &BuffCI, nullptr, &Result.Handle);
    return Result;
}

ImageWrap CreateSparseImage(const int4& Dim, Uint32& MipLevels, VkImageCreateFlags Flags = 0)
{
    auto vkDevice = TestingEnvironmentVk::GetInstance()->GetVkDevice();

    MipLevels = ComputeMipLevelsCount(Dim.x, Dim.y, Dim.z);

    VkImageCreateInfo ImgCI{};
    ImgCI.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImgCI.flags         = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | Flags;
    ImgCI.imageType     = Dim.z > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    ImgCI.format        = VK_FORMAT_R8G8B8A8_UNORM;
    ImgCI.extent.width  = Dim.x;
    ImgCI.extent.height = Dim.y;
    ImgCI.extent.depth  = Dim.z;
    ImgCI.mipLevels     = MipLevels;
    ImgCI.arrayLayers   = Dim.w;
    ImgCI.samples       = VK_SAMPLE_COUNT_1_BIT;
    ImgCI.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ImgCI.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    ImgCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    ImageWrap Result;
    vkCreateImage(vkDevice, &ImgCI, nullptr, &Result.Handle);
    return Result;
}

DeviceMemoryWrap CreateMemory(VkDevice Dev, VkDeviceSize Size, uint32_t TypeIndex)
{
    VkMemoryAllocateInfo AllocInfo{};
    AllocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    AllocInfo.allocationSize  = Size;
    AllocInfo.memoryTypeIndex = TypeIndex;

    DeviceMemoryWrap Result;
    vkAllocateMemory(Dev, &AllocInfo, nullptr, &Result.Handle);
    return Result;
}

DeviceMemoryWrap CreateMemoryForBuffer(VkDeviceSize Size, VkBuffer Buffer)
{
    auto pEnv     = TestingEnvironmentVk::GetInstance();
    auto vkDevice = pEnv->GetVkDevice();

    VkMemoryRequirements MemReq{};
    vkGetBufferMemoryRequirements(vkDevice, Buffer, &MemReq);

    // Vulkan spec does not guarantie that buffer contains 64Kb blocks.
    VERIFY_EXPR(StandardBlockSize == MemReq.alignment);

    auto TypeIndex = pEnv->GetMemoryTypeIndex(MemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    return CreateMemory(vkDevice, Size, TypeIndex);
}

DeviceMemoryWrap CreateMemoryForImage(VkDeviceSize Size, VkImage Image)
{
    auto pEnv     = TestingEnvironmentVk::GetInstance();
    auto vkDevice = pEnv->GetVkDevice();

    VkMemoryRequirements MemReq{};
    vkGetImageMemoryRequirements(vkDevice, Image, &MemReq);

    // Texture may have non-standard block size, this is not supported in tests
    VERIFY_EXPR(StandardBlockSize == MemReq.alignment);

    auto TypeIndex = pEnv->GetMemoryTypeIndex(MemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    return CreateMemory(vkDevice, Size, TypeIndex);
}

RefCntAutoPtr<IBuffer> CreateBufferFromVkBuffer(VkBuffer Buffer, Uint64 Size)
{
    auto* pEnv = TestingEnvironmentVk::GetInstance();

    RefCntAutoPtr<IRenderDeviceVk> pDeviceVk(pEnv->GetDevice(), IID_RenderDeviceVk);
    if (pDeviceVk == nullptr)
        return {};

    BufferDesc BuffDesc;
    BuffDesc.Name              = "Sparse buffer from Vulkan resource";
    BuffDesc.Size              = Size;
    BuffDesc.Usage             = USAGE_SPARSE;
    BuffDesc.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = 4;

    RefCntAutoPtr<IBuffer> pBufferWrapper;
    pDeviceVk->CreateBufferFromVulkanResource(Buffer, BuffDesc, RESOURCE_STATE_UNDEFINED, &pBufferWrapper);
    return pBufferWrapper;
}

RefCntAutoPtr<ITexture> CreateTextureFromVkImage(VkImage Image, const int4& Dim, Uint32 MipLevels)
{
    auto* pEnv = TestingEnvironmentVk::GetInstance();

    RefCntAutoPtr<IRenderDeviceVk> pDeviceVk(pEnv->GetDevice(), IID_RenderDeviceVk);
    if (pDeviceVk == nullptr)
        return {};

    TextureDesc TexDesc;
    TexDesc.Name      = "Sparse texture from Vulkan resource";
    TexDesc.Type      = Dim.z > 1 ? RESOURCE_DIM_TEX_3D : (Dim.w > 1 ? RESOURCE_DIM_TEX_2D_ARRAY : RESOURCE_DIM_TEX_2D);
    TexDesc.Width     = Dim.x;
    TexDesc.Height    = Dim.y;
    TexDesc.Depth     = TexDesc.Type == RESOURCE_DIM_TEX_3D ? Dim.z : Dim.w;
    TexDesc.MipLevels = MipLevels;
    TexDesc.Usage     = USAGE_SPARSE;
    TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
    TexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;

    RefCntAutoPtr<ITexture> pTextureWrapper;
    pDeviceVk->CreateTextureFromVulkanImage(Image, TexDesc, RESOURCE_STATE_UNDEFINED, &pTextureWrapper);
    return pTextureWrapper;
}

template <typename FnType>
void WithCmdQueue(const FnType& Fn)
{
    auto* pEnv     = TestingEnvironmentVk::GetInstance();
    auto* pContext = pEnv->GetDeviceContext();

    RefCntAutoPtr<ICommandQueueVk> pQueueVk{pContext->LockCommandQueue(), IID_CommandQueueVk};

    auto vkQueue = pQueueVk->GetVkQueue();

    Fn(vkQueue);

    vkQueueWaitIdle(vkQueue);

    pContext->UnlockCommandQueue();
}

void GetSparseRequirements(VkImage Image, std::vector<VkSparseImageMemoryRequirements>& SparseReq)
{
    auto     vkDevice       = TestingEnvironmentVk::GetInstance()->GetVkDevice();
    uint32_t SparseReqCount = 0;
    vkGetImageSparseMemoryRequirements(vkDevice, Image, &SparseReqCount, nullptr);

    if (SparseReqCount == 0)
        return;

    SparseReq.resize(SparseReqCount);
    vkGetImageSparseMemoryRequirements(vkDevice, Image, &SparseReqCount, SparseReq.data());
}

} // namespace


void SparseMemorySparseBufferTestVk(const SparseMemoryTestBufferHelper& Helper)
{
    const Uint32 BufferSize = StandardBlockSize * 4;
    VERIFY_EXPR(BufferSize == Helper.BufferSize);

    auto Buffer = CreateSparseBuffer(BufferSize);
    ASSERT_TRUE(Buffer);

    auto Memory = CreateMemoryForBuffer(StandardBlockSize * 6, Buffer.Handle);
    ASSERT_TRUE(Memory);

    WithCmdQueue([&](VkQueue vkQueue) {
        VkSparseMemoryBind Binds[4] = {};

        Binds[0].resourceOffset = StandardBlockSize * 0;
        Binds[1].resourceOffset = StandardBlockSize * 1;
        Binds[2].resourceOffset = StandardBlockSize * 2;
        Binds[3].resourceOffset = StandardBlockSize * 3;

        Binds[0].memoryOffset = StandardBlockSize * 0;
        Binds[1].memoryOffset = StandardBlockSize * 1;
        Binds[2].memoryOffset = StandardBlockSize * 3;
        Binds[3].memoryOffset = StandardBlockSize * 5;

        Binds[0].memory = Memory.Handle;
        Binds[1].memory = Memory.Handle;
        Binds[2].memory = Memory.Handle;
        Binds[3].memory = Memory.Handle;

        Binds[0].size = StandardBlockSize;
        Binds[1].size = StandardBlockSize;
        Binds[2].size = StandardBlockSize;
        Binds[3].size = StandardBlockSize;

        VkSparseBufferMemoryBindInfo BuffBind{};
        BuffBind.buffer    = Buffer.Handle;
        BuffBind.bindCount = _countof(Binds);
        BuffBind.pBinds    = Binds;

        VkBindSparseInfo BindInfo{};
        BindInfo.sType           = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        BindInfo.bufferBindCount = 1;
        BindInfo.pBufferBinds    = &BuffBind;

        vkQueueBindSparse(vkQueue, 1, &BindInfo, VK_NULL_HANDLE);
    });

    auto pBufferWrapper = CreateBufferFromVkBuffer(Buffer.Handle, BufferSize);
    ASSERT_NE(pBufferWrapper, nullptr);

    Helper.FillAndDraw(pBufferWrapper);
}


void SparseMemorySparseResidentBufferTestVk(const SparseMemoryTestBufferHelper& Helper)
{
    const Uint32 BufferSize = StandardBlockSize * 8;
    VERIFY_EXPR(BufferSize == Helper.BufferSize);

    auto Buffer = CreateSparseBuffer(BufferSize);
    ASSERT_TRUE(Buffer);

    auto Memory = CreateMemoryForBuffer(StandardBlockSize * 6, Buffer.Handle);
    ASSERT_TRUE(Memory);

    WithCmdQueue([&](VkQueue vkQueue) {
        VkSparseMemoryBind Binds[4] = {};

        Binds[0].resourceOffset = StandardBlockSize * 0;
        Binds[1].resourceOffset = StandardBlockSize * 2;
        Binds[2].resourceOffset = StandardBlockSize * 3;
        Binds[3].resourceOffset = StandardBlockSize * 6;

        Binds[0].memoryOffset = StandardBlockSize * 0;
        Binds[1].memoryOffset = StandardBlockSize * 1;
        Binds[2].memoryOffset = StandardBlockSize * 3;
        Binds[3].memoryOffset = StandardBlockSize * 5;

        Binds[0].memory = Memory.Handle;
        Binds[1].memory = Memory.Handle;
        Binds[2].memory = Memory.Handle;
        Binds[3].memory = Memory.Handle;

        Binds[0].size = StandardBlockSize;
        Binds[1].size = StandardBlockSize;
        Binds[2].size = StandardBlockSize;
        Binds[3].size = StandardBlockSize;

        VkSparseBufferMemoryBindInfo BuffBind{};
        BuffBind.buffer    = Buffer.Handle;
        BuffBind.bindCount = _countof(Binds);
        BuffBind.pBinds    = Binds;

        VkBindSparseInfo BindInfo{};
        BindInfo.sType           = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        BindInfo.bufferBindCount = 1;
        BindInfo.pBufferBinds    = &BuffBind;

        vkQueueBindSparse(vkQueue, 1, &BindInfo, VK_NULL_HANDLE);
    });

    auto pBufferWrapper = CreateBufferFromVkBuffer(Buffer.Handle, BufferSize);
    ASSERT_NE(pBufferWrapper, nullptr);

    Helper.FillAndDraw(pBufferWrapper);
}


void SparseMemorySparseResidentAliasedBufferTestVk(const SparseMemoryTestBufferHelper& Helper)
{
    const Uint32 BufferSize = StandardBlockSize * 8;
    VERIFY_EXPR(BufferSize == Helper.BufferSize);

    auto Buffer = CreateSparseBuffer(BufferSize, VK_BUFFER_CREATE_SPARSE_ALIASED_BIT);
    ASSERT_TRUE(Buffer);

    auto Memory = CreateMemoryForBuffer(StandardBlockSize * 6, Buffer.Handle);
    ASSERT_TRUE(Memory);

    WithCmdQueue([&](VkQueue vkQueue) {
        VkSparseMemoryBind Binds[5] = {};

        Binds[0].resourceOffset = StandardBlockSize * 0;
        Binds[1].resourceOffset = StandardBlockSize * 1;
        Binds[2].resourceOffset = StandardBlockSize * 2;
        Binds[3].resourceOffset = StandardBlockSize * 3;
        Binds[4].resourceOffset = StandardBlockSize * 5;

        Binds[0].memoryOffset = StandardBlockSize * 0;
        Binds[1].memoryOffset = StandardBlockSize * 2;
        Binds[2].memoryOffset = StandardBlockSize * 0;
        Binds[3].memoryOffset = StandardBlockSize * 1;
        Binds[4].memoryOffset = StandardBlockSize * 5;

        Binds[0].memory = Memory.Handle;
        Binds[1].memory = Memory.Handle;
        Binds[2].memory = Memory.Handle;
        Binds[3].memory = Memory.Handle;
        Binds[4].memory = Memory.Handle;

        Binds[0].size = StandardBlockSize;
        Binds[1].size = StandardBlockSize;
        Binds[2].size = StandardBlockSize;
        Binds[3].size = StandardBlockSize;
        Binds[4].size = StandardBlockSize;

        VkSparseBufferMemoryBindInfo BuffBind{};
        BuffBind.buffer    = Buffer.Handle;
        BuffBind.bindCount = _countof(Binds);
        BuffBind.pBinds    = Binds;

        VkBindSparseInfo BindInfo{};
        BindInfo.sType           = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        BindInfo.bufferBindCount = 1;
        BindInfo.pBufferBinds    = &BuffBind;

        vkQueueBindSparse(vkQueue, 1, &BindInfo, VK_NULL_HANDLE);
    });

    auto pBufferWrapper = CreateBufferFromVkBuffer(Buffer.Handle, BufferSize);
    ASSERT_NE(pBufferWrapper, nullptr);

    Helper.FillAndDraw(pBufferWrapper);
}


void SparseMemorySparseTextureTestVk(const SparseMemoryTestTextureHelper& Helper)
{
    const auto   TexDim    = Helper.TextureSize;
    Uint32       MipLevels = 0;
    const Uint64 PoolSize  = StandardBlockSize * 8 * TexDim.w;

    auto Texture = CreateSparseImage(TexDim, MipLevels);
    ASSERT_TRUE(Texture);

    auto Memory = CreateMemoryForImage(PoolSize, Texture.Handle);
    ASSERT_TRUE(Memory);

    std::vector<VkSparseImageMemoryRequirements> SparseReq;
    GetSparseRequirements(Texture.Handle, SparseReq);
    ASSERT_EQ(SparseReq.size(), 1u);

    const auto& MipInfo  = SparseReq.front();
    const auto& TileSize = SparseReq.front().formatProperties.imageGranularity;

    WithCmdQueue([&](VkQueue vkQueue) {
        std::vector<VkSparseImageMemoryBind> Binds;
        std::vector<VkSparseMemoryBind>      OpaqueBinds;

        Uint64 MemOffset = 0;
        for (Uint32 Slice = 0; Slice < static_cast<Uint32>(TexDim.w); ++Slice)
        {
            for (Uint32 Mip = 0; Mip < MipInfo.imageMipTailFirstLod; ++Mip)
            {
                const int Width  = std::max(1u, static_cast<Uint32>(TexDim.x) >> Mip);
                const int Height = std::max(1u, static_cast<Uint32>(TexDim.y) >> Mip);
                for (int y = 0; y < Height; y += TileSize.height)
                {
                    for (int x = 0; x < Width; x += TileSize.width)
                    {
                        Binds.emplace_back();
                        auto& Bind = Binds.back();

                        Bind.memory                 = Memory.Handle;
                        Bind.memoryOffset           = MemOffset;
                        Bind.flags                  = 0;
                        Bind.extent.width           = std::min(TileSize.width, static_cast<uint32_t>(Width - x));
                        Bind.extent.height          = std::min(TileSize.height, static_cast<uint32_t>(Height - y));
                        Bind.extent.depth           = 1;
                        Bind.offset                 = {x, y, 0};
                        Bind.subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        Bind.subresource.arrayLayer = Slice;
                        Bind.subresource.mipLevel   = Mip;

                        MemOffset += StandardBlockSize;
                    }
                }
            }

            // Mip tail
            if (Slice == 0 || (MipInfo.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) == 0)
            {
                for (Uint32 OffsetInMipTail = 0; OffsetInMipTail < MipInfo.imageMipTailSize; OffsetInMipTail += StandardBlockSize)
                {
                    OpaqueBinds.emplace_back();
                    auto& Bind = OpaqueBinds.back();

                    Bind.resourceOffset = MipInfo.imageMipTailOffset + OffsetInMipTail + MipInfo.imageMipTailStride * Slice;
                    Bind.memory         = Memory.Handle;
                    Bind.memoryOffset   = MemOffset;
                    Bind.size           = StandardBlockSize;
                    Bind.flags          = 0;

                    MemOffset += Bind.size;
                }
            }
        }
        VERIFY_EXPR(MemOffset <= PoolSize);

        VkSparseImageMemoryBindInfo ImageBind{};
        ImageBind.image     = Texture.Handle;
        ImageBind.bindCount = static_cast<Uint32>(Binds.size());
        ImageBind.pBinds    = Binds.data();

        VkSparseImageOpaqueMemoryBindInfo ImgOpaqueBind{};
        ImgOpaqueBind.image     = Texture.Handle;
        ImgOpaqueBind.bindCount = static_cast<Uint32>(OpaqueBinds.size());
        ImgOpaqueBind.pBinds    = OpaqueBinds.data();

        VkBindSparseInfo BindInfo{};
        BindInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        if (ImageBind.bindCount > 0)
        {
            BindInfo.imageBindCount = 1;
            BindInfo.pImageBinds    = &ImageBind;
        }
        if (ImgOpaqueBind.bindCount > 0)
        {
            BindInfo.imageOpaqueBindCount = 1;
            BindInfo.pImageOpaqueBinds    = &ImgOpaqueBind;
        }
        vkQueueBindSparse(vkQueue, 1, &BindInfo, VK_NULL_HANDLE);
    });

    auto pTextureWrapper = CreateTextureFromVkImage(Texture.Handle, TexDim, MipLevels);
    ASSERT_NE(pTextureWrapper, nullptr);

    Helper.FillAndDraw(pTextureWrapper);
}


void SparseMemorySparseResidencyTextureTestVk(const SparseMemoryTestTextureHelper& Helper)
{
    const auto   TexDim    = Helper.TextureSize;
    Uint32       MipLevels = 0;
    const Uint64 PoolSize  = StandardBlockSize * 8 * TexDim.w;

    auto Texture = CreateSparseImage(TexDim, MipLevels);
    ASSERT_TRUE(Texture);

    auto Memory = CreateMemoryForImage(PoolSize, Texture.Handle);
    ASSERT_TRUE(Memory);

    std::vector<VkSparseImageMemoryRequirements> SparseReq;
    GetSparseRequirements(Texture.Handle, SparseReq);
    ASSERT_EQ(SparseReq.size(), 1u);

    const auto& MipInfo  = SparseReq.front();
    const auto& TileSize = SparseReq.front().formatProperties.imageGranularity;

    WithCmdQueue([&](VkQueue vkQueue) {
        std::vector<VkSparseImageMemoryBind> Binds;
        std::vector<VkSparseMemoryBind>      OpaqueBinds;

        Uint64 MemOffset = 0;
        for (Uint32 Slice = 0; Slice < static_cast<Uint32>(TexDim.w); ++Slice)
        {
            for (Uint32 Mip = 0, Idx = 0; Mip < MipInfo.imageMipTailFirstLod; ++Mip)
            {
                const int Width  = std::max(1u, static_cast<Uint32>(TexDim.x) >> Mip);
                const int Height = std::max(1u, static_cast<Uint32>(TexDim.y) >> Mip);
                for (int y = 0; y < Height; y += TileSize.height)
                {
                    for (int x = 0; x < Width; x += TileSize.width)
                    {
                        Binds.emplace_back();
                        auto& Bind = Binds.back();

                        Bind.flags                  = 0;
                        Bind.extent.width           = std::min(TileSize.width, static_cast<uint32_t>(Width - x));
                        Bind.extent.height          = std::min(TileSize.height, static_cast<uint32_t>(Height - y));
                        Bind.extent.depth           = 1;
                        Bind.offset                 = {x, y, 0};
                        Bind.subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        Bind.subresource.arrayLayer = Slice;
                        Bind.subresource.mipLevel   = Mip;

                        if ((++Idx & 2) == 0)
                        {
                            Bind.memory       = Memory.Handle;
                            Bind.memoryOffset = MemOffset;
                            MemOffset += StandardBlockSize;
                        }
                    }
                }
            }

            // Mip tail
            if (Slice == 0 || (MipInfo.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) == 0)
            {
                for (Uint32 OffsetInMipTail = 0; OffsetInMipTail < MipInfo.imageMipTailSize; OffsetInMipTail += StandardBlockSize)
                {
                    OpaqueBinds.emplace_back();
                    auto& Bind = OpaqueBinds.back();

                    Bind.resourceOffset = MipInfo.imageMipTailOffset + OffsetInMipTail + MipInfo.imageMipTailStride * Slice;
                    Bind.memory         = Memory.Handle;
                    Bind.memoryOffset   = MemOffset;
                    Bind.size           = StandardBlockSize;
                    Bind.flags          = 0;

                    MemOffset += Bind.size;
                }
            }
        }
        VERIFY_EXPR(MemOffset <= PoolSize);

        VkSparseImageMemoryBindInfo ImageBind{};
        ImageBind.image     = Texture.Handle;
        ImageBind.bindCount = static_cast<Uint32>(Binds.size());
        ImageBind.pBinds    = Binds.data();

        VkSparseImageOpaqueMemoryBindInfo ImgOpaqueBind{};
        ImgOpaqueBind.image     = Texture.Handle;
        ImgOpaqueBind.bindCount = static_cast<Uint32>(OpaqueBinds.size());
        ImgOpaqueBind.pBinds    = OpaqueBinds.data();

        VkBindSparseInfo BindInfo{};
        BindInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        if (ImageBind.bindCount > 0)
        {
            BindInfo.imageBindCount = 1;
            BindInfo.pImageBinds    = &ImageBind;
        }
        if (ImgOpaqueBind.bindCount > 0)
        {
            BindInfo.imageOpaqueBindCount = 1;
            BindInfo.pImageOpaqueBinds    = &ImgOpaqueBind;
        }
        vkQueueBindSparse(vkQueue, 1, &BindInfo, VK_NULL_HANDLE);
    });

    auto pTextureWrapper = CreateTextureFromVkImage(Texture.Handle, TexDim, MipLevels);
    ASSERT_NE(pTextureWrapper, nullptr);

    Helper.FillAndDraw(pTextureWrapper);
}


void SparseMemorySparseResidencyAliasedTextureTestVk(const SparseMemoryTestTextureHelper& Helper)
{
    const auto   TexDim    = Helper.TextureSize;
    Uint32       MipLevels = 0;
    const Uint64 PoolSize  = StandardBlockSize * 8 * TexDim.w;

    auto Texture = CreateSparseImage(TexDim, MipLevels, VK_IMAGE_CREATE_SPARSE_ALIASED_BIT);
    ASSERT_TRUE(Texture);

    auto Memory = CreateMemoryForImage(PoolSize, Texture.Handle);
    ASSERT_TRUE(Memory);

    std::vector<VkSparseImageMemoryRequirements> SparseReq;
    GetSparseRequirements(Texture.Handle, SparseReq);
    ASSERT_EQ(SparseReq.size(), 1u);

    const auto& MipInfo  = SparseReq.front();
    const auto& TileSize = SparseReq.front().formatProperties.imageGranularity;

    WithCmdQueue([&](VkQueue vkQueue) {
        std::vector<VkSparseImageMemoryBind> Binds;
        std::vector<VkSparseMemoryBind>      OpaqueBinds;

        // Mip tail - must not alias with other tiles
        Uint64       InitialOffset = 0;
        const Uint32 MipTailSlices = (MipInfo.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) != 0 ? 1 : static_cast<Uint32>(TexDim.w);
        for (Uint32 Slice = 0; Slice < MipTailSlices; ++Slice)
        {
            for (Uint32 OffsetInMipTail = 0; OffsetInMipTail < MipInfo.imageMipTailSize;)
            {
                OpaqueBinds.emplace_back();
                auto& Bind = OpaqueBinds.back();

                Bind.resourceOffset = MipInfo.imageMipTailOffset + OffsetInMipTail + MipInfo.imageMipTailStride * Slice;
                Bind.memory         = Memory.Handle;
                Bind.memoryOffset   = InitialOffset;
                Bind.size           = StandardBlockSize;
                Bind.flags          = 0;
                InitialOffset += Bind.size;
                OffsetInMipTail += StandardBlockSize;
            }
        }

        // tiles may alias
        for (Uint32 Slice = 0; Slice < static_cast<Uint32>(TexDim.w); ++Slice)
        {
            Uint64 MemOffset = InitialOffset;
            for (Uint32 Mip = 0, Idx = 0; Mip < MipInfo.imageMipTailFirstLod; ++Mip)
            {
                const int Width  = std::max(1u, static_cast<Uint32>(TexDim.x) >> Mip);
                const int Height = std::max(1u, static_cast<Uint32>(TexDim.y) >> Mip);
                for (int y = 0; y < Height; y += TileSize.height)
                {
                    for (int x = 0; x < Width; x += TileSize.width)
                    {
                        if (++Idx > 3)
                        {
                            Idx       = 0;
                            MemOffset = InitialOffset;
                        }

                        Binds.emplace_back();
                        auto& Bind = Binds.back();

                        Bind.memory                 = Memory.Handle;
                        Bind.memoryOffset           = MemOffset;
                        Bind.flags                  = 0;
                        Bind.extent.width           = std::min(TileSize.width, static_cast<uint32_t>(Width - x));
                        Bind.extent.height          = std::min(TileSize.height, static_cast<uint32_t>(Height - y));
                        Bind.extent.depth           = 1;
                        Bind.offset                 = {x, y, 0};
                        Bind.subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        Bind.subresource.arrayLayer = Slice;
                        Bind.subresource.mipLevel   = Mip;

                        MemOffset += StandardBlockSize;
                        VERIFY_EXPR(MemOffset <= PoolSize);
                    }
                }
            }
            InitialOffset += 3 * StandardBlockSize;
        }

        VkSparseImageMemoryBindInfo ImageBind{};
        ImageBind.image     = Texture.Handle;
        ImageBind.bindCount = static_cast<Uint32>(Binds.size());
        ImageBind.pBinds    = Binds.data();

        VkSparseImageOpaqueMemoryBindInfo ImgOpaqueBind{};
        ImgOpaqueBind.image     = Texture.Handle;
        ImgOpaqueBind.bindCount = static_cast<Uint32>(OpaqueBinds.size());
        ImgOpaqueBind.pBinds    = OpaqueBinds.data();

        VkBindSparseInfo BindInfo{};
        BindInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        if (ImageBind.bindCount > 0)
        {
            BindInfo.imageBindCount = 1;
            BindInfo.pImageBinds    = &ImageBind;
        }
        if (ImgOpaqueBind.bindCount > 0)
        {
            BindInfo.imageOpaqueBindCount = 1;
            BindInfo.pImageOpaqueBinds    = &ImgOpaqueBind;
        }
        vkQueueBindSparse(vkQueue, 1, &BindInfo, VK_NULL_HANDLE);
    });

    auto pTextureWrapper = CreateTextureFromVkImage(Texture.Handle, TexDim, MipLevels);
    ASSERT_NE(pTextureWrapper, nullptr);

    Helper.FillAndDraw(pTextureWrapper);
}


void SparseMemorySparseTexture3DTestVk(const SparseMemoryTestTextureHelper& Helper)
{
    const auto   TexDim    = Helper.TextureSize;
    Uint32       MipLevels = 0;
    const Uint64 PoolSize  = StandardBlockSize * 16;

    auto Texture = CreateSparseImage(TexDim, MipLevels);
    ASSERT_TRUE(Texture);

    auto Memory = CreateMemoryForImage(PoolSize, Texture.Handle);
    ASSERT_TRUE(Memory);

    std::vector<VkSparseImageMemoryRequirements> SparseReq;
    GetSparseRequirements(Texture.Handle, SparseReq);
    ASSERT_EQ(SparseReq.size(), 1u);

    const auto& MipInfo  = SparseReq.front();
    const auto& TileSize = SparseReq.front().formatProperties.imageGranularity;

    WithCmdQueue([&](VkQueue vkQueue) {
        std::vector<VkSparseImageMemoryBind> Binds;
        std::vector<VkSparseMemoryBind>      OpaqueBinds;

        Uint64 MemOffset = 0;
        for (Uint32 Mip = 0; Mip < MipInfo.imageMipTailFirstLod; ++Mip)
        {
            const int Width  = std::max(1u, static_cast<Uint32>(TexDim.x) >> Mip);
            const int Height = std::max(1u, static_cast<Uint32>(TexDim.y) >> Mip);
            const int Depth  = std::max(1u, static_cast<Uint32>(TexDim.z) >> Mip);
            for (int z = 0; z < Depth; z += TileSize.depth)
            {
                for (int y = 0; y < Height; y += TileSize.height)
                {
                    for (int x = 0; x < Width; x += TileSize.width)
                    {
                        Binds.emplace_back();
                        auto& Bind = Binds.back();

                        Bind.memory                 = Memory.Handle;
                        Bind.memoryOffset           = MemOffset;
                        Bind.flags                  = 0;
                        Bind.extent.width           = std::min(TileSize.width, static_cast<uint32_t>(Width - x));
                        Bind.extent.height          = std::min(TileSize.height, static_cast<uint32_t>(Height - y));
                        Bind.extent.depth           = std::min(TileSize.depth, static_cast<uint32_t>(Depth - z));
                        Bind.offset                 = {x, y, z};
                        Bind.subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        Bind.subresource.arrayLayer = 0;
                        Bind.subresource.mipLevel   = Mip;

                        MemOffset += StandardBlockSize;
                    }
                }
            }
        }

        // Mip tail
        for (Uint32 OffsetInMipTail = 0; OffsetInMipTail < MipInfo.imageMipTailSize; OffsetInMipTail += StandardBlockSize)
        {
            OpaqueBinds.emplace_back();
            auto& Bind = OpaqueBinds.back();

            Bind.resourceOffset = MipInfo.imageMipTailOffset + OffsetInMipTail;
            Bind.memory         = Memory.Handle;
            Bind.memoryOffset   = MemOffset;
            Bind.size           = StandardBlockSize;
            Bind.flags          = 0;

            MemOffset += Bind.size;
        }
        VERIFY_EXPR(MemOffset <= PoolSize);

        VkSparseImageMemoryBindInfo ImageBind{};
        ImageBind.image     = Texture.Handle;
        ImageBind.bindCount = static_cast<Uint32>(Binds.size());
        ImageBind.pBinds    = Binds.data();

        VkSparseImageOpaqueMemoryBindInfo ImgOpaqueBind{};
        ImgOpaqueBind.image     = Texture.Handle;
        ImgOpaqueBind.bindCount = static_cast<Uint32>(OpaqueBinds.size());
        ImgOpaqueBind.pBinds    = OpaqueBinds.data();

        VkBindSparseInfo BindInfo{};
        BindInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        if (ImageBind.bindCount > 0)
        {
            BindInfo.imageBindCount = 1;
            BindInfo.pImageBinds    = &ImageBind;
        }
        if (ImgOpaqueBind.bindCount > 0)
        {
            BindInfo.imageOpaqueBindCount = 1;
            BindInfo.pImageOpaqueBinds    = &ImgOpaqueBind;
        }
        vkQueueBindSparse(vkQueue, 1, &BindInfo, VK_NULL_HANDLE);
    });

    auto pTextureWrapper = CreateTextureFromVkImage(Texture.Handle, TexDim, MipLevels);
    ASSERT_NE(pTextureWrapper, nullptr);

    Helper.FillAndDraw(pTextureWrapper);
}

} // namespace Testing

} // namespace Diligent
