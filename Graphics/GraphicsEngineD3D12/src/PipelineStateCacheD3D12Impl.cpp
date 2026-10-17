/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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
#include "PipelineStateCacheD3D12Impl.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "DataBlobImpl.hpp"
#include "StringTools.hpp"

namespace Diligent
{

PipelineStateCacheD3D12Impl::PipelineStateCacheD3D12Impl(IReferenceCounters*                 pRefCounters,
                                                         RenderDeviceD3D12Impl*              pRenderDeviceD3D12,
                                                         const PipelineStateCacheCreateInfo& CreateInfo) :
    // clang-format off
    TPipelineStateCacheBase
    {
        pRefCounters,
        pRenderDeviceD3D12,
        CreateInfo,
        false
    }
// clang-format on
{
    auto* pDevice1 = pRenderDeviceD3D12->GetD3D12Device1();
    if (pDevice1 == nullptr)
    {
        LOG_WARNING_MESSAGE("D3D12 pipeline libraries are unavailable; native caching is disabled.");
        return;
    }

    // D3D12 does not copy the serialized data: retain our own copy until the library is released.
    if (CreateInfo.pCacheData != nullptr && CreateInfo.CacheDataSize != 0)
    {
        m_pInitialData = DataBlobImpl::Create(CreateInfo.CacheDataSize, CreateInfo.pCacheData);
        const HRESULT LoadResult = pDevice1->CreatePipelineLibrary(m_pInitialData->GetConstDataPtr(), m_pInitialData->GetSize(), IID_PPV_ARGS(&m_pLibrary));
        if (FAILED(LoadResult))
        {
            LOG_WARNING_MESSAGE("D3D12 pipeline library data is invalid or incompatible; using an empty cache (HRESULT ", LoadResult, ").");
            m_pInitialData.Release();
        }
    }

    HRESULT hr = m_pLibrary ? S_OK : pDevice1->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&m_pLibrary));
    if (FAILED(hr))
    {
        LOG_WARNING_MESSAGE("D3D12 pipeline library is unavailable; native caching is disabled (HRESULT ", hr, ").");
        return;
    }
    m_pLibrary->QueryInterface(IID_PPV_ARGS(&m_pLibrary1));
}

PipelineStateCacheD3D12Impl::~PipelineStateCacheD3D12Impl()
{
    // D3D12 object can only be destroyed when it is no longer used by the GPU
    m_pLibrary1.Release();
    GetDevice()->SafeReleaseDeviceObject(std::make_pair(std::move(m_pInitialData), std::move(m_pLibrary)), ~Uint64{0});
}

CComPtr<ID3D12DeviceChild> PipelineStateCacheD3D12Impl::LoadComputePipeline(const wchar_t* Name, const D3D12_COMPUTE_PIPELINE_STATE_DESC& Desc)
{
    if (Name == nullptr)
    {
        DEV_ERROR("Pipeline name must not be null");
        return {};
    }

    CComPtr<ID3D12DeviceChild> d3d12PSO;
    if (m_pLibrary && (m_Desc.Mode & PSO_CACHE_MODE_LOAD) != 0)
    {
        std::lock_guard<std::mutex> Lock{m_LibraryMtx};
        HRESULT hr = m_pLibrary->LoadComputePipeline(Name, &Desc, IID_PPV_ARGS(&d3d12PSO));
        if ((m_Desc.Flags & PSO_CACHE_FLAG_VERBOSE) != 0)
            LOG_INFO_MESSAGE(SUCCEEDED(hr) ? "PSO cache hit (compute): " : "PSO cache miss (compute): ", NarrowString(Name));
    }
    return d3d12PSO;
}

CComPtr<ID3D12DeviceChild> PipelineStateCacheD3D12Impl::LoadGraphicsPipeline(const wchar_t* Name, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& Desc)
{
    if (Name == nullptr)
    {
        DEV_ERROR("Pipeline name must not be null");
        return {};
    }

    CComPtr<ID3D12DeviceChild> d3d12PSO;
    if (m_pLibrary && (m_Desc.Mode & PSO_CACHE_MODE_LOAD) != 0)
    {
        std::lock_guard<std::mutex> Lock{m_LibraryMtx};
        HRESULT hr = m_pLibrary->LoadGraphicsPipeline(Name, &Desc, IID_PPV_ARGS(&d3d12PSO));
        if ((m_Desc.Flags & PSO_CACHE_FLAG_VERBOSE) != 0)
            LOG_INFO_MESSAGE(SUCCEEDED(hr) ? "PSO cache hit (graphics): " : "PSO cache miss (graphics): ", NarrowString(Name));
    }
    return d3d12PSO;
}

#ifdef D3D12_H_HAS_MESH_SHADER
CComPtr<ID3D12DeviceChild> PipelineStateCacheD3D12Impl::LoadPipeline(const wchar_t* Name, const D3D12_PIPELINE_STATE_STREAM_DESC& Desc)
{
    VERIFY_EXPR(Name != nullptr);
    CComPtr<ID3D12PipelineState> d3d12PSO;
    if (m_pLibrary1 && (m_Desc.Mode & PSO_CACHE_MODE_LOAD) != 0)
    {
        std::lock_guard<std::mutex> Lock{m_LibraryMtx};
        const HRESULT hr = m_pLibrary1->LoadPipeline(Name, &Desc, IID_PPV_ARGS(&d3d12PSO));
        if ((m_Desc.Flags & PSO_CACHE_FLAG_VERBOSE) != 0)
            LOG_INFO_MESSAGE(SUCCEEDED(hr) ? "PSO cache hit (mesh): " : "PSO cache miss (mesh): ", NarrowString(Name), " (HRESULT ", hr, ")");
    }
    return CComPtr<ID3D12DeviceChild>{d3d12PSO.p};
}
#endif

bool PipelineStateCacheD3D12Impl::StorePipeline(const wchar_t* Name, ID3D12DeviceChild* pPSO)
{
    VERIFY_EXPR(Name != nullptr);
    if (!m_pLibrary || (m_Desc.Mode & PSO_CACHE_MODE_STORE) == 0)
        return false;

    std::lock_guard<std::mutex> Lock{m_LibraryMtx};
    HRESULT hr = m_pLibrary->StorePipeline(Name, static_cast<ID3D12PipelineState*>(pPSO));
    if (FAILED(hr) && (m_Desc.Flags & PSO_CACHE_FLAG_VERBOSE) != 0)
        LOG_INFO_MESSAGE("PSO cache store failed: ", NarrowString(Name), " (HRESULT ", hr, "; it may already be stored).");

    return SUCCEEDED(hr);
}

void PipelineStateCacheD3D12Impl::GetData(IDataBlob** ppBlob)
{
    DEV_CHECK_ERR(ppBlob != nullptr, "ppBlob must not be null");
    *ppBlob = nullptr;
    if (!m_pLibrary)
        return;

    std::lock_guard<std::mutex> Lock{m_LibraryMtx};

    RefCntAutoPtr<DataBlobImpl> pDataBlob = DataBlobImpl::Create(m_pLibrary->GetSerializedSize());

    HRESULT hr = m_pLibrary->Serialize(pDataBlob->GetDataPtr(), pDataBlob->GetSize());
    if (FAILED(hr))
    {
        LOG_ERROR_MESSAGE("Failed to serialize D3D12 pipeline library");
        return;
    }

    *ppBlob = pDataBlob.Detach();
}

} // namespace Diligent
