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
/// Definition of the Diligent::ShaderBindingTableVkImpl class

#include "EngineVkImplTraits.hpp"
#include "ShaderBindingTableBase.hpp"
#include "BufferVkImpl.hpp"
#include "TopLevelASVkImpl.hpp"
#include "PipelineStateVkImpl.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "VulkanUtilities/VulkanObjectWrappers.hpp"

namespace Diligent
{

class ShaderBindingTableVkImpl final : public ShaderBindingTableBase<EngineVkImplTraits>
{
public:
    using TShaderBindingTableBase = ShaderBindingTableBase<EngineVkImplTraits>;

    ShaderBindingTableVkImpl(IReferenceCounters*           pRefCounters,
                             RenderDeviceVkImpl*           pRenderDeviceVk,
                             const ShaderBindingTableDesc& Desc,
                             bool                          bIsDeviceInternal = false);
    ~ShaderBindingTableVkImpl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_ShaderBindingTableVk, TShaderBindingTableBase)

    virtual const BindingTableVk& DILIGENT_CALL_TYPE GetVkBindingTable() const override final { return m_VkBindingTable; }

    using BindingTable = TShaderBindingTableBase::BindingTable;
    void GetData(BufferVkImpl*& pSBTBufferVk,
                 BindingTable&  RayGenShaderRecord,
                 BindingTable&  MissShaderTable,
                 BindingTable&  HitGroupTable,
                 BindingTable&  CallableShaderTable)
    {
        TShaderBindingTableBase::GetData(pSBTBufferVk, RayGenShaderRecord, MissShaderTable, HitGroupTable, CallableShaderTable);

        // clang-format off
        m_VkBindingTable.RaygenShader   = {pSBTBufferVk->GetVkDeviceAddress() + RayGenShaderRecord.Offset,  RayGenShaderRecord.Stride,  RayGenShaderRecord.Size };
        m_VkBindingTable.MissShader     = {pSBTBufferVk->GetVkDeviceAddress() + MissShaderTable.Offset,     MissShaderTable.Stride,     MissShaderTable.Size    };
        m_VkBindingTable.HitShader      = {pSBTBufferVk->GetVkDeviceAddress() + HitGroupTable.Offset,       HitGroupTable.Stride,       HitGroupTable.Size      };
        m_VkBindingTable.CallableShader = {pSBTBufferVk->GetVkDeviceAddress() + CallableShaderTable.Offset, CallableShaderTable.Stride, CallableShaderTable.Size};

        const auto ShaderGroupBaseAlignment = m_pDevice->GetProperties().ShaderGroupBaseAlignment;
        VERIFY_EXPR(m_VkBindingTable.RaygenShader.deviceAddress   % ShaderGroupBaseAlignment == 0);
        VERIFY_EXPR(m_VkBindingTable.MissShader.deviceAddress     % ShaderGroupBaseAlignment == 0);
        VERIFY_EXPR(m_VkBindingTable.HitShader.deviceAddress      % ShaderGroupBaseAlignment == 0);
        VERIFY_EXPR(m_VkBindingTable.CallableShader.deviceAddress % ShaderGroupBaseAlignment == 0);
        // clang-format on
    }

private:
    BindingTableVk m_VkBindingTable = {};
};

} // namespace Diligent
