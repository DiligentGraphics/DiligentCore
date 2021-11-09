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
#include <mutex>

#include "Dearchiver.h"
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
                            IArchive*           pSource,
                            DeviceType          DevType);

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_DeviceObjectArchive, TObjectBase)

protected:
    static constexpr Uint32 HeaderMagicNumber = 0xDE00000A;
    static constexpr Uint32 HeaderVersion     = 1;
    static constexpr Uint32 DataPtrAlign      = sizeof(Uint64);

    friend class ArchiverImpl;

    // Archive header contains offsets for blocks.
    // Any block can be added or removed without patching all offsets in archive,
    // you need to patch only base offsets.
    enum class BlockOffsetType : Uint32
    {
        // Device specific data
        OpenGL,
        Direct3D11,
        Direct3D12,
        Vulkan,
        Metal,

        //Direct3D12_PSOCache,
        //Vulkan_PSOCache,
        //Metal_PSOCache,

        //Direct3D12_Debug,
        //Vulkan_Debug,

        Count
    };
    using TBlockBaseOffsets = std::array<Uint32, static_cast<Uint32>(BlockOffsetType::Count)>;

    struct ArchiveHeader
    {
        Uint32            MagicNumber;
        Uint32            Version;
        TBlockBaseOffsets BlockBaseOffsets;
        Uint32            NumChunks;
        //ChunkHeader     Chunks  [NumChunks]
    };
    static_assert(sizeof(ArchiveHeader) == 32, "Archive header size must be 32 bytes");

    enum class ChunkType : Uint32
    {
        ArchiveDebugInfo = 1,
        ResourceSignature,
        GraphicsPipelineStates,
        ComputePipelineStates,
        RayTracingPipelineStates,
        RenderPass,
        Shaders,
        //PipelineCache,
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
        using Uint32Array = std::array<Uint32, static_cast<Uint32>(DeviceType::Count)>;

        static constexpr Uint32 InvalidOffset = ~0u;

        ChunkType   Type;
        Uint32Array m_DeviceSpecificDataSize;
        Uint32Array m_DeviceSpecificDataOffset;

        Uint32 GetSize(DeviceType DevType) const { return m_DeviceSpecificDataSize[static_cast<Uint32>(DevType)]; }
        Uint32 GetOffset(DeviceType DevType) const { return m_DeviceSpecificDataOffset[static_cast<Uint32>(DevType)]; }
        Uint32 GetEndOffset(DeviceType DevType) const { return GetOffset(DevType) + GetSize(DevType); }

        void InitOffsets() { m_DeviceSpecificDataOffset.fill(InvalidOffset); }

        void SetSize(DeviceType DevType, Uint32 Size) { m_DeviceSpecificDataSize[static_cast<Uint32>(DevType)] = Size; }
        void SetOffset(DeviceType DevType, Uint32 Offset) { m_DeviceSpecificDataOffset[static_cast<Uint32>(DevType)] = Offset; }
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

    struct ShadersDataHeader : BaseDataHeader
    {};

    struct RPDataHeader
    {
        ChunkType Type;
    };

    struct FileOffsetAndSize
    {
        Uint32 Offset;
        Uint32 Size;
    };

private:
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
    using TPRSOffsetAndCacheMap  = TNameOffsetMap<IPipelineResourceSignature>;
    using TPSOOffsetAndCacheMap  = TNameOffsetMap<IPipelineState>;
    using TRPOffsetAndCacheMap   = TNameOffsetMap<IRenderPass>;
    using TResourceOffsetAndSize = std::vector<FileOffsetAndSize>;

    TPRSOffsetAndCacheMap  m_PRSMap;
    TPSOOffsetAndCacheMap  m_GraphicsPSOMap;
    TPSOOffsetAndCacheMap  m_ComputePSOMap;
    TPSOOffsetAndCacheMap  m_RayTracingPSOMap;
    TRPOffsetAndCacheMap   m_RenderPassMap;
    TResourceOffsetAndSize m_Shaders;

    std::mutex m_PRSMapGuard;
    std::mutex m_GraphicsPSOMapGuard;
    std::mutex m_ComputePSOMapGuard;
    std::mutex m_RayTracingPSOMapGuard;
    std::mutex m_RenderPassMapGuard;
    std::mutex m_ShadersGuard;

    struct
    {
        String GitHash;
    } m_DebugInfo;

    RefCntAutoPtr<IArchive> m_pSource; // archive source is thread-safe
    const DeviceType        m_DevType;
    TBlockBaseOffsets       m_BaseOffsets = {};

    template <typename ResType>
    void ReadNamedResources(const ChunkHeader& Chunk, TNameOffsetMap<ResType>& NameAndOffset, std::mutex& Guard) noexcept(false);
    void ReadIndexedResources(const ChunkHeader& Chunk, TResourceOffsetAndSize& Resources, std::mutex& Guard) noexcept(false);
    void ReadArchiveDebugInfo(const ChunkHeader& Chunk) noexcept(false);

    template <typename ResType>
    bool GetCachedResource(const String& Name, TNameOffsetMap<ResType>& Cache, std::mutex& Guard, ResType*& pResource);
    template <typename ResType>
    void CacheResource(const String& Name, TNameOffsetMap<ResType>& Cache, std::mutex& Guard, ResType* pResource);

protected:
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

private:
    bool ReadPRSData(const String& Name, PRSData& PRS);
    bool GetCachedPRS(const String& Name, IPipelineResourceSignature*& pSignature);
    void CachePRSResource(const String& Name, IPipelineResourceSignature* pSignature);

    bool ReadGraphicsPSOData(const String& Name, PSOData<GraphicsPipelineStateCreateInfo>& PSO);
    bool ReadComputePSOData(const String& Name, PSOData<ComputePipelineStateCreateInfo>& PSO);
    bool ReadRayTracingPSOData(const String& Name, PSOData<RayTracingPipelineStateCreateInfo>& PSO);

    bool GetCachedGraphicsPSO(const String& Name, IPipelineState*& pPSO);
    void CacheGraphicsPSOResource(const String& Name, IPipelineState* pPSO);
    bool GetCachedComputePSO(const String& Name, IPipelineState*& pPSO);
    void CacheComputePSOResource(const String& Name, IPipelineState* pPSO);
    bool GetCachedRayTracingPSO(const String& Name, IPipelineState*& pPSO);
    void CacheRayTracingPSOResource(const String& Name, IPipelineState* pPSO);

    bool LoadShaders(Serializer<SerializerMode::Read>&    Ser,
                     IRenderDevice*                       pDevice,
                     std::vector<RefCntAutoPtr<IShader>>& Shaders);

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
                          std::mutex&                    Guard,
                          const String&                  ResourceName,
                          DynamicLinearAllocator&        Allocator,
                          const char*                    ResTypeName,
                          const FnType&                  Fn);

    template <typename HeaderType, typename FnType>
    void LoadDeviceSpecificData(const HeaderType&       Header,
                                DynamicLinearAllocator& Allocator,
                                const char*             ResTypeName,
                                BlockOffsetType         BlockType,
                                const FnType&           Fn);

    static constexpr Uint32 DefaultSRBAllocationGranularity = 1;

    template <typename CreateInfoType>
    bool CreateResourceSignatures(PSOData<CreateInfoType>& PSO, IRenderDevice* pDevice);

    template <typename CreateInfoType>
    struct ReleaseTempResourceRefs
    {
        PSOData<CreateInfoType>& PSO;

        explicit ReleaseTempResourceRefs(PSOData<CreateInfoType>& _PSO) :
            PSO{_PSO} {}

        ~ReleaseTempResourceRefs();
    };

    bool CreateRenderPass(PSOData<GraphicsPipelineStateCreateInfo>& PSO, IRenderDevice* pDevice);

