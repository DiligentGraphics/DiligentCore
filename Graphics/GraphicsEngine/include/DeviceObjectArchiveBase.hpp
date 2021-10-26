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

/// \file
/// Implementation of the Diligent::DeviceObjectArchiveBase class

// Archive file format
//
// | ArchiveHeader |
//
// | ChunkHeader | --> offset --> | NamedResourceArrayHeader |
//
// | NamedResourceArrayHeader | --> offset --> | ***DataHeader |
//
// | ***DataHeader | --> offset --> | device specific data |

#include <array>
#include <shared_mutex>

#include "DeviceObjectArchive.h"
#include "PipelineResourceSignatureBase.hpp"
#include "PipelineState.h"

#include "ObjectBase.hpp"
#include "HashUtils.hpp"
#include "RefCntAutoPtr.hpp"
#include "DynamicLinearAllocator.hpp"
#include "Serializer.hpp"

namespace Diligent
{

/// Class implementing base functionality of the device object archive object
class DeviceObjectArchiveBase : public ObjectBase<IDeviceObjectArchive>
{
public:
    // Base interface that this class inherits.
    using BaseInterface = IDeviceObjectArchive;

    using TObjectBase = ObjectBase<BaseInterface>;

    enum class DeviceType : Uint32
    {
        OpenGL, // same as GLES
        Direct3D11,
        Direct3D12,
        Vulkan,
        Metal,
        Count
    };

    /// \param pRefCounters - Reference counters object that controls the lifetime of this device object archive.
    /// \param pSource      - AZ TODO
    DeviceObjectArchiveBase(IReferenceCounters* pRefCounters,
                            IArchiveSource*     pSource,
                            DeviceType          DevType);

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_DeviceObjectArchive, TObjectBase)

protected:
    static constexpr Uint32 HeaderMagicNumber = 0xDE00000A;
    static constexpr Uint32 HeaderVersion     = 1;
    static constexpr Uint32 DataPtrAlign      = sizeof(Uint64);

    friend class ArchiveBuilderImpl;

    struct ArchiveHeader
    {
        Uint32 MagicNumber;
        Uint32 Version;
        Uint32 NumChunks;
        //ChunkHeader Chunks      [NumChunks]
    };
    static_assert(sizeof(ArchiveHeader) == 12, "Archive header size must be 12 bytes");

    enum class ChunkType : Uint32
    {
        ArchiveDebugInfo = 1,
        ResourceSignature,
        GraphicsPipelineStates,
        ComputePipelineStates,
        RayTracingPipelineStates,
        RenderPass,
        //PipelineCache,
        //ShaderSources,
        //ShaderBinary,
        Count
    };

    struct ChunkHeader
    {
        ChunkType Type;
        Uint32    Size;
        Uint32    Offset; // offset to NamedResourceArrayHeader
    };

    struct NamedResourceArrayHeader
    {
        Uint32 Count;
        //Uint32 NameLength    [Count]
        //Uint32 ***DataSize   [Count]
        //Uint32 ***DataOffset [Count] // for PRSDataHeader / PSODataHeader
        //char   NameData      []
    };

    struct BaseDataHeader
    {
        using Uint32Array = std::array<Uint32, Uint32{DeviceType::Count}>;

        ChunkType   Type;
        Uint32Array DeviceSpecificDataSize;
        Uint32Array DeviceSpecificDataOffset;

        Uint32 GetDeviceSpecificDataSize(DeviceType DevType) const { return DeviceSpecificDataSize[Uint32{DevType}]; }
        Uint32 GetDeviceSpecificDataOffset(DeviceType DevType) const { return DeviceSpecificDataOffset[Uint32{DevType}]; }
        Uint32 GetDeviceSpecificDataEndOffset(DeviceType DevType) const { return DeviceSpecificDataOffset[Uint32{DevType}] + DeviceSpecificDataSize[Uint32{DevType}]; }

        void SetDeviceSpecificDataSize(DeviceType DevType, Uint32 Size) { DeviceSpecificDataSize[Uint32{DevType}] = Size; }
        void SetDeviceSpecificDataOffset(DeviceType DevType, Uint32 Offset) { DeviceSpecificDataOffset[Uint32{DevType}] = Offset; }
    };

    struct PRSDataHeader : BaseDataHeader
    {
        //PipelineResourceSignatureDesc
        //PipelineResourceSignatureSerializedData
    };

    struct PSODataHeader : BaseDataHeader
    {
        //GraphicsPipelineStateCreateInfo | ComputePipelineStateCreateInfo | RayTracingPipelineStateCreateInfo
    };

