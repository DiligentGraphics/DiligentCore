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
/// Definition of the Diligent::ShaderBindingTableVkImpl class

#include "BufferVkImpl.hpp"
#include "RenderDeviceVk.h"
#include "RenderDeviceVkImpl.hpp"
#include "ShaderBindingTableVk.h"
#include "ShaderBindingTableBase.hpp"
#include "VulkanUtilities/VulkanObjectWrappers.hpp"

namespace Diligent
{

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

    virtual void DILIGENT_CALL_TYPE BindRayGenShader(const char* ShaderGroupName, const void* Data, Uint32 DataSize) override;

    virtual void DILIGENT_CALL_TYPE BindMissShader(const char* ShaderGroupName, Uint32 MissIndex, const void* Data, Uint32 DataSize) override;

    virtual void DILIGENT_CALL_TYPE BindHitGroup(ITopLevelAS* pTLAS,
                                                 const char*  InstanceName,
                                                 const char*  GeometryName,
                                                 Uint32       RayOffsetInHitGroupIndex,
                                                 const char*  ShaderGroupName,
                                                 const void*  Data,
                                                 Uint32       DataSize) override;

    virtual void DILIGENT_CALL_TYPE BindHitGroups(ITopLevelAS* pTLAS,
                                                  const char*  InstanceName,
                                                  Uint32       RayOffsetInHitGroupIndex,
                                                  const char*  ShaderGroupName,
                                                  const void*  Data,
                                                  Uint32       DataSize) override;

    virtual void DILIGENT_CALL_TYPE BindCallableShader(Uint32      Index,
                                                       const char* ShaderName,
                                                       const void* Data,
                                                       Uint32      DataSize) override;

    virtual void DILIGENT_CALL_TYPE BindAll(const BindAllAttribs& Attribs) override;

    virtual void DILIGENT_CALL_TYPE GetVkStridedBufferRegions(VkStridedBufferRegionKHR& RaygenShaderBindingTable,
                                                              VkStridedBufferRegionKHR& MissShaderBindingTable,
                                                              VkStridedBufferRegionKHR& HitShaderBindingTable,
                                                              VkStridedBufferRegionKHR& CallableShaderBindingTable) override;

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_ShaderBindingTableVk, TShaderBindingTableBase);

private:
    void ValidateDesc(const ShaderBindingTableDesc& Desc) const;

private:
    RefCntAutoPtr<BufferVkImpl> m_pBuffer;
    std::vector<Uint32>         m_ShaderRecords;

    Uint32 m_MissShadersOffset        = 0;
    Uint32 m_HitGroupsOffset          = 0;
    Uint32 m_CallbaleShadersOffset    = 0;
    Uint32 m_MissShaderCount          = 0;
    Uint32 m_HitGroupCount            = 0;
    Uint32 m_CallableShaderCount      = 0;
    Uint32 m_ShaderGroupHandleSize    = 0;
    Uint32 m_ShaderGroupBaseAlignment = 0;
};

} // namespace Diligent
