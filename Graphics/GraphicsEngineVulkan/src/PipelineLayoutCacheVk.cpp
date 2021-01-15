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

#include "PipelineLayoutCacheVk.hpp"
#include "RenderDeviceVkImpl.hpp"

namespace Diligent
{

bool PipelineLayoutCacheVk::PipelineLayoutCompare::operator()(const PipelineLayoutVk* lhs, const PipelineLayoutVk* rhs) const noexcept
{
    if (lhs->GetSignatureCount() != rhs->GetSignatureCount())
        return false;

    for (Uint32 i = 0, Cnt = lhs->GetSignatureCount(); i < Cnt; ++i)
    {
        if (lhs->GetSignature(i) != rhs->GetSignature(i))
            return false;
    }
    return true;
}

PipelineLayoutCacheVk::~PipelineLayoutCacheVk()
{
    std::lock_guard<std::mutex> Lock{m_Mutex};
    VERIFY(m_Cache.empty(), "All pipeline layouts must be released");
}

RefCntAutoPtr<PipelineLayoutVk> PipelineLayoutCacheVk::GetLayout(IPipelineResourceSignature** ppSignatures, Uint32 SignatureCount)
{
    RefCntAutoPtr<PipelineLayoutVk> pNewLayout;
    m_DeviceVk.CreatePipelineLayout(ppSignatures, SignatureCount, &pNewLayout);

    if (pNewLayout == nullptr)
        return {};

    std::lock_guard<std::mutex> Lock{m_Mutex};

    PipelineLayoutVk* pResult = *m_Cache.insert(pNewLayout).first;

    if (pNewLayout == pResult)
    {
        pNewLayout->Finalize();
        void(pNewLayout.Detach());
    }
    return RefCntAutoPtr<PipelineLayoutVk>{pResult};
}

void PipelineLayoutCacheVk::OnDestroyLayout(PipelineLayoutVk* pLayout)
{
    std::lock_guard<std::mutex> Lock{m_Mutex};

    m_Cache.erase(pLayout);
}

} // namespace Diligent
