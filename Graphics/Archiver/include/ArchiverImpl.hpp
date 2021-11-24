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

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <array>
#include <vector>
#include <memory>

#include "Archiver.h"
#include "ArchiverFactory.h"
#include "RenderDevice.h"
#include "PipelineResourceSignature.h"
#include "PipelineState.h"

#include "DeviceObjectArchiveBase.hpp"
#include "RefCntAutoPtr.hpp"
#include "ObjectBase.hpp"

#include "HashUtils.hpp"
#include "BasicMath.hpp"
#include "PlatformMisc.hpp"
#include "DataBlobImpl.hpp"
#include "MemoryFileStream.hpp"
#include "FixedLinearAllocator.hpp"

#include "SerializationDeviceImpl.hpp"
#include "SerializedMemory.hpp"
#include "SerializableShaderImpl.hpp"
#include "SerializableRenderPassImpl.hpp"
#include "SerializableResourceSignatureImpl.hpp"

namespace Diligent
{

class ArchiverImpl final : public ObjectBase<IArchiver>
{
public:
    using TBase = ObjectBase<IArchiver>;

    ArchiverImpl(IReferenceCounters* pRefCounters, SerializationDeviceImpl* pDevice);
    ~ArchiverImpl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_Archiver, TBase)

    /// Implementation of IArchiver::SerializeToBlob().
    virtual Bool DILIGENT_CALL_TYPE SerializeToBlob(IDataBlob** ppBlob) override final;

    /// Implementation of IArchiver::SerializeToStream().
    virtual Bool DILIGENT_CALL_TYPE SerializeToStream(IFileStream* pStream) override final;

    /// Implementation of IArchiver::AddGraphicsPipelineState().
    virtual Bool DILIGENT_CALL_TYPE AddGraphicsPipelineState(const GraphicsPipelineStateCreateInfo& PSOCreateInfo,
                                                             const PipelineStateArchiveInfo&        ArchiveInfo) override final;

    /// Implementation of IArchiver::AddComputePipelineState().
    virtual Bool DILIGENT_CALL_TYPE AddComputePipelineState(const ComputePipelineStateCreateInfo& PSOCreateInfo,
                                                            const PipelineStateArchiveInfo&       ArchiveInfo) override final;

    /// Implementation of IArchiver::AddRayTracingPipelineState().
    virtual Bool DILIGENT_CALL_TYPE AddRayTracingPipelineState(const RayTracingPipelineStateCreateInfo& PSOCreateInfo,
                                                               const PipelineStateArchiveInfo&          ArchiveInfo) override final;

    /// Implementation of IArchiver::AddTilePipelineState().
    virtual Bool DILIGENT_CALL_TYPE AddTilePipelineState(const TilePipelineStateCreateInfo& PSOCreateInfo,
                                                         const PipelineStateArchiveInfo&    ArchiveInfo) override final;

    /// Implementation of IArchiver::AddPipelineResourceSignature().
    virtual Bool DILIGENT_CALL_TYPE AddPipelineResourceSignature(const PipelineResourceSignatureDesc& SignatureDesc,
                                                                 const ResourceSignatureArchiveInfo&  ArchiveInfo) override final;

private:
    using DeviceType               = DeviceObjectArchiveBase::DeviceType;
    using ArchiveHeader            = DeviceObjectArchiveBase::ArchiveHeader;
    using ChunkType                = DeviceObjectArchiveBase::ChunkType;
    using ChunkHeader              = DeviceObjectArchiveBase::ChunkHeader;
    using NamedResourceArrayHeader = DeviceObjectArchiveBase::NamedResourceArrayHeader;
    using FileOffsetAndSize        = DeviceObjectArchiveBase::FileOffsetAndSize;
    using PRSDataHeader            = DeviceObjectArchiveBase::PRSDataHeader;
    using PSODataHeader            = DeviceObjectArchiveBase::PSODataHeader;
    using RPDataHeader             = DeviceObjectArchiveBase::RPDataHeader;
    using ShadersDataHeader        = DeviceObjectArchiveBase::ShadersDataHeader;
    using TPRSNames                = DeviceObjectArchiveBase::TPRSNames;
    using ShaderIndexArray         = DeviceObjectArchiveBase::ShaderIndexArray;

