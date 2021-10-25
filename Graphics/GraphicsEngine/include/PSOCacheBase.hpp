/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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
/// Implementation of the Diligent::PSOCacheBase template class

#include "PSOCache.h"
#include "DeviceObjectBase.hpp"

namespace Diligent
{

/// Validates PSO cache create info and throws an exception in case of an error.
void ValidatePSOCacheCreateInfo(const PSOCacheCreateInfo& CreateInfo) noexcept(false);

/// Template class implementing base functionality of the pipeline state cache object

/// \tparam EngineImplTraits - Engine implementation type traits.
template <typename EngineImplTraits>
class PSOCacheBase : public DeviceObjectBase<typename EngineImplTraits::PSOCacheInterface, typename EngineImplTraits::RenderDeviceImplType, PSOCacheDesc>
{
public:
    // Base interface that this class inherits (IPSOCacheD3D12, IPSOCacheVk, etc.).
    using BaseInterface = typename EngineImplTraits::PSOCacheInterface;

    // Render device implementation type (RenderDeviceD3D12Impl, RenderDeviceVkImpl, etc.).
    using RenderDeviceImplType = typename EngineImplTraits::RenderDeviceImplType;

    using TDeviceObjectBase = DeviceObjectBase<BaseInterface, RenderDeviceImplType, PSOCacheDesc>;

    /// \param pRefCounters         - Reference counters object that controls the lifetime of this PSO cache.
    /// \param pDevice              - Pointer to the device.
    /// \param CreateInfo           - PSO cache create info.
    /// \param bIsDeviceInternal    - Flag indicating if the PSO cache is an internal device object and
    ///							      must not keep a strong reference to the device.
    PSOCacheBase(IReferenceCounters*       pRefCounters,
                 RenderDeviceImplType*     pDevice,
                 const PSOCacheCreateInfo& CreateInfo,
                 bool                      bIsDeviceInternal) :
        TDeviceObjectBase{pRefCounters, pDevice, CreateInfo.Desc, bIsDeviceInternal}
    {
        ValidatePSOCacheCreateInfo(CreateInfo);
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_PSOCache, TDeviceObjectBase)
};

} // namespace Diligent
