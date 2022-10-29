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
#include <memory>

#include "ObjectBase.hpp"
#include "ShaderBase.hpp"
#include "RefCntAutoPtr.hpp"
#include "SerializationDevice.h"
#include "SerializedShader.h"
#include "Archiver.h"
#include "Dearchiver.h"
#include "ArchiverFactory.h"
#include "ArchiverFactoryLoader.h"
#include "XXH128Hasher.hpp"
#include "CallbackWrapper.hpp"
#include "GraphicsUtilities.h"

namespace Diligent
{

#define PROXY_METHOD(Object, RetType, MethodName)                  \
    virtual RetType DILIGENT_CALL_TYPE MethodName() override final \
    {                                                              \
        return Object->MethodName();                               \
    }

#define PROXY_CONST_METHOD(Object, RetType, MethodName)                  \
    virtual RetType DILIGENT_CALL_TYPE MethodName() const override final \
    {                                                                    \
        return Object->MethodName();                                     \
    }

#define PROXY_METHOD1(Object, RetType, MethodName, Type1, Arg1)              \
    virtual RetType DILIGENT_CALL_TYPE MethodName(Type1 Arg1) override final \
    {                                                                        \
        return Object->MethodName(Arg1);                                     \
    }

#define PROXY_CONST_METHOD1(Object, RetType, MethodName, Type1, Arg1)              \
    virtual RetType DILIGENT_CALL_TYPE MethodName(Type1 Arg1) const override final \
    {                                                                              \
        return Object->MethodName(Arg1);                                           \
    }

#define PROXY_METHOD2(Object, RetType, MethodName, Type1, Arg1, Type2, Arg2)             \
    virtual RetType DILIGENT_CALL_TYPE MethodName(Type1 Arg1, Type2 Arg2) override final \
    {                                                                                    \
        return Object->MethodName(Arg1, Arg2);                                           \
    }

#define PROXY_CONST_METHOD2(Object, RetType, MethodName, Type1, Arg1, Type2, Arg2)             \
    virtual RetType DILIGENT_CALL_TYPE MethodName(Type1 Arg1, Type2 Arg2) const override final \
    {                                                                                          \
        return Object->MethodName(Arg1, Arg2);                                                 \
    }

#define PROXY_METHOD3(Object, RetType, MethodName, Type1, Arg1, Type2, Arg2, Type3, Arg3)            \
    virtual RetType DILIGENT_CALL_TYPE MethodName(Type1 Arg1, Type2 Arg2, Type3 Arg3) override final \
    {                                                                                                \
        return Object->MethodName(Arg1, Arg2, Arg3);                                                 \
    }

class RenderStateCacheImpl;

class ReloadableShader final : public ObjectBase<IShader>
{
public:
    using TBase = ObjectBase<IShader>;

    // {6BFAAABD-FE55-4420-B0C8-5C4B4F5F8D65}
    static constexpr INTERFACE_ID IID_InternalImpl =
        {0x6bfaaabd, 0xfe55, 0x4420, {0xb0, 0xc8, 0x5c, 0x4b, 0x4f, 0x5f, 0x8d, 0x65}};

    ReloadableShader(IReferenceCounters*     pRefCounters,
                     RenderStateCacheImpl*   pStateCache,
                     IShader*                pShader,
                     const ShaderCreateInfo& CreateInfo);

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final
    {
        if (ppInterface == nullptr)
            return;

        if (IID == IID_InternalImpl || IID == IID_Shader || IID == IID_DeviceObject || IID == IID_Unknown)
        {
            *ppInterface = this;
            (*ppInterface)->AddRef();
        }
        else
        {
            m_pShader->QueryInterface(IID, ppInterface);
        }
    }

    PROXY_CONST_METHOD(m_pShader, const ShaderDesc&, GetDesc)
    PROXY_CONST_METHOD(m_pShader, Int32, GetUniqueID)
    PROXY_METHOD1(m_pShader, void, SetUserData, IObject*, pUserData)
    PROXY_CONST_METHOD(m_pShader, IObject*, GetUserData)
    PROXY_CONST_METHOD(m_pShader, Uint32, GetResourceCount)
    PROXY_CONST_METHOD2(m_pShader, void, GetResourceDesc, Uint32, Index, ShaderResourceDesc&, ResourceDesc)
    PROXY_CONST_METHOD2(m_pShader, void, GetBytecode, const void**, ppBytecode, Uint64&, Size)

    static void Create(RenderStateCacheImpl*   pStateCache,
                       IShader*                pShader,
                       const ShaderCreateInfo& CreateInfo,
                       IShader**               ppReloadableShader)
    {
        try
        {
            RefCntAutoPtr<ReloadableShader> pReloadableShader{MakeNewRCObj<ReloadableShader>()(pStateCache, pShader, CreateInfo)};
            *ppReloadableShader = pReloadableShader.Detach();
        }
        catch (...)
        {
            LOG_ERROR("Failed to create reloadable shader");
        }
    }

