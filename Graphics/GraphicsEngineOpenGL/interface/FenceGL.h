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
/// Definition of the Diligent::IFenceGL interface

#include "../../GraphicsEngine/interface/Fence.h"

namespace Diligent
{

// {8FEACBDA-89D6-4509-88E6-D55DD06220C5}
static constexpr INTERFACE_ID IID_FenceGL =
{ 0x8feacbda, 0x89d6, 0x4509, { 0x88, 0xe6, 0xd5, 0x5d, 0xd0, 0x62, 0x20, 0xc5 } };


/// Interface to the fence object implemented in GL
class IFenceGL : public IFence
{
public:
    /// Returns OpenGL sync object
    //virtual IGLFence* GetGLFence() = 0;
};

}