    static constexpr auto InvalidOffset   = DeviceObjectArchiveBase::BaseDataHeader::InvalidOffset;
    static constexpr auto DeviceDataCount = static_cast<size_t>(DeviceType::Count);
    static constexpr auto ChunkCount      = static_cast<size_t>(ChunkType::Count);

    using TPerDeviceData = std::array<SerializedMemory, DeviceDataCount>;

    template <typename Type>
    using TNamedObjectHashMap = std::unordered_map<HashMapStringKey, Type, HashMapStringKey::Hasher>;

    struct PRSData
    {
        RefCntAutoPtr<SerializableResourceSignatureImpl> pPRS;

        const SerializedMemory& GetSharedData() const;
        const SerializedMemory& GetDeviceData(Uint32 Idx) const;
    };
    TNamedObjectHashMap<PRSData> m_PRSMap;

    struct SerializablePRSHasher
    {
        size_t operator()(const RefCntAutoPtr<SerializableResourceSignatureImpl>& PRS) const
        {
            return PRS ? PRS->CalcHash() : 0;
        }
    };
    struct SerializablePRSEqual
    {
        bool operator()(const RefCntAutoPtr<SerializableResourceSignatureImpl>& Lhs, const RefCntAutoPtr<SerializableResourceSignatureImpl>& Rhs) const
        {
            if ((Lhs == nullptr) != (Rhs == nullptr))
                return false;
            if ((Lhs == nullptr) && (Rhs == nullptr))
                return true;
            return *Lhs == *Rhs;
        }
    };
    std::unordered_set<RefCntAutoPtr<SerializableResourceSignatureImpl>, SerializablePRSHasher, SerializablePRSEqual> m_PRSCache;

    struct RPData
    {
        RefCntAutoPtr<SerializableRenderPassImpl> pRP;

        const SerializedMemory& GetSharedData() const;
    };
    using RPMapType = std::unordered_map<HashMapStringKey, RPData, HashMapStringKey::Hasher>;
    RPMapType m_RPMap;

    struct ShaderKey
    {
        std::shared_ptr<SerializedMemory> Ptr;

        bool operator==(const ShaderKey& Rhs) const { return *Ptr == *Rhs.Ptr; }

        struct Hash
        {
            size_t operator()(const ShaderKey& Key) const { return Key.Ptr->CalcHash(); }
        };
    };

    struct PerDeviceShaders
    {
        std::vector<ShaderKey>                                                   List;
        std::unordered_map<ShaderKey, /*Index in List*/ size_t, ShaderKey::Hash> Map;
    };
    std::array<PerDeviceShaders, static_cast<Uint32>(DeviceType::Count)> m_Shaders;

    template <typename CreateInfoType>
    struct TPSOData
    {
        SerializedMemory DescMem;
        CreateInfoType*  pCreateInfo = nullptr;
        SerializedMemory SharedData;
        TPerDeviceData   PerDeviceData;
        // AZ TODO: serialize base bindings ?

        const SerializedMemory& GetSharedData() const { return SharedData; }
    };
    using GraphicsPSOData   = TPSOData<GraphicsPipelineStateCreateInfo>;
    using ComputePSOData    = TPSOData<ComputePipelineStateCreateInfo>;
    using TilePSOData       = TPSOData<TilePipelineStateCreateInfo>;
    using RayTracingPSOData = TPSOData<RayTracingPipelineStateCreateInfo>;

    TNamedObjectHashMap<GraphicsPSOData>   m_GraphicsPSOMap;
    TNamedObjectHashMap<ComputePSOData>    m_ComputePSOMap;
    TNamedObjectHashMap<TilePSOData>       m_TilePSOMap;
    TNamedObjectHashMap<RayTracingPSOData> m_RayTracingPSOMap;

