/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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

#include "ScreenCapture.hpp"

namespace Diligent
{

ScreenCapture::ScreenCapture(IRenderDevice* pDevice) :
    m_pDevice{pDevice}
{
    FenceDesc fenceDesc;
    fenceDesc.Name = "Screen capture fence";
    m_pDevice->CreateFence(fenceDesc, &m_pFence);
}

void ScreenCapture::Capture(ISwapChain* pSwapChain, IDeviceContext* pContext, Uint32 FrameId)
{
    ITextureView*        pCurrentRTV        = pSwapChain->GetCurrentBackBufferRTV();
    ITexture*            pCurrentBackBuffer = pCurrentRTV->GetTexture();
    const SwapChainDesc& SCDesc             = pSwapChain->GetDesc();

    RefCntAutoPtr<ITexture> pStagingTexture;

    {
        std::lock_guard<std::mutex> Lock{m_AvailableTexturesMtx};
        while (!m_AvailableTextures.empty() && !pStagingTexture)
        {
            pStagingTexture = std::move(m_AvailableTextures.back());
            m_AvailableTextures.pop_back();
            const TextureDesc& TexDesc = pStagingTexture->GetDesc();
            if (!(TexDesc.Width == SCDesc.Width &&
                  TexDesc.Height == SCDesc.Height &&
                  TexDesc.Format == SCDesc.ColorBufferFormat))
            {
                pStagingTexture.Release();
            }
        }
    }

    if (!pStagingTexture)
    {
        TextureDesc TexDesc;
        TexDesc.Name           = "Staging texture for screen capture";
        TexDesc.Type           = RESOURCE_DIM_TEX_2D;
        TexDesc.Width          = SCDesc.Width;
        TexDesc.Height         = SCDesc.Height;
        TexDesc.Format         = SCDesc.ColorBufferFormat;
        TexDesc.Usage          = USAGE_STAGING;
        TexDesc.CPUAccessFlags = CPU_ACCESS_READ;
        m_pDevice->CreateTexture(TexDesc, nullptr, &pStagingTexture);
    }

    CopyTextureAttribs CopyAttribs(pCurrentBackBuffer, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, pStagingTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->CopyTexture(CopyAttribs);
    pContext->EnqueueSignal(m_pFence, m_CurrentFenceValue);

    {
        std::lock_guard<std::mutex> Lock{m_PendingTexturesMtx};
        m_PendingTextures.emplace_back(std::move(pStagingTexture), FrameId, m_CurrentFenceValue);
    }

    ++m_CurrentFenceValue;
}


ScreenCapture::CaptureInfo ScreenCapture::GetCapture()
{
    CaptureInfo Capture;

    std::lock_guard<std::mutex> Lock{m_PendingTexturesMtx};
    if (!m_PendingTextures.empty())
    {
        PendingTextureInfo& OldestCapture       = m_PendingTextures.front();
        Uint64              CompletedFenceValue = m_pFence->GetCompletedValue();
        if (OldestCapture.Fence <= CompletedFenceValue)
        {
            Capture.pTexture = std::move(OldestCapture.pTex);
            Capture.Id       = OldestCapture.Id;
            m_PendingTextures.pop_front();
        }
    }
    return Capture;
}

bool ScreenCapture::HasCapture()
{
    std::lock_guard<std::mutex> Lock{m_PendingTexturesMtx};
    if (!m_PendingTextures.empty())
    {
        const PendingTextureInfo& OldestCapture       = m_PendingTextures.front();
        Uint64                    CompletedFenceValue = m_pFence->GetCompletedValue();
        return OldestCapture.Fence <= CompletedFenceValue;
    }
    else
    {
        return false;
    }
}

void ScreenCapture::RecycleStagingTexture(RefCntAutoPtr<ITexture>&& pTexture)
{
    std::lock_guard<std::mutex> Lock{m_AvailableTexturesMtx};
    m_AvailableTextures.emplace_back(std::move(pTexture));
}

} // namespace Diligent
