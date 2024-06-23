/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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
/// Declaration of Diligent::TextureWebGPUImpl class

#include "EngineWebGPUImplTraits.hpp"
#include "TextureBase.hpp"
#include "TextureViewWebGPUImpl.hpp" // Required by TextureBase
#include "WebGPUObjectWrappers.hpp"

namespace Diligent
{

/// Texture implementation in WebGPU backend.
class TextureWebGPUImpl final : public TextureBase<EngineWebGPUImplTraits>
{
public:
    using TTextureBase = TextureBase<EngineWebGPUImplTraits>;

    TextureWebGPUImpl(IReferenceCounters*        pRefCounters,
                      FixedBlockMemoryAllocator& TexViewObjAllocator,
                      RenderDeviceWebGPUImpl*    pDevice,
                      const TextureDesc&         Desc,
                      const TextureData*         pInitData = nullptr);

    // Attaches to an existing WebGPU resource
    TextureWebGPUImpl(IReferenceCounters*        pRefCounters,
                      FixedBlockMemoryAllocator& TexViewObjAllocator,
                      RenderDeviceWebGPUImpl*    pDevice,
                      const TextureDesc&         Desc,
                      RESOURCE_STATE             InitialState,
                      WGPUTexture                wgpuTextureHandle);

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_TextureWebGPU, TTextureBase)

    /// Implementation of ITexture::GetNativeHandle() in WebGPU backend.
    Uint64 DILIGENT_CALL_TYPE GetNativeHandle() override final;

    /// Implementation of ITextureWebGPU::GetWebGPUTexture() in WebGPU backend.
    WGPUTexture DILIGENT_CALL_TYPE GetWebGPUTexture() const override final;

    WGPUBuffer GetWebGPUStagingBuffer() const;

    void* Map(MAP_TYPE MapType, Uint32 MapFlags);

    void Unmap();

    static constexpr Uint32 StagingDataAlignment = 16;

    static constexpr Uint64 CopyTextureRawStride = 256;

private:
    void CreateViewInternal(const TextureViewDesc& ViewDesc,
                            ITextureView**         ppView,
                            bool                   bIsDefaultView) override;

private:
    enum class TextureMapState
    {
        None,
        Read,
        Write
    };

    WebGPUTextureWrapper m_wgpuTexture;
    WebGPUBufferWrapper  m_wgpuStagingBuffer;
    std::vector<uint8_t> m_MappedData;
    TextureMapState      m_MapState = TextureMapState::None;
};

} // namespace Diligent