    bool Reload();

private:
    RefCntAutoPtr<RenderStateCacheImpl> m_pStateCache;
    RefCntAutoPtr<IShader>              m_pShader;
    ShaderCreateInfoWrapper             m_CreateInfo;
};

constexpr INTERFACE_ID ReloadableShader::IID_InternalImpl;

class ReloadablePipelineState final : public ObjectBase<IPipelineState>
{
public:
    using TBase = ObjectBase<IPipelineState>;

    // {1F325E25-496B-41B4-A1F9-242302ABCDD4}
    static constexpr INTERFACE_ID IID_InternalImpl =
        {0x1f325e25, 0x496b, 0x41b4, {0xa1, 0xf9, 0x24, 0x23, 0x2, 0xab, 0xcd, 0xd4}};

    ReloadablePipelineState(IReferenceCounters*            pRefCounters,
                            RenderStateCacheImpl*          pStateCache,
                            IPipelineState*                pPipeline,
                            const PipelineStateCreateInfo& CreateInfo);

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final
    {
        if (ppInterface == nullptr)
            return;

        if (IID == IID_InternalImpl || IID == IID_PipelineState || IID == IID_DeviceObject || IID == IID_Unknown)
        {
            *ppInterface = this;
            (*ppInterface)->AddRef();
        }
        else
        {
            m_pPipeline->QueryInterface(IID, ppInterface);
        }
    }

    PROXY_CONST_METHOD(m_pPipeline, const PipelineStateDesc&, GetDesc)
    PROXY_CONST_METHOD(m_pPipeline, Int32, GetUniqueID)
    PROXY_METHOD1(m_pPipeline, void, SetUserData, IObject*, pUserData)
    PROXY_CONST_METHOD(m_pPipeline, IObject*, GetUserData)
    PROXY_CONST_METHOD(m_pPipeline, const GraphicsPipelineDesc&, GetGraphicsPipelineDesc)
    PROXY_CONST_METHOD(m_pPipeline, const RayTracingPipelineDesc&, GetRayTracingPipelineDesc)
    PROXY_CONST_METHOD(m_pPipeline, const TilePipelineDesc&, GetTilePipelineDesc)
    PROXY_METHOD3(m_pPipeline, void, BindStaticResources, SHADER_TYPE, ShaderStages, IResourceMapping*, pResourceMapping, BIND_SHADER_RESOURCES_FLAGS, Flags)
    PROXY_CONST_METHOD1(m_pPipeline, Uint32, GetStaticVariableCount, SHADER_TYPE, ShaderType)
    PROXY_METHOD2(m_pPipeline, IShaderResourceVariable*, GetStaticVariableByName, SHADER_TYPE, ShaderType, const Char*, Name)
    PROXY_METHOD2(m_pPipeline, IShaderResourceVariable*, GetStaticVariableByIndex, SHADER_TYPE, ShaderType, Uint32, Index)
    PROXY_METHOD2(m_pPipeline, void, CreateShaderResourceBinding, IShaderResourceBinding**, ppShaderResourceBinding, bool, InitStaticResources)
    PROXY_CONST_METHOD1(m_pPipeline, void, InitializeStaticSRBResources, IShaderResourceBinding*, pShaderResourceBinding)
    PROXY_CONST_METHOD1(m_pPipeline, bool, IsCompatibleWith, const IPipelineState*, pPSO)
    PROXY_CONST_METHOD(m_pPipeline, Uint32, GetResourceSignatureCount)
    PROXY_CONST_METHOD1(m_pPipeline, IPipelineResourceSignature*, GetResourceSignature, Uint32, Index)

    static void Create(RenderStateCacheImpl*          pStateCache,
                       IPipelineState*                pPipeline,
                       const PipelineStateCreateInfo& CreateInfo,
                       IPipelineState**               ppReloadablePipeline)
    {
        try
        {
            RefCntAutoPtr<ReloadablePipelineState> pReloadablePipeline{MakeNewRCObj<ReloadablePipelineState>()(pStateCache, pPipeline, CreateInfo)};
            *ppReloadablePipeline = pReloadablePipeline.Detach();
        }
        catch (...)
        {
            LOG_ERROR("Failed to create reloadable shader");
        }
    }

    bool Reload(ModifyPipelineReloadInfoCallbackType ModifyReloadInfo);

private:
    RefCntAutoPtr<RenderStateCacheImpl> m_pStateCache;
    RefCntAutoPtr<IPipelineState>       m_pPipeline;
};

constexpr INTERFACE_ID ReloadablePipelineState::IID_InternalImpl;


/// Implementation of IRenderStateCache
class RenderStateCacheImpl final : public ObjectBase<IRenderStateCache>
{
public:
    using TBase = ObjectBase<IRenderStateCache>;

public:
    RenderStateCacheImpl(IReferenceCounters*               pRefCounters,
                         const RenderStateCacheCreateInfo& CreateInfo);

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_RenderStateCache, TBase);

