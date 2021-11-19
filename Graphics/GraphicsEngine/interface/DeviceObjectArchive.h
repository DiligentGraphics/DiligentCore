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
/// Definition of the Diligent::IDeviceObjectArchive interface and related data structures

#include "../../../Primitives/interface/Object.h"
#include "../../../Primitives/interface/DataBlob.h"
#include "../../../Primitives/interface/FileStream.h"
#include "../../../Platforms/interface/PlatformDefinitions.h"
#include "GraphicsTypes.h"
#include "Archive.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// clang-format on

// {9152EA56-ED52-4857-AF05-99A9432ABCB6}
static const INTERFACE_ID IID_DeviceObjectArchive =
    {0x9152ea56, 0xed52, 0x4857, {0xaf, 0x5, 0x99, 0xa9, 0x43, 0x2a, 0xbc, 0xb6}};

#define DILIGENT_INTERFACE_NAME IDeviceObjectArchive
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

// clang-format off

#define IDeviceObjectArchiveInclusiveMethods \
    IDeviceObjectInclusiveMethods \
    /*IDeviceObjectArchiveMethods DeviceObjectArchive*/

#if DILIGENT_CPP_INTERFACE

/// Device object archive interface
DILIGENT_BEGIN_INTERFACE(IDeviceObjectArchive, IObject)
{
    // AZ TODO
    //VIRTUAL Bool METHOD(PrefetchPipelineStates)(THIS_
    //                                            const Char* const* pNames,
    //                                            Uint32             NameCount) PURE;
};
DILIGENT_END_INTERFACE

#endif // DILIGENT_CPP_INTERFACE


#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

typedef struct IDeviceObjectArchiveVtbl
{
    IDeviceObjectArchiveInclusiveMethods;
} IDeviceObjectArchiveVtbl;

typedef struct IDeviceObjectArchive
{
    struct IDeviceObjectArchiveVtbl* pVtbl;
} IDeviceObjectArchive;

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
