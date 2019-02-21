/*     Copyright 2015-2019 Egor Yusov
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

/// \file
/// Definition input layout

#include "GraphicsTypes.h"

namespace Diligent
{

static constexpr Uint32 iMaxLayoutElements = 16;

/// Description of a single element of the input layout
struct LayoutElement
{
    static constexpr Uint32 AutoOffset = static_cast<Uint32>(-1);
    static constexpr Uint32 AutoStride = static_cast<Uint32>(-1);

    /// Input index of the element, which is specified in the vertex shader.
    Uint32 InputIndex       = 0;

    /// Buffer slot index that this element is read from.
    Uint32 BufferSlot       = 0;

    /// Number of components in the element. Allowed values are 1, 2, 3, and 4.
    Uint32 NumComponents    = 0;

    /// Type of the element components, see Diligent::VALUE_TYPE for details.
    VALUE_TYPE ValueType    = VT_FLOAT32;

    /// For signed and unsigned integer value types 
    /// (VT_INT8, VT_INT16, VT_INT32, VT_UINT8, VT_UINT16, VT_UINT32)
    /// indicates if the value should be normalized to [-1,+1] or 
    /// [0, 1] range respectively. For floating point types
    /// (VT_FLOAT16 and VT_FLOAT32), this member is ignored.
    Bool IsNormalized       = True;

    /// Relative offset, in bytes, to the element bits.
    /// If this value is set to LayoutElement::AutoOffset (default value), the offset will
    /// be computed automatically by placing the element right after the previous one.
    Uint32 RelativeOffset   = AutoOffset;

    /// Stride, in bytes, between two elements, for this buffer slot.
    /// If this value is set to LayoutElement::AutoStride, the stride will be
    /// computed automatically assuming that all elements in the same buffer slot are
    /// packed one after another. If the buffer slot contains multiple layout elements,
    /// they all must specify the same stride or use AutoStride value.
    Uint32 Stride           = AutoOffset;

    /// Input frequency
    enum FREQUENCY : Int32
    {
        /// Frequency is undefined.
        FREQUENCY_UNDEFINED = 0,

        /// Input data is per-vertex data.
        FREQUENCY_PER_VERTEX,

        /// Input data is per-instance data.
        FREQUENCY_PER_INSTANCE,

        /// Helper value that stores the total number of frequencies in the enumeration.
        FREQUENCY_NUM_FREQUENCIES
    };
    FREQUENCY Frequency         = FREQUENCY_PER_VERTEX;
    
    /// The number of instances to draw using the same per-instance data before advancing 
    /// in the buffer by one element.
    Uint32 InstanceDataStepRate = 1;

    LayoutElement()noexcept{}

    /// Initializes the structure
    LayoutElement(Uint32     _InputIndex, 
                  Uint32     _BufferSlot, 
                  Uint32     _NumComponents, 
                  VALUE_TYPE _ValueType,
                  Bool       _IsNormalized         = LayoutElement{}.IsNormalized, 
                  Uint32     _RelativeOffset       = LayoutElement{}.RelativeOffset,
                  Uint32     _Stride               = LayoutElement{}.Stride,
                  FREQUENCY  _Frequency            = LayoutElement{}.Frequency,
                  Uint32     _InstanceDataStepRate = LayoutElement{}.InstanceDataStepRate)noexcept : 
        InputIndex          (_InputIndex),
        BufferSlot          (_BufferSlot),
        NumComponents       (_NumComponents),
        ValueType           (_ValueType),
        IsNormalized        (_IsNormalized),
        RelativeOffset      (_RelativeOffset),
        Stride              (_Stride),
        Frequency           (_Frequency),
        InstanceDataStepRate(_InstanceDataStepRate)
    {}
};

/// Layout description

/// This structure is used by IRenderDevice::CreatePipelineState().
struct InputLayoutDesc 
{
    /// Array of layout elements
    const LayoutElement*  LayoutElements = nullptr;
    /// Number of layout elements
    Uint32 NumElements                   = 0;

    InputLayoutDesc()noexcept{}

    InputLayoutDesc(const LayoutElement* _LayoutElements, 
                    Uint32               _NumElements)noexcept :
        LayoutElements(_LayoutElements),
        NumElements   (_NumElements)
    {}
};

}
