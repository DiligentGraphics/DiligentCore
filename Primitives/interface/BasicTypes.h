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

#include <cstdint>
#include <string>

namespace Diligent
{
    typedef float       Float32; ///< 32-bit float

    typedef int64_t     Int64;   ///< 64-bit signed integer
    typedef int32_t     Int32;   ///< 32-bit signed integer
    typedef int16_t     Int16;   ///< 16-bit signed integer
    typedef int8_t      Int8;    ///< 8-bit signed integer

    typedef uint64_t    Uint64;  ///< 64-bit unsigned integer
    typedef uint32_t    Uint32;  ///< 32-bit unsigned integer
    typedef uint16_t    Uint16;  ///< 16-bit unsigned integer
    typedef uint8_t     Uint8;   ///< 8-bit unsigned integer

    typedef size_t SizeType;
    typedef void* PVoid;

    typedef bool Bool;          ///< Boolean
    static constexpr Bool False = false;
    static constexpr Bool True = true;

    typedef char Char;
    typedef std::basic_string<Char> String; ///< String variable
}