    struct RPDataHeader
    {
        ChunkType Type;
    };

private:
    struct FileOffsetAndSize
    {
        Uint32 Offset;
        Uint32 Size;
    };

    template <typename T>
    struct FileOffsetAndResCache : FileOffsetAndSize
    {
        RefCntWeakPtr<T> Cache;

        FileOffsetAndResCache(const FileOffsetAndSize& OffsetAndSize) :
            FileOffsetAndSize{OffsetAndSize}
        {}
    };

    template <typename T>
    using TNameOffsetMap = std::unordered_map<String, FileOffsetAndResCache<T>>;
    //using TNameOffsetMap = std::unordered_map<HashMapStringKey, FileOffsetAndSize, HashMapStringKey::Hasher>; // AZ TODO
    using TPRSOffsetAndCacheMap = TNameOffsetMap<IPipelineResourceSignature>;
    using TPSOOffsetAndCacheMap = TNameOffsetMap<IPipelineState>;
    using TRPOffsetAndCacheMap  = TNameOffsetMap<IRenderPass>;

    TPRSOffsetAndCacheMap m_PRSMap;
    TPSOOffsetAndCacheMap m_GraphicsPSOMap;
    TPSOOffsetAndCacheMap m_ComputePSOMap;
    TPSOOffsetAndCacheMap m_RayTracingPSOMap;
    TRPOffsetAndCacheMap  m_RenderPassMap;

    std::shared_mutex m_PRSMapGuard;
    std::shared_mutex m_GraphicsPSOMapGuard;
    std::shared_mutex m_ComputePSOMapGuard;
    std::shared_mutex m_RayTracingPSOMapGuard;
    std::shared_mutex m_RenderPassMapGuard;

    struct
    {
        String GitHash;
    } m_DebugInfo;

    RefCntAutoPtr<IArchiveSource> m_pSource; // archive source is thread-safe
    const DeviceType              m_DevType;

    template <typename ResType>
    void ReadNamedResources(const ChunkHeader& Chunk, TNameOffsetMap<ResType>& NameAndOffset, std::shared_mutex& Guard) noexcept(false);
    void ReadArchiveDebugInfo(const ChunkHeader& Chunk) noexcept(false);

    template <typename ResType>
    bool GetCachedResource(const String& Name, TNameOffsetMap<ResType>& Cache, std::shared_mutex& Guard, ResType*& pResource);
    template <typename ResType>
    void CacheResource(const String& Name, TNameOffsetMap<ResType>& Cache, std::shared_mutex& Guard, ResType* pResource);

private:
    using TPRSNames = std::array<const char*, MAX_RESOURCE_SIGNATURES>;

    struct PRSData
    {
        DynamicLinearAllocator                  Allocator;
        const PRSDataHeader*                    pHeader = nullptr;
        PipelineResourceSignatureDesc           Desc{};
        PipelineResourceSignatureSerializedData Serialized{};

        explicit PRSData(IMemoryAllocator& Allocator, Uint32 BlockSize = 4 << 10) :
            Allocator{Allocator, BlockSize}
        {}
    };
    bool ReadPRSData(const String& Name, PRSData& PRS);
    bool GetCachedPRS(const String& Name, IPipelineResourceSignature*& pSignature);
    void CachePRSResource(const String& Name, IPipelineResourceSignature* pSignature);

    template <typename CreateInfoType>
    struct PSOData
    {
        DynamicLinearAllocator Allocator;
        const PSODataHeader*   pHeader = nullptr;
        CreateInfoType         CreateInfo{};
        TPRSNames              PRSNames;
        const char*            RenderPassName = nullptr;

        explicit PSOData(IMemoryAllocator& Allocator, Uint32 BlockSize = 4 << 10) :
            Allocator{Allocator, BlockSize}
        {}
    };
    bool ReadGraphicsPSOData(const String& Name, PSOData<GraphicsPipelineStateCreateInfo>& PSO);
    bool ReadComputePSOData(const String& Name, PSOData<ComputePipelineStateCreateInfo>& PSO);
    bool ReadRayTracingPSOData(const String& Name, PSOData<RayTracingPipelineStateCreateInfo>& PSO);

