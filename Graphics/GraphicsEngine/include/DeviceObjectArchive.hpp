/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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
/// Implementation of the Diligent::DeviceObjectArchive class

#include <array>
#include <vector>
#include <unordered_map>

#include "GraphicsTypes.h"
#include "Archive.h"
#include "FileStream.h"

#include "HashUtils.hpp"
#include "RefCntAutoPtr.hpp"
#include "DynamicLinearAllocator.hpp"
#include "Serializer.hpp"

namespace Diligent
{

/// Device object archive object implementation.
class DeviceObjectArchive
{
public:
    enum class DeviceType : Uint32
    {
        OpenGL, // Same as GLES
        Direct3D11,
        Direct3D12,
        Vulkan,
        Metal_MacOS,
        Metal_iOS,
        Count
    };

    using TPRSNames = std::array<const char*, MAX_RESOURCE_SIGNATURES>;

    struct ShaderIndexArray
    {
        const Uint32* pIndices = nullptr;
        Uint32        Count    = 0;
    };

    // Serialized pipeline state auxiliary data
    struct SerializedPSOAuxData
    {
        // Shaders have been serialized without the shader reflection information.
        bool NoShaderReflection = false;
    };

    enum class ResourceType : Uint32
    {
        Undefined = 0,
        ResourceSignature,
        GraphicsPipeline,
        ComputePipeline,
        RayTracingPipeline,
        TilePipeline,
        RenderPass,
        Count
    };

    static constexpr Uint32 HeaderMagicNumber = 0xDE00000A;
    static constexpr Uint32 ArchiveVersion    = 3;

    struct ArchiveHeader
    {
        Uint32      MagicNumber = HeaderMagicNumber;
        Uint32      Version     = ArchiveVersion;
        Uint32      APIVersion  = DILIGENT_API_VERSION;
        const char* GitHash     = nullptr;
    };

    struct ResourceData
    {
        // Device-agnostic data (e.g. description)
        SerializedData Common;

        // Device-specific data (e.g. patched shader byte code, device-specific resource signature data, etc.)
        std::array<SerializedData, static_cast<size_t>(DeviceType::Count)> DeviceSpecific;
    };

    struct NamedResourceKey
    {
        NamedResourceKey(ResourceType _Type,
                         const char*  _Name,
                         bool         CopyName = false) noexcept :
            Type{_Type},
            Name{_Name, CopyName}
        {}

        struct Hasher
        {
            size_t operator()(const NamedResourceKey& Key) const
            {
                return ComputeHash(static_cast<size_t>(Key.Type), Key.Name.GetHash());
            }
        };

        bool operator==(const NamedResourceKey& Key) const
        {
            return Type == Key.Type && Name == Key.Name;
        }

        const char* GetName() const
        {
            return Name.GetStr();
        }

        ResourceType GetType() const
        {
            return Type;
        }

    private:
        const ResourceType Type;
        HashMapStringKey   Name;
    };

public:
    /// \param pArchive - Source data that this archive will be created from.
    DeviceObjectArchive(IArchive* pArchive) noexcept(false);
    DeviceObjectArchive() noexcept;

    void RemoveDeviceData(DeviceType Dev) noexcept(false);
    void AppendDeviceData(const DeviceObjectArchive& Src, DeviceType Dev) noexcept(false);

    void Deserialize(const void* pData, size_t Size) noexcept(false);
    void Serialize(IFileStream* pStream) const noexcept(false);
    void Serialize(IDataBlob** ppDataBlob) const;

    std::string ToString() const;

    static DeviceType RenderDeviceTypeToArchiveDeviceType(RENDER_DEVICE_TYPE Type);

    static const char* ResourceTypeToString(ResourceType Type);

    template <typename ReourceDataType>
    bool LoadResourceCommonData(ResourceType     Type,
                                const char*      Name,
                                ReourceDataType& ResData) const
    {
        auto it = m_NamedResources.find(NamedResourceKey{Type, Name});
        if (it == m_NamedResources.end())
        {
            LOG_ERROR_MESSAGE("Resource '", Name, "' is not present in the archive");
            return false;
        }
        VERIFY_EXPR(SafeStrEqual(Name, it->first.GetName()));
        // Use string copy from the map
        Name = it->first.GetName();

        Serializer<SerializerMode::Read> Ser{it->second.Common};

        auto Res = ResData.Deserialize(Name, Ser);
        VERIFY_EXPR(Ser.IsEnded());
        return Res;
    }

    const SerializedData& GetDeviceSpecificData(ResourceType Type,
                                                const char*  Name,
                                                DeviceType   DevType) const noexcept;

    ResourceData& GetResourceData(ResourceType Type, const char* Name) noexcept
    {
        return m_NamedResources[NamedResourceKey{Type, Name, true}];
    }

    auto& GetDeviceShaders(DeviceType Type) noexcept
    {
        return m_DeviceShaders[static_cast<size_t>(Type)];
    }

    const auto& GetSerializedShader(DeviceType Type, size_t Idx) const noexcept
    {
        auto& DeviceShaders = m_DeviceShaders[static_cast<size_t>(Type)];
        if (Idx < DeviceShaders.size())
            return DeviceShaders[Idx];

        static const SerializedData NullData;
        return NullData;
    }

    const auto& GetNamedResources() const
    {
        return m_NamedResources;
    }

private:
    std::unordered_map<NamedResourceKey, ResourceData, NamedResourceKey::Hasher> m_NamedResources;

    std::array<std::vector<SerializedData>, static_cast<size_t>(DeviceType::Count)> m_DeviceShaders;

    RefCntAutoPtr<IDataBlob> m_pRawData;
};

} // namespace Diligent
