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

#include "RenderStateCache.h"

#include <array>
#include <unordered_map>
#include <mutex>
#include <vector>

#include "ObjectBase.hpp"
#include "RefCntAutoPtr.hpp"
#include "SerializationDevice.h"
#include "SerializedShader.h"
#include "Archiver.h"
#include "Dearchiver.h"
#include "ArchiverFactory.h"
#include "ArchiverFactoryLoader.h"
#include "XXH128Hasher.hpp"
#include "CallbackWrapper.hpp"

namespace Diligent
{

/// Implementation of IRenderStateCache
class RenderStateCacheImpl final : public ObjectBase<IRenderStateCache>
{
public:
    using TBase = ObjectBase<IRenderStateCache>;

public:
    RenderStateCacheImpl(IReferenceCounters*               pRefCounters,
                         const RenderStateCacheCreateInfo& CreateInfo) :
        TBase{pRefCounters},
        m_pDevice{CreateInfo.pDevice},
        m_DeviceType{CreateInfo.pDevice != nullptr ? CreateInfo.pDevice->GetDeviceInfo().Type : RENDER_DEVICE_TYPE_UNDEFINED}
    {
        if (CreateInfo.pDevice == nullptr)
            LOG_ERROR_AND_THROW("CreateInfo.pDevice must not be null");

        IArchiverFactory* pArchiverFactory = nullptr;
#if EXPLICITLY_LOAD_ARCHIVER_FACTORY_DLL
        auto GetArchiverFactory = LoadArchiverFactory();
        if (GetArchiverFactory != nullptr)
        {
            pArchiverFactory = GetArchiverFactory();
        }
#else
        pArchiverFactory = GetArchiverFactory();
#endif
        VERIFY_EXPR(pArchiverFactory != nullptr);

        SerializationDeviceCreateInfo SerializationDeviceCI;
        SerializationDeviceCI.DeviceInfo  = m_pDevice->GetDeviceInfo();
        SerializationDeviceCI.AdapterInfo = m_pDevice->GetAdapterInfo();

        pArchiverFactory->CreateSerializationDevice(SerializationDeviceCI, &m_pSerializationDevice);
        if (!m_pSerializationDevice)
            LOG_ERROR_AND_THROW("Failed to create serialization device");

        m_pSerializationDevice->AddRenderDevice(m_pDevice);

        pArchiverFactory->CreateArchiver(m_pSerializationDevice, &m_pArchiver);
        if (!m_pArchiver)
            LOG_ERROR_AND_THROW("Failed to create archiver");

        DearchiverCreateInfo DearchiverCI;
        m_pDevice->GetEngineFactory()->CreateDearchiver(DearchiverCI, &m_pDearchiver);
        if (!m_pDearchiver)
            LOG_ERROR_AND_THROW("Failed to create dearchiver");
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_RenderStateCache, TBase);

    virtual bool DILIGENT_CALL_TYPE Load(const IDataBlob* pArchive,
                                         bool             MakeCopy) override final
    {
        return m_pDearchiver->LoadArchive(pArchive, MakeCopy);
    }