    virtual bool DILIGENT_CALL_TYPE Load(const IDataBlob* pArchive,
                                         bool             MakeCopy) override final
    {
        return m_pDearchiver->LoadArchive(pArchive, MakeCopy);
    }

    virtual bool DILIGENT_CALL_TYPE CreateShader(const ShaderCreateInfo& ShaderCI,
                                                 IShader**               ppShader) override final;

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
        return CreatePipelineState(PSOCreateInfo, ppPipelineState);
    }

    virtual bool DILIGENT_CALL_TYPE CreateTilePipelineState(
        const TilePipelineStateCreateInfo& PSOCreateInfo,
        IPipelineState**                   ppPipelineState) override final
    {
        return CreatePipelineState(PSOCreateInfo, ppPipelineState);
    }

    virtual Bool DILIGENT_CALL_TYPE WriteToBlob(IDataBlob** ppBlob) override final
    {
        // Load new render states from archiver to dearchiver

        RefCntAutoPtr<IDataBlob> pNewData;
        m_pArchiver->SerializeToBlob(&pNewData);
        if (!pNewData)
        {
            LOG_ERROR_MESSAGE("Failed to serialize render state data");
            return false;
        }

        if (!m_pDearchiver->LoadArchive(pNewData))
        {
            LOG_ERROR_MESSAGE("Failed to load new render state data");
            return false;
        }

        m_pArchiver->Reset();

        return m_pDearchiver->Store(ppBlob);
    }

    virtual Bool DILIGENT_CALL_TYPE WriteToStream(IFileStream* pStream) override final
    {
        DEV_CHECK_ERR(pStream != nullptr, "pStream must not be null");
        if (pStream == nullptr)
            return false;

        RefCntAutoPtr<IDataBlob> pDataBlob;
        if (!WriteToBlob(&pDataBlob))
            return false;

        return pStream->Write(pDataBlob->GetConstDataPtr(), pDataBlob->GetSize());
    }

    virtual void DILIGENT_CALL_TYPE Reset() override final
    {
        m_pDearchiver->Reset();
        m_pArchiver->Reset();
        m_Shaders.clear();
        m_Pipelines.clear();
    }

    virtual Uint32 DILIGENT_CALL_TYPE Reload(ModifyPipelineReloadInfoCallbackType ModifyReloadInfo) override final;

    bool CreateShaderInternal(const ShaderCreateInfo& ShaderCI,
                              IShader**               ppShader);

    template <typename CreateInfoType>
    bool CreatePipelineStateInternal(const CreateInfoType& PSOCreateInfo,
                                     IPipelineState**      ppPipelineState);

private:
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

    static std::string MakeHashStr(const char* Name, const XXH128Hash& Hash)
    {
        std::string HashStr = HashToStr(Hash.LowPart, Hash.HighPart);
        if (Name != nullptr)
            HashStr = std::string{Name} + " [" + HashStr + ']';
        return HashStr;
    }

    template <typename CreateInfoType>
    struct SerializedPsoCIWrapperBase;

    template <typename CreateInfoType>
    struct SerializedPsoCIWrapper : SerializedPsoCIWrapperBase<CreateInfoType>
    {};

    template <typename CreateInfoType>
    bool CreatePipelineState(const CreateInfoType& PSOCreateInfo,
                             IPipelineState**      ppPipelineState);

private:
    RefCntAutoPtr<IRenderDevice>                   m_pDevice;
    const RENDER_DEVICE_TYPE                       m_DeviceType;
    const RenderStateCacheCreateInfo               m_CI;
    RefCntAutoPtr<IShaderSourceInputStreamFactory> m_pReloadSource;
    RefCntAutoPtr<ISerializationDevice>            m_pSerializationDevice;
    RefCntAutoPtr<IArchiver>                       m_pArchiver;
    RefCntAutoPtr<IDearchiver>                     m_pDearchiver;

    std::mutex                                             m_ShadersMtx;
    std::unordered_map<XXH128Hash, RefCntWeakPtr<IShader>> m_Shaders;

    std::mutex                                           m_ReloadableShadersMtx;
    std::unordered_map<IShader*, RefCntWeakPtr<IShader>> m_ReloadableShaders;

    std::mutex                                                    m_PipelinesMtx;
    std::unordered_map<XXH128Hash, RefCntWeakPtr<IPipelineState>> m_Pipelines;

    std::mutex                                                         m_ReloadablePipelinesMtx;
    std::unordered_map<IPipelineState*, RefCntWeakPtr<IPipelineState>> m_ReloadablePipelines;
};

