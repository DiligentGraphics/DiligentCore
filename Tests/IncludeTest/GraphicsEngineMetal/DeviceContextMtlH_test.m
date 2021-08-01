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
 *  In no event and under no legal theory, whether in tort (including neMtligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly neMtligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include <Metal/Metal.h>

#define Rect DiligentRect
#include "DiligentCore/Graphics/GraphicsEngineMetal/interface/DeviceContextMtl.h"

void TestDeviceContextMtl_CInterface(IDeviceContextMtl* pContext)
{
    id<MTLCommandBuffer> mtlCmdBuf = IDeviceContextMtl_GetMtlCommandBuffer(pContext);
    (void)mtlCmdBuf;

    IDeviceContextMtl_SetComputeThreadgroupMemoryLength(pContext, 32u, 0u);

    if (@available(macos 11.0, ios 11.0, *))
    {
        IDeviceContextMtl_SetTileThreadgroupMemoryLength(pContext, 32u, 0u, 0u);
    }
}
