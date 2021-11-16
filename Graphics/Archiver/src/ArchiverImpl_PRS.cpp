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

#include "ArchiverImpl.hpp"

namespace Diligent
{

const SerializedMemory& ArchiverImpl::PRSData::GetSharedData() const
{
    return pPRS->GetSharedSerializedMemory();
}

const SerializedMemory& ArchiverImpl::PRSData::GetDeviceData(Uint32 Idx) const
{
    const SerializedMemory* Result = nullptr;
    switch (static_cast<DeviceType>(Idx))
    {
        // clang-format off
        case DeviceType::Direct3D11:
#if D3D11_SUPPORTED
            Result = pPRS->GetSerializedMemoryD3D11();
#endif
            break;
        case DeviceType::Direct3D12:
#if D3D12_SUPPORTED
            Result = pPRS->GetSerializedMemoryD3D12();
#endif
            break;
        case DeviceType::OpenGL:
#if GL_SUPPORTED || GLES_SUPPORTED
            Result = pPRS->GetSerializedMemoryGL();
#endif
            break;
        case DeviceType::Vulkan:
#if VULKAN_SUPPORTED
            Result = pPRS->GetSerializedMemoryVk();
#endif
            break;
        case DeviceType::Metal:
#if METAL_SUPPORTED
            Result = pPRS->GetSerializedMemoryMtl();
#endif
            break;
        // clang-format on
        case DeviceType::Count:
            break;
    }

    if (Result != nullptr)
        return *Result;

    static const SerializedMemory Empty;
    return Empty;
}

bool ArchiverImpl::AddPipelineResourceSignature(IPipelineResourceSignature* pPRS)
{
    DEV_CHECK_ERR(pPRS != nullptr, "pPRS must not be null");
    if (pPRS == nullptr)
        return false;

    auto* pPRSImpl        = ClassPtrCast<SerializableResourceSignatureImpl>(pPRS);
    auto  IterAndInserted = m_PRSMap.emplace(String{pPRSImpl->GetDesc().Name}, PRSData{});

    if (!IterAndInserted.second)
    {
        if (IterAndInserted.first->second.pPRS != pPRSImpl)
        {
            LOG_ERROR_MESSAGE("Pipeline resource signature must have unique name");
            return false;
        }
        else
            return true;
    }

    m_PRSCache.insert(RefCntAutoPtr<SerializableResourceSignatureImpl>{pPRSImpl});

    IterAndInserted.first->second.pPRS = pPRSImpl;
    return true;
}

bool ArchiverImpl::CachePipelineResourceSignature(RefCntAutoPtr<IPipelineResourceSignature>& pPRS)
{
    auto* pPRSImpl        = pPRS.RawPtr<SerializableResourceSignatureImpl>();
    auto  IterAndInserted = m_PRSCache.insert(RefCntAutoPtr<SerializableResourceSignatureImpl>{pPRSImpl});

    // Found same PRS in cache
    if (!IterAndInserted.second)
    {
        pPRS     = *IterAndInserted.first;
        pPRSImpl = pPRS.RawPtr<SerializableResourceSignatureImpl>();

#ifdef DILIGENT_DEBUG
        auto Iter = m_PRSMap.find(String{pPRSImpl->GetDesc().Name});
        VERIFY_EXPR(Iter != m_PRSMap.end());
        VERIFY_EXPR(Iter->second.pPRS == pPRSImpl);
#endif
        return true;
    }

    return AddPipelineResourceSignature(pPRS);
}

Bool ArchiverImpl::AddPipelineResourceSignature(const PipelineResourceSignatureDesc& SignatureDesc,
                                                const ResourceSignatureArchiveInfo&  ArchiveInfo)
{
    RefCntAutoPtr<IPipelineResourceSignature> pPRS;
    m_pSerializationDevice->CreatePipelineResourceSignature(SignatureDesc, ArchiveInfo.DeviceBits, &pPRS);
    if (!pPRS)
        return false;

    return AddPipelineResourceSignature(pPRS);
}

String ArchiverImpl::UniquePRSName()
{
    String       PRSName = "Default PRS - ";
    const size_t Pos     = PRSName.length();

    // AZ TODO: optimize (binary search?)
    for (Uint32 Index = 0; Index < 10000; ++Index)
    {
        PRSName.resize(Pos);
        PRSName += std::to_string(Index);

        if (m_PRSMap.find(PRSName) == m_PRSMap.end())
            return PRSName;
    }
    return "";
}

} // namespace Diligent
