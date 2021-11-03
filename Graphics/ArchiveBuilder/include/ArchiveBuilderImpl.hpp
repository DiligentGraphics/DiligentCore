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
#include <array>
#include <bitset>

#include "ArchiveBuilder.h"
#include "ArchiveBuilderFactory.h"
#include "RenderDevice.h"

#include "PipelineResourceSignatureBase.hpp"
#include "DeviceObjectArchiveBase.hpp"
#include "RefCntAutoPtr.hpp"
#include "ObjectBase.hpp"

#include "HashUtils.hpp"
#include "BasicMath.hpp"
#include "PlatformMisc.hpp"
#include "DataBlobImpl.hpp"
#include "MemoryFileStream.hpp"
#include "PipelineStateBase.hpp"

#include "DummyRenderDevice.hpp"
#include "SerializedMemory.hpp"
#include "SerializableShaderImpl.hpp"
#include "SerializableRenderPassImpl.hpp"
#include "SerializableResourceSignatureImpl.hpp"

#if D3D12_SUPPORTED
#    include "../../GraphicsEngineD3D12/include/pch.h"
#    include "RenderDeviceD3D12Impl.hpp"
#    include "PipelineResourceSignatureD3D12Impl.hpp"
#    include "PipelineStateD3D12Impl.hpp"
#    include "ShaderD3D12Impl.hpp"
#    include "DeviceObjectArchiveD3D12Impl.hpp"
#endif
#if VULKAN_SUPPORTED
#    include "VulkanUtilities/VulkanHeaders.h"
#    include "RenderDeviceVkImpl.hpp"
#    include "PipelineResourceSignatureVkImpl.hpp"
#    include "PipelineStateVkImpl.hpp"
#    include "ShaderVkImpl.hpp"
#    include "DeviceObjectArchiveVkImpl.hpp"
#endif

namespace Diligent
{

class ArchiveBuilderImpl final : public ObjectBase<IArchiveBuilder>
{
public:
    using TBase = ObjectBase<IArchiveBuilder>;

    ArchiveBuilderImpl(IReferenceCounters* pRefCounters, DummyRenderDevice* pDevice, IArchiveBuilderFactory* pFactory);
    ~ArchiveBuilderImpl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_ArchiveBuilder, TBase)

    /// Implementation of IArchiveBuilder::SerializeToBlob().
    virtual Bool DILIGENT_CALL_TYPE SerializeToBlob(IDataBlob** ppBlob) override final;

    /// Implementation of IArchiveBuilder::SerializeToStream().
    virtual Bool DILIGENT_CALL_TYPE SerializeToStream(IFileStream* pStream) override final;

    /// Implementation of IArchiveBuilder::ArchiveGraphicsPipelineState().
    virtual Bool DILIGENT_CALL_TYPE ArchiveGraphicsPipelineState(const GraphicsPipelineStateCreateInfo& PSOCreateInfo,
                                                                 const PipelineStateArchiveInfo&        ArchiveInfo) override final;

    /// Implementation of IArchiveBuilder::ArchiveComputePipelineState().
    virtual Bool DILIGENT_CALL_TYPE ArchiveComputePipelineState(const ComputePipelineStateCreateInfo& PSOCreateInfo,
                                                                const PipelineStateArchiveInfo&       ArchiveInfo) override final;

    /// Implementation of IArchiveBuilder::ArchiveRayTracingPipelineState().
    virtual Bool DILIGENT_CALL_TYPE ArchiveRayTracingPipelineState(const RayTracingPipelineStateCreateInfo& PSOCreateInfo,
                                                                   const PipelineStateArchiveInfo&          ArchiveInfo) override final;

    /// Implementation of IArchiveBuilder::ArchiveTilePipelineState().
    virtual Bool DILIGENT_CALL_TYPE ArchiveTilePipelineState(const TilePipelineStateCreateInfo& PSOCreateInfo,
                                                             const PipelineStateArchiveInfo&    ArchiveInfo) override final;

