/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include "GPUTestingEnvironment.hpp"
#include "EngineFactoryWebGPU.h"
#include "RenderDeviceWebGPU.h"
#include "SwapChainWebGPU.h"
#include "TextureWebGPU.h"
#include "GraphicsAccessories.hpp"

#include "gtest/gtest.h"

#if PLATFORM_WIN32

#    include "WinHPreface.h"
#    include <Windows.h>
#    include "WinHPostface.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

// These tests use a real surface: the testing environment's offscreen swap chain
// does not exercise acquisition, presentation, or surface texture ownership.
class SwapChainWebGPUTest : public testing::TestWithParam<TEXTURE_FORMAT>
{
protected:
    void SetUp() override
    {
        auto* pEnv = GPUTestingEnvironment::GetInstance();
        if (pEnv->GetDevice()->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_WEBGPU)
            GTEST_SKIP();

        m_Window = CreateWindowExW(0, L"STATIC", L"WebGPU swap chain test", WS_OVERLAPPEDWINDOW,
                                   0, 0, 128, 128, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        ASSERT_NE(m_Window, nullptr);
    }

    void TearDown() override
    {
        if (m_Window)
        {
            auto* pEnv = GPUTestingEnvironment::GetInstance();
            pEnv->GetDeviceContext()->WaitForIdle();
            pEnv->GetDeviceContext()->InvalidateState();
            m_pSwapChain.Release();
            DestroyWindow(m_Window);
            pEnv->Reset();
        }
    }

    void CreateSwapChain(bool IsPrimary = true)
    {
        auto*                               pEnv = GPUTestingEnvironment::GetInstance();
        RefCntAutoPtr<IEngineFactoryWebGPU> pFactory{pEnv->GetDevice()->GetEngineFactory(), IID_EngineFactoryWebGPU};
        SwapChainDesc                       Desc;
        Desc.Width             = 64;
        Desc.Height            = 32;
        Desc.ColorBufferFormat = GetParam();
        Desc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
        Desc.Usage             = SWAP_CHAIN_USAGE_RENDER_TARGET | SWAP_CHAIN_USAGE_SHADER_RESOURCE | SWAP_CHAIN_USAGE_COPY_SOURCE;
        Desc.IsPrimary         = IsPrimary;
        pFactory->CreateSwapChainWebGPU(pEnv->GetDevice(), pEnv->GetDeviceContext(), Desc, Win32NativeWindow{m_Window}, &m_pSwapChain);
        ASSERT_NE(m_pSwapChain, nullptr);
    }

    void VerifyClear(ITexture* pBackBuffer)
    {
        auto*       pEnv     = GPUTestingEnvironment::GetInstance();
        auto*       pContext = pEnv->GetDeviceContext();
        TextureDesc Desc     = pBackBuffer->GetDesc();
        Desc.Name            = "Surface readback";
        Desc.BindFlags       = BIND_NONE;
        Desc.Usage           = USAGE_STAGING;
        Desc.CPUAccessFlags  = CPU_ACCESS_READ;
        RefCntAutoPtr<ITexture> pStaging;
        pEnv->GetDevice()->CreateTexture(Desc, nullptr, &pStaging);
        ASSERT_NE(pStaging, nullptr);

        CopyTextureAttribs Copy;
        Copy.pSrcTexture              = pBackBuffer;
        Copy.pDstTexture              = pStaging;
        Copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        Copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        pContext->CopyTexture(Copy);
        pContext->WaitForIdle();

        MappedTextureSubresource Mapped;
        pContext->MapTextureSubresource(pStaging, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, Mapped);
        ASSERT_NE(Mapped.pData, nullptr);
        const bool SRGB       = IsSRGBFormat(Desc.Format);
        const bool BGRA       = SRGBFormatToUnorm(Desc.Format) == TEX_FORMAT_BGRA8_UNORM;
        const int  Expected[] = {SRGB ? 137 : 64, SRGB ? 188 : 128, SRGB ? 225 : 191, 255};
        for (Uint32 Y = 0; Y < Desc.Height; ++Y)
        {
            const auto* pRow = static_cast<const Uint8*>(Mapped.pData) + Y * Mapped.Stride;
            for (Uint32 X = 0; X < Desc.Width; ++X)
            {
                for (Uint32 C = 0; C < 4; ++C)
                {
                    const Uint32 Channel = BGRA && C != 1 && C != 3 ? 2 - C : C;
                    EXPECT_NEAR(pRow[X * 4 + Channel], Expected[C], 1);
                }
            }
        }
        pContext->UnmapTextureSubresource(pStaging, 0, 0);
    }

    HWND                      m_Window = nullptr;
    RefCntAutoPtr<ISwapChain> m_pSwapChain;
};

TEST_P(SwapChainWebGPUTest, RenderAndPresent)
{
    ASSERT_NO_FATAL_FAILURE(CreateSwapChain());
    auto*                           pContext = GPUTestingEnvironment::GetInstance()->GetDeviceContext();
    RefCntAutoPtr<ISwapChainWebGPU> pSwapChainWebGPU{m_pSwapChain, IID_SwapChainWebGPU};
    const float                     Color[] = {0.25f, 0.5f, 0.75f, 1.0f};
    for (Uint32 Frame = 0; Frame < 3; ++Frame)
    {
        ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        ASSERT_NE(pRTV, nullptr);
        EXPECT_EQ(m_pSwapChain->GetCurrentBackBufferRTV(), pRTV);
        EXPECT_EQ(pRTV->GetDesc().Format, m_pSwapChain->GetDesc().ColorBufferFormat);
        EXPECT_EQ(pRTV->GetTexture()->GetDesc().Format, m_pSwapChain->GetDesc().ColorBufferFormat);
        EXPECT_EQ(IsSRGBFormat(pRTV->GetDesc().Format), IsSRGBFormat(GetParam()));
        EXPECT_NE(pRTV->GetTexture()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE), nullptr);

        RefCntAutoPtr<ITextureWebGPU> pTextureWebGPU{pRTV->GetTexture(), IID_TextureWebGPU};
        WGPUSurfaceTexture            SurfaceTexture{};
        wgpuSurfaceGetCurrentTexture(pSwapChainWebGPU->GetWebGPUSurface(), &SurfaceTexture);
        EXPECT_EQ(SurfaceTexture.texture, pTextureWebGPU->GetWebGPUTexture());
        if (SurfaceTexture.texture)
            wgpuTextureRelease(SurfaceTexture.texture);
        EXPECT_NE(wgpuTextureGetUsage(pTextureWebGPU->GetWebGPUTexture()) & WGPUTextureUsage_TextureBinding, 0u);
        EXPECT_NE(wgpuTextureGetUsage(pTextureWebGPU->GetWebGPUTexture()) & WGPUTextureUsage_CopySrc, 0u);

        pContext->SetRenderTargets(1, &pRTV, m_pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->ClearRenderTarget(pRTV, Color, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ASSERT_NO_FATAL_FAILURE(VerifyClear(pRTV->GetTexture()));
        const Uint64 FrameNumber = pContext->GetFrameNumber();
        m_pSwapChain->Present(Frame & 1);
        EXPECT_EQ(pContext->GetFrameNumber(), FrameNumber + 1);
    }
}

TEST_P(SwapChainWebGPUTest, ResizeAndDestroyWithAcquiredTexture)
{
    ASSERT_NO_FATAL_FAILURE(CreateSwapChain(false));
    auto*         pContext = GPUTestingEnvironment::GetInstance()->GetDeviceContext();
    const float   Color[]  = {0.25f, 0.5f, 0.75f, 1.0f};
    ITextureView* pRTV     = m_pSwapChain->GetCurrentBackBufferRTV();
    ASSERT_NE(pRTV, nullptr);
    pContext->SetRenderTargets(1, &pRTV, m_pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearRenderTarget(pRTV, Color, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Resize must submit the pending clear before invalidating the acquired texture.
    m_pSwapChain->Resize(96, 48);
    pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ASSERT_NE(pRTV, nullptr);
    EXPECT_EQ(pRTV->GetTexture()->GetDesc().Width, 96u);
    EXPECT_EQ(pRTV->GetTexture()->GetDesc().Height, 48u);
    ASSERT_NE(m_pSwapChain->GetDepthBufferDSV(), nullptr);
    EXPECT_EQ(m_pSwapChain->GetDepthBufferDSV()->GetTexture()->GetDesc().Width, 96u);
    EXPECT_EQ(m_pSwapChain->GetDepthBufferDSV()->GetTexture()->GetDesc().Height, 48u);
    pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearRenderTarget(pRTV, Color, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ASSERT_NO_FATAL_FAILURE(VerifyClear(pRTV->GetTexture()));
    const Uint64 FrameNumber = pContext->GetFrameNumber();
    m_pSwapChain->Present(0);
    EXPECT_EQ(pContext->GetFrameNumber(), FrameNumber);

    // Present without acquiring another texture must not call native surface Present.
    m_pSwapChain->Present(0);
    pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ASSERT_NE(pRTV, nullptr);
    pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearRenderTarget(pRTV, Color, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pSwapChain.Release();
    pContext->WaitForIdle();
}

INSTANTIATE_TEST_SUITE_P(Formats, SwapChainWebGPUTest, testing::Values(TEX_FORMAT_RGBA8_UNORM, TEX_FORMAT_RGBA8_UNORM_SRGB, TEX_FORMAT_BGRA8_UNORM, TEX_FORMAT_BGRA8_UNORM_SRGB));

} // namespace

#endif