    SerializationDeviceImpl* const m_pSerializationDevice;

private:
    using TChunkData      = FixedLinearAllocator;
    using TChunkDataArray = std::array<TChunkData, ChunkCount>;
    struct PendingData
    {
        std::vector<Uint8>                              HeaderData;                   // ArchiveHeader, ChunkHeader[]
        TChunkDataArray                                 ChunkData;                    // NamedResourceArrayHeader
        std::array<Uint32*, ChunkCount>                 DataOffsetArrayPerChunk = {}; // pointer to NamedResourceArrayHeader::DataOffset - offsets to ***DataHeader
        std::array<Uint32, ChunkCount>                  ResourceCountPerChunk   = {}; //
        std::vector<Uint8>                              SharedData;                   // ***DataHeader
        std::array<std::vector<Uint8>, DeviceDataCount> PerDeviceData;                // device specific data
        size_t                                          OffsetInFile = 0;
    };

    void ReserveSpace(size_t& SharedDataSize, std::array<size_t, DeviceDataCount>& PerDeviceDataSize) const;
    void WriteDebugInfo(PendingData& Pending) const;
    void WriteResourceSignatureData(PendingData& Pending) const;
    void WriteShaderData(PendingData& Pending) const;
    void WriteRenderPassData(PendingData& Pending) const;
    template <typename PSOType>
    void WritePSOData(PendingData& Pending, TNamedObjectHashMap<PSOType>& Map, ChunkType Chunk) const;
    void UpdateOffsetsInArchive(PendingData& Pending) const;
    void WritePendingDataToStream(const PendingData& Pending, IFileStream* pStream) const;

    using TShaderIndices = std::vector<Uint32>; // shader data indices in device specific block

    template <typename CreateInfoType>
    bool SerializePSO(TNamedObjectHashMap<TPSOData<CreateInfoType>>& PSOMap,
                      const CreateInfoType&                          PSOCreateInfo,
                      const PipelineStateArchiveInfo&                ArchiveInfo) noexcept;

    void SerializeShaderBytecode(TShaderIndices& ShaderIndices, DeviceType DevType, const ShaderCreateInfo& CI, const void* Bytecode, size_t BytecodeSize);
    void SerializeShaderSource(TShaderIndices& ShaderIndices, DeviceType DevType, const ShaderCreateInfo& CI);

    struct DefaultPRSInfo
    {
        RefCntAutoPtr<IPipelineResourceSignature> pPRS;
        RENDER_DEVICE_TYPE_FLAGS                  DeviceFlags = RENDER_DEVICE_TYPE_FLAG_NONE;
        String                                    UniqueName;
    };

    template <typename CreateInfoType>
    bool PatchShadersVk(CreateInfoType& CreateInfo, TPSOData<CreateInfoType>& Data, DefaultPRSInfo& DefPRS);

    template <typename CreateInfoType>
    bool PatchShadersD3D12(CreateInfoType& CreateInfo, TPSOData<CreateInfoType>& Data, DefaultPRSInfo& DefPRS);

    template <typename CreateInfoType>
    bool PatchShadersD3D11(CreateInfoType& CreateInfo, TPSOData<CreateInfoType>& Data, DefaultPRSInfo& DefPRS);

    template <typename CreateInfoType>
    bool PatchShadersGL(CreateInfoType& CreateInfo, TPSOData<CreateInfoType>& Data, DefaultPRSInfo& DefPRS);

    template <typename CreateInfoType>
    bool PatchShadersMtl(CreateInfoType& CreateInfo, TPSOData<CreateInfoType>& Data, DefaultPRSInfo& DefPRS);


    void SerializeShadersForPSO(const TShaderIndices& ShaderIndices, SerializedMemory& DeviceData) const;

    template <typename MapType>
    static TChunkData InitNamedResourceArrayHeader(const MapType& Map,
                                                   Uint32*&       DataSizeArray,
                                                   Uint32*&       DataOffsetArray);

    bool AddPipelineResourceSignature(IPipelineResourceSignature* pPRS);
    bool CachePipelineResourceSignature(RefCntAutoPtr<IPipelineResourceSignature>& pPRS);
    bool AddRenderPass(IRenderPass* pRP);

