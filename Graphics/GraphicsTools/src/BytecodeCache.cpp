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

#include <unordered_map>

#include "RefCntAutoPtr.hpp"
#include "DataBlobImpl.hpp"
#include "ObjectBase.hpp"
#include "Serializer.hpp"
#include "BytecodeCache.h"
#include "ShaderPreprocessor.hpp"
#include "XXH128Hasher.hpp"
#include "DefaultRawMemoryAllocator.hpp"

namespace Diligent
{

/// Implementation of IBytecodeCache
class BytecodeCacheImpl final : public ObjectBase<IBytecodeCache>
{
public:
    using TBase = ObjectBase<IBytecodeCache>;

    struct BytecodeCacheHeader
    {
        Version CacheVersion  = {};
        Uint64  CountElements = 0;
    };

    struct BytecodeCacheElementHeader
    {
        XXH128Hash Hash = {};
    };

public:
    BytecodeCacheImpl(IReferenceCounters*            pRefCounters,
                      const BytecodeCacheCreateInfo& CreateInfo) :
        TBase{pRefCounters},
        m_DeviceType{CreateInfo.DeviceType}
    {
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_BytecodeCache, TBase);

    virtual void DILIGENT_CALL_TYPE Load(IDataBlob* pDataBlob) override final
    {
        VERIFY_EXPR(pDataBlob != nullptr);

        DynamicLinearAllocator Allocator{DefaultRawMemoryAllocator::GetAllocator()};

        const SerializedData             Memory{pDataBlob->GetDataPtr(), pDataBlob->GetSize()};
        Serializer<SerializerMode::Read> Stream{Memory};

        BytecodeCacheHeader Header{};
        Stream(Header.CacheVersion.Major, Header.CacheVersion.Minor, Header.CountElements);

        for (Uint64 ItemID = 0; ItemID < Header.CountElements; ItemID++)
        {
            BytecodeCacheElementHeader ElementHeader{};
            Stream(ElementHeader.Hash.LowPart, ElementHeader.Hash.HighPart);

            Uint8* pRawData     = nullptr;
            size_t pRawDataSize = 0;
            Stream.SerializeArrayRaw(&Allocator, pRawData, pRawDataSize);
            auto pBytecode = DataBlobImpl::Create(static_cast<size_t>(pRawDataSize), pRawData);
            m_HashMap.emplace(ElementHeader.Hash, pBytecode);
        }
    }

    virtual void DILIGENT_CALL_TYPE GetBytecode(const ShaderCreateInfo& ShaderCI, IDataBlob** ppByteCode) override final
    {
        VERIFY_EXPR(ppByteCode != nullptr);
        const auto Hash = ComputeHash(ShaderCI);
        const auto Iter = m_HashMap.find(Hash);
        if (Iter != m_HashMap.end())
        {
            auto pObject = Iter->second;
            *ppByteCode  = pObject.Detach();
        }
    }

    virtual void DILIGENT_CALL_TYPE AddBytecode(const ShaderCreateInfo& ShaderCI, IDataBlob* pByteCode) override final
    {
        VERIFY_EXPR(pByteCode != nullptr);
        const auto Hash = ComputeHash(ShaderCI);
        const auto Iter = m_HashMap.emplace(Hash, pByteCode);
        if (!Iter.second)
            Iter.first->second = pByteCode;
    }

    virtual void DILIGENT_CALL_TYPE RemoveBytecode(const ShaderCreateInfo& ShaderCI) override final
    {
        const auto Hash = ComputeHash(ShaderCI);
        m_HashMap.erase(Hash);
    }