RenderStateCacheImpl::RenderStateCacheImpl(IReferenceCounters*               pRefCounters,
                                           const RenderStateCacheCreateInfo& CreateInfo) :
    TBase{pRefCounters},
    // clang-format off
    m_pDevice      {CreateInfo.pDevice},
    m_DeviceType   {CreateInfo.pDevice != nullptr ? CreateInfo.pDevice->GetDeviceInfo().Type : RENDER_DEVICE_TYPE_UNDEFINED},
    m_CI           {CreateInfo},
    m_pReloadSource{CreateInfo.pReloadSource}
// clang-format on
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
    pArchiverFactory       = GetArchiverFactory();
#endif
    VERIFY_EXPR(pArchiverFactory != nullptr);

    SerializationDeviceCreateInfo SerializationDeviceCI;
    SerializationDeviceCI.DeviceInfo  = m_pDevice->GetDeviceInfo();
    SerializationDeviceCI.AdapterInfo = m_pDevice->GetAdapterInfo();

    switch (m_DeviceType)
    {
        case RENDER_DEVICE_TYPE_D3D11:
            SerializationDeviceCI.D3D11.FeatureLevel = SerializationDeviceCI.DeviceInfo.APIVersion;
            break;

        case RENDER_DEVICE_TYPE_D3D12:
            GetRenderDeviceD3D12MaxShaderVersion(m_pDevice, SerializationDeviceCI.D3D12.ShaderVersion);
            break;

        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
            // Nothing to do
            break;

        case RENDER_DEVICE_TYPE_VULKAN:
            SerializationDeviceCI.Vulkan.ApiVersion = SerializationDeviceCI.DeviceInfo.APIVersion;
            break;

        case RENDER_DEVICE_TYPE_METAL:
            break;

        default:
            UNEXPECTED("Unknown device type");
    }

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

#define RENDER_STATE_CACHE_LOG(...)                                \
    do                                                             \
    {                                                              \
        if (m_CI.EnableLogging)                                    \
        {                                                          \
            LOG_INFO_MESSAGE("Render state cache: ", __VA_ARGS__); \
        }                                                          \
    } while (false)

bool RenderStateCacheImpl::CreateShader(const ShaderCreateInfo& ShaderCI,
                                        IShader**               ppShader)
{
    if (ppShader == nullptr)
    {
        DEV_ERROR("ppShader must not be null");
        return false;
    }
    DEV_CHECK_ERR(*ppShader == nullptr, "Overwriting reference to existing shader may cause memory leaks");

    *ppShader = nullptr;

    RefCntAutoPtr<IShader> pShader;

    const auto FoundInCache = CreateShaderInternal(ShaderCI, &pShader);
    if (!pShader)
        return false;

    if (m_CI.EnableHotReload)
    {
        {
            std::lock_guard<std::mutex> Guard{m_ReloadableShadersMtx};

            auto it = m_ReloadableShaders.find(pShader);
            if (it != m_ReloadableShaders.end())
            {
                if (auto pReloadableShader = it->second.Lock())
                    *ppShader = pReloadableShader.Detach();
            }
        }

        if (*ppShader == nullptr)
        {
            auto _ShaderCI = ShaderCI;
            if (m_pReloadSource)
                _ShaderCI.pShaderSourceStreamFactory = m_pReloadSource;
            ReloadableShader::Create(this, pShader, _ShaderCI, ppShader);

            std::lock_guard<std::mutex> Guard{m_ReloadableShadersMtx};
            m_ReloadableShaders.emplace(pShader, RefCntWeakPtr<IShader>(*ppShader));
        }
    }
    else
    {
        *ppShader = pShader.Detach();
    }

    return FoundInCache;
}

