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
    explicit DeviceObjectArchiveBase(IReferenceCounters* pRefCounters) :
        TObjectBase{pRefCounters}
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_DeviceObjectArchive, TObjectBase)

    /// Implementation of IDeviceObjectArchive::Deserialize().
    virtual Bool DILIGENT_CALL_TYPE Deserialize(IArchiveSource* pSource) override final;

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

    struct PRSDataHeader
    {
        ChunkType Type;
        Uint32    DeviceSpecificDataSize[RENDER_DEVICE_TYPE_COUNT];
        Uint32    DeviceSpecificDataOffset[RENDER_DEVICE_TYPE_COUNT];
        //PipelineResourceSignatureDesc
        //PipelineResourceSignatureSerializedData
    };

    struct PSODataHeader
    {
        ChunkType Type;
        Uint32    DeviceSpecificDataSize[RENDER_DEVICE_TYPE_COUNT];
        Uint32    DeviceSpecificDataOffset[RENDER_DEVICE_TYPE_COUNT];
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

    struct
    {
        String GitHash;
    } m_DebugInfo;

protected:
    RefCntAutoPtr<IArchiveSource> m_pSource;
    std::shared_mutex             m_Guard;


protected:
    void Clear();
    void ReadArchiveDebugInfo(const ChunkHeader& Chunk);
    void ReadNamedResources(const ChunkHeader& Chunk, TNameOffsetMap& NameAndOffset);

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