    bool GetCachedGraphicsPSO(const String& Name, IPipelineState*& pPSO);
    void CacheGraphicsPSOResource(const String& Name, IPipelineState* pPSO);
    bool GetCachedComputePSO(const String& Name, IPipelineState*& pPSO);
    void CacheComputePSOResource(const String& Name, IPipelineState* pPSO);
    bool GetCachedRayTracingPSO(const String& Name, IPipelineState*& pPSO);
    void CacheRayTracingPSOResource(const String& Name, IPipelineState* pPSO);

    struct RPData
    {
        DynamicLinearAllocator Allocator;
        const RPDataHeader*    pHeader = nullptr;
        RenderPassDesc         Desc{};

        explicit RPData(IMemoryAllocator& Allocator, Uint32 BlockSize = 4 << 10) :
            Allocator{Allocator, BlockSize}
        {}
    };
    bool ReadRPData(const String& Name, RPData& RP);
    bool GetCachedRP(const String& Name, IRenderPass*& pRP);
    void CacheRPResource(const String& Name, IRenderPass* pRP);

    template <typename ResType, typename FnType>
    bool LoadResourceData(const TNameOffsetMap<ResType>& NameAndOffset,
                          std::shared_mutex&             Guard,
                          const String&                  ResourceName,
                          DynamicLinearAllocator&        Allocator,
                          const char*                    ResTypeName,
                          const FnType&                  Fn);

    template <typename HeaderType, typename FnType>
    void LoadDeviceSpecificData(const HeaderType&       Header,
                                DynamicLinearAllocator& Allocator,
                                const char*             ResTypeName,
                                const FnType&           Fn)
    {
        if (Header.GetDeviceSpecificDataSize(m_DevType) == 0)
        {
            LOG_ERROR_MESSAGE("Device specific data is not specified for ", ResTypeName);
            return;
        }
        if (Header.GetDeviceSpecificDataEndOffset(m_DevType) > m_pSource->GetSize())
        {
            LOG_ERROR_MESSAGE("Invalid offset in archive");
            return;
        }

        const auto DataSize = Header.GetDeviceSpecificDataSize(m_DevType);
        auto*      pData    = Allocator.Allocate(DataSize, DataPtrAlign);
        if (!m_pSource->Read(Header.GetDeviceSpecificDataOffset(m_DevType), pData, DataSize))
        {
            LOG_ERROR_MESSAGE("Failed to read resource signature data");
            return;
        }

        Serializer<SerializerMode::Read> Ser{pData, DataSize};
        return Fn(Ser);
    }

    static constexpr Uint32 DefaultSRBAllocationGranularity = 1;

    template <typename CreateInfoType, typename RenderDeviceImplType>
    bool CreateResourceSignatures(PSOData<CreateInfoType>& PSO, RenderDeviceImplType* pDevice);

    template <typename CreateInfoType>
    struct ReleaseTempResourceRefs
    {
        PSOData<CreateInfoType>& PSO;

        explicit ReleaseTempResourceRefs(PSOData<CreateInfoType>& _PSO) :
            PSO{_PSO} {}

        ~ReleaseTempResourceRefs();
    };

    template <typename RenderDeviceImplType>
    bool CreateRenderPass(PSOData<GraphicsPipelineStateCreateInfo>& PSO, RenderDeviceImplType* pDevice);

protected:
    template <template <SerializerMode> class TSerializerImpl,
              typename TSerializedData,
              typename RenderDeviceImplType>
    void UnpackResourceSignature(const ResourceSignatureUnpackInfo& DeArchiveInfo, RenderDeviceImplType* pDevice, IPipelineResourceSignature*& pSignature);

public:
    template <typename RenderDeviceImplType>
    void UnpackGraphicsPSO(const PipelineStateUnpackInfo& DeArchiveInfo, RenderDeviceImplType* pDevice, IPipelineState*& pPSO);
    template <typename RenderDeviceImplType>
    void UnpackComputePSO(const PipelineStateUnpackInfo& DeArchiveInfo, RenderDeviceImplType* pDevice, IPipelineState*& pPSO);
    template <typename RenderDeviceImplType>
    void UnpackRayTracingPSO(const PipelineStateUnpackInfo& DeArchiveInfo, RenderDeviceImplType* pDevice, IPipelineState*& pPSO);
    template <typename RenderDeviceImplType>
    void UnpackRenderPass(const RenderPassUnpackInfo& DeArchiveInfo, RenderDeviceImplType* pDevice, IRenderPass*& pRP);

    virtual void UnpackResourceSignature(const ResourceSignatureUnpackInfo& DeArchiveInfo, IRenderDevice* pDevice, IPipelineResourceSignature*& pSignature) = 0;

