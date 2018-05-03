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
/// Declaration of Diligent::PipelineStateVkImpl class

#include "RenderDeviceVk.h"
#include "PipelineStateVk.h"
#include "PipelineStateBase.h"
#include "PipelineLayout.h"
#include "ShaderResourceLayoutVk.h"
#include "AdaptiveFixedBlockAllocator.h"
#include "VulkanUtilities/VulkanObjectWrappers.h"

/// Namespace for the Direct3D11 implementation of the graphics engine
namespace Diligent
{

class FixedBlockMemoryAllocator;
/// Implementation of the Diligent::IRenderDeviceVk interface
class PipelineStateVkImpl : public PipelineStateBase<IPipelineStateVk, IRenderDeviceVk>
{
public:
    typedef PipelineStateBase<IPipelineStateVk, IRenderDeviceVk> TPipelineStateBase;

    PipelineStateVkImpl( IReferenceCounters *pRefCounters, class RenderDeviceVkImpl *pDeviceVk, const PipelineStateDesc &PipelineDesc );
    ~PipelineStateVkImpl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface );
   
    virtual void BindShaderResources( IResourceMapping *pResourceMapping, Uint32 Flags )override;

    virtual void CreateShaderResourceBinding( IShaderResourceBinding **ppShaderResourceBinding )override;

    virtual bool IsCompatibleWith(const IPipelineState *pPSO)const override final;

    virtual VkRenderPass GetVkRenderPass()const override final{return m_RenderPass;}

    virtual VkPipeline GetVkPipeline()const override final { return m_Pipeline; }

    //ShaderResourceCacheVk* CommitAndTransitionShaderResources(IShaderResourceBinding *pShaderResourceBinding, 
    //                                                             class CommandContext &Ctx,
    //                                                             bool CommitResources,
    //                                                             bool TransitionResources)const;
    
    //const RootSignature& GetRootSignature()const{return m_RootSig;}
    
    //const ShaderResourceLayoutVk& GetShaderResLayout(SHADER_TYPE ShaderType)const;
    
    //bool dbgContainsShaderResources()const;

    //IMemoryAllocator &GetResourceCacheDataAllocator(){return m_ResourceCacheDataAllocator;}
    //IMemoryAllocator &GetShaderResourceLayoutDataAllocator(Uint32 ActiveShaderInd)
    //{
    //    VERIFY_EXPR(ActiveShaderInd < m_NumShaders);
    //    auto *pAllocator = m_ResLayoutDataAllocators.GetAllocator(ActiveShaderInd);
    //    return pAllocator != nullptr ? *pAllocator : GetRawAllocator();
    //}

    //IShaderVariable *GetDummyShaderVar(){return &m_DummyVar;}

private:

    void CreateRenderPass(const VulkanUtilities::VulkanLogicalDevice &LogicalDevice);

#if 0
    void ParseShaderResourceLayout(IShader *pShader);

    /// Vk device
    RootSignature m_RootSig;
    DummyShaderVariable m_DummyVar;
    
    // Looks like there may be a bug in msvc: when allocators are declared as 
    // an array and if an exception is thrown from constructor, the app crashes
    class ResLayoutDataAllocators
    {
    public:
        ~ResLayoutDataAllocators()
        {
            for(size_t i=0; i < _countof(m_pAllocators); ++i)
                if(m_pAllocators[i] != nullptr)
                    DESTROY_POOL_OBJECT(m_pAllocators[i]);
        }
        void Init(Uint32 NumActiveShaders, Uint32 SRBAllocationGranularity)
        {
            VERIFY_EXPR(NumActiveShaders <= _countof(m_pAllocators) );
            for(Uint32 i=0; i < NumActiveShaders; ++i)
                m_pAllocators[i] = NEW_POOL_OBJECT(AdaptiveFixedBlockAllocator, "Shader resource layout data allocator", GetRawAllocator(), SRBAllocationGranularity);
        }
        AdaptiveFixedBlockAllocator *GetAllocator(Uint32 ActiveShaderInd)
        {
            VERIFY_EXPR(ActiveShaderInd < _countof(m_pAllocators) );
            return m_pAllocators[ActiveShaderInd];
        }
    private:
        AdaptiveFixedBlockAllocator *m_pAllocators[5] = {};
    }m_ResLayoutDataAllocators; // Allocators must be defined before default SRB

    ShaderResourceLayoutVk* m_pShaderResourceLayouts[6] = {};
    AdaptiveFixedBlockAllocator m_ResourceCacheDataAllocator; // Use separate allocator for every shader stage

    // Do not use strong reference to avoid cyclic references
    // Default SRB must be defined after allocators
    std::unique_ptr<class ShaderResourceBindingVkImpl, STDDeleter<ShaderResourceBindingVkImpl, FixedBlockMemoryAllocator> > m_pDefaultShaderResBinding;
#endif
    VulkanUtilities::RenderPassWrapper m_RenderPass;
    VulkanUtilities::PipelineWrapper m_Pipeline;
    VulkanUtilities::PipelineLayoutWrapper m_PipelineLayout;

};

}
