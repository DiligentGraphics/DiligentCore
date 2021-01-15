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

    Uint32 GetDynamicBufferCount() const { return m_DynamicBufferCount; }
    Uint8  GetNumShaderStages() const { return m_NumShaders; }
    Uint32 GetTotalResourceCount() const { return m_Desc.NumResources; }

    static constexpr Uint8 InvalidSamplerInd = 0xFF;

    struct PackedBindingIndex
    {
        Uint16 Binding;
        Uint8  DescSet : 1; // 0 - static, 1 - dynamic
        Uint8  SamplerInd = InvalidSamplerInd;
    };

    const PackedBindingIndex& GetBinding(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_pBindingIndices[ResIndex];
    }

    const PipelineResourceDesc& GetResource(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_Desc.Resources[ResIndex];
    }

    VkDescriptorSetLayout GetStaticVkDescriptorSetLayout() const { return m_VkDescSetLayouts[0]; }
    VkDescriptorSetLayout GetDynamicVkDescriptorSetLayout() const { return m_VkDescSetLayouts[1]; }

    virtual void DILIGENT_CALL_TYPE CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                bool                     InitStaticResources) override final;

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) override final;

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    virtual Uint32 DILIGENT_CALL_TYPE GetStaticVariableCount(SHADER_TYPE ShaderType) const override final;

    using VkDescSetArray = std::array<VkDescriptorSet, MAX_RESOURCE_SIGNATURES * 2>;

    struct DescriptorSetBindInfo
    {
        VkDescSetArray               vkSets;
        std::vector<uint32_t>        DynamicOffsets;
        const ShaderResourceCacheVk* pResourceCache          = nullptr;
        Uint32                       SetCout                 = 0;
        Uint32                       DynamicOffsetCount      = 0;
        bool                         DynamicBuffersPresent   = false;
        bool                         DynamicDescriptorsBound = false;
#ifdef DILIGENT_DEBUG
        //const PipelineLayoutVk* pDbgPipelineLayout = nullptr;
#endif
        DescriptorSetBindInfo() :
            DynamicOffsets(64)
        {
        }

        void Reset()
        {
            pResourceCache          = nullptr;
            SetCout                 = 0;
            DynamicOffsetCount      = 0;
            DynamicBuffersPresent   = false;
            DynamicDescriptorsBound = false;

#ifdef DILIGENT_DEBUG
            // In release mode, do not clear vectors as this causes unnecessary work
            DynamicOffsets.clear();

            //pDbgPipelineLayout = nullptr;
#endif
        }
    };

    void PrepareDescriptorSets(DeviceContextVkImpl*         pCtxVkImpl,
                               VkPipelineBindPoint          BindPoint,
                               const ShaderResourceCacheVk& ResourceCache,
                               DescriptorSetBindInfo&       BindInfo,
                               VkDescriptorSet              VkDynamicDescrSet) const;

    static VkDescriptorType GetVkDescriptorType(const PipelineResourceDesc& Res);

    SHADER_TYPE GetShaderStageType(Uint32 StageIndex) const;

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

private:
    void Destruct();

    VulkanUtilities::DescriptorSetLayoutWrapper m_VkDescSetLayouts[2];

    PackedBindingIndex* m_pBindingIndices = nullptr; // [m_Desc.NumResources]

    SHADER_TYPE m_ShaderStages = SHADER_TYPE_UNKNOWN;

    Uint16 m_DynamicBufferCount = 0;
    Uint8  m_NumShaders         = 0;

    std::array<Int8, MAX_SHADERS_IN_PIPELINE> m_StaticVarIndex = {-1, -1, -1, -1, -1, -1};
    static_assert(MAX_SHADERS_IN_PIPELINE == 6, "Please update the initializer list above");

    ShaderResourceCacheVk*   m_pResourceCache = nullptr;
    ShaderVariableManagerVk* m_StaticVarsMgrs = nullptr; // [0..MAX_SHADERS_IN_PIPELINE]

    SRBMemoryAllocator m_SRBMemAllocator;
};


} // namespace Diligent
