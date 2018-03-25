/*     Copyright 2015-2018 Egor Yusov
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
/// Defines graphics engine utilities

#include "../../GraphicsEngine/interface/Texture.h"
#include "../../GraphicsEngine/interface/Buffer.h"
#include "../../GraphicsEngine/interface/RenderDevice.h"

namespace Diligent
{
void CreateUniformBuffer( IRenderDevice *pDevice, Uint32 Size, const Char *Name, IBuffer **ppBuffer, USAGE Usage = USAGE_DYNAMIC, Uint32 BindFlag = BIND_UNIFORM_BUFFER, Uint32 CPUAccessFlags = CPU_ACCESS_WRITE );
void GenerateCheckerBoardPattern(Uint32 Width, Uint32 Height, TEXTURE_FORMAT Fmt, Uint32 HorzCells, Uint32 VertCells, Uint8 *pData, Uint32 StrideInBytes );
}