bool RenderStateCacheImpl::CreateShaderInternal(const ShaderCreateInfo& ShaderCI,
                                                IShader**               ppShader)
{
    VERIFY_EXPR(ppShader != nullptr && *ppShader == nullptr);

    XXH128State Hasher;
#ifdef DILIGENT_DEBUG
    constexpr bool IsDebug = true;
#else
    constexpr bool IsDebug = false;
#endif
    Hasher.Update(ShaderCI, m_DeviceType, IsDebug);
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
                RENDER_STATE_CACHE_LOG("Reusing existing shader '", (ShaderCI.Desc.Name ? ShaderCI.Desc.Name : ""), "'.");
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

    // Try to find the shader in the loaded archive
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
        RefCntAutoPtr<IShader> pShader;
        m_pDearchiver->UnpackShader(UnpackInfo, &pShader);
        if (pShader)
        {
            if (pShader->GetDesc() == ShaderCI.Desc)
            {
                RENDER_STATE_CACHE_LOG("Found shader '", HashStr, "'.");
                *ppShader = pShader.Detach();
                return true;
            }
            else
            {
                LOG_ERROR_MESSAGE("Description of shader '", (ShaderCI.Desc.Name != nullptr ? ShaderCI.Desc.Name : "<unnamed>"),
                                  "' does not match the description of the shader unpacked from the cache. This may be the result of a "
                                  "hash conflict, but the probability of this should be virtually zero.");
            }
        }
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
        {
            if (m_pArchiver->AddShader(pArchivedShader))
                RENDER_STATE_CACHE_LOG("Added shader '", HashStr, "'.");
            else
                LOG_ERROR_MESSAGE("Failed to archive shader '", HashStr, "'.");
        }
    }

    if (pArchivedShader)
    {
        RefCntAutoPtr<ISerializedShader> pSerializedShader{pArchivedShader, IID_SerializedShader};
        VERIFY(pSerializedShader, "Shader object is not a serialized shader");
        if (pSerializedShader)
        {
            if (RefCntAutoPtr<IShader> pShader{pSerializedShader->GetDeviceShader(m_DeviceType)})
            {
                if (pShader->GetDesc() == ShaderCI.Desc)
                {
                    *ppShader = pShader.Detach();
                    return FoundInArchive;
                }
                else
                {
                    LOG_ERROR_MESSAGE("Description of shader '", (ShaderCI.Desc.Name != nullptr ? ShaderCI.Desc.Name : "<unnamed>"),
                                      "' does not match the description of the shader recently added to the cache. This may be the result of a "
                                      "hash conflict, but the probability of this should be virtually zero.");
                }
            }
            else
            {
                UNEXPECTED("Device shader must not be null");
            }
        }
    }

    if (*ppShader == nullptr)
    {
        m_pDevice->CreateShader(ShaderCI, ppShader);
    }

    return false;
}

template <typename CreateInfoType>
struct RenderStateCacheImpl::SerializedPsoCIWrapperBase
{
    SerializedPsoCIWrapperBase(ISerializationDevice* pSerializationDevice, RENDER_DEVICE_TYPE DeviceType, const CreateInfoType& _CI) :
        CI{_CI},
        ppSignatures{_CI.ppResourceSignatures, _CI.ppResourceSignatures + _CI.ResourceSignaturesCount}
    {
        CI.ppResourceSignatures = !ppSignatures.empty() ? ppSignatures.data() : nullptr;

        // Replace signatures with serialized signatures
        for (size_t i = 0; i < ppSignatures.size(); ++i)
        {
            auto& pSign = ppSignatures[i];
            if (pSign == nullptr)
                continue;
            const auto&                  SignDesc = pSign->GetDesc();
            ResourceSignatureArchiveInfo ArchiveInfo;
            ArchiveInfo.DeviceFlags = static_cast<ARCHIVE_DEVICE_DATA_FLAGS>(1 << DeviceType);
            RefCntAutoPtr<IPipelineResourceSignature> pSerializedSign;
            pSerializationDevice->CreatePipelineResourceSignature(SignDesc, ArchiveInfo, &pSerializedSign);
            if (!pSerializedSign)
            {
                LOG_ERROR_AND_THROW("Failed to serialize pipeline resource signature '", SignDesc.Name, "'.");
            }
            pSign = pSerializedSign;
            SerializedObjects.emplace_back(std::move(pSerializedSign));
        }
    }

    SerializedPsoCIWrapperBase(const SerializedPsoCIWrapperBase&) = delete;
    SerializedPsoCIWrapperBase(SerializedPsoCIWrapperBase&&)      = delete;
    SerializedPsoCIWrapperBase& operator=(const SerializedPsoCIWrapperBase&) = delete;
    SerializedPsoCIWrapperBase& operator=(SerializedPsoCIWrapperBase&&) = delete;

    void SetName(const char* Name)
    {
        VERIFY_EXPR(Name != nullptr);
        CI.PSODesc.Name = Name;
    }

    operator const CreateInfoType&()
    {
        return CI;
    }

protected:
    // Replaces shader with a serialized shader
    void SerializeShader(ISerializationDevice* pSerializationDevice, RENDER_DEVICE_TYPE DeviceType, IShader*& pShader)
    {
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
            if (DeviceType == RENDER_DEVICE_TYPE_GL || DeviceType == RENDER_DEVICE_TYPE_METAL)
            {
                ShaderCI.Source   = static_cast<const char*>(ShaderCI.ByteCode);
                ShaderCI.ByteCode = nullptr;
                if (DeviceType == RENDER_DEVICE_TYPE_GL)
                    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM;
                else if (DeviceType == RENDER_DEVICE_TYPE_METAL)
                    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_MSL_VERBATIM;
                else
                    UNEXPECTED("Unexpected device type");
            }
            ShaderArchiveInfo ArchiveInfo;
            ArchiveInfo.DeviceFlags = static_cast<ARCHIVE_DEVICE_DATA_FLAGS>(1 << DeviceType);
            pSerializationDevice->CreateShader(ShaderCI, ArchiveInfo, &pSerializedShader);
            if (!pSerializedShader)
            {
                LOG_ERROR_AND_THROW("Failed to serialize shader '", ShaderCI.Desc.Name, "'.");
            }
        }

