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
        Metal_iOS,
        Metal_MacOS,
        Count
    };
    static DeviceType RenderDeviceTypeToArchiveDeviceType(RENDER_DEVICE_TYPE RenderDeviceType);

    using TPRSNames = std::array<const char*, MAX_RESOURCE_SIGNATURES>;

    struct ShaderIndexArray
    {
        const Uint32* pIndices = nullptr;
        Uint32        Count    = 0;
    };


    /// \param pRefCounters - Reference counters object that controls the lifetime of this device object archive.
    /// \param pArchive     - Source data that this archive will be created from.
    /// \param DevType      - Device type.
    DeviceObjectArchiveBase(IReferenceCounters* pRefCounters,
                            IArchive*           pArchive,
                            DeviceType          DevType);

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_DeviceObjectArchive, TObjectBase)

protected:
    static constexpr Uint32 HeaderMagicNumber = 0xDE00000A;
    static constexpr Uint32 HeaderVersion     = 2;
    static constexpr Uint32 DataPtrAlign      = sizeof(Uint64);

    friend class ArchiverImpl;
    friend class ArchiveRepacker;

    // Archive header contains offsets for blocks.
    // Any block can be added or removed without patching all offsets in the archive,
    // you need to patch only the base offsets.
    enum class BlockOffsetType : Uint32
    {
        // Device-specific data
        OpenGL,
        Direct3D11,
        Direct3D12,
        Vulkan,
        Metal_iOS,
        Metal_MacOS,

        //Direct3D12_PSOCache,
        //Vulkan_PSOCache,
        //Metal_PSOCache,

        //Direct3D12_Debug,
        //Vulkan_Debug,

        Count
    };
    using TBlockBaseOffsets = std::array<Uint32, static_cast<size_t>(BlockOffsetType::Count)>;

