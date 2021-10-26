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

    /// \param pRefCounters - Reference counters object that controls the lifetime of this device object archive.
    /// \param pSource      - AZ TODO
    DeviceObjectArchiveBase(IReferenceCounters* pRefCounters,
                            IArchiveSource*     pSource);

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_DeviceObjectArchive, TObjectBase)

    enum class DeviceType : Uint32
    {
        OpenGL, // same as GLES
        Direct3D11,
        Direct3D12,
        Vulkan,
        Metal,
        Count
    };

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

private:
    struct FileOffsetAndSize
    {
        Uint32 Offset;
        Uint32 Size;
    };

    //using TNameOffsetMap = std::unordered_map<HashMapStringKey, FileOffsetAndSize, HashMapStringKey::Hasher>;
    using TNameOffsetMap = std::unordered_map<String, FileOffsetAndSize>;
    TNameOffsetMap m_PRSMap;
    TNameOffsetMap m_GraphicsPSOMap;
    TNameOffsetMap m_ComputePSOMap;
    TNameOffsetMap m_RayTracingPSOMap;

    std::shared_mutex m_PRSMapGuard;
    std::shared_mutex m_GraphicsPSOMapGuard;
    std::shared_mutex m_ComputePSOMapGuard;
    std::shared_mutex m_RayTracingPSOMapGuard;

    struct
    {
        String GitHash;
    } m_DebugInfo;

    RefCntAutoPtr<IArchiveSource> m_pSource; // archive source is thread-safe

    void ReadArchiveDebugInfo(const ChunkHeader& Chunk) noexcept(false);
    void ReadNamedResources(const ChunkHeader& Chunk, TNameOffsetMap& NameAndOffset, std::shared_mutex& Quard) noexcept(false);

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
    bool ReadPRSData(const String& Name, PRSData& PRS);


    template <typename CreateInfoType>
    struct PSOData
    {
        DynamicLinearAllocator Allocator;
        const PSODataHeader*   pHeader = nullptr;
        CreateInfoType         CreateInfo{};

        explicit PSOData(IMemoryAllocator& Allocator, Uint32 BlockSize = 4 << 10) :
            Allocator{Allocator, BlockSize}
        {}
    };
    bool ReadGraphicsPSOData(const String& Name, PSOData<GraphicsPipelineStateCreateInfo>& PSO);
    bool ReadComputePSOData(const String& Name, PSOData<ComputePipelineStateCreateInfo>& PSO);
    bool ReadRayTracingPSOData(const String& Name, PSOData<RayTracingPipelineStateCreateInfo>& PSO);

    template <typename FnType>
    bool LoadResourceData(const TNameOffsetMap&   NameAndOffset,
                          std::shared_mutex&      Quard,
                          const String&           ResourceName,
                          DynamicLinearAllocator& Allocator,
                          const char*             ResTypeName,
                          const FnType&           Fn);

    template <typename HeaderType, typename FnType>
    void LoadDeviceSpecificData(DeviceType              DevType,
                                const HeaderType&       Header,
                                DynamicLinearAllocator& Allocator,
                                const char*             ResTypeName,
                                const FnType&           Fn)
    {
        if (Header.GetDeviceSpecificDataSize(DevType) == 0)
        {
            LOG_ERROR_MESSAGE("Device specific data is not specified for ", ResTypeName);
            return;
        }
        if (Header.GetDeviceSpecificDataEndOffset(DevType) > m_pSource->GetSize())
        {
            LOG_ERROR_MESSAGE("Invalid offset in archive");
            return;
        }

        const auto DataSize = Header.GetDeviceSpecificDataSize(DevType);
        auto*      pData    = Allocator.Allocate(DataSize, DataPtrAlign);
        if (!m_pSource->Read(Header.GetDeviceSpecificDataOffset(DevType), pData, DataSize))
        {
            LOG_ERROR_MESSAGE("Failed to read resource signature data");
            return;
        }

        Serializer<SerializerMode::Read> Ser{pData, DataSize};
        return Fn(Ser);
    }

public:
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

        static void SerializePRS(Serializer<Mode>&                               Ser,
                                 TQual<PipelineResourceSignatureDesc>&           Desc,
                                 TQual<PipelineResourceSignatureSerializedData>& Serialized,
                                 DynamicLinearAllocator*                         Allocator);

        static void SerializeGraphicsPSO(Serializer<Mode>&                       Ser,
                                         TQual<GraphicsPipelineStateCreateInfo>& CreateInfo,
                                         DynamicLinearAllocator*                 Allocator);

        static void SerializeComputePSO(Serializer<Mode>&                      Ser,
                                        TQual<ComputePipelineStateCreateInfo>& CreateInfo,
                                        DynamicLinearAllocator*                Allocator);

        static void SerializeRayTracingPSO(Serializer<Mode>&                         Ser,
                                           TQual<RayTracingPipelineStateCreateInfo>& CreateInfo,
                                           DynamicLinearAllocator*                   Allocator);
    };
};

DECL_TRIVIALLY_SERIALIZABLE(BlendStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(RasterizerStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(DepthStencilStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(SampleDesc);

} // namespace Diligent
