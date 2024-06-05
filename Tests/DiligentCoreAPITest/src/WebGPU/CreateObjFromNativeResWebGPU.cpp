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

#include <webgpu/webgpu.h>

#include "RenderDeviceWebGPU.h"
#include "TextureWebGPU.h"
#include "BufferWebGPU.h"
#include "GPUTestingEnvironment.hpp"
#include "WebGPU/CreateObjFromNativeResWebGPU.hpp"

#include "gtest/gtest.h"

namespace Diligent
{

namespace Testing
{

void TestCreateObjFromNativeResWebGPU::CreateTexture(ITexture* pTexture)
{
    RefCntAutoPtr<IRenderDeviceWebGPU> pDeviceWebGPU(m_pDevice, IID_RenderDeviceWebGPU);
    RefCntAutoPtr<ITextureWebGPU>      pTextureWebGPU(pTexture, IID_TextureWebGPU);
    ASSERT_NE(pDeviceWebGPU, nullptr);
    ASSERT_NE(pTextureWebGPU, nullptr);

    const auto& SrcTexDesc        = pTexture->GetDesc();
    const auto  wgpuTextureHandle = pTextureWebGPU->GetWebGPUTexture();
    ASSERT_NE(wgpuTextureHandle, nullptr);

    RefCntAutoPtr<ITexture> pAttachedTexture;
    pDeviceWebGPU->CreateTextureFromWebGPUTexture(wgpuTextureHandle, SrcTexDesc, RESOURCE_STATE_UNKNOWN, &pAttachedTexture);
    ASSERT_NE(pAttachedTexture, nullptr);

    const auto& TestTexDesc = pAttachedTexture->GetDesc();
    EXPECT_EQ(TestTexDesc, SrcTexDesc);

    RefCntAutoPtr<ITextureWebGPU> pAttachedTextureWebGPU(pAttachedTexture, IID_TextureWebGPU);
    ASSERT_NE(pAttachedTextureWebGPU, nullptr);
    EXPECT_EQ(pAttachedTextureWebGPU->GetWebGPUTexture(), wgpuTextureHandle);
    //  EXPECT_EQ(BitCast<WGPUTexture>(pAttachedTextureWebGPU->GetNativeHandle()), wgpuTextureHandle);
}

void TestCreateObjFromNativeResWebGPU::CreateBuffer(IBuffer* pBuffer)
{
    RefCntAutoPtr<IRenderDeviceWebGPU> pDeviceWebGPU(m_pDevice, IID_RenderDeviceWebGPU);
    RefCntAutoPtr<IBufferWebGPU>       pBufferWebGPU(pBuffer, IID_BufferWebGPU);
    ASSERT_NE(pDeviceWebGPU, nullptr);
    ASSERT_NE(pBufferWebGPU, nullptr);

    const auto& SrcBufDesc       = pBuffer->GetDesc();
    const auto  wgpuBufferHandle = pBufferWebGPU->GetWebGPUBuffer();
    ASSERT_NE(wgpuBufferHandle, nullptr);

    RefCntAutoPtr<IBuffer> pAttachedBuffer;
    pDeviceWebGPU->CreateBufferFromWebGPUBuffer(wgpuBufferHandle, SrcBufDesc, RESOURCE_STATE_UNKNOWN, &pAttachedBuffer);
    ASSERT_NE(pAttachedBuffer, nullptr);

    const auto& TestBufDesc = pAttachedBuffer->GetDesc();
    EXPECT_EQ(TestBufDesc, SrcBufDesc);

    RefCntAutoPtr<IBufferWebGPU> pAttachedBufferWebGPU(pAttachedBuffer, IID_BufferWebGPU);
    ASSERT_NE(pAttachedBufferWebGPU, nullptr);
    EXPECT_EQ(pAttachedBufferWebGPU->GetWebGPUBuffer(), wgpuBufferHandle);
    //  EXPECT_EQ(BitCast<WGPUBuffer>(pAttachedBufferWebGPU->GetNativeHandle()), wgpuBufferHandle);
}

} // namespace Testing

} // namespace Diligent