#define CHECK_HEADER_SIZE(Header, Size)                                                                                                           \
    static_assert(sizeof(Header) % 8 == 0, "sizeof(" #Header ") must be a multiple of 8. Use padding to align it.");                              \
    static_assert(sizeof(Header) == Size, "sizeof(" #Header ") must be " #Size ". Reading binary archive will result in invalid memory access."); \
    static_assert(sizeof(Header) % alignof(Header) == 0, "sizeof(" #Header ") is not a multiple of its alignment.");

    struct ArchiveHeader
    {
        Uint32            MagicNumber      = 0;
        Uint32            Version          = 0;
        TBlockBaseOffsets BlockBaseOffsets = {};
        Uint32            NumChunks        = 0;
        //ChunkHeader     Chunks  [NumChunks]
        Uint32 _Padding = ~0u;
    };
    CHECK_HEADER_SIZE(ArchiveHeader, 40)

    enum class ChunkType : Uint32
    {
        Undefined = 0,
        ArchiveDebugInfo,
        ResourceSignature,
        GraphicsPipelineStates,
        ComputePipelineStates,
        RayTracingPipelineStates,
        TilePipelineStates,
        RenderPass,
        Shaders,
        //PipelineCache,
        Count
    };

    struct ChunkHeader
    {
        ChunkType Type     = ChunkType::Undefined;
        Uint32    Size     = 0;
        Uint32    Offset   = 0; // offset to NamedResourceArrayHeader
        Uint32    _Padding = ~0u;

        bool operator==(const ChunkHeader& Rhs) const { return Type == Rhs.Type && Size == Rhs.Size && Offset == Rhs.Offset; }
    };
    CHECK_HEADER_SIZE(ChunkHeader, 16)

    struct NamedResourceArrayHeader
    {
        Uint32 Count = 0;
        //Uint32 NameLength    [Count]
        //Uint32 ***DataSize   [Count]
        //Uint32 ***DataOffset [Count] // for PRSDataHeader / PSODataHeader
        //char   NameData      []
        Uint32 _Padding = ~0u;
    };
    CHECK_HEADER_SIZE(NamedResourceArrayHeader, 8)

    struct BaseDataHeader
    {
        using Uint32Array = std::array<Uint32, static_cast<size_t>(DeviceType::Count)>;

        static constexpr Uint32 InvalidOffset = ~0u;

        BaseDataHeader(ChunkType _Type) noexcept :
            Type{_Type}
        {
            DeviceSpecificDataOffset.fill(Uint32{InvalidOffset});
        }

        const ChunkType Type      = ChunkType::Undefined;
        const Uint32    _Padding0 = ~0u;

        Uint32Array DeviceSpecificDataSize{};
        Uint32Array DeviceSpecificDataOffset{};

        Uint32 GetSize(DeviceType DevType) const { return DeviceSpecificDataSize[static_cast<size_t>(DevType)]; }
        Uint32 GetOffset(DeviceType DevType) const { return DeviceSpecificDataOffset[static_cast<size_t>(DevType)]; }
        Uint32 GetEndOffset(DeviceType DevType) const { return GetOffset(DevType) + GetSize(DevType); }

        void SetSize(DeviceType DevType, Uint32 Size) { DeviceSpecificDataSize[static_cast<size_t>(DevType)] = Size; }
        void SetOffset(DeviceType DevType, Uint32 Offset) { DeviceSpecificDataOffset[static_cast<size_t>(DevType)] = Offset; }
    };
    CHECK_HEADER_SIZE(BaseDataHeader, 56)

    struct PRSDataHeader : BaseDataHeader
    {
        PRSDataHeader(ChunkType _Type) noexcept :
            BaseDataHeader{_Type}
        {
            VERIFY_EXPR(Type == ChunkType::ResourceSignature);
        }
        //PipelineResourceSignatureDesc
        //PipelineResourceSignatureSerializedData
    };
    CHECK_HEADER_SIZE(PRSDataHeader, 56)


    struct PSODataHeader : BaseDataHeader
    {
        PSODataHeader(ChunkType _Type) noexcept :
            BaseDataHeader{_Type}
        {
            VERIFY_EXPR((Type == ChunkType::GraphicsPipelineStates ||
                         Type == ChunkType::ComputePipelineStates ||
                         Type == ChunkType::RayTracingPipelineStates ||
                         Type == ChunkType::TilePipelineStates));
        }

        //GraphicsPipelineStateCreateInfo | ComputePipelineStateCreateInfo | TilePipelineStateCreateInfo | RayTracingPipelineStateCreateInfo
    };
    CHECK_HEADER_SIZE(PSODataHeader, 56)


    struct ShadersDataHeader : BaseDataHeader
    {
        ShadersDataHeader(ChunkType _Type = ChunkType::Shaders) noexcept :
            BaseDataHeader{_Type}
        {
            VERIFY_EXPR(Type == ChunkType::Shaders);
        }
    };
    CHECK_HEADER_SIZE(ShadersDataHeader, 56)


    struct RPDataHeader
    {
        RPDataHeader(ChunkType _Type) noexcept :
            Type{_Type}
        {
            VERIFY_EXPR(Type == ChunkType::RenderPass);
        }

        const ChunkType Type      = ChunkType::RenderPass;
        const Uint32    _Padding1 = ~0u;
    };
    CHECK_HEADER_SIZE(RPDataHeader, 8)

#undef CHECK_HEADER_SIZE

private:
    struct FileOffsetAndSize
    {
        Uint32 Offset = 0;
        Uint32 Size   = 0;

        bool operator==(const FileOffsetAndSize& Rhs) const { return Offset == Rhs.Offset && Size == Rhs.Size; }
    };

    template <typename ResPtrType>
    struct FileOffsetAndResCache : FileOffsetAndSize
    {
        ResPtrType Cache;

        FileOffsetAndResCache() {}
        FileOffsetAndResCache(const FileOffsetAndSize& OffsetAndSize) :
            FileOffsetAndSize{OffsetAndSize}
        {}
    };

    using NameOffsetMap = std::unordered_map<HashMapStringKey, FileOffsetAndSize, HashMapStringKey::Hasher>;
    template <typename ResPtrType>
    using TNameOffsetMap = std::unordered_map<HashMapStringKey, FileOffsetAndResCache<ResPtrType>, HashMapStringKey::Hasher>;
    template <typename T>
    using TNameOffsetMapAndWeakCache = TNameOffsetMap<RefCntWeakPtr<T>>;

    using TPRSOffsetAndCacheMap = TNameOffsetMapAndWeakCache<IPipelineResourceSignature>;
    using TPSOOffsetAndCacheMap = TNameOffsetMapAndWeakCache<IPipelineState>;
    using TRPOffsetAndCacheMap  = TNameOffsetMapAndWeakCache<IRenderPass>;
    using TShaderOffsetAndCache = std::vector<FileOffsetAndResCache<RefCntAutoPtr<IShader>>>; // reference to the shader is not acquired in PSO, so weak ptr has no effect

    TPRSOffsetAndCacheMap m_PRSMap;
    TPSOOffsetAndCacheMap m_GraphicsPSOMap;
    TPSOOffsetAndCacheMap m_ComputePSOMap;
    TPSOOffsetAndCacheMap m_TilePSOMap;
    TPSOOffsetAndCacheMap m_RayTracingPSOMap;
    TRPOffsetAndCacheMap  m_RenderPassMap;
    TShaderOffsetAndCache m_Shaders;

    std::mutex m_PRSMapGuard;
    std::mutex m_GraphicsPSOMapGuard;
    std::mutex m_ComputePSOMapGuard;
    std::mutex m_TilePSOMapGuard;
    std::mutex m_RayTracingPSOMapGuard;
    std::mutex m_RenderPassMapGuard;
    std::mutex m_ShadersGuard;

    struct
    {
        String GitHash;
        Uint32 APIVersion = 0;
    } m_DebugInfo;

    RefCntAutoPtr<IArchive> m_pArchive; // archive is thread-safe
    const DeviceType        m_DevType;
    TBlockBaseOffsets       m_BaseOffsets = {};

    template <typename MapType>
    static void ReadNamedResources(IArchive* pArchive, const ChunkHeader& Chunk, MapType& NameAndOffset, std::mutex& Guard) noexcept(false);
    static void ReadNamedResources2(IArchive* pArchive, const ChunkHeader& Chunk, NameOffsetMap& NameAndOffset) noexcept(false);
    template <typename ResType>
    void ReadNamedResources(const ChunkHeader& Chunk, TNameOffsetMap<ResType>& NameAndOffset, std::mutex& Guard) noexcept(false);
    void ReadIndexedResources(const ChunkHeader& Chunk, TShaderOffsetAndCache& Resources, std::mutex& Guard) noexcept(false);
    void ReadArchiveDebugInfo(const ChunkHeader& Chunk) noexcept(false);

    template <typename ResType>
    bool GetCachedResource(const char* Name, TNameOffsetMapAndWeakCache<ResType>& Cache, std::mutex& Guard, ResType** ppResource);
    template <typename ResType>
    void CacheResource(const char* Name, TNameOffsetMapAndWeakCache<ResType>& Cache, std::mutex& Guard, ResType* pResource);

    BlockOffsetType GetBlockOffsetType() const;

protected:
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
        TPRSNames              PRSNames{};
        const char*            RenderPassName = nullptr;

        explicit PSOData(IMemoryAllocator& Allocator, Uint32 BlockSize = 4 << 10) :
            Allocator{Allocator, BlockSize}
        {}
    };

private:
    bool ReadPRSData(const char* Name, PRSData& PRS);

    template <typename PSOHashMapType, typename PSOCreateInfoType, typename... ExtraArgsType>
    bool ReadPSOData(ChunkType                   Type,
                     const char*                 Name,
                     PSOHashMapType&             PSOMap,
                     std::mutex&                 PSOMapGuard,
                     const char*                 ResTypeName,
                     PSOData<PSOCreateInfoType>& PSO,
                     ExtraArgsType&&... ExtraArgs);

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
    bool ReadRPData(const char* Name, RPData& RP);

    template <typename ResType, typename FnType>
    bool LoadResourceData(const TNameOffsetMap<ResType>& NameAndOffset,
                          std::mutex&                    Guard,
                          const char*                    ResourceName,
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
    virtual void ReadAndCreateShader(Serializer<SerializerMode::Read>& Ser, ShaderCreateInfo& ShaderCI, IRenderDevice* pDevice, IShader** ppShader);

    static constexpr Uint32 GetHeaderVersion() { return HeaderVersion; }

public:
    void UnpackGraphicsPSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO);
    void UnpackComputePSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO);
    void UnpackRayTracingPSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO);
    void UnpackTilePSO(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState*& pPSO);
    void UnpackRenderPass(const RenderPassUnpackInfo& DeArchiveInfo, IRenderPass*& pRP);

    virtual void DILIGENT_CALL_TYPE ClearResourceCache() override final;
};


