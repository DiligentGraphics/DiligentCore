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
/// Definition of the Diligent::IQueryD3D12 interface

#include "../../GraphicsEngine/interface/Query.h"

namespace Diligent
{

// {72D109BE-7D70-4E54-84EF-C649DA190B2C}
static constexpr INTERFACE_ID IID_QueryD3D12 =
    {0x72d109be, 0x7d70, 0x4e54, {0x84, 0xef, 0xc6, 0x49, 0xda, 0x19, 0xb, 0x2c}};

/// Exposes Direct3D12-specific functionality of a Query object.
class IQueryD3D12 : public IQuery
{
    /// Returns the Direct3D12 query heap that internal query object resides in.
    virtual ID3D12QueryHeap* GetD3D12QueryHeap() = 0;

    /// Returns the index of a query object in Direct3D12 query heap.
    virtual Uint32 GetQueryHeapIndex() const = 0;
};

} // namespace Diligent
