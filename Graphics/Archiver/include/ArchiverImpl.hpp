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

#include "Archiver.h"
#include "ArchiverFactory.h"
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

#include "SerializationDeviceImpl.hpp"
#include "SerializedMemory.hpp"
#include "SerializableShaderImpl.hpp"
#include "SerializableRenderPassImpl.hpp"
#include "SerializableResourceSignatureImpl.hpp"

#if D3D11_SUPPORTED
#    include "../../GraphicsEngineD3D11/include/pch.h"
#    include "RenderDeviceD3D11Impl.hpp"
#    include "PipelineResourceSignatureD3D11Impl.hpp"
#    include "PipelineStateD3D11Impl.hpp"
#    include "ShaderD3D11Impl.hpp"
#    include "DeviceObjectArchiveD3D11Impl.hpp"
#endif
#if D3D12_SUPPORTED
#    include "../../GraphicsEngineD3D12/include/pch.h"
#    include "RenderDeviceD3D12Impl.hpp"
#    include "PipelineResourceSignatureD3D12Impl.hpp"
#    include "PipelineStateD3D12Impl.hpp"
#    include "ShaderD3D12Impl.hpp"
#    include "DeviceObjectArchiveD3D12Impl.hpp"
#endif
#if GL_SUPPORTED || GLES_SUPPORTED
#    include "../../GraphicsEngineOpenGL/include/pch.h"
#    include "RenderDeviceGLImpl.hpp"
#    include "PipelineResourceSignatureGLImpl.hpp"
#    include "PipelineStateGLImpl.hpp"
#    include "ShaderGLImpl.hpp"
#    include "DeviceObjectArchiveGLImpl.hpp"
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

    /// Implementation of IArchiver::ArchiveGraphicsPipelineState().
    virtual Bool DILIGENT_CALL_TYPE ArchiveGraphicsPipelineState(const GraphicsPipelineStateCreateInfo& PSOCreateInfo,
                                                                 const PipelineStateArchiveInfo&        ArchiveInfo) override final;

    /// Implementation of IArchiver::ArchiveComputePipelineState().
    virtual Bool DILIGENT_CALL_TYPE ArchiveComputePipelineState(const ComputePipelineStateCreateInfo& PSOCreateInfo,
                                                                const PipelineStateArchiveInfo&       ArchiveInfo) override final;

    /// Implementation of IArchiver::ArchiveRayTracingPipelineState().
    virtual Bool DILIGENT_CALL_TYPE ArchiveRayTracingPipelineState(const RayTracingPipelineStateCreateInfo& PSOCreateInfo,
                                                                   const PipelineStateArchiveInfo&          ArchiveInfo) override final;

    /// Implementation of IArchiver::ArchiveTilePipelineState().
    virtual Bool DILIGENT_CALL_TYPE ArchiveTilePipelineState(const TilePipelineStateCreateInfo& PSOCreateInfo,
                                                             const PipelineStateArchiveInfo&    ArchiveInfo) override final;

    /// Implementation of IArchiver::ArchivePipelineResourceSignature().
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
    static constexpr Uint32 DeviceDataCount = static_cast<Uint32>(DeviceType::Count);
    static constexpr Uint32 ChunkCount      = static_cast<Uint32>(ChunkType::Count);
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
    std::array<PerDeviceShaders, static_cast<Uint32>(DeviceType::Count)> m_Shaders;

    template <typename CreateInfoType>
    struct TPSOData
    {
        SerializedMemory DescMem;
        CreateInfoType*  pCreateInfo = nullptr;
        SerializedMemory SharedData;
        TPerDeviceData   PerDeviceData;

        const SerializedMemory& GetSharedData() const { return SharedData; }
    };
    using GraphicsPSOData   = TPSOData<GraphicsPipelineStateCreateInfo>;
    using ComputePSOData    = TPSOData<ComputePipelineStateCreateInfo>;
    using TilePSOData       = TPSOData<TilePipelineStateCreateInfo>;
    using RayTracingPSOData = TPSOData<RayTracingPipelineStateCreateInfo>;

    std::unordered_map<String, GraphicsPSOData>   m_GraphicsPSOMap;
    std::unordered_map<String, ComputePSOData>    m_ComputePSOMap;
    std::unordered_map<String, TilePSOData>       m_TilePSOMap;
    std::unordered_map<String, RayTracingPSOData> m_RayTracingPSOMap;

    SerializationDeviceImpl* m_pSerializationDevice = nullptr;

private:
    struct PendingData
    {
        // AZ TODO: use SerializedMemory instead of vector
        std::vector<Uint8>                              HeaderData;                   // ArchiveHeader, ChunkHeader[]
        std::array<std::vector<Uint8>, ChunkCount>      ChunkData;                    // NamedResourceArrayHeader
        std::array<Uint32*, ChunkCount>                 DataOffsetArrayPerChunk = {}; // pointer to NamedResourceArrayHeader::DataOffset - offsets to ***DataHeader
        std::array<Uint32, ChunkCount>                  ResourceCountPerChunk   = {}; //
        std::vector<Uint8>                              SharedData;                   // ***DataHeader
        std::array<std::vector<Uint8>, DeviceDataCount> PerDeviceData;                // device specific data
        size_t                                          OffsetInFile = 0;
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
    bool SerializePSO(std::unordered_map<String, TPSOData<CreateInfoType>>& PSOMap,
                      const CreateInfoType&                                 PSOCreateInfo,
                      const PipelineStateArchiveInfo&                       ArchiveInfo) noexcept;

    void SerializeShaderBytecode(TShaderIndices& ShaderIndices, DeviceType DevType, const ShaderCreateInfo& CI, const void* Bytecode, size_t BytecodeSize);
    void SerializeShaderSource(TShaderIndices& ShaderIndices, DeviceType DevType, const ShaderCreateInfo& CI);

    template <typename CreateInfoType>
    bool PatchShadersVk(const CreateInfoType& CreateInfo, TShaderIndices& ShaderIndices);
    template <typename CreateInfoType>
    bool PatchShadersD3D12(const CreateInfoType& CreateInfo, TShaderIndices& ShaderIndices);
    template <typename CreateInfoType>
    bool PatchShadersD3D11(const CreateInfoType& CreateInfo, TShaderIndices& ShaderIndices);
    template <typename CreateInfoType>
    bool PatchShadersGL(const CreateInfoType& CreateInfo, TShaderIndices& ShaderIndices);

    void SerializeShadersForPSO(const TShaderIndices& ShaderIndices, SerializedMemory& DeviceData) const;

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
