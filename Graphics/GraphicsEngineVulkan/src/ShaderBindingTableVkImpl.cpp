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

#include "pch.h"
#include "ShaderBindingTableVkImpl.hpp"
#include "VulkanTypeConversions.hpp"

namespace Diligent
{

ShaderBindingTableVkImpl::ShaderBindingTableVkImpl(IReferenceCounters*           pRefCounters,
                                                   RenderDeviceVkImpl*           pRenderDeviceVk,
                                                   const ShaderBindingTableDesc& Desc,
                                                   bool                          bIsDeviceInternal) :
    TShaderBindingTableBase{pRefCounters, pRenderDeviceVk, Desc, bIsDeviceInternal},
    m_MissShadersOffset{0},
    m_HitGroupsOffset{0},
    m_CallbaleShadersOffset{0},
    m_MissShaderCount{0},
    m_HitGroupCount{0},
    m_CallableShaderCount{0}
{
    ValidateDesc(Desc);

    auto& Props = GetDevice()->GetPhysicalDevice().GetExtProperties().RayTracing;

    m_ShaderGroupHandleSize    = Props.shaderGroupHandleSize;
    m_ShaderGroupBaseAlignment = Props.shaderGroupBaseAlignment;
}

ShaderBindingTableVkImpl::~ShaderBindingTableVkImpl()
{
}

void ShaderBindingTableVkImpl::ValidateDesc(const ShaderBindingTableDesc& Desc) const
{
    auto& Props = GetDevice()->GetPhysicalDevice().GetExtProperties().RayTracing;

    if (Desc.ShaderRecordSize + Props.shaderGroupHandleSize > Props.maxShaderGroupStride)
    {
        LOG_ERROR_AND_THROW("Description of Shader binding table '", (Desc.Name ? Desc.Name : ""),
                            "' is invalid: ShaderRecordSize is too big, max size is: ", Props.maxShaderGroupStride - Props.shaderGroupHandleSize);
    }
}

void ShaderBindingTableVkImpl::Verify() const
{
    // AZ TODO
}

void ShaderBindingTableVkImpl::Reset(const ShaderBindingTableDesc& Desc)
{
    try
    {
        ValidateShaderBindingTableDesc(Desc);
        ValidateDesc(Desc);
    }
    catch (const std::runtime_error&)
    {
        return;
    }

    m_Desc = Desc;

    // free memory
    decltype(m_ShaderRecords) temp;
    std::swap(temp, m_ShaderRecords);

    m_MissShadersOffset     = 0;
    m_HitGroupsOffset       = 0;
    m_CallbaleShadersOffset = 0;
    m_MissShaderCount       = 0;
    m_HitGroupCount         = 0;
    m_CallableShaderCount   = 0;
}

void ShaderBindingTableVkImpl::ResetHitGroups(Uint32 HitShadersPerInstance)
{
    // AZ TODO
}

void ShaderBindingTableVkImpl::BindRayGenShader(const char* ShaderGroupName, const void* Data, Uint32 DataSize)
{
    // AZ TODO
}

void ShaderBindingTableVkImpl::BindMissShader(const char* ShaderGroupName, Uint32 MissIndex, const void* Data, Uint32 DataSize)
{
    // AZ TODO
}

void ShaderBindingTableVkImpl::BindHitGroup(ITopLevelAS* pTLAS,
                                            const char*  InstanceName,
                                            const char*  GeometryName,
                                            Uint32       RayOffsetInHitGroupIndex,
                                            const char*  ShaderGroupName,
                                            const void*  Data,
                                            Uint32       DataSize)
{
    // AZ TODO
}

void ShaderBindingTableVkImpl::BindHitGroups(ITopLevelAS* pTLAS,
                                             const char*  InstanceName,
                                             Uint32       RayOffsetInHitGroupIndex,
                                             const char*  ShaderGroupName,
                                             const void*  Data,
                                             Uint32       DataSize)
{
    // AZ TODO
}

void ShaderBindingTableVkImpl::BindCallableShader(Uint32      Index,
                                                  const char* ShaderName,
                                                  const void* Data,
                                                  Uint32      DataSize)
{
    // AZ TODO
}

void ShaderBindingTableVkImpl::BindAll(const BindAllAttribs& Attribs)
{
    // AZ TODO
}

void ShaderBindingTableVkImpl::GetVkStridedBufferRegions(VkStridedBufferRegionKHR& RaygenShaderBindingTable,
                                                         VkStridedBufferRegionKHR& MissShaderBindingTable,
                                                         VkStridedBufferRegionKHR& HitShaderBindingTable,
                                                         VkStridedBufferRegionKHR& CallableShaderBindingTable)
{
    auto& Props = GetDevice()->GetPhysicalDevice().GetExtProperties().RayTracing;

    const VkDeviceSize Stride = m_Desc.ShaderRecordSize + Props.shaderGroupHandleSize;
    VERIFY_EXPR(Stride <= Props.maxShaderGroupStride);

    RaygenShaderBindingTable.buffer = m_pBuffer->GetVkBuffer();
    RaygenShaderBindingTable.offset = 0;
    RaygenShaderBindingTable.size   = Stride;
    RaygenShaderBindingTable.stride = Stride;

    if (m_MissShaderCount > 0)
    {
        MissShaderBindingTable.buffer = m_pBuffer->GetVkBuffer();
        MissShaderBindingTable.offset = m_MissShadersOffset;
        MissShaderBindingTable.size   = Stride * m_MissShaderCount;
        MissShaderBindingTable.stride = Stride;
    }
    else
        MissShaderBindingTable = {};

    if (m_HitGroupCount > 0)
    {
        HitShaderBindingTable.buffer = m_pBuffer->GetVkBuffer();
        HitShaderBindingTable.offset = m_HitGroupsOffset;
        HitShaderBindingTable.size   = Stride * m_HitGroupCount;
        HitShaderBindingTable.stride = Stride;
    }
    else
        HitShaderBindingTable = {};

    if (m_CallableShaderCount > 0)
    {
        CallableShaderBindingTable.buffer = m_pBuffer->GetVkBuffer();
        CallableShaderBindingTable.offset = m_CallbaleShadersOffset;
        CallableShaderBindingTable.size   = Stride * m_CallableShaderCount;
        CallableShaderBindingTable.stride = Stride;
    }
    else
        CallableShaderBindingTable = {};
}

} // namespace Diligent