template <SerializerMode Mode>
struct PSOSerializer_ArrayHelper
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
struct PSOSerializer_ArrayHelper<SerializerMode::Read>
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


template <SerializerMode Mode>
struct PSOSerializer
{
    template <typename T>
    using TQual = typename Serializer<Mode>::template TQual<T>;

    using TPRSNames        = DeviceObjectArchiveBase::TPRSNames;
    using ShaderIndexArray = DeviceObjectArchiveBase::ShaderIndexArray;

    static void SerializeImmutableSampler(Serializer<Mode>&            Ser,
                                          TQual<ImmutableSamplerDesc>& SampDesc);

    static void SerializePRSDesc(Serializer<Mode>&                               Ser,
                                 TQual<PipelineResourceSignatureDesc>&           Desc,
                                 TQual<PipelineResourceSignatureSerializedData>& Serialized,
                                 DynamicLinearAllocator*                         Allocator);

    static void SerializePSOCreateInfo(Serializer<Mode>&               Ser,
                                       TQual<PipelineStateCreateInfo>& CreateInfo,
                                       TQual<TPRSNames>&               PRSNames,
                                       DynamicLinearAllocator*         Allocator);

    static void SerializePSOCreateInfo(Serializer<Mode>&                       Ser,
                                       TQual<GraphicsPipelineStateCreateInfo>& CreateInfo,
                                       TQual<TPRSNames>&                       PRSNames,
                                       DynamicLinearAllocator*                 Allocator,
                                       TQual<const char*>&                     RenderPassName);