        pShader = pSerializedShader;
        SerializedObjects.emplace_back(std::move(pSerializedShader));
    }

protected:
    CreateInfoType CI;

    std::vector<IPipelineResourceSignature*> ppSignatures;
    std::vector<RefCntAutoPtr<IObject>>      SerializedObjects;
};

template <>
struct RenderStateCacheImpl::SerializedPsoCIWrapper<GraphicsPipelineStateCreateInfo> : SerializedPsoCIWrapperBase<GraphicsPipelineStateCreateInfo>
{
    SerializedPsoCIWrapper(ISerializationDevice* pSerializationDevice, RENDER_DEVICE_TYPE DeviceType, const GraphicsPipelineStateCreateInfo& _CI) :
        SerializedPsoCIWrapperBase<GraphicsPipelineStateCreateInfo>{pSerializationDevice, DeviceType, _CI}
    {
        SerializeShader(pSerializationDevice, DeviceType, CI.pVS);
        SerializeShader(pSerializationDevice, DeviceType, CI.pPS);
        SerializeShader(pSerializationDevice, DeviceType, CI.pDS);
        SerializeShader(pSerializationDevice, DeviceType, CI.pHS);
        SerializeShader(pSerializationDevice, DeviceType, CI.pGS);
        SerializeShader(pSerializationDevice, DeviceType, CI.pAS);
        SerializeShader(pSerializationDevice, DeviceType, CI.pMS);

        // Replace render pass with serialized render pass
        if (CI.GraphicsPipeline.pRenderPass != nullptr)
        {
            const auto& RPDesc = CI.GraphicsPipeline.pRenderPass->GetDesc();

            RefCntAutoPtr<IRenderPass> pSerializedRP;
            pSerializationDevice->CreateRenderPass(RPDesc, &pSerializedRP);
            if (!pSerializedRP)
            {
                LOG_ERROR_AND_THROW("Failed to serialize render pass '", RPDesc.Name, "'.");
            }
            CI.GraphicsPipeline.pRenderPass = pSerializedRP;
            SerializedObjects.emplace_back(std::move(pSerializedRP));
        }
    }
};

template <>
struct RenderStateCacheImpl::SerializedPsoCIWrapper<ComputePipelineStateCreateInfo> : SerializedPsoCIWrapperBase<ComputePipelineStateCreateInfo>
{
    SerializedPsoCIWrapper(ISerializationDevice* pSerializationDevice, RENDER_DEVICE_TYPE DeviceType, const ComputePipelineStateCreateInfo& _CI) :
        SerializedPsoCIWrapperBase<ComputePipelineStateCreateInfo>{pSerializationDevice, DeviceType, _CI}
    {
        SerializeShader(pSerializationDevice, DeviceType, CI.pCS);
    }
};

template <>
struct RenderStateCacheImpl::SerializedPsoCIWrapper<TilePipelineStateCreateInfo> : SerializedPsoCIWrapperBase<TilePipelineStateCreateInfo>
{
    SerializedPsoCIWrapper(ISerializationDevice* pSerializationDevice, RENDER_DEVICE_TYPE DeviceType, const TilePipelineStateCreateInfo& _CI) :
        SerializedPsoCIWrapperBase<TilePipelineStateCreateInfo>{pSerializationDevice, DeviceType, _CI}
    {
        SerializeShader(pSerializationDevice, DeviceType, CI.pTS);
    }
};