protected:
    using CreateSignatureType = std::function<void(PRSData& PRS, Serializer<SerializerMode::Read>& Ser, IPipelineResourceSignature*& pSignature)>;
    void UnpackResourceSignatureImpl(const ResourceSignatureUnpackInfo& DeArchiveInfo,
                                     IPipelineResourceSignature*&       pSignature,
                                     const CreateSignatureType&         CreateSignature);

    virtual void UnpackResourceSignature(const ResourceSignatureUnpackInfo& DeArchiveInfo, IPipelineResourceSignature*& pSignature) = 0;

public:
    void UnpackGraphicsPSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO);
    void UnpackComputePSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO);
    void UnpackRayTracingPSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO);
    void UnpackRenderPass(const RenderPassUnpackInfo& DeArchiveInfo, IRenderPass*& pRP);

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
            auto* pArray = Allocator->ConstructArray<T>(Count);
            DstArray     = pArray;
            return pArray;
        }
    };

    struct ShaderIndexArray
    {
        const Uint32* pIndices = nullptr;
        Uint32        Count    = 0;
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

        static void SerializeTilePSO(Serializer<Mode>&                   Ser,
                                     TQual<TilePipelineStateCreateInfo>& CreateInfo,
                                     TQual<TPRSNames>&                   PRSNames,
                                     DynamicLinearAllocator*             Allocator);

        static void SerializeRayTracingPSO(Serializer<Mode>&                         Ser,
                                           TQual<RayTracingPipelineStateCreateInfo>& CreateInfo,
                                           TQual<TPRSNames>&                         PRSNames,
                                           DynamicLinearAllocator*                   Allocator);

        static void SerializeRenderPass(Serializer<Mode>&       Ser,
                                        TQual<RenderPassDesc>&  RPDesc,
                                        DynamicLinearAllocator* Allocator);

        static void SerializeShaders(Serializer<Mode>&        Ser,
                                     TQual<ShaderIndexArray>& Shaders,
                                     DynamicLinearAllocator*  Allocator);
    };
};

DECL_TRIVIALLY_SERIALIZABLE(BlendStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(RasterizerStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(DepthStencilStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(SampleDesc);

} // namespace Diligent
