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

// clang-format off

/// \file
/// Definition of the Diligent::IPSOCache interface and related data structures

#include "../../../Primitives/interface/Object.h"
#include "../../../Primitives/interface/DataBlob.h"
#include "../../../Platforms/interface/PlatformDefinitions.h"
#include "GraphicsTypes.h"
#include "DeviceObject.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

/// Pipeline state pbject cache description
struct PSOCacheDesc DILIGENT_DERIVE(DeviceObjectAttribs)

    // ImmediateContextMask ?
};
typedef struct PSOCacheDesc PSOCacheDesc;

/// Pipeline state pbject cache create info
struct PSOCacheCreateInfo
{
    PSOCacheDesc Desc;

    // AZ TODO
    /// All fields can be null to create empty cache and use it to build PSO cache
    const void* pCacheData    DEFAULT_INITIALIZER(nullptr);

    Uint32      CacheDataSize DEFAULT_INITIALIZER(0);
};
typedef struct PSOCacheCreateInfo PSOCacheCreateInfo;

// clang-format on

// {6AC86F22-FFF4-493C-8C1F-C539D934F4BC}
static const INTERFACE_ID IID_PSOCache =
    {0x6ac86f22, 0xfff4, 0x493c, {0x8c, 0x1f, 0xc5, 0x39, 0xd9, 0x34, 0xf4, 0xbc}};


#define DILIGENT_INTERFACE_NAME IPSOCache
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IPSOCacheInclusiveMethods  \
    IDeviceObjectInclusiveMethods; \
    IPSOCacheMethods PSOCache

// clang-format off

/// Pipeline state object cache interface
DILIGENT_BEGIN_INTERFACE(IPSOCache, IDeviceObject)
{
    /// AZ TODO
    VIRTUAL void METHOD(GetData)(THIS_
                                 IDataBlob** ppBlob) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

#    define IPSOCache_GetData(This, ...)  CALL_IFACE_METHOD(PSOCache, GetData, This, __VA_ARGS__)

#endif

DILIGENT_END_NAMESPACE