template <>
struct RenderStateCacheImpl::SerializedPsoCIWrapper<RayTracingPipelineStateCreateInfo> : SerializedPsoCIWrapperBase<RayTracingPipelineStateCreateInfo>
{
    SerializedPsoCIWrapper(ISerializationDevice* pSerializationDevice, RENDER_DEVICE_TYPE DeviceType, const RayTracingPipelineStateCreateInfo& _CI) :
        SerializedPsoCIWrapperBase<RayTracingPipelineStateCreateInfo>{pSerializationDevice, DeviceType, _CI},
        // clang-format off
        pGeneralShaders      {_CI.pGeneralShaders,       _CI.pGeneralShaders       + _CI.GeneralShaderCount},
        pTriangleHitShaders  {_CI.pTriangleHitShaders,   _CI.pTriangleHitShaders   + _CI.TriangleHitShaderCount},
        pProceduralHitShaders{_CI.pProceduralHitShaders, _CI.pProceduralHitShaders + _CI.ProceduralHitShaderCount}
    // clang-format on
    {
        CI.pGeneralShaders       = pGeneralShaders.data();
        CI.pTriangleHitShaders   = pTriangleHitShaders.data();
        CI.pProceduralHitShaders = pProceduralHitShaders.data();

        for (auto& GeneralShader : pGeneralShaders)
        {
            SerializeShader(pSerializationDevice, DeviceType, GeneralShader.pShader);
        }

        for (auto& TriHitShader : pTriangleHitShaders)
        {
            SerializeShader(pSerializationDevice, DeviceType, TriHitShader.pAnyHitShader);
            SerializeShader(pSerializationDevice, DeviceType, TriHitShader.pClosestHitShader);
        }

        for (auto& ProcHitShader : pProceduralHitShaders)
        {
            SerializeShader(pSerializationDevice, DeviceType, ProcHitShader.pAnyHitShader);
            SerializeShader(pSerializationDevice, DeviceType, ProcHitShader.pClosestHitShader);
            SerializeShader(pSerializationDevice, DeviceType, ProcHitShader.pIntersectionShader);
        }
    }

private:
    std::vector<RayTracingGeneralShaderGroup>       pGeneralShaders;
    std::vector<RayTracingTriangleHitShaderGroup>   pTriangleHitShaders;
    std::vector<RayTracingProceduralHitShaderGroup> pProceduralHitShaders;
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

template <typename CreateInfoType>
bool RenderStateCacheImpl::CreatePipelineState(const CreateInfoType& PSOCreateInfo,
                                               IPipelineState**      ppPipelineState)
{
    if (ppPipelineState == nullptr)
    {
        DEV_ERROR("ppPipelineState must not be null");
        return false;
    }
    DEV_CHECK_ERR(*ppPipelineState == nullptr, "Overwriting reference to existing pipeline state may cause memory leaks");

    *ppPipelineState = nullptr;

    RefCntAutoPtr<IPipelineState> pPSO;

    const auto FoundInCache = CreatePipelineStateInternal(PSOCreateInfo, &pPSO);
    if (!pPSO)
        return false;

    if (m_CI.EnableHotReload)
    {
        {
            std::lock_guard<std::mutex> Guard{m_ReloadablePipelinesMtx};

            auto it = m_ReloadablePipelines.find(pPSO);
            if (it != m_ReloadablePipelines.end())
            {
                if (auto pReloadablePSO = it->second.Lock())
                    *ppPipelineState = pReloadablePSO.Detach();
            }
        }

        if (*ppPipelineState == nullptr)
        {
            ReloadablePipelineState::Create(this, pPSO, PSOCreateInfo, ppPipelineState);

            std::lock_guard<std::mutex> Guard{m_ReloadablePipelinesMtx};
            m_ReloadablePipelines.emplace(pPSO, RefCntWeakPtr<IPipelineState>(*ppPipelineState));
        }
    }
    else
    {
        *ppPipelineState = pPSO.Detach();
    }

    return FoundInCache;
}

template <typename CreateInfoType>
bool RenderStateCacheImpl::CreatePipelineStateInternal(const CreateInfoType& PSOCreateInfo,
                                                       IPipelineState**      ppPipelineState)
{
    VERIFY_EXPR(ppPipelineState != nullptr && *ppPipelineState == nullptr);

    XXH128State Hasher;
    Hasher.Update(PSOCreateInfo, m_DeviceType);
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
                RENDER_STATE_CACHE_LOG("Reusing existing PSO '", (PSOCreateInfo.PSODesc.Name ? PSOCreateInfo.PSODesc.Name : ""), "'.");
                return true;
            }
            else
            {
                m_Pipelines.erase(it);
            }
        }
    }

    const auto HashStr = MakeHashStr(PSOCreateInfo.PSODesc.Name, Hash);

    bool FoundInCache = false;
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
        RefCntAutoPtr<IPipelineState> pPSO;
        m_pDearchiver->UnpackPipelineState(UnpackInfo, &pPSO);
        if (pPSO)
        {
            if (pPSO->GetDesc() == PSOCreateInfo.PSODesc)
            {
                *ppPipelineState = pPSO.Detach();
                FoundInCache     = true;
            }
            else
            {
                LOG_ERROR_MESSAGE("Description of pipeline state '", (PSOCreateInfo.PSODesc.Name != nullptr ? PSOCreateInfo.PSODesc.Name : "<unnamed>"),
                                  "' does not match the description of the pipeline unpacked from the cache. This may be the result of a "
                                  "hash conflict, but the probability of this should be virtually zero.");
            }
        }
    }

    if (*ppPipelineState == nullptr)
    {
        m_pDevice->CreatePipelineState(PSOCreateInfo, ppPipelineState);
        if (*ppPipelineState == nullptr)
            return false;
    }

    {
        std::lock_guard<std::mutex> Guard{m_PipelinesMtx};
        m_Pipelines.emplace(Hash, *ppPipelineState);
    }

    if (FoundInCache)
    {
        RENDER_STATE_CACHE_LOG("Found PSO '", HashStr, "'.");
        return true;
    }

    if (m_pArchiver->GetPipelineState(PSOCreateInfo.PSODesc.PipelineType, HashStr.c_str()) != nullptr)
        return true;

    try
    {
        // Make a copy of create info that contains serialized objects
        SerializedPsoCIWrapper<CreateInfoType> SerializedPsoCI{m_pSerializationDevice, m_DeviceType, PSOCreateInfo};
        SerializedPsoCI.SetName(HashStr.c_str());

        PipelineStateArchiveInfo ArchiveInfo;
        ArchiveInfo.DeviceFlags = static_cast<ARCHIVE_DEVICE_DATA_FLAGS>(1 << m_DeviceType);
        RefCntAutoPtr<IPipelineState> pSerializedPSO;
        m_pSerializationDevice->CreatePipelineState(SerializedPsoCI, ArchiveInfo, &pSerializedPSO);

        if (pSerializedPSO)
        {
            if (m_pArchiver->AddPipelineState(pSerializedPSO))
                RENDER_STATE_CACHE_LOG("Added PSO '", HashStr, "'.");
            else
                LOG_ERROR_MESSAGE("Failed to archive PSO '", HashStr, "'.");
        }
    }
    catch (...)
    {
    }

    return false;
}