    virtual bool DILIGENT_CALL_TYPE CreateShader(const ShaderCreateInfo& ShaderCI,
                                                 IShader**               ppShader) override final
    {
        *ppShader = nullptr;

        XXH128State Hasher;
        Hasher.Update(ShaderCI);
        const auto Hash = Hasher.Digest();

        // First, try to check if the shader has already been requested
        {
            std::lock_guard<std::mutex> Guard{m_ShadersMtx};
            auto                        it = m_Shaders.find(Hash);
            if (it != m_Shaders.end())
            {
                if (auto pShader = it->second.Lock())
                {
                    *ppShader = pShader.Detach();
                    return true;
                }
                else
                {
                    m_Shaders.erase(it);
                }
            }
        }

        class AddShaderHelper
        {
        public:
            AddShaderHelper(RenderStateCacheImpl& Cache, const XXH128Hash& Hash, IShader** ppShader) :
                m_Cache{Cache},
                m_Hash{Hash},
                m_ppShader{ppShader}
            {
            }

            ~AddShaderHelper()
            {
                if (*m_ppShader != nullptr)
                {
                    std::lock_guard<std::mutex> Guard{m_Cache.m_ShadersMtx};
                    m_Cache.m_Shaders.emplace(m_Hash, *m_ppShader);
                }
            }

        private:
            RenderStateCacheImpl& m_Cache;
            const XXH128Hash&     m_Hash;
            IShader** const       m_ppShader;
        };
        AddShaderHelper AutoAddShader{*this, Hash, ppShader};

        const auto HashStr = MakeHashStr(ShaderCI.Desc.Name, Hash);

        // Try to find shader in the loaded archive
        {
            auto Callback = MakeCallback(
                [&ShaderCI](ShaderDesc& Desc) {
                    Desc.Name = ShaderCI.Desc.Name;
                });

            ShaderUnpackInfo UnpackInfo;
            UnpackInfo.Name             = HashStr.c_str();
            UnpackInfo.pDevice          = m_pDevice;
            UnpackInfo.ModifyShaderDesc = Callback;
            UnpackInfo.pUserData        = Callback;
            m_pDearchiver->UnpackShader(UnpackInfo, ppShader);
            if (*ppShader != nullptr)
                return true;
        }

        // Next, try to find the shader in the archiver
        RefCntAutoPtr<IShader> pArchivedShader{m_pArchiver->GetShader(HashStr.c_str())};
        const auto             FoundInArchive = pArchivedShader != nullptr;
        if (!pArchivedShader)
        {
            auto ArchiveShaderCI      = ShaderCI;
            ArchiveShaderCI.Desc.Name = HashStr.c_str();
            ShaderArchiveInfo ArchiveInfo;
            ArchiveInfo.DeviceFlags = static_cast<ARCHIVE_DEVICE_DATA_FLAGS>(1 << m_DeviceType);
            m_pSerializationDevice->CreateShader(ArchiveShaderCI, ArchiveInfo, &pArchivedShader);
            if (pArchivedShader)
                m_pArchiver->AddShader(pArchivedShader);
        }

        if (pArchivedShader)
        {
            RefCntAutoPtr<ISerializedShader> pSerializedShader{pArchivedShader, IID_SerializedShader};
            VERIFY(pSerializedShader, "Shader object is not a serialized shader");
            if (pSerializedShader)
            {
                *ppShader = pSerializedShader->GetDeviceShader(m_DeviceType);
                if (*ppShader != nullptr)
                {
                    (*ppShader)->AddRef();
                    return FoundInArchive;
                }
                else
                {
                    UNEXPECTED("Byte code must not be null");
                }
            }
        }

        if (*ppShader == nullptr)
        {
            m_pDevice->CreateShader(ShaderCI, ppShader);
        }

        return FoundInArchive;
    }

    virtual bool DILIGENT_CALL_TYPE CreateGraphicsPipelineState(
        const GraphicsPipelineStateCreateInfo& PSOCreateInfo,
        IPipelineState**                       ppPipelineState) override final
    {
        return CreatePipelineState(PSOCreateInfo, ppPipelineState);
    }

    virtual bool DILIGENT_CALL_TYPE CreateComputePipelineState(
        const ComputePipelineStateCreateInfo& PSOCreateInfo,
        IPipelineState**                      ppPipelineState) override final
    {
        return CreatePipelineState(PSOCreateInfo, ppPipelineState);
    }

    virtual bool DILIGENT_CALL_TYPE CreateRayTracingPipelineState(
        const RayTracingPipelineStateCreateInfo& PSOCreateInfo,
        IPipelineState**                         ppPipelineState) override final
    {
        auto CI = PSOCreateInfo;

        std::vector<RayTracingGeneralShaderGroup>       pGeneralShaders{CI.pGeneralShaders, CI.pGeneralShaders + CI.GeneralShaderCount};
        std::vector<RayTracingTriangleHitShaderGroup>   pTriangleHitShaders{CI.pTriangleHitShaders, CI.pTriangleHitShaders + CI.TriangleHitShaderCount};
        std::vector<RayTracingProceduralHitShaderGroup> pProceduralHitShaders{CI.pProceduralHitShaders, CI.pProceduralHitShaders + CI.ProceduralHitShaderCount};

        CI.pGeneralShaders       = pGeneralShaders.data();
        CI.pTriangleHitShaders   = pTriangleHitShaders.data();
        CI.pProceduralHitShaders = pProceduralHitShaders.data();

        return CreatePipelineState(CI, ppPipelineState);
    }

    virtual bool DILIGENT_CALL_TYPE CreateTilePipelineState(
        const TilePipelineStateCreateInfo& PSOCreateInfo,
        IPipelineState**                   ppPipelineState) override final
    {
        return CreatePipelineState(PSOCreateInfo, ppPipelineState);
    }

    virtual Bool DILIGENT_CALL_TYPE WriteToBlob(IDataBlob** ppBlob) override final
    {
        return m_pArchiver->SerializeToBlob(ppBlob);
    }

