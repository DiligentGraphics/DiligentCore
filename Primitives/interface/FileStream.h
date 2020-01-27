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
/// Defines Diligent::IFileStream interface

#include "Object.h"
#include "DataBlob.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

/// IFileStream interface unique identifier
// {E67F386C-6A5A-4A24-A0CE-C66435465D41}
static const struct INTERFACE_ID IID_FileStream =
    {0xe67f386c, 0x6a5a, 0x4a24, {0xa0, 0xce, 0xc6, 0x64, 0x35, 0x46, 0x5d, 0x41}};

// clang-format off

#if DILIGENT_C_INTERFACE
#    define THIS  struct IFileStream*
#    define THIS_ struct IFileStream*,
#endif

/// Base interface for a file stream
DILIGENT_INTERFACE(IFileStream, IObject)
{
    /// Reads data from the stream
    VIRTUAL bool METHOD(Read)(THIS_
                              void*  Data,
                              size_t BufferSize) PURE;

    VIRTUAL void METHOD(ReadBlob)(THIS_
                                  IDataBlob* pData) PURE;

    /// Writes data to the stream
    VIRTUAL bool METHOD(Write)(THIS_
                               const void* Data, 
                               size_t      Size) PURE;

    VIRTUAL size_t METHOD(GetSize)(THIS) PURE;

    VIRTUAL bool METHOD(IsValid)(THIS) PURE;
};

    // clang-format on

#if DILIGENT_C_INTERFACE

#    undef THIS
#    undef THIS_

struct IFileStreamVtbl
{
    struct IObjectMethods     Object;
    struct IFileStreamMethods FileStream;
};

typedef struct IFileStream
{
    struct IFileStreamVtbl* pVtbl;
} IFileStream;

// clang-format off

#    define IFileStream_Read(This, ...)     (This)->pVtbl->FileStream.Read    ((IFileStream*)(This), __VA_ARGS__)
#    define IFileStream_ReadBlob(This, ...) (This)->pVtbl->FileStream.ReadBlob((IFileStream*)(This), __VA_ARGS__)
#    define IFileStream_Write(This, ...)    (This)->pVtbl->FileStream.Write   ((IFileStream*)(This), __VA_ARGS__)
#    define IFileStream_GetSize(This)       (This)->pVtbl->FileStream.GetSize ((IFileStream*)(This))
#    define IFileStream_IsValid(This)       (This)->pVtbl->FileStream.IsValid ((IFileStream*)(This))

// clang-format on

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
