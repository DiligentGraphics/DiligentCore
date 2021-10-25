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
/// Definition of the Diligent::IArchiveSource interface and related data structures

#include "../../../Primitives/interface/Object.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// clang-format on

// {49C98F50-CD7D-4F3D-9432-42DD531A7B1D}
static const INTERFACE_ID IID_ArchiveSource =
    {0x49c98f50, 0xcd7d, 0x4f3d, {0x94, 0x32, 0x42, 0xdd, 0x53, 0x1a, 0x7b, 0x1d}};

#define DILIGENT_INTERFACE_NAME IArchiveSource
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IArchiveSourceInclusiveMethods \
    IObjectInclusiveMethods;           \
    IArchiveSourceMethods ArchiveSource

// clang-format off


/// Archive source interface
DILIGENT_BEGIN_INTERFACE(IArchiveSource, IObject)
{
    // AZ TODO
    VIRTUAL Bool METHOD(Read)(THIS_
                              Uint64 Pos,
                              void*  pData,
                              Uint64 Size) PURE;
    
    // AZ TODO
    VIRTUAL Uint64 METHOD(GetSize)(THIS) PURE;
    
    // AZ TODO
    VIRTUAL Uint64 METHOD(GetPos)(THIS) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

// clang-format off

#    define IArchiveSource_Read(This, ...)  CALL_IFACE_METHOD(ArchiveSource, Read,    This, __VA_ARGS__)
#    define IArchiveSource_GetSize(This)    CALL_IFACE_METHOD(ArchiveSource, GetSize, This)
#    define IArchiveSource_GetPos(This)     CALL_IFACE_METHOD(ArchiveSource, GetPos,  This)

// clang-format on

#endif

DILIGENT_END_NAMESPACE