    virtual Bool DILIGENT_CALL_TYPE WriteToStream(IFileStream* pStream) override final
    {
        return m_pArchiver->SerializeToStream(pStream);
    }

    virtual void DILIGENT_CALL_TYPE Reset() override final
    {
        m_pDearchiver->Reset();
        m_pArchiver->Reset();
        m_Shaders.clear();
    }

private:
    static std::string MakeHashStr(const char* Name, const XXH128Hash& Hash)
    {
        std::string HashStr = HashToStr(Hash.LowPart, Hash.HighPart);
        if (Name != nullptr)
            HashStr = std::string{Name} + " [" + HashStr + ']';
        return HashStr;
    }

    template <typename HandlerType>
    void ProcessPipelineShaders(GraphicsPipelineStateCreateInfo& CI, HandlerType&& Handler)
    {
        Handler(CI.pVS);
        Handler(CI.pPS);
        Handler(CI.pDS);
        Handler(CI.pHS);
        Handler(CI.pGS);
        Handler(CI.pAS);
        Handler(CI.pMS);
    }

    template <typename HandlerType>
    void ProcessPipelineShaders(ComputePipelineStateCreateInfo& CI, HandlerType&& Handler)
    {
        Handler(CI.pCS);
    }

    template <typename HandlerType>
    void ProcessPipelineShaders(RayTracingPipelineStateCreateInfo& CI, HandlerType&& Handler)
    {
        for (size_t i = 0; i < CI.GeneralShaderCount; ++i)
        {
            auto& GeneralShader = const_cast<RayTracingGeneralShaderGroup&>(CI.pGeneralShaders[i]);
            Handler(GeneralShader.pShader);
        }

        for (size_t i = 0; i < CI.TriangleHitShaderCount; ++i)
        {
            auto& TriHitShader = const_cast<RayTracingTriangleHitShaderGroup&>(CI.pTriangleHitShaders[i]);
            Handler(TriHitShader.pAnyHitShader);
            Handler(TriHitShader.pClosestHitShader);
        }

        for (size_t i = 0; i < CI.ProceduralHitShaderCount; ++i)
        {
            auto& ProcHitShader = const_cast<RayTracingProceduralHitShaderGroup&>(CI.pProceduralHitShaders[i]);
            Handler(ProcHitShader.pAnyHitShader);
            Handler(ProcHitShader.pClosestHitShader);
            Handler(ProcHitShader.pIntersectionShader);
        }
    }

    template <typename HandlerType>
    void ProcessPipelineShaders(TilePipelineStateCreateInfo& CI, HandlerType&& Handler)
    {
        Handler(CI.pTS);
    }

