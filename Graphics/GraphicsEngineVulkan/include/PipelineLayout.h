/*     Copyright 2015-2018 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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
/// Declaration of Diligent::PipelineLayout class
#include <array>

#include "ShaderBase.h"
#include "ShaderResourceLayoutVk.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"
#include "VulkanUtilities/VulkanLogicalDevice.h"

namespace Diligent
{

class RenderDeviceVkImpl;


/// Implementation of the Diligent::PipelineLayout class
class PipelineLayout
{
public:
    static VkDescriptorType GetVkDescriptorType(const SPIRVShaderResourceAttribs &Res);

    PipelineLayout();
    void Release(RenderDeviceVkImpl *pDeviceVkImpl);

#if 0
    void AllocateStaticSamplers(IShader* const *ppShaders, Uint32 NumShaders);
#endif
    void Finalize(const VulkanUtilities::VulkanLogicalDevice& LogicalDevice);

    VkPipelineLayout GetVkPipelineLayout()const{return m_LayoutMgr.GetVkPipelineLayout();}
#if 0
    void InitResourceCache(RenderDeviceVkImpl *pDeviceVkImpl, class ShaderResourceCacheVk& ResourceCache, IMemoryAllocator &CacheMemAllocator)const;
    
    void InitStaticSampler(SHADER_TYPE ShaderType, const String &TextureName, const D3DShaderResourceAttribs &ShaderResAttribs);
#endif

    void AllocateResourceSlot(const SPIRVShaderResourceAttribs &ResAttribs, 
                              SHADER_TYPE ShaderType, 
                              Uint32 &DescriptorSet, 
                              Uint32 &Binding,
                              Uint32 &OffsetFromTableStart,
                              std::vector<uint32_t> &SPIRV);

#if 0
    // This method should be thread-safe as it does not modify any object state
    void (PipelineLayout::*CommitDescriptorHandles)(RenderDeviceVkImpl *pRenderDeviceVk, 
                                                   ShaderResourceCacheVk& ResourceCache, 
                                                   class CommandContext &Ctx, 
                                                   bool IsCompute)const = nullptr;

    void (PipelineLayout::*TransitionAndCommitDescriptorHandles)(RenderDeviceVkImpl *pRenderDeviceVk, 
                                                                ShaderResourceCacheVk& ResourceCache, 
                                                                class CommandContext &Ctx, 
                                                                bool IsCompute)const = nullptr;

    void TransitionResources(ShaderResourceCacheVk& ResourceCache, 
                             class CommandContext &Ctx)const;

    void CommitRootViews(ShaderResourceCacheVk& ResourceCache, 
                         class CommandContext &Ctx, 
                         bool IsCompute,
                         Uint32 ContextId)const;
#endif
    Uint32 GetTotalDescriptors(SHADER_VARIABLE_TYPE VarType)const
    {
        VERIFY_EXPR(VarType >= 0 && VarType < SHADER_VARIABLE_TYPE_NUM_TYPES);
        return m_LayoutMgr.GetDescriptorSet(VarType).TotalDescriptors;
    }

    bool IsSameAs(const PipelineLayout& RS)const
    {
        return m_LayoutMgr == RS.m_LayoutMgr;
    }
    size_t GetHash()const
    {
        return m_LayoutMgr.GetHash();
    }

private:

    class DescriptorSetLayoutManager
    {
    public:
        struct DescriptorSetLayout
        {
            DescriptorSetLayout() = default;
            DescriptorSetLayout(DescriptorSetLayout&&) = default;
            DescriptorSetLayout(const DescriptorSetLayout&) = delete;
            DescriptorSetLayout& operator = (const DescriptorSetLayout&) = delete;
            DescriptorSetLayout& operator = (DescriptorSetLayout&&) = delete;
            
            uint32_t TotalDescriptors = 0;
            int8_t SetIndex = -1;
            uint16_t NumLayoutBindings = 0;
            VkDescriptorSetLayoutBinding* pBindings = nullptr;
            VulkanUtilities::DescriptorSetLayoutWrapper VkLayout;
            
            ~DescriptorSetLayout();
            void AddBinding(const VkDescriptorSetLayoutBinding &Binding, IMemoryAllocator &MemAllocator);
            void Finalize(const VulkanUtilities::VulkanLogicalDevice &LogicalDevice, IMemoryAllocator &MemAllocator, VkDescriptorSetLayoutBinding* pNewBindings);
            void Release(RenderDeviceVkImpl *pRenderDeviceVk);

            bool operator == (const DescriptorSetLayout& rhs)const;
            bool operator != (const DescriptorSetLayout& rhs)const{return !(*this == rhs);}
            size_t GetHash()const;

        private:
            void ReserveMemory(Uint32 NumBindings, IMemoryAllocator &MemAllocator);
            static size_t GetMemorySize(Uint32 NumBindings);
        };

        DescriptorSetLayoutManager(IMemoryAllocator &MemAllocator);
        ~DescriptorSetLayoutManager();

        DescriptorSetLayoutManager(const DescriptorSetLayoutManager&) = delete;
        DescriptorSetLayoutManager& operator= (const DescriptorSetLayoutManager&) = delete;
        DescriptorSetLayoutManager(DescriptorSetLayoutManager&&) = delete;
        DescriptorSetLayoutManager& operator= (DescriptorSetLayoutManager&&) = delete;
        
        void Finalize(const VulkanUtilities::VulkanLogicalDevice &LogicalDevice);
        void Release(RenderDeviceVkImpl *pRenderDeviceVk);

        DescriptorSetLayout& GetDescriptorSet(SHADER_VARIABLE_TYPE VarType){return m_DescriptorSetLayouts[VarType == SHADER_VARIABLE_TYPE_DYNAMIC ? 1 : 0];}
        const DescriptorSetLayout& GetDescriptorSet(SHADER_VARIABLE_TYPE VarType)const { return m_DescriptorSetLayouts[VarType == SHADER_VARIABLE_TYPE_DYNAMIC ? 1 : 0]; }

        bool operator == (const DescriptorSetLayoutManager& rhs)const;
        bool operator != (const DescriptorSetLayoutManager& rhs)const {return !(*this == rhs);}
        size_t GetHash()const;
        VkPipelineLayout GetVkPipelineLayout()const{return m_VkPipelineLayout;}

        void AllocateResourceSlot(const SPIRVShaderResourceAttribs &ResAttribs,
                                  SHADER_TYPE ShaderType,
                                  Uint32 &DescriptorSet,
                                  Uint32 &Binding, 
                                  Uint32 &OffsetFromTableStart);
    private:
        IMemoryAllocator &m_MemAllocator;
        VulkanUtilities::PipelineLayoutWrapper m_VkPipelineLayout;
        std::array<DescriptorSetLayout, 2> m_DescriptorSetLayouts;
        std::vector<VkDescriptorSetLayoutBinding, STDAllocatorRawMem<VkDescriptorSetLayoutBinding>> m_LayoutBindings;
        uint8_t m_ActiveSets = 0;
    };

    IMemoryAllocator &m_MemAllocator;
    DescriptorSetLayoutManager m_LayoutMgr;

#if 0
    struct StaticSamplerAttribs
    {
        StaticSamplerDesc SamplerDesc;
        UINT ShaderRegister = static_cast<UINT>(-1);
        UINT ArraySize = 0;
        UINT RegisterSpace = 0;
        Vk_SHADER_VISIBILITY ShaderVisibility = static_cast<Vk_SHADER_VISIBILITY>(-1);
        
        StaticSamplerAttribs(){}
        StaticSamplerAttribs(const StaticSamplerDesc& SamDesc, Vk_SHADER_VISIBILITY Visibility) : 
            SamplerDesc(SamDesc),
            ShaderVisibility(Visibility)
        {}
    };
    // Note: sizeof(m_StaticSamplers) == 56 (MS compiler, release x64)
    std::vector<StaticSamplerAttribs, STDAllocatorRawMem<StaticSamplerAttribs> > m_StaticSamplers;

    // Commits descriptor handles for static and mutable variables
    template<bool PerformResourceTransitions>
    void CommitDescriptorHandlesInternal_SM(RenderDeviceVkImpl *pRenderDeviceVk, 
                                            ShaderResourceCacheVk& ResourceCache, 
                                            class CommandContext &Ctx, 
                                            bool IsCompute)const;
    template<bool PerformResourceTransitions>
    // Commits descriptor handles for static, mutable, and dynamic variables
    void CommitDescriptorHandlesInternal_SMD(RenderDeviceVkImpl *pRenderDeviceVk, 
                                            ShaderResourceCacheVk& ResourceCache, 
                                            class CommandContext &Ctx, 
                                            bool IsCompute)const;
#endif
};

}
