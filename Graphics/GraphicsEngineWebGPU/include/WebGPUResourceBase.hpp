/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "GraphicsTypes.h"
#include "RefCntAutoPtr.hpp"
#include "WebGPUObjectWrappers.hpp"
#include "SyncPointWebGPU.hpp"

/// \file
/// Implementation of the Diligent::WebGPUResourceBase class

namespace Diligent
{

class RenderDeviceWebGPUImpl;

/// Base implementation of a WebGPU resource
class WebGPUResourceBase
{
public:
    struct StagingBufferInfo
    {
        WebGPUResourceBase&                Resource;
        WebGPUBufferWrapper                wgpuBuffer;
        RefCntAutoPtr<SyncPointWebGPUImpl> pSyncPoint = {};
    };

    WebGPUResourceBase(IReferenceCounters* pRefCounters, size_t MaxPendingBuffers);
    ~WebGPUResourceBase();

    StagingBufferInfo* GetStagingBufferInfo(WGPUDevice wgpuDevice, const char* ResourceName, CPU_ACCESS_FLAGS Access);

    void FlushPendingWrites(StagingBufferInfo& Buffer);
    void ProcessAsyncReadback(StagingBufferInfo& Buffer);

protected:
    void* Map(MAP_TYPE MapType, Uint64 Offset);
    void  Unmap();

private:
    StagingBufferInfo* FindStagingWriteBuffer(WGPUDevice wgpuDevice, const char* ResourceName);
    StagingBufferInfo* FindStagingReadBuffer(WGPUDevice wgpuDevice, const char* ResourceName);

private:
    IReferenceCounters* const m_pRefCounters;

protected:
    enum class MapState
    {
        None,
        Read,
        Write
    };
    using StagingBufferList = std::vector<StagingBufferInfo>;

    StagingBufferList    m_StagingBuffers;
    std::vector<uint8_t> m_MappedData;
    MapState             m_MapState = MapState::None;
};

} // namespace Diligent