    static void SerializePSOCreateInfo(Serializer<Mode>&                      Ser,
                                       TQual<ComputePipelineStateCreateInfo>& CreateInfo,
                                       TQual<TPRSNames>&                      PRSNames,
                                       DynamicLinearAllocator*                Allocator);

    static void SerializePSOCreateInfo(Serializer<Mode>&                   Ser,
                                       TQual<TilePipelineStateCreateInfo>& CreateInfo,
                                       TQual<TPRSNames>&                   PRSNames,
                                       DynamicLinearAllocator*             Allocator);

    static void SerializePSOCreateInfo(Serializer<Mode>&                                     Ser,
                                       TQual<RayTracingPipelineStateCreateInfo>&             CreateInfo,
                                       TQual<TPRSNames>&                                     PRSNames,
                                       DynamicLinearAllocator*                               Allocator,
                                       const std::function<void(Uint32&, TQual<IShader*>&)>& ShaderToIndex);

    static void SerializeRenderPassDesc(Serializer<Mode>&       Ser,
                                        TQual<RenderPassDesc>&  RPDesc,
                                        DynamicLinearAllocator* Allocator);

    static void SerializeShaders(Serializer<Mode>&        Ser,
                                 TQual<ShaderIndexArray>& Shaders,
                                 DynamicLinearAllocator*  Allocator);
};


DECL_TRIVIALLY_SERIALIZABLE(BlendStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(RasterizerStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(DepthStencilStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(SampleDesc);

} // namespace Diligent