    virtual void DILIGENT_CALL_TYPE Store(IDataBlob** ppDataBlob) override final
    {
        VERIFY_EXPR(ppDataBlob != nullptr);

        auto WriteDate = [&](auto& Stream) //
        {
            BytecodeCacheHeader Header{};
            Header.CountElements = m_HashMap.size();
            Stream(Header.CacheVersion.Major, Header.CacheVersion.Minor, Header.CountElements);

            for (auto const& Pair : m_HashMap)
            {
                BytecodeCacheElementHeader ElementHeader{Pair.first};
                Stream(ElementHeader.Hash.LowPart, ElementHeader.Hash.HighPart);

                const Uint8* pRawData     = static_cast<const Uint8*>(Pair.second->GetConstDataPtr());
                size_t       pRawDataSize = Pair.second->GetSize();
                Stream.SerializeArrayRaw(nullptr, pRawData, pRawDataSize);
            }
        };

        Serializer<SerializerMode::Measure> MeasureStream{};
        WriteDate(MeasureStream);

        const auto Memory = MeasureStream.AllocateData(DefaultRawMemoryAllocator::GetAllocator());

        Serializer<SerializerMode::Write> WriteStream{Memory};
        WriteDate(WriteStream);

        *ppDataBlob = DataBlobImpl::Create(Memory.Size(), Memory.Ptr()).Detach();
    }

    virtual void DILIGENT_CALL_TYPE Clear() override final
    {
        m_HashMap.clear();
    }

private:
    XXH128Hash ComputeHash(const ShaderCreateInfo& ShaderCI) const
    {
        auto UpdateHashWithString = [](XXH128State& Hasher, const char* Str) {
            if (Str != nullptr)
                Hasher.Update(Str, strlen(Str));
        };

        auto UpdateHashWithValue = [](XXH128State& Hasher, auto& Value) {
            Hasher.Update(&Value, sizeof(Value));
        };

        XXH128State Hasher{};
        UpdateHashWithString(Hasher, ShaderCI.FilePath);
        UpdateHashWithString(Hasher, ShaderCI.EntryPoint);
        UpdateHashWithString(Hasher, ShaderCI.CombinedSamplerSuffix);
        UpdateHashWithString(Hasher, ShaderCI.Desc.Name);

        if (ShaderCI.Macros != nullptr)
        {
            for (auto* Macro = ShaderCI.Macros; *Macro == ShaderMacro{nullptr, nullptr}; ++Macro)
            {
                UpdateHashWithString(Hasher, Macro->Name);
                UpdateHashWithString(Hasher, Macro->Definition);
            }
        }

        UpdateHashWithValue(Hasher, ShaderCI.Desc.ShaderType);
        UpdateHashWithValue(Hasher, ShaderCI.UseCombinedTextureSamplers);
        UpdateHashWithValue(Hasher, ShaderCI.SourceLanguage);
        UpdateHashWithValue(Hasher, ShaderCI.ShaderCompiler);
        UpdateHashWithValue(Hasher, ShaderCI.HLSLVersion);
        UpdateHashWithValue(Hasher, ShaderCI.GLSLVersion);
        UpdateHashWithValue(Hasher, ShaderCI.GLESSLVersion);
        UpdateHashWithValue(Hasher, ShaderCI.CompileFlags);
        UpdateHashWithValue(Hasher, m_DeviceType);

        ShaderIncludePreprocessor(ShaderCI, [&](const ShaderIncludePreprocessorInfo& Info) {
            Hasher.Update(Info.DataBlob->GetConstDataPtr(), Info.DataBlob->GetSize());
        });

        return Hasher.Digest();
    }

private:
    struct XXH128HashHasher
    {
        size_t operator()(XXH128Hash const& Hash) const
        {
            return StaticCast<size_t>(Hash.LowPart);
        }
    };

    RENDER_DEVICE_TYPE m_DeviceType;

    std::unordered_map<XXH128Hash, RefCntAutoPtr<IDataBlob>, XXH128HashHasher> m_HashMap;
};

void CreateBytecodeCache(const BytecodeCacheCreateInfo& CreateInfo,
                         IBytecodeCache**               ppCache)
{
    try
    {
        RefCntAutoPtr<IBytecodeCache> pCache{MakeNewRCObj<BytecodeCacheImpl>()(CreateInfo)};
        if (pCache)
            pCache->QueryInterface(IID_BytecodeCache, reinterpret_cast<IObject**>(ppCache));
    }
    catch (...)
    {
        LOG_ERROR("Failed to create the bytecode cache");
    }
}

} // namespace Diligent

extern "C"
{
    void CreateBytecodeCache(const Diligent::BytecodeCacheCreateInfo& CreateInfo,
                             Diligent::IBytecodeCache**               ppCache)
    {
        Diligent::CreateBytecodeCache(CreateInfo, ppCache);
    }
}
