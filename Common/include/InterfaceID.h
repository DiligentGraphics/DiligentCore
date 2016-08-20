/*     Copyright 2015-2016 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "BasicTypes.h"

/// Unique identification structures
namespace Diligent
{
    /// Describes unique identifier
    struct INTERFACE_ID
    {
        Diligent::Uint32 Data1;
        Diligent::Uint16 Data2;
        Diligent::Uint16 Data3;
        Diligent::Uint8  Data4[8];
        
        bool operator == (const INTERFACE_ID& rhs)const
        {
            return Data1 == rhs.Data1 && 
                   Data2 == rhs.Data2 &&
                   Data3 == rhs.Data3 &&
                   memcmp(Data4, rhs.Data4, sizeof(Data4)) == 0;
        }
    };

    /// Unknown interface
    static const INTERFACE_ID IID_Unknown = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
}