Uint32 RenderStateCacheImpl::Reload(ModifyPipelineReloadInfoCallbackType ModifyReloadInfo)
{
    if (!m_CI.EnableHotReload)
    {
        DEV_ERROR("This render state cache was not created with hot reload enabled. Set EnableHotReload to true.");
        return 0;
    }

    Uint32 NumStatesReloaded = 0;

    {
        std::lock_guard<std::mutex> Guard{m_ReloadableShadersMtx};
        for (auto shader_it : m_ReloadableShaders)
        {
            if (auto pShader = shader_it.second.Lock())
            {
                RefCntAutoPtr<ReloadableShader> pReloadableShader{pShader, ReloadableShader::IID_InternalImpl};
                if (pReloadableShader)
                {
                    if (pReloadableShader->Reload())
                        ++NumStatesReloaded;
                }
                else
                {
                    UNEXPECTED("Shader object is not a ReloadableShader");
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> Guard{m_ReloadablePipelinesMtx};
        for (auto pso_it : m_ReloadablePipelines)
        {
            if (auto pPSO = pso_it.second.Lock())
            {
                RefCntAutoPtr<ReloadablePipelineState> pReloadablePSO{pPSO, ReloadablePipelineState::IID_InternalImpl};
                if (pPSO)
                {
                    if (pReloadablePSO->Reload(ModifyReloadInfo))
                        ++NumStatesReloaded;
                }
                else
                {
                    UNEXPECTED("Shader object is not a ReloadableShader");
                }
            }
        }
    }

    return NumStatesReloaded;
}

ReloadableShader::ReloadableShader(IReferenceCounters*     pRefCounters,
                                   RenderStateCacheImpl*   pStateCache,
                                   IShader*                pShader,
                                   const ShaderCreateInfo& CreateInfo) :
    TBase{pRefCounters},
    m_pStateCache{pStateCache},
    m_pShader{pShader},
    m_CreateInfo{CreateInfo, GetRawAllocator()}
{}

bool ReloadableShader::Reload()
{
    RefCntAutoPtr<IShader> pNewShader;
    bool                   FoundInCache = m_pStateCache->CreateShaderInternal(m_CreateInfo, &pNewShader);
    if (pNewShader)
    {
        m_pShader = pNewShader;
    }
    else
    {
        const auto* Name = m_CreateInfo.Get().Desc.Name;
        LOG_ERROR_MESSAGE("Failed to reload shader '", (Name ? Name : "<unnamed>"), ";.");
    }
    return !FoundInCache;
}


ReloadablePipelineState::ReloadablePipelineState(IReferenceCounters*            pRefCounters,
                                                 RenderStateCacheImpl*          pStateCache,
                                                 IPipelineState*                pPipeline,
                                                 const PipelineStateCreateInfo& CreateInfo) :
    TBase{pRefCounters},
    m_pStateCache{pStateCache},
    m_pPipeline{pPipeline}
{}

bool ReloadablePipelineState::Reload(ModifyPipelineReloadInfoCallbackType ModifyReloadInfo)
{
    return false;
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
