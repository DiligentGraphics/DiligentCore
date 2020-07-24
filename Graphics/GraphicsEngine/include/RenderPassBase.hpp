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
/// Implementation of the Diligent::RenderPassBase template class

#include "RenderPass.h"
#include "DeviceObjectBase.hpp"
#include "RenderDeviceBase.hpp"

namespace Diligent
{

void ValidateRenderPassDesc(const RenderPassDesc& Desc);

/// Template class implementing base functionality for the render pass object.

/// \tparam BaseInterface - base interface that this class will inheret
///                         (Diligent::IRenderPassVk).
/// \tparam RenderDeviceImplType - type of the render device implementation
///                                (Diligent::RenderDeviceD3D11Impl, Diligent::RenderDeviceD3D12Impl,
///                                 Diligent::RenderDeviceGLImpl, or Diligent::RenderDeviceVkImpl)
template <class BaseInterface, class RenderDeviceImplType>
class RenderPassBase : public DeviceObjectBase<BaseInterface, RenderDeviceImplType, RenderPassDesc>
{
public:
    using TDeviceObjectBase = DeviceObjectBase<BaseInterface, RenderDeviceImplType, RenderPassDesc>;

    /// \param pRefCounters      - reference counters object that controls the lifetime of this render pass.
    /// \param pDevice           - pointer to the device.
    /// \param Desc              - Render pass description.
    /// \param bIsDeviceInternal - flag indicating if the RenderPass is an internal device object and
    ///							   must not keep a strong reference to the device.
    RenderPassBase(IReferenceCounters*   pRefCounters,
                   RenderDeviceImplType* pDevice,
                   const RenderPassDesc& Desc,
                   bool                  bIsDeviceInternal = false) :
        TDeviceObjectBase{pRefCounters, pDevice, Desc, bIsDeviceInternal}
    {
        ValidateRenderPassDesc(Desc);

        if (Desc.AttachmentCount != 0)
        {
            auto* pAttachments =
                ALLOCATE(GetRawAllocator(), "Memory for RenderPassAttachmentDesc array", RenderPassAttachmentDesc, Desc.AttachmentCount);
            this->m_Desc.pAttachments = pAttachments;
            for (Uint32 i = 0; i < Desc.AttachmentCount; ++i)
            {
                pAttachments[i] = Desc.pAttachments[i];
            }
        }

        if (Desc.SubpassCount != 0)
        {
            auto* pSubpasses =
                ALLOCATE(GetRawAllocator(), "Memory for SubpassDesc array", SubpassDesc, Desc.SubpassCount);
            this->m_Desc.pSubpasses = pSubpasses;
            for (Uint32 i = 0; i < Desc.SubpassCount; ++i)
            {
                pSubpasses[i] = Desc.pSubpasses[i];
            }
        }

        if (Desc.DependencyCount != 0)
        {
            auto* pDependencies =
                ALLOCATE(GetRawAllocator(), "Memory for SubpassDependencyDesc array", SubpassDependencyDesc, Desc.DependencyCount);
            this->m_Desc.pDependencies = pDependencies;
            for (Uint32 i = 0; i < Desc.DependencyCount; ++i)
            {
                pDependencies[i] = Desc.pDependencies[i];
            }
        }
    }

    ~RenderPassBase()
    {
        auto& RawAllocator = GetRawAllocator();
        if (this->m_Desc.pAttachments != nullptr)
            RawAllocator.Free(const_cast<RenderPassAttachmentDesc*>(this->m_Desc.pAttachments));
        if (this->m_Desc.pSubpasses != nullptr)
            RawAllocator.Free(const_cast<SubpassDesc*>(this->m_Desc.pSubpasses));
        if (this->m_Desc.pDependencies != nullptr)
            RawAllocator.Free(const_cast<SubpassDependencyDesc*>(this->m_Desc.pDependencies));
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_RenderPass, TDeviceObjectBase)
};

} // namespace Diligent