    template <typename CreateInfoType>
    bool CreatePipelineState(CreateInfoType   PSOCreateInfo,
                             IPipelineState** ppPipelineState)
    {
        *ppPipelineState = nullptr;

        XXH128State Hasher;
        Hasher.Update(PSOCreateInfo);
        const auto Hash = Hasher.Digest();

        // First, try to check if the PSO has already been requested
        {
            std::lock_guard<std::mutex> Guard{m_PipelinesMtx};

            auto it = m_Pipelines.find(Hash);
            if (it != m_Pipelines.end())
            {
                if (auto pPSO = it->second.Lock())
                {
                    *ppPipelineState = pPSO.Detach();
                    return true;
                }
                else
                {
                    m_Pipelines.erase(it);
                }
            }
        }

        const auto HashStr = MakeHashStr(PSOCreateInfo.PSODesc.Name, Hash);

        // Try to find PSO in the loaded archive
        {
            auto Callback = MakeCallback(
                [&PSOCreateInfo](PipelineStateCreateInfo& CI) {
                    CI.PSODesc.Name = PSOCreateInfo.PSODesc.Name;
                });

            PipelineStateUnpackInfo UnpackInfo;
            UnpackInfo.PipelineType                  = PSOCreateInfo.PSODesc.PipelineType;
            UnpackInfo.Name                          = HashStr.c_str();
            UnpackInfo.pDevice                       = m_pDevice;
            UnpackInfo.ModifyPipelineStateCreateInfo = Callback;
            UnpackInfo.pUserData                     = Callback;
            m_pDearchiver->UnpackPipelineState(UnpackInfo, ppPipelineState);
            if (*ppPipelineState != nullptr)
                return true;
        }

        m_pDevice->CreatePipelineState(PSOCreateInfo, ppPipelineState);
        if (*ppPipelineState == nullptr)
            return false;

        {
            std::lock_guard<std::mutex> Guard{m_PipelinesMtx};
            m_Pipelines.emplace(Hash, *ppPipelineState);
        }

        if (m_pArchiver->GetPipelineState(PSOCreateInfo.PSODesc.PipelineType, HashStr.c_str()) != nullptr)
            return true;

        // Replace shaders with serialized shaders
        std::vector<RefCntAutoPtr<IObject>> TmpObjects;
        ProcessPipelineShaders(PSOCreateInfo,
                               [&](IShader*& pShader) {
                                   if (pShader == nullptr)
                                       return;

                                   RefCntAutoPtr<IObject> pObject;
                                   pShader->GetReferenceCounters()->QueryObject(&pObject);
                                   RefCntAutoPtr<IShader> pSerializedShader{pObject, IID_SerializedShader};
                                   if (!pSerializedShader)
                                   {
                                       ShaderCreateInfo ShaderCI;
                                       ShaderCI.Desc = pShader->GetDesc();

                                       Uint64 Size = 0;
                                       pShader->GetBytecode(&ShaderCI.ByteCode, Size);
                                       ShaderCI.ByteCodeSize = static_cast<size_t>(Size);
                                       if (m_DeviceType == RENDER_DEVICE_TYPE_GL)
                                       {
                                           ShaderCI.Source   = static_cast<const char*>(ShaderCI.ByteCode);
                                           ShaderCI.ByteCode = nullptr;
                                           if (ShaderCI.SourceLength > 1)
                                           {
                                               --ShaderCI.SourceLength;
                                           }
                                           ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM;
                                       }
                                       ShaderArchiveInfo ArchiveInfo;
                                       ArchiveInfo.DeviceFlags = static_cast<ARCHIVE_DEVICE_DATA_FLAGS>(1 << m_DeviceType);
                                       m_pSerializationDevice->CreateShader(ShaderCI, ArchiveInfo, &pSerializedShader);
                                   }

                                   pShader = pSerializedShader;
                                   TmpObjects.emplace_back(std::move(pSerializedShader));
                               });

        RefCntAutoPtr<IPipelineState> pSerializedPSO;
        {
            PipelineStateArchiveInfo ArchiveInfo;
            ArchiveInfo.DeviceFlags    = static_cast<ARCHIVE_DEVICE_DATA_FLAGS>(1 << m_DeviceType);
            PSOCreateInfo.PSODesc.Name = HashStr.c_str();
            m_pSerializationDevice->CreatePipelineState(PSOCreateInfo, ArchiveInfo, &pSerializedPSO);
        }
        if (pSerializedPSO)
        {
            m_pArchiver->AddPipelineState(pSerializedPSO);
        }

        return false;
    }

    static std::string HashToStr(Uint64 Low, Uint64 High)
    {
        static constexpr std::array<char, 16> Symbols = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

        std::string Str;
        for (auto Part : {High, Low})
        {
            for (Uint64 i = 0; i < 16; ++i)
                Str += Symbols[(Part >> (Uint64{60} - i * 4)) & 0xFu];
        }

        return Str;
    }

private:
    RefCntAutoPtr<IRenderDevice>        m_pDevice;
    const RENDER_DEVICE_TYPE            m_DeviceType;
    RefCntAutoPtr<ISerializationDevice> m_pSerializationDevice;
    RefCntAutoPtr<IArchiver>            m_pArchiver;
    RefCntAutoPtr<IDearchiver>          m_pDearchiver;

    std::mutex                                             m_ShadersMtx;
    std::unordered_map<XXH128Hash, RefCntWeakPtr<IShader>> m_Shaders;

    std::mutex                                                    m_PipelinesMtx;
    std::unordered_map<XXH128Hash, RefCntWeakPtr<IPipelineState>> m_Pipelines;
};

void CreateRenderStateCache(const RenderStateCacheCreateInfo& CreateInfo,
                            IRenderStateCache**               ppCache)
{
    try
    {
        RefCntAutoPtr<IRenderStateCache> pCache{MakeNewRCObj<RenderStateCacheImpl>()(CreateInfo)};
        if (pCache)
            pCache->QueryInterface(IID_RenderStateCache, reinterpret_cast<IObject**>(ppCache));
    }
    catch (...)
    {
        LOG_ERROR("Failed to create the bytecode cache");
    }
}

} // namespace Diligent

extern "C"
{
    void CreateRenderStateCache(const Diligent::RenderStateCacheCreateInfo& CreateInfo,
                                Diligent::IRenderStateCache**               ppCache)
    {
        Diligent::CreateRenderStateCache(CreateInfo, ppCache);
    }
}