    template <SerializerMode Mode>
    struct ArraySerializerHelper
    {
        template <typename T>
        static const T* Create(const T*                SrcArray,
                               Uint32                  Count,
                               DynamicLinearAllocator* Allocator)
        {
            VERIFY_EXPR(Allocator == nullptr);
            VERIFY_EXPR((SrcArray != nullptr) == (Count != 0));
            return SrcArray;
        }
    };

    template <>
    struct ArraySerializerHelper<SerializerMode::Read>
    {
        template <typename T>
        static T* Create(const T*&               DstArray,
                         Uint32                  Count,
                         DynamicLinearAllocator* Allocator)
        {
            VERIFY_EXPR(Allocator != nullptr);
            VERIFY_EXPR(DstArray == nullptr);
            auto* pArray = Allocator->Allocate<T>(Count);
            DstArray     = pArray;
            return pArray;
        }
    };

    template <SerializerMode Mode>
    struct SerializerImpl
    {
        template <typename T>
        using TQual = typename Serializer<Mode>::template TQual<T>;

        static void SerializeImmutableSampler(Serializer<Mode>&            Ser,
                                              TQual<ImmutableSamplerDesc>& SampDesc);

        static void SerializePRS(Serializer<Mode>&                               Ser,
                                 TQual<PipelineResourceSignatureDesc>&           Desc,
                                 TQual<PipelineResourceSignatureSerializedData>& Serialized,
                                 DynamicLinearAllocator*                         Allocator);

        static void SerializePSO(Serializer<Mode>&               Ser,
                                 TQual<PipelineStateCreateInfo>& CreateInfo,
                                 TQual<TPRSNames>&               PRSNames,
                                 DynamicLinearAllocator*         Allocator);

        static void SerializeGraphicsPSO(Serializer<Mode>&                       Ser,
                                         TQual<GraphicsPipelineStateCreateInfo>& CreateInfo,
                                         TQual<TPRSNames>&                       PRSNames,
                                         TQual<const char*>&                     RenderPassName,
                                         DynamicLinearAllocator*                 Allocator);

        static void SerializeComputePSO(Serializer<Mode>&                      Ser,
                                        TQual<ComputePipelineStateCreateInfo>& CreateInfo,
                                        TQual<TPRSNames>&                      PRSNames,
                                        DynamicLinearAllocator*                Allocator);

        static void SerializeRayTracingPSO(Serializer<Mode>&                         Ser,
                                           TQual<RayTracingPipelineStateCreateInfo>& CreateInfo,
                                           TQual<TPRSNames>&                         PRSNames,
                                           DynamicLinearAllocator*                   Allocator);

        static void SerializeRenderPass(Serializer<Mode>&       Ser,
                                        TQual<RenderPassDesc>&  RPDesc,
                                        DynamicLinearAllocator* Allocator);
    };
};

