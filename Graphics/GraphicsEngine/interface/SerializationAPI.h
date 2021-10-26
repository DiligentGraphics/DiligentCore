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
/// Definition of the Diligent::ISerializationAPI interface and related data structures

#include "../../../Primitives/interface/DataBlob.h"
#include "PipelineResourceSignature.h"
#include "PipelineState.h"
#include "DeviceObjectArchive.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// clang-format on

// {ACB3F67A-CE3B-4212-9592-879122D3C191}
static const INTERFACE_ID IID_SerializationAPI =
    {0xacb3f67a, 0xce3b, 0x4212, {0x95, 0x92, 0x87, 0x91, 0x22, 0xd3, 0xc1, 0x91}};

#define DILIGENT_INTERFACE_NAME ISerializationAPI
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define ISerializationAPIInclusiveMethods \
    IDeviceObjectInclusiveMethods;        \
    ISerializationAPIMethods SerializationAPI

// clang-format off


/// Serialization API interface
DILIGENT_BEGIN_INTERFACE(ISerializationAPI, IObject)
{
    // AZ TODO
    VIRTUAL void METHOD(CreateDeviceObjectArchive)(THIS_
                                                   IArchiveSource*        pSource,
                                                   IDeviceObjectArchive** ppArchive) PURE;
    
    // AZ TODO
    VIRTUAL void METHOD(CreateArchiveSourceFromFile)(THIS_
                                                     const Char*      Path,
                                                     IArchiveSource** ppSource) PURE;
    
    // AZ TODO
    VIRTUAL void METHOD(CreateArchiveSourceFromBlob)(THIS_
                                                     IDataBlob*       pBlob,
                                                     IArchiveSource** ppSource) PURE;

    /// Resource signatures used by the PSO will be unpacked from the same archive.
    VIRTUAL void METHOD(UnpackPipelineState)(THIS_
                                             const PipelineStateUnpackInfo REF DeArchiveInfo,
                                             IPipelineState**                  ppPSO) PURE;

    VIRTUAL void METHOD(UnpackResourceSignature)(THIS_
                                                 const ResourceSignatureUnpackInfo REF DeArchiveInfo,
                                                 IPipelineResourceSignature**          ppSignature) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

#    define ISerializationAPI_CreateDeviceObjectArchive(This, ...)    CALL_IFACE_METHOD(SerializationAPI, CreateDeviceObjectArchive,   This, __VA_ARGS__)
#    define ISerializationAPI_CreateArchiveSourceFromFile(This, ...)  CALL_IFACE_METHOD(SerializationAPI, CreateArchiveSourceFromFile, This, __VA_ARGS__)
#    define ISerializationAPI_CreateArchiveSourceFromBlob(This, ...)  CALL_IFACE_METHOD(SerializationAPI, CreateArchiveSourceFromBlob, This, __VA_ARGS__)
#    define ISerializationAPI_UnpackPipelineState(This, ...)          CALL_IFACE_METHOD(SerializationAPI, UnpackPipelineState,         This, __VA_ARGS__)
#    define ISerializationAPI_UnpackResourceSignature(This, ...)      CALL_IFACE_METHOD(SerializationAPI, UnpackResourceSignature,     This, __VA_ARGS__)

#endif

DILIGENT_END_NAMESPACE
