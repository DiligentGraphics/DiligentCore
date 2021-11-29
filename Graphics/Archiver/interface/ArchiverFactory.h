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
/// Defines Diligent::IArchiverFactory interface

#include "../../../Primitives/interface/Object.h"
#include "Archiver.h"
#include "SerializationDevice.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {F20B91EB-BDE3-4615-81CC-F720AA32410E}
static const INTERFACE_ID IID_ArchiverFactory =
    {0xf20b91eb, 0xbde3, 0x4615, {0x81, 0xcc, 0xf7, 0x20, 0xaa, 0x32, 0x41, 0xe}};

#define DILIGENT_INTERFACE_NAME IArchiverFactory
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IArchiverFactoryInclusiveMethods \
    IObjectInclusiveMethods;             \
    IArchiverFactoryMethods ArchiverFactory

// clang-format off

/// Defines the methods to manipulate an archiver factory object
DILIGENT_BEGIN_INTERFACE(IArchiverFactory, IObject)
{
    /// Creates a serialization device.
    VIRTUAL void METHOD(CreateSerializationDevice)(THIS_
                                                   const SerializationDeviceCreateInfo REF CreateInfo,
                                                   ISerializationDevice**                  ppDevice) PURE;

    /// Creates an archiver.
    VIRTUAL void METHOD(CreateArchiver)(THIS_
                                        ISerializationDevice* pDevice,
                                        IArchiver**           ppArchiver) PURE;

    /// Creates a default shader source input stream factory

    /// \param [in]  SearchDirectories           - Semicolon-separated list of search directories.
    /// \param [out] ppShaderSourceStreamFactory - Memory address where the pointer to the shader source stream factory will be written.
    VIRTUAL void METHOD(CreateDefaultShaderSourceStreamFactory)(
                        THIS_
                        const Char*                              SearchDirectories,
                        struct IShaderSourceInputStreamFactory** ppShaderSourceFactory) CONST PURE;


    /// Remove device specific data from archive and write new archive to the stream.

    /// \param [in]  pSrcArchive - Source archive from which device specific data will be removed.
    /// \param [in]  DeviceFlags - Combination of device types that will be removed.
    /// \param [in]  pStream     - Destination file stream.
    VIRTUAL Bool METHOD(RemoveDeviceData)(THIS_
                                          IArchive*                 pSrcArchive,
                                          ARCHIVE_DEVICE_DATA_FLAGS DeviceFlags,
                                          IFileStream*              pStream) CONST PURE;

    /// Copy device specific data from source archive to destination and write new archive to the stream.

    /// \param [in]  pSrcArchive    - Source archive to which new device specific data will be added.
    /// \param [in]  DeviceFlags    - Combination of device types that will be copied.
    /// \param [in]  pDeviceArchive - Archive which contains same shared data and device specific data.
    /// \param [in]  pStream        - Destination file stream.
    VIRTUAL Bool METHOD(AppendDeviceData)(THIS_
                                          IArchive*                 pSrcArchive,
                                          ARCHIVE_DEVICE_DATA_FLAGS DeviceFlags,
                                          IArchive*                 pDeviceArchive,
                                          IFileStream*              pStream) CONST PURE;

    /// Print archive content for debuging and validating.
    VIRTUAL Bool METHOD(PrintArchiveContent)(THIS_
                                             IArchive* pArchive) CONST PURE;

};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

#    define IArchiverFactory_CreateArchiver(This, ...)                          CALL_IFACE_METHOD(ArchiverFactory, CreateArchiver,                         This, __VA_ARGS__)
#    define IArchiverFactory_CreateSerializationDevice(This, ...)               CALL_IFACE_METHOD(ArchiverFactory, CreateSerializationDevice,              This, __VA_ARGS__)
#    define IArchiverFactory_CreateDefaultShaderSourceStreamFactory(This, ...)  CALL_IFACE_METHOD(ArchiverFactory, CreateDefaultShaderSourceStreamFactory, This, __VA_ARGS__)
#    define IArchiverFactory_RemoveDeviceData(This, ...)                        CALL_IFACE_METHOD(ArchiverFactory, RemoveDeviceData,                       This, __VA_ARGS__)
#    define IArchiverFactory_AppendDeviceData(This, ...)                        CALL_IFACE_METHOD(ArchiverFactory, AppendDeviceData,                       This, __VA_ARGS__)
#    define IArchiverFactory_PrintArchiveContent(This, ...)                     CALL_IFACE_METHOD(ArchiverFactory, PrintArchiveContent,                    This, __VA_ARGS__)

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