    template <SerializerMode Mode>
    void SerializeDebugInfo(Serializer<Mode>& Ser) const;

    String UniquePRSName();

    template <typename FnType>
    bool CreateDefaultResourceSignature(DefaultPRSInfo& DefPRS, const FnType& CreatePRS);
};

#if D3D11_SUPPORTED
extern template bool ArchiverImpl::PatchShadersD3D11<GraphicsPipelineStateCreateInfo>(GraphicsPipelineStateCreateInfo& CreateInfo, TPSOData<GraphicsPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersD3D11<ComputePipelineStateCreateInfo>(ComputePipelineStateCreateInfo& CreateInfo, TPSOData<ComputePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersD3D11<TilePipelineStateCreateInfo>(TilePipelineStateCreateInfo& CreateInfo, TPSOData<TilePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersD3D11<RayTracingPipelineStateCreateInfo>(RayTracingPipelineStateCreateInfo& CreateInfo, TPSOData<RayTracingPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
#endif

#if D3D12_SUPPORTED
extern template bool ArchiverImpl::PatchShadersD3D12<GraphicsPipelineStateCreateInfo>(GraphicsPipelineStateCreateInfo& CreateInfo, TPSOData<GraphicsPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersD3D12<ComputePipelineStateCreateInfo>(ComputePipelineStateCreateInfo& CreateInfo, TPSOData<ComputePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersD3D12<TilePipelineStateCreateInfo>(TilePipelineStateCreateInfo& CreateInfo, TPSOData<TilePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersD3D12<RayTracingPipelineStateCreateInfo>(RayTracingPipelineStateCreateInfo& CreateInfo, TPSOData<RayTracingPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
#endif

#if GL_SUPPORTED
extern template bool ArchiverImpl::PatchShadersGL<GraphicsPipelineStateCreateInfo>(GraphicsPipelineStateCreateInfo& CreateInfo, TPSOData<GraphicsPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersGL<ComputePipelineStateCreateInfo>(ComputePipelineStateCreateInfo& CreateInfo, TPSOData<ComputePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersGL<TilePipelineStateCreateInfo>(TilePipelineStateCreateInfo& CreateInfo, TPSOData<TilePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersGL<RayTracingPipelineStateCreateInfo>(RayTracingPipelineStateCreateInfo& CreateInfo, TPSOData<RayTracingPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
#endif

#if VULKAN_SUPPORTED
extern template bool ArchiverImpl::PatchShadersVk<GraphicsPipelineStateCreateInfo>(GraphicsPipelineStateCreateInfo& CreateInfo, TPSOData<GraphicsPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersVk<ComputePipelineStateCreateInfo>(ComputePipelineStateCreateInfo& CreateInfo, TPSOData<ComputePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersVk<TilePipelineStateCreateInfo>(TilePipelineStateCreateInfo& CreateInfo, TPSOData<TilePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersVk<RayTracingPipelineStateCreateInfo>(RayTracingPipelineStateCreateInfo& CreateInfo, TPSOData<RayTracingPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
#endif

#if METAL_SUPPORTED
extern template bool ArchiverImpl::PatchShadersMtl<GraphicsPipelineStateCreateInfo>(GraphicsPipelineStateCreateInfo& CreateInfo, TPSOData<GraphicsPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersMtl<ComputePipelineStateCreateInfo>(ComputePipelineStateCreateInfo& CreateInfo, TPSOData<ComputePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersMtl<TilePipelineStateCreateInfo>(TilePipelineStateCreateInfo& CreateInfo, TPSOData<TilePipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
extern template bool ArchiverImpl::PatchShadersMtl<RayTracingPipelineStateCreateInfo>(RayTracingPipelineStateCreateInfo& CreateInfo, TPSOData<RayTracingPipelineStateCreateInfo>& Data, DefaultPRSInfo& DefPRS);
#endif

} // namespace Diligent
