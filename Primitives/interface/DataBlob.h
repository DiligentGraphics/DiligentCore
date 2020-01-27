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
/// Defines Diligent::IDataBlob interface

#include "Object.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)


// {F578FF0D-ABD2-4514-9D32-7CB454D4A73B}
static const struct INTERFACE_ID IID_DataBlob =
    {0xf578ff0d, 0xabd2, 0x4514, {0x9d, 0x32, 0x7c, 0xb4, 0x54, 0xd4, 0xa7, 0x3b}};

// clang-format off

#if DILIGENT_C_INTERFACE
#    define THIS  struct IDataBlob*
#    define THIS_ struct IDataBlob*,
#endif

/// Base interface for a file stream
DILIGENT_INTERFACE(IDataBlob, IObject)
{
    /// Sets the size of the internal data buffer
    VIRTUAL void METHOD(Resize)(THIS_
                                        size_t NewSize) PURE;

    /// Returns the size of the internal data buffer
    VIRTUAL size_t METHOD(GetSize)(THIS) PURE;

    /// Returns the pointer to the internal data buffer
    VIRTUAL void* METHOD(GetDataPtr)(THIS) PURE;
};

    // clang-format on

#if DILIGENT_C_INTERFACE

#    undef THIS
#    undef THIS_

struct IDataBlobVtbl
{
    struct IObjectMethods   Object;
    struct IDataBlobMethods DataBlob;
};

typedef struct IDataBlob
{
    struct IDataBlobVtbl* pVtbl;
} IDataBlob;

// clang-format off

#    define IDataBlob_Resize(This, ...)  (This)->pVtbl->DataBlob.Resize    ((IDataBlob*)(This), __VA_ARGS__)
#    define IDataBlob_GetSize(This)      (This)->pVtbl->DataBlob.GetSize   ((IDataBlob*)(This))
#    define IDataBlob_GetDataPtr(This)   (This)->pVtbl->DataBlob.GetDataPtr((IDataBlob*)(This))

// clang-format on

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
