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

#pragma once

/// \file
/// Declaration of Diligent::RootSignatureD3D12 class
#include <array>
#include <mutex>
#include <unordered_set>

#include "D3D12TypeConversions.hpp"
#include "ShaderResourceCacheD3D12.hpp"
#include "PipelineResourceSignatureD3D12Impl.hpp"
#include "PrivateConstants.h"
#include "ShaderResources.hpp"

namespace Diligent
{

class RenderDeviceD3D12Impl;
class PipelineResourceSignatureD3D12Impl;

/// Implementation of the Diligent::RootSignature class
class RootSignatureD3D12 final : public ObjectBase<IObject>
{
public:
    RootSignatureD3D12(IReferenceCounters*                                      pRefCounters,
                       RenderDeviceD3D12Impl*                                   pDeviceD3D12Impl,
                       const RefCntAutoPtr<PipelineResourceSignatureD3D12Impl>* ppSignatures,
                       Uint32                                                   SignatureCount);
    ~RootSignatureD3D12();

    void Finalize();

    size_t GetHash() const { return m_Hash; }

    Uint32 GetSignatureCount() const { return m_SignatureCount; }

    PipelineResourceSignatureD3D12Impl* GetSignature(Uint32 index) const
    {
        VERIFY_EXPR(index < m_SignatureCount);
        return m_Signatures[index].RawPtr<PipelineResourceSignatureD3D12Impl>();
    }

    ID3D12RootSignature* GetD3D12RootSignature() const
    {
        VERIFY_EXPR(m_pd3d12RootSignature);
        return m_pd3d12RootSignature;
    }

    Uint32 GetFirstRootIndex(Uint32 BindingIndex) const
    {
        VERIFY_EXPR(BindingIndex < m_SignatureCount);
        return m_FirstRootIndex[BindingIndex];
    }

    using SignatureArrayType = std::array<RefCntAutoPtr<PipelineResourceSignatureD3D12Impl>, MAX_RESOURCE_SIGNATURES>;

private:
    using FirstRootIndexArrayType            = std::array<Uint32, MAX_RESOURCE_SIGNATURES>; // AZ TODO: use 8 or 16 bit int
    FirstRootIndexArrayType m_FirstRootIndex = {};

    size_t                       m_Hash = 0;
    CComPtr<ID3D12RootSignature> m_pd3d12RootSignature;

    // The number of resource signatures used by this root signature
    // (Maximum is MAX_RESOURCE_SIGNATURES)
    Uint8              m_SignatureCount = 0;
    SignatureArrayType m_Signatures     = {};

    RenderDeviceD3D12Impl* m_pDeviceD3D12Impl;
};



class LocalRootSignatureD3D12
{
public:
    LocalRootSignatureD3D12(const char* pCBName, Uint32 ShaderRecordSize);

    bool IsShaderRecord(const D3DShaderResourceAttribs& CB);

    ID3D12RootSignature* Create(ID3D12Device* pDevice);

private:
    static constexpr Uint32 InvalidBindPoint = ~0u;

    const char*                  m_pName            = nullptr;
    Uint32                       m_BindPoint        = InvalidBindPoint;
    const Uint32                 m_ShaderRecordSize = 0;
    CComPtr<ID3D12RootSignature> m_pd3d12RootSignature;
};



class RootSignatureCacheD3D12
{
public:
    RootSignatureCacheD3D12(RenderDeviceD3D12Impl& DeviceD3D12Impl);

    // clang-format off
    RootSignatureCacheD3D12             (const RootSignatureCacheD3D12&) = delete;
    RootSignatureCacheD3D12             (RootSignatureCacheD3D12&&)      = delete;
    RootSignatureCacheD3D12& operator = (const RootSignatureCacheD3D12&) = delete;
    RootSignatureCacheD3D12& operator = (RootSignatureCacheD3D12&&)      = delete;
    // clang-format on

    ~RootSignatureCacheD3D12();

    RefCntAutoPtr<RootSignatureD3D12> GetRootSig(const RefCntAutoPtr<PipelineResourceSignatureD3D12Impl>* ppSignatures, Uint32 SignatureCount);

    void OnDestroyRootSig(RootSignatureD3D12* pRootSig);

private:
    struct RootSignatureHash
    {
        std::size_t operator()(const RootSignatureD3D12* Key) const noexcept
        {
            return Key->GetHash();
        }
    };

    struct RootSignatureCompare
    {
        bool operator()(const RootSignatureD3D12* lhs, const RootSignatureD3D12* rhs) const noexcept;
    };

private:
    RenderDeviceD3D12Impl& m_DeviceD3D12Impl;

    std::mutex m_RootSigCacheGuard;

    std::unordered_set<RootSignatureD3D12*, RootSignatureHash, RootSignatureCompare> m_RootSigCache;
};

} // namespace Diligent
