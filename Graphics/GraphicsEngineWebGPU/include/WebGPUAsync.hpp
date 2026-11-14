/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include <limits>

#include "WebGPUDefinitions.hpp"
#include "DebugUtilities.hpp"

#if PLATFORM_WEB
#    include <emscripten.h>
#endif

namespace Diligent
{

// The instance must enable TimedWaitAny. Browser callers must also enter Wasm
// through a suspension-capable Asyncify/JSPI entry point.
inline bool WaitForWebGPUFuture(WGPUInstance Instance, WGPUFuture Future)
{
#if PLATFORM_WEB
    if (!emscripten_has_asyncify())
    {
        LOG_ERROR_MESSAGE("Blocking WebGPU waits require Asyncify or JSPI on the Web. Use non-blocking synchronization methods.");
        return false;
    }
#endif

    WGPUFutureWaitInfo WaitInfo{};
    WaitInfo.future             = Future;
    const WGPUWaitStatus Status = wgpuInstanceWaitAny(Instance, 1, &WaitInfo, (std::numeric_limits<uint64_t>::max)());
    if (Status != WGPUWaitStatus_Success || !WaitInfo.completed)
    {
        LOG_ERROR_MESSAGE("Failed to wait for WebGPU operation (status ", static_cast<int>(Status), "). The instance must enable TimedWaitAny.");
        return false;
    }
    return true;
}

} // namespace Diligent