DECL_TRIVIALLY_SERIALIZABLE(BlendStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(RasterizerStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(DepthStencilStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(SampleDesc);


template <>
inline DeviceObjectArchiveBase::ReleaseTempResourceRefs<GraphicsPipelineStateCreateInfo>::~ReleaseTempResourceRefs()
{
    if (PSO.CreateInfo.ppResourceSignatures != nullptr)
    {
        for (Uint32 i = 0; i < PSO.CreateInfo.ResourceSignaturesCount; ++i)
        {
            if (PSO.CreateInfo.ppResourceSignatures[i] != nullptr)
                PSO.CreateInfo.ppResourceSignatures[i]->Release();
        }
    }

    if (PSO.CreateInfo.GraphicsPipeline.pRenderPass != nullptr)
    {
        PSO.CreateInfo.GraphicsPipeline.pRenderPass->Release();
    }

    // AZ TODO: release shaders
}

template <>
inline DeviceObjectArchiveBase::ReleaseTempResourceRefs<ComputePipelineStateCreateInfo>::~ReleaseTempResourceRefs()
{
    if (PSO.CreateInfo.ppResourceSignatures != nullptr)
    {
        for (Uint32 i = 0; i < PSO.CreateInfo.ResourceSignaturesCount; ++i)
        {
            if (PSO.CreateInfo.ppResourceSignatures[i] != nullptr)
                PSO.CreateInfo.ppResourceSignatures[i]->Release();
        }
    }

    // AZ TODO: release shaders
}

template <>
inline DeviceObjectArchiveBase::ReleaseTempResourceRefs<RayTracingPipelineStateCreateInfo>::~ReleaseTempResourceRefs()
{
    if (PSO.CreateInfo.ppResourceSignatures != nullptr)
    {
        for (Uint32 i = 0; i < PSO.CreateInfo.ResourceSignaturesCount; ++i)
        {
            if (PSO.CreateInfo.ppResourceSignatures[i] != nullptr)
                PSO.CreateInfo.ppResourceSignatures[i]->Release();
        }
    }

    // AZ TODO: release shaders
}

template <typename CreateInfoType, typename RenderDeviceImplType>
bool DeviceObjectArchiveBase::CreateResourceSignatures(PSOData<CreateInfoType>& PSO, RenderDeviceImplType* pRenderDevice)
{
    if (PSO.CreateInfo.ResourceSignaturesCount == 0)
        return true;

    auto* ppResourceSignatures = PSO.Allocator.Allocate<IPipelineResourceSignature*>(PSO.CreateInfo.ResourceSignaturesCount);

    ResourceSignatureUnpackInfo UnpackInfo;
    UnpackInfo.SRBAllocationGranularity = DefaultSRBAllocationGranularity;

    PSO.CreateInfo.ppResourceSignatures = ppResourceSignatures;
    for (Uint32 i = 0; i < PSO.CreateInfo.ResourceSignaturesCount; ++i)
    {
        UnpackInfo.Name = PSO.PRSNames[i];
        UnpackResourceSignature(UnpackInfo, pRenderDevice, ppResourceSignatures[i]); // Reference released in ~ReleaseTempResourceRefs()
        if (ppResourceSignatures[i] == nullptr)
            return false;
    }
    return true;
}

template <template <SerializerMode> class TSerializerImpl,
          typename TSerializedData,
          typename RenderDeviceImplType>
void DeviceObjectArchiveBase::UnpackResourceSignature(const ResourceSignatureUnpackInfo& DeArchiveInfo, RenderDeviceImplType* pRenderDevice, IPipelineResourceSignature*& pSignature)
{
    VERIFY_EXPR(pRenderDevice != nullptr);

    if (GetCachedPRS(DeArchiveInfo.Name, pSignature))
        return;

    PRSData PRS{GetRawAllocator()};
    if (!ReadPRSData(DeArchiveInfo.Name, PRS))
        return;

    PRS.Desc.SRBAllocationGranularity = DeArchiveInfo.SRBAllocationGranularity;

    LoadDeviceSpecificData(
        *PRS.pHeader,
        PRS.Allocator,
        "Resource signature",
        [&](Serializer<SerializerMode::Read>& Ser) //
        {
            TSerializedData SerializedData;
            SerializedData.Base = PRS.Serialized;
            TSerializerImpl<SerializerMode::Read>::SerializePRS(Ser, SerializedData, &PRS.Allocator);
            VERIFY_EXPR(Ser.IsEnd());

            pRenderDevice->CreatePipelineResourceSignature(PRS.Desc, SerializedData, &pSignature);
            CachePRSResource(DeArchiveInfo.Name, pSignature);
        });
}

template <typename RenderDeviceImplType>
bool DeviceObjectArchiveBase::CreateRenderPass(PSOData<GraphicsPipelineStateCreateInfo>& PSO, RenderDeviceImplType* pRenderDevice)
{
    if (PSO.RenderPassName == nullptr)
        return true;

    RenderPassUnpackInfo UnpackInfo;
    UnpackInfo.Name = PSO.RenderPassName;

    IRenderPass* pRP = nullptr;
    UnpackRenderPass(UnpackInfo, pRenderDevice, pRP); // Reference released in ~ReleaseTempResourceRefs()
    if (pRP == nullptr)
        return false;

    PSO.CreateInfo.GraphicsPipeline.pRenderPass = pRP;
    return true;
}

template <typename RenderDeviceImplType>
void DeviceObjectArchiveBase::UnpackGraphicsPSO(const PipelineStateUnpackInfo& DeArchiveInfo, RenderDeviceImplType* pRenderDevice, IPipelineState*& pPSO)
{
    VERIFY_EXPR(pRenderDevice != nullptr);

    if (GetCachedGraphicsPSO(DeArchiveInfo.Name, pPSO))
        return;

    PSOData<GraphicsPipelineStateCreateInfo> PSO{GetRawAllocator()};
    if (!ReadGraphicsPSOData(DeArchiveInfo.Name, PSO))
        return;

    ReleaseTempResourceRefs<GraphicsPipelineStateCreateInfo> ReleaseRefs{PSO};

    if (!CreateRenderPass(PSO, pRenderDevice))
        return;

    if (!CreateResourceSignatures(PSO, pRenderDevice))
        return;

    PSO.CreateInfo.PSODesc.SRBAllocationGranularity = DeArchiveInfo.SRBAllocationGranularity;
    PSO.CreateInfo.PSODesc.ImmediateContextMask     = DeArchiveInfo.ImmediateContextMask;

    LoadDeviceSpecificData(
        *PSO.pHeader,
        PSO.Allocator,
        "Graphics pipeline",
        [&](Serializer<SerializerMode::Read>& Ser) //
        {
            // AZ TODO

            pRenderDevice->CreateGraphicsPipelineState(PSO.CreateInfo, &pPSO);
            CacheGraphicsPSOResource(DeArchiveInfo.Name, pPSO);
        });
}

template <typename RenderDeviceImplType>
void DeviceObjectArchiveBase::UnpackComputePSO(const PipelineStateUnpackInfo& DeArchiveInfo, RenderDeviceImplType* pRenderDevice, IPipelineState*& pPSO)
{
    VERIFY_EXPR(pRenderDevice != nullptr);

    if (GetCachedComputePSO(DeArchiveInfo.Name, pPSO))
        return;

    PSOData<ComputePipelineStateCreateInfo> PSO{GetRawAllocator()};
    if (!ReadComputePSOData(DeArchiveInfo.Name, PSO))
        return;

    ReleaseTempResourceRefs<ComputePipelineStateCreateInfo> ReleaseRefs{PSO};

    if (!CreateResourceSignatures(PSO, pRenderDevice))
        return;

    PSO.CreateInfo.PSODesc.SRBAllocationGranularity = DeArchiveInfo.SRBAllocationGranularity;
    PSO.CreateInfo.PSODesc.ImmediateContextMask     = DeArchiveInfo.ImmediateContextMask;

    LoadDeviceSpecificData(
        *PSO.pHeader,
        PSO.Allocator,
        "Compute pipeline",
        [&](Serializer<SerializerMode::Read>& Ser) //
        {
            // AZ TODO

            pRenderDevice->CreateComputePipelineState(PSO.CreateInfo, &pPSO);
            CacheComputePSOResource(DeArchiveInfo.Name, pPSO);
        });
}

template <typename RenderDeviceImplType>
void DeviceObjectArchiveBase::UnpackRayTracingPSO(const PipelineStateUnpackInfo& DeArchiveInfo, RenderDeviceImplType* pRenderDevice, IPipelineState*& pPSO)
{
    VERIFY_EXPR(pRenderDevice != nullptr);

    if (GetCachedRayTracingPSO(DeArchiveInfo.Name, pPSO))
        return;

    PSOData<RayTracingPipelineStateCreateInfo> PSO{GetRawAllocator()};
    if (!ReadRayTracingPSOData(DeArchiveInfo.Name, PSO))
        return;

    ReleaseTempResourceRefs<RayTracingPipelineStateCreateInfo> ReleaseRefs{PSO};

    if (!CreateResourceSignatures(PSO, pRenderDevice))
        return;

    PSO.CreateInfo.PSODesc.SRBAllocationGranularity = DeArchiveInfo.SRBAllocationGranularity;
    PSO.CreateInfo.PSODesc.ImmediateContextMask     = DeArchiveInfo.ImmediateContextMask;

    LoadDeviceSpecificData(
        *PSO.pHeader,
        PSO.Allocator,
        "Ray tracing pipeline",
        [&](Serializer<SerializerMode::Read>& Ser) //
        {
            // AZ TODO

            pRenderDevice->CreateRayTracingPipelineState(PSO.CreateInfo, &pPSO);
            CacheRayTracingPSOResource(DeArchiveInfo.Name, pPSO);
        });
}

template <typename RenderDeviceImplType>
void DeviceObjectArchiveBase::UnpackRenderPass(const RenderPassUnpackInfo& DeArchiveInfo, RenderDeviceImplType* pRenderDevice, IRenderPass*& pRP)
{
    VERIFY_EXPR(pRenderDevice != nullptr);

    if (GetCachedRP(DeArchiveInfo.Name, pRP))
        return;

    RPData RP{GetRawAllocator()};
    if (!ReadRPData(DeArchiveInfo.Name, RP))
        return;

    pRenderDevice->CreateRenderPass(RP.Desc, &pRP);
    CacheRPResource(DeArchiveInfo.Name, pRP);
}

} // namespace Diligent
