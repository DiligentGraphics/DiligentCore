/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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
/// Definition of the Diligent::BottomLevelASVkImpl, Diligent::TopLevelASVkImpl, Diligent::ShaderBindingTableVkImpl classes

#include "RenderDeviceVk.h"
#include "RenderDeviceVkImpl.hpp"
#include "RayTracingVk.h"
#include "RayTracingBase.hpp"
#include "VulkanUtilities/VulkanObjectWrappers.hpp"

namespace Diligent
{

class BottomLevelASVkImpl final : public BottomLevelASBase<IBottomLevelASVk, RenderDeviceVkImpl>
{
public:
    using TBottomLevelASBase = BottomLevelASBase<IBottomLevelASVk, RenderDeviceVkImpl>;

    BottomLevelASVkImpl(IReferenceCounters*      pRefCounters,
                        RenderDeviceVkImpl*      pRenderDeviceVk,
                        const BottomLevelASDesc& Desc,
                        bool                     bIsDeviceInternal = false);
    ~BottomLevelASVkImpl();

    virtual ScratchBufferSizes DILIGENT_CALL_TYPE GetScratchBufferSizes() const override { return m_ScratchSize; }

    virtual VkAccelerationStructureKHR DILIGENT_CALL_TYPE GetVkBLAS() const override { return m_VulkanBLAS; }

    virtual VkDeviceAddress DILIGENT_CALL_TYPE GetVkDeviceAddress() const override { return m_DeviceAddress; }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_BottomLevelASVk, TBottomLevelASBase);

private:
    VkDeviceAddress                         m_DeviceAddress;
    VulkanUtilities::AccelStructWrapper     m_VulkanBLAS;
    VulkanUtilities::VulkanMemoryAllocation m_MemoryAllocation;
    ScratchBufferSizes                      m_ScratchSize;
};


class TopLevelASVkImpl final : public TopLevelASBase<ITopLevelASVk, RenderDeviceVkImpl>
{
public:
    using TTopLevelASBase = TopLevelASBase<ITopLevelASVk, RenderDeviceVkImpl>;

    TopLevelASVkImpl(IReferenceCounters*   pRefCounters,
                     RenderDeviceVkImpl*   pRenderDeviceVk,
                     const TopLevelASDesc& Desc,
                     bool                  bIsDeviceInternal = false);
    ~TopLevelASVkImpl();

    virtual ScratchBufferSizes DILIGENT_CALL_TYPE GetScratchBufferSizes() const override { return m_ScratchSize; }

    virtual VkAccelerationStructureKHR DILIGENT_CALL_TYPE GetVkTLAS() const override { return m_VulkanTLAS; }

    virtual VkDeviceAddress DILIGENT_CALL_TYPE GetVkDeviceAddress() const override { return m_DeviceAddress; }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_TopLevelASVk, TTopLevelASBase);

private:
    VkDeviceAddress                         m_DeviceAddress;
    VulkanUtilities::AccelStructWrapper     m_VulkanTLAS;
    VulkanUtilities::VulkanMemoryAllocation m_MemoryAllocation;
    ScratchBufferSizes                      m_ScratchSize;
};


class ShaderBindingTableVkImpl final : public ShaderBindingTableBase<IShaderBindingTableVk, RenderDeviceVkImpl>
{
public:
    using TShaderBindingTableBase = ShaderBindingTableBase<IShaderBindingTableVk, RenderDeviceVkImpl>;

    ShaderBindingTableVkImpl(IReferenceCounters*           pRefCounters,
                             RenderDeviceVkImpl*           pRenderDeviceVk,
                             const ShaderBindingTableDesc& Desc,
                             bool                          bIsDeviceInternal = false);
    ~ShaderBindingTableVkImpl();

    virtual void DILIGENT_CALL_TYPE Verify() const override;

    virtual void DILIGENT_CALL_TYPE Reset(const ShaderBindingTableDesc& Desc) override;

    virtual void DILIGENT_CALL_TYPE ResetHitGroups(Uint32 HitShadersPerInstance) override;

    virtual void DILIGENT_CALL_TYPE BindRayGenShader(const char* ShaderGroupName) override;

    virtual void DILIGENT_CALL_TYPE BindRayGenShader(const char* ShaderGroupName, const void* Data, Uint32 DataSize) override;

    virtual void DILIGENT_CALL_TYPE BindMissShader(const char* ShaderGroupName, Uint32 MissIndex) override;

    virtual void DILIGENT_CALL_TYPE BindMissShader(const char* ShaderGroupName, Uint32 MissIndex, const void* Data, Uint32 DataSize) override;

    virtual void DILIGENT_CALL_TYPE BindHitGroup(ITopLevelAS* pTLAS,
                                                 const char*  InstanceName,
                                                 const char*  GeometryName,
                                                 Uint32       RayOffsetInHitGroupIndex,
                                                 const char*  ShaderGroupName) override;

    virtual void DILIGENT_CALL_TYPE BindHitGroup(ITopLevelAS* pTLAS,
                                                 const char*  InstanceName,
                                                 const char*  GeometryName,
                                                 Uint32       RayOffsetInHitGroupIndex,
                                                 const char*  ShaderGroupName,
                                                 const void*  Data,
                                                 Uint32       DataSize) override;

    virtual void DILIGENT_CALL_TYPE BindHitGroup(ITopLevelAS* pTLAS,
                                                 const char*  InstanceName,
                                                 Uint32       RayOffsetInHitGroupIndex,
                                                 const char*  ShaderGroupName) override;

    virtual void DILIGENT_CALL_TYPE BindHitGroup(ITopLevelAS* pTLAS,
                                                 const char*  InstanceName,
                                                 Uint32       RayOffsetInHitGroupIndex,
                                                 const char*  ShaderGroupName,
                                                 const void*  Data,
                                                 Uint32       DataSize) override;

    virtual void DILIGENT_CALL_TYPE BindCallableShader(Uint32      Index,
                                                       const char* ShaderName) override;

    virtual void DILIGENT_CALL_TYPE BindCallableShader(Uint32      Index,
                                                       const char* ShaderName,
                                                       const void* Data,
                                                       Uint32      DataSize) override;

    virtual void DILIGENT_CALL_TYPE BindAll(const BindAllAttribs& Attribs) override;

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_ShaderBindingTableVk, TShaderBindingTableBase);

private:
};

} // namespace Diligent
