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
/// Definition of the engine constants

#include "../../../Primitives/interface/BasicTypes.h"

namespace Diligent
{
    /// Maximum number of input buffer slots.
    /// D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT == 32
    static constexpr Uint32 MaxBufferSlots          = 32;

    /// Maximum number of simultaneous render targets.
    static constexpr Uint32 MaxRenderTargets        = 8;

    /// Maximum number of viewports.
    static constexpr Uint32 MaxViewports            = 16;

    /// Maximum number of shader stages in a pipeline.
    static constexpr Uint32 MaxShadersInPipeline    = 5;
}
