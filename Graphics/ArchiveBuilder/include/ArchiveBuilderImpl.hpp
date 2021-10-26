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
#include "ArchiveBuilder.h"

#include "PipelineResourceSignatureBase.hpp"
#include "DeviceObjectArchiveBase.hpp"
#include "RefCntAutoPtr.hpp"
#include "ObjectBase.hpp"

namespace Diligent
{

class ArchiveBuilderImpl final : public ObjectBase<IArchiveBuilder>
{
public:
    using TBase = ObjectBase<IArchiveBuilder>;

    explicit ArchiveBuilderImpl(IReferenceCounters* pRefCounters);
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

    /// Implementation of IArchiveBuilder::ArchiveRenderPass().
    virtual Bool DILIGENT_CALL_TYPE ArchiveRenderPass(const RenderPassDesc&        Desc,
                                                      const RenderPassArchiveInfo& ArchiveInfo) override final;

private:
    using DeviceType               = DeviceObjectArchiveBase::DeviceType;
    using ArchiveHeader            = DeviceObjectArchiveBase::ArchiveHeader;
    using ChunkType                = DeviceObjectArchiveBase::ChunkType;
    using ChunkHeader              = DeviceObjectArchiveBase::ChunkHeader;
    using NamedResourceArrayHeader = DeviceObjectArchiveBase::NamedResourceArrayHeader;
    using PRSDataHeader            = DeviceObjectArchiveBase::PRSDataHeader;
    using PSODataHeader            = DeviceObjectArchiveBase::PSODataHeader;
    using RPDataHeader             = DeviceObjectArchiveBase::RPDataHeader;
    using TPRSNames                = DeviceObjectArchiveBase::TPRSNames;

    struct TSerializedMem
    {
        void*  Ptr  = nullptr;
        size_t Size = 0;

        TSerializedMem() {}
        TSerializedMem(void* _Ptr, size_t _Size) :
            Ptr{_Ptr}, Size{_Size} {}

        TSerializedMem(TSerializedMem&& Other) :
            Ptr{Other.Ptr}, Size{Other.Size}
        {
            Other.Ptr  = nullptr;
            Other.Size = 0;
        }

        ~TSerializedMem();

        TSerializedMem& operator=(TSerializedMem&& Rhs);

        explicit operator bool() const { return Ptr != nullptr; }
    };

    static constexpr Uint32 DeviceDataCount = Uint32{DeviceType::Count} + 1;

    struct PRSData
    {
        TSerializedMem                           DescMem;
        PipelineResourceSignatureDesc*           pDesc       = nullptr;
        PipelineResourceSignatureSerializedData* pSerialized = nullptr;

    private:
        std::array<TSerializedMem, DeviceDataCount> m_PerDeviceData;

    public:
        TSerializedMem&       GetSharedData() { return m_PerDeviceData[0]; }
        TSerializedMem const& GetSharedData() const { return m_PerDeviceData[0]; }
        TSerializedMem&       GetDeviceData(DeviceType DevType) { return m_PerDeviceData[Uint32{DevType} + 1]; }
        TSerializedMem const& GetDeviceData(DeviceType DevType) const { return m_PerDeviceData[Uint32{DevType} + 1]; }
        TSerializedMem&       GetData(Uint32 Ind) { return m_PerDeviceData[Ind]; }
        TSerializedMem const& GetData(Uint32 Ind) const { return m_PerDeviceData[Ind]; }
    };
    std::unordered_map<String, PRSData> m_PRSMap;

    struct RPData
    {
        TSerializedMem SharedData;

        TSerializedMem const& GetSharedData() const { return SharedData; }
    };
    std::unordered_map<String, RPData> m_RPMap;

    struct GraphicsPSOData
    {
        TSerializedMem                   DescMem;
        GraphicsPipelineStateCreateInfo* pCreateInfo = nullptr;

    private:
        std::array<TSerializedMem, DeviceDataCount> m_PerDeviceData;

    public:
        TSerializedMem&       GetSharedData() { return m_PerDeviceData[0]; }
        TSerializedMem const& GetSharedData() const { return m_PerDeviceData[0]; }
        TSerializedMem&       GetDeviceData(DeviceType DevType) { return m_PerDeviceData[Uint32{DevType} + 1]; }
        TSerializedMem&       GetData(Uint32 Ind) { return m_PerDeviceData[Ind]; }
    };
    std::unordered_map<String, PRSData> m_GraphicsPSOMap;

private:
    struct PendingData
    {
        std::array<std::vector<Uint8>, Uint32{ChunkType::Count}>   ChunkData;                             // NamedResourceArrayHeader
        std::array<std::vector<Uint8>, DeviceDataCount>            ArchiveData;                           // ***DataHeader, device specific data
        std::array<Uint32*, Uint32{ChunkType::Count}>              DataOffsetArrayPerChunk          = {}; // pointer to NamedResourceArrayHeader::DataOffset - offsets to ***DataHeader
        std::array<Uint32, Uint32{ChunkType::Count}>               ResourceCountPerChunk            = {};
        std::array<std::vector<Uint32*>, Uint32{ChunkType::Count}> DeviceSpecificDataOffsetPerChunk = {};
    };

    void ReserveSpace(std::array<size_t, DeviceDataCount>& ArchiveDataSize) const;
    void WriteResourceSignatureData(PendingData& Dst) const;
    void WriteRenderPassData(PendingData& Dst) const;
    void WriteGraphicsPSOData(PendingData& Dst) const;
};

} // namespace Diligent
