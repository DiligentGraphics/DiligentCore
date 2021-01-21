/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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
/// Declaration of Diligent::PipelineResourceSignatureVkImpl class
#include <array>
#include <bitset>

#include "PipelineResourceSignatureBase.hpp"
#include "VulkanUtilities/VulkanObjectWrappers.hpp"
#include "VulkanUtilities/VulkanLogicalDevice.hpp"
#include "VulkanUtilities/VulkanCommandBuffer.hpp"
#include "SRBMemoryAllocator.hpp"

namespace Diligent
{
class RenderDeviceVkImpl;
class ShaderResourceCacheVk;
class ShaderVariableManagerVk;
class DeviceContextVkImpl;

enum class DescriptorType : Uint8
{
    Sampler,
    CombinedImageSampler,
    SeparateImage,
    StorageImage,
    StorageImage_ReadOnly,
    UniformTexelBuffer,
    StorageTexelBuffer,
    StorageTexelBuffer_ReadOnly,
    UniformBuffer,
    StorageBuffer,
    StorageBuffer_ReadOnly,
    UniformBufferDynamic,
    StorageBufferDynamic,
    StorageBufferDynamic_ReadOnly,
    InputAttachment,
    AccelerationStructure,
    Count,
    Unknown = 0xFF,
};

RESOURCE_STATE DescriptorTypeToResourceState(DescriptorType Type);


/// Implementation of the Diligent::PipelineResourceSignatureVkImpl class
class PipelineResourceSignatureVkImpl final : public PipelineResourceSignatureBase<IPipelineResourceSignature, RenderDeviceVkImpl>
{
public:
    using TPipelineResourceSignatureBase = PipelineResourceSignatureBase<IPipelineResourceSignature, RenderDeviceVkImpl>;

    PipelineResourceSignatureVkImpl(IReferenceCounters*                  pRefCounters,
                                    RenderDeviceVkImpl*                  pDevice,
                                    const PipelineResourceSignatureDesc& Desc,
                                    bool                                 bIsDeviceInternal = false);
    ~PipelineResourceSignatureVkImpl();

    Uint32      GetDynamicBufferCount() const { return m_DynamicBufferCount; }
    Uint8       GetNumShaderStages() const { return m_NumShaders; }
    SHADER_TYPE GetShaderStageType(Uint32 StageIndex) const;
    Uint32      GetTotalResourceCount() const { return m_Desc.NumResources; }
    Uint32      GetNumDescriptorSets() const;

    static constexpr Uint32 InvalidSamplerInd = ~0u;

    struct ResourceAttribs
    {
        DescriptorType Type;
        Uint32         CacheOffset; // for ShaderResourceCacheVk
        Uint16         BindingIndex;
        Uint8          DescrSet : 1;
        Uint8          ImmutableSamplerAssigned : 1;
        Uint32         SamplerInd;

        ResourceAttribs() :
            CacheOffset{~0u}, BindingIndex{0xFFFF}, DescrSet{0}, SamplerInd{InvalidSamplerInd}, ImmutableSamplerAssigned{0}
        {}
    };

    const ResourceAttribs& GetAttribs(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_pResourceAttribs[ResIndex];
    }

    const PipelineResourceDesc& GetResource(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_Desc.Resources[ResIndex];
    }

    VkDescriptorSetLayout GetStaticVkDescriptorSetLayout() const { return m_VkDescSetLayouts[0]; }
    VkDescriptorSetLayout GetDynamicVkDescriptorSetLayout() const { return m_VkDescSetLayouts[1]; }

    bool HasStaticDescrSet() const { return GetStaticVkDescriptorSetLayout() != VK_NULL_HANDLE; }
    bool HasDynamicDescrSet() const { return GetDynamicVkDescriptorSetLayout() != VK_NULL_HANDLE; }

    virtual void DILIGENT_CALL_TYPE CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                bool                     InitStaticResources) override final;

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) override final;

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    virtual Uint32 DILIGENT_CALL_TYPE GetStaticVariableCount(SHADER_TYPE ShaderType) const override final;

    virtual void DILIGENT_CALL_TYPE BindStaticResources(Uint32            ShaderFlags,
                                                        IResourceMapping* pResourceMapping,
                                                        Uint32            Flags) override final;

    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineResourceSignature* pPRS) const override final
    {
        return IsCompatibleWith(*ValidatedCast<const PipelineResourceSignatureVkImpl>(pPRS));
    }

    SRBMemoryAllocator& GetSRBMemoryAllocator()
    {
        return m_SRBMemAllocator;
    }

    void InitResourceCache(ShaderResourceCacheVk& ResourceCache,
                           IMemoryAllocator&      CacheMemAllocator,
                           const char*            DbgPipelineName) const;

    // Initializes resource slots in the ResourceCache
    void InitializeResourceMemoryInCache(ShaderResourceCacheVk& ResourceCache) const;

    void InitializeStaticSRBResources(ShaderResourceCacheVk& ResourceCache) const;

    static String GetPrintName(const PipelineResourceDesc& ResDesc, Uint32 ArrayInd);

    void BindResource(IDeviceObject*         pObj,
                      Uint32                 ArrayIndex,
                      Uint32                 ResIndex,
                      ShaderResourceCacheVk& ResourceCache) const;

    void CommitDynamicResources(const ShaderResourceCacheVk& ResourceCache,
                                VkDescriptorSet              vkDynamicDescriptorSet) const;

    bool IsCompatibleWith(const PipelineResourceSignatureVkImpl& Other) const;

    bool IsIncompatibleWith(const PipelineResourceSignatureVkImpl& Other) const
    {
        return GetHash() != Other.GetHash();
    }

private:
    void Destruct();

    void ReserveSpaceForStaticVarsMgrs(const PipelineResourceSignatureDesc& Desc,
                                       FixedLinearAllocator&                MemPool,
                                       Int8&                                StaticVarStageCount,
                                       Uint32&                              StaticVarCount,
                                       Uint8                                DSMapping[2]);

    void CreateLayout(Uint32 StaticVarCount, const Uint8 DSMapping[2]);

    Uint32 FindAssignedSampler(const PipelineResourceDesc& SepImg) const;

    // returns descriptor set index in resource cache
    Uint32 GetStaticDescrSetIndex() const;
    Uint32 GetDynamicDescrSetIndex() const;

    using ImmutableSamplerPtrType = RefCntAutoPtr<ISampler>;

private:
    VulkanUtilities::DescriptorSetLayoutWrapper m_VkDescSetLayouts[2];

    ResourceAttribs* m_pResourceAttribs = nullptr; // [m_Desc.NumResources]

    SHADER_TYPE m_ShaderStages = SHADER_TYPE_UNKNOWN;

    Uint32 m_DynamicBufferCount : 29; // buffers with dynamic offsets
    Uint32 m_NumShaders : 3;

    std::array<Int8, MAX_SHADERS_IN_PIPELINE> m_StaticVarIndex = {-1, -1, -1, -1, -1, -1};
    static_assert(MAX_SHADERS_IN_PIPELINE == 6, "Please update the initializer list above");

    ShaderResourceCacheVk*   m_pResourceCache = nullptr;
    ShaderVariableManagerVk* m_StaticVarsMgrs = nullptr; // [m_NumShaders]

    ImmutableSamplerPtrType* m_ImmutableSamplers = nullptr; // [m_Desc.NumImmutableSamplers]
    SRBMemoryAllocator       m_SRBMemAllocator;
};


} // namespace Diligent