    /// Implementation of IArchiveBuilder::ArchivePipelineResourceSignature().
    virtual Bool DILIGENT_CALL_TYPE ArchivePipelineResourceSignature(const PipelineResourceSignatureDesc& SignatureDesc,
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

    static constexpr auto   InvalidOffset   = DeviceObjectArchiveBase::BaseDataHeader::InvalidOffset;
    static constexpr Uint32 DeviceDataCount = Uint32{DeviceType::Count};
    using TPerDeviceData                    = std::array<SerializedMemory, DeviceDataCount>;

    struct PRSData
    {
        RefCntAutoPtr<SerializableResourceSignatureImpl> pPRS;

        const SerializedMemory& GetSharedData() const;
        const SerializedMemory& GetDeviceData(Uint32 Idx) const;
    };
    //std::unordered_map<HashMapStringKey, PRSData, HashMapStringKey::Hasher> m_PRSMap;
    std::unordered_map<String, PRSData> m_PRSMap;

    struct RPData
    {
        RefCntAutoPtr<SerializableRenderPassImpl> pRP;

        const SerializedMemory& GetSharedData() const;
    };
    std::unordered_map<String, RPData> m_RPMap;

    struct ShaderKey
    {
        SerializedMemory Data;

        bool operator==(const ShaderKey& Rhs) const;
    };
    struct ShaderKeyHash
    {
        size_t operator()(const ShaderKey& Key) const;
    };
    struct PerDeviceShaders
    {
        std::unordered_map<ShaderKey, /*Index*/ size_t, ShaderKeyHash> Map;
    };
    std::array<PerDeviceShaders, Uint32{DeviceType::Count}> m_Shaders;

    struct GraphicsPSOData
    {
        SerializedMemory                 DescMem;
        GraphicsPipelineStateCreateInfo* pCreateInfo = nullptr;
        SerializedMemory                 SharedData;
        TPerDeviceData                   PerDeviceData;

        const SerializedMemory& GetSharedData() const { return SharedData; }
    };
    std::unordered_map<String, GraphicsPSOData> m_GraphicsPSOMap;

    DummyRenderDevice*      m_pRenderDevice   = nullptr;
    IArchiveBuilderFactory* m_pArchiveFactory = nullptr;

private:
    struct PendingData
    {
        // AZ TODO: use SerializedMemory instead of vector
        std::vector<Uint8>                                       HeaderData;                   // ArchiveHeader, ChunkHeader[]
        std::array<std::vector<Uint8>, Uint32{ChunkType::Count}> ChunkData;                    // NamedResourceArrayHeader
        std::array<Uint32*, Uint32{ChunkType::Count}>            DataOffsetArrayPerChunk = {}; // pointer to NamedResourceArrayHeader::DataOffset - offsets to ***DataHeader
        std::array<Uint32, Uint32{ChunkType::Count}>             ResourceCountPerChunk   = {}; //
        std::vector<Uint8>                                       SharedData;                   // ***DataHeader
        std::array<std::vector<Uint8>, DeviceDataCount>          PerDeviceData;                // device specific data
        size_t                                                   OffsetInFile = 0;
    };

    void ReserveSpace(size_t& SharedDataSize, std::array<size_t, DeviceDataCount>& PerDeviceDataSize) const;
    void WriteResourceSignatureData(PendingData& Pending) const;
    void WriteShaderData(PendingData& Pending) const;
    void WriteRenderPassData(PendingData& Pending) const;
    void WriteGraphicsPSOData(PendingData& Pending) const;
    void UpdateOffsetsInArchive(PendingData& Pending) const;
    void WritePendingDataToStream(const PendingData& Pending, IFileStream* pStream) const;

    using TShaderIndices = std::vector<Uint32>; // shader data indices in device specific block

    template <typename CreateInfoType>
    bool PatchShadersVk(const CreateInfoType&           CreateInfo,
                        const PipelineStateArchiveInfo& ArchiveInfo,
                        TShaderIndices&                 ShaderIndices);
    void SerializeShadersForPSO(const TShaderIndices& ShaderIndices,
                                SerializedMemory&     DeviceData) const;

    template <typename DataType>
    static void InitNamedResourceArrayHeader(std::vector<Uint8>&                         ChunkData,
                                             const std::unordered_map<String, DataType>& Map,
                                             Uint32*&                                    DataSizeArray,
                                             Uint32*&                                    DataOffsetArray);

    bool AddPipelineResourceSignature(IPipelineResourceSignature* pPRS);
    bool AddRenderPass(IRenderPass* pRP);
};

template <SerializerMode Mode>
using SerializerImpl = DeviceObjectArchiveBase::SerializerImpl<Mode>;

} // namespace Diligent
