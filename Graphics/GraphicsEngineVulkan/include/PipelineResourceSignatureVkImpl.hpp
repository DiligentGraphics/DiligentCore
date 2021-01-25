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
#include "SRBMemoryAllocator.hpp"

namespace Diligent
{
class RenderDeviceVkImpl;
class ShaderResourceCacheVk;
class ShaderVariableManagerVk;

enum class DescriptorType : Uint8
{
    Sampler,
    CombinedImageSampler,
    SeparateImage,
    StorageImage,
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

    Uint32 GetDynamicOffsetCount() const { return m_DynamicUniformBufferCount + m_DynamicStorageBufferCount; }
    Uint32 GetDynamicUniformBufferCount() const { return m_DynamicUniformBufferCount; }
    Uint32 GetDynamicStorageBufferCount() const { return m_DynamicStorageBufferCount; }
    Uint32 GetNumDescriptorSets() const;

    Uint32      GetNumShaderStages() const { return m_NumShaders; }
    SHADER_TYPE GetShaderStageType(Uint32 StageIndex) const;

    static constexpr Uint32 InvalidSamplerInd = (1u << 16) - 1;

    enum class CacheContentType
    {
        Signature = 0, // only static resources
        SRB       = 1  // in SRB
    };

    // sizeof(ResourceAttribs) == 16, x64
    struct ResourceAttribs
    {
    private:
        static constexpr Uint32 _DescrTypeBits       = 4;
        static constexpr Uint32 _DescrSetBits        = 1;
        static constexpr Uint32 _BindingIndexBits    = 16;
        static constexpr Uint32 _SamplerIndBits      = 16;
        static constexpr Uint32 _SamplerAssignedBits = 1;
        static constexpr Uint32 _BitsSumm            = ((sizeof(Uint32) * 8 * 2) + _DescrTypeBits + _DescrSetBits + _BindingIndexBits + _SamplerIndBits + _SamplerAssignedBits + 31) & ~31;

        static_assert((1u << _DescrTypeBits) >= static_cast<Uint32>(DescriptorType::Count), "not enought bits to store DescriptorType values");
        static_assert((1u << _SamplerIndBits) - 1 == InvalidSamplerInd, "InvalidSamplerInd is incorrect");
        static_assert((1u << _DescrSetBits) == MAX_DESCR_SET_PER_SIGNATURE, "not enoght bits to store descriptor set index");
        static_assert((1u << _BindingIndexBits) >= MAX_RESOURCES_IN_SIGNATURE, "not enoght bits to store resource binding index");

    public:
        // clang-format off
        Uint32  BindingIndex         : _BindingIndexBits;
        Uint32  SamplerInd           : _SamplerIndBits;     // index in m_Desc.Resources and m_pResourceAttribs
        Uint32  DescrType            : _DescrTypeBits;
        Uint32  DescrSet             : _DescrSetBits;
        Uint32  ImtblSamplerAssigned : _SamplerAssignedBits;
        Uint32  CacheOffsets[2];                            // static and static/mutable/dynamic offsets for ShaderResourceCacheVk

        ResourceAttribs()
        {
            static_assert(_BitsSumm == sizeof(ResourceAttribs) * 8, "fields is not properly packed");
        }

        Uint32 CacheOffset(CacheContentType CacheType) const { return CacheOffsets[static_cast<Uint32>(CacheType)]; }

        DescriptorType Type()                       const { return static_cast<DescriptorType>(DescrType); }
        bool           IsImmutableSamplerAssigned() const { return ImtblSamplerAssigned; }
        // clang-format on
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
    using ImmutableSamplerPtrType = RefCntAutoPtr<ISampler>;
    using CacheOffsetsType        = std::array<Uint32, 3 * MAX_DESCR_SET_PER_SIGNATURE>; // [dynamic uniform buffers, dynamic storage buffers, other] * [descriptor sets] includes ArraySize
    using BindingCountType        = std::array<Uint32, 3 * MAX_DESCR_SET_PER_SIGNATURE>; // [dynamic uniform buffers, dynamic storage buffers, other] * [descriptor sets] without ArraySize

    void Destruct();

    void ReserveSpaceForStaticVarsMgrs(const PipelineResourceSignatureDesc& Desc,
                                       Int8&                                StaticVarStageCount,
                                       Uint32&                              StaticVarCount,
                                       CacheOffsetsType&                    CacheSizes,
                                       BindingCountType&                    BindingCount,
                                       Uint8                                DSMapping[MAX_DESCR_SET_PER_SIGNATURE]);

    void CreateLayout(const CacheOffsetsType& CacheSizes,
                      const BindingCountType& BindingCount,
                      const Uint8             DSMapping[MAX_DESCR_SET_PER_SIGNATURE]);

    size_t CalculateHash() const;

    Uint32 FindAssignedSampler(const PipelineResourceDesc& SepImg) const;

    // returns descriptor set index in resource cache
    Uint32 GetStaticDescrSetIndex() const;
    Uint32 GetDynamicDescrSetIndex() const;

private:
    VulkanUtilities::DescriptorSetLayoutWrapper m_VkDescSetLayouts[MAX_DESCR_SET_PER_SIGNATURE];

    ResourceAttribs* m_pResourceAttribs = nullptr; // [m_Desc.NumResources]

    SHADER_TYPE m_ShaderStages = SHADER_TYPE_UNKNOWN;

    Uint16 m_DynamicUniformBufferCount = 0;
    Uint16 m_DynamicStorageBufferCount = 0;

    std::array<Int8, MAX_SHADERS_IN_PIPELINE> m_StaticVarIndex = {-1, -1, -1, -1, -1, -1};
    static_assert(MAX_SHADERS_IN_PIPELINE == 6, "Please update the initializer list above");

    Uint8 m_NumShaders = 0;

    ShaderResourceCacheVk*   m_pResourceCache = nullptr;
    ShaderVariableManagerVk* m_StaticVarsMgrs = nullptr; // [m_NumShaders]

    ImmutableSamplerPtrType* m_ImmutableSamplers = nullptr; // [m_Desc.NumImmutableSamplers]
    SRBMemoryAllocator       m_SRBMemAllocator;
};


} // namespace Diligent
