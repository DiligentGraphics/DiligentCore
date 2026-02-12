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

#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

/// Create info structure for GPU upload manager
struct GPUUploadManagerCreateInfo
{
    /// Pointer to the render device. Must not be null.
    IRenderDevice* pDevice DEFAULT_INITIALIZER(nullptr);

    /// Pointer to the device context. Must not be null.
    IDeviceContext* pContext DEFAULT_INITIALIZER(nullptr);

    /// Size of the staging buffer page.
    Uint32 PageSize DEFAULT_INITIALIZER(4 * 1024 * 1024);

    /// Initial number of upload pages. If the manager runs out of pages to write to,
    /// it will create new ones as needed. This parameter controls how many pages are created at startup.
    Uint32 InitialPageCount DEFAULT_INITIALIZER(1);

    /// Maximum number of pages that the manager is allowed to create.
    /// 0 means no limit.
    Uint32 MaxPageCount DEFAULT_INITIALIZER(64);
};
typedef struct GPUUploadManagerCreateInfo GPUUploadManagerCreateInfo;

/// Callback function type for GPU upload enqueued callback.
/// This callback is called when the GPU copy operation is scheduled for execution
/// in the render thread.
typedef void (*GPUUploadEnqueuedCallbackType)(IBuffer* pDstBuffer,
                                              Uint32   DstOffset,
                                              Uint32   NumBytes,
                                              void*    pUserData);


/// GPU upload manager statistics.
struct GPUUploadManagerStats
{
    /// The number of pages in the manager.
    Uint32 NumPages DEFAULT_INITIALIZER(0);

    /// The number of free pages that are ready to be written to.
    Uint32 NumFreePages DEFAULT_INITIALIZER(0);

    /// The number of pages that are currently being used by the GPU for copy operations.
    Uint32 NumInFlightPages DEFAULT_INITIALIZER(0);

    /// The peak pending update size in bytes. This is the maximum total size of all pending buffer updates
    /// that could not be enqueued immediately due to lack of free pages.
    Uint32 PeakTotalPendingUpdateSize DEFAULT_INITIALIZER(0);

    /// Peak size of a single update in bytes.
    Uint32 PeakUpdateSize DEFAULT_INITIALIZER(0);
};
typedef struct GPUUploadManagerStats GPUUploadManagerStats;


// clang-format off

#define DILIGENT_INTERFACE_NAME IGPUUploadManager
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IGPUUploadManagerInclusiveMethods \
    IDeviceObjectInclusiveMethods;        \
    IGPUUploadManagerMethods GPUUploadManager

/// Asynchronous GPU upload manager
DILIGENT_BEGIN_INTERFACE(IGPUUploadManager, IObject)
{
    /// Executes pending render-thread operations.
    ///
    /// The method can be called in parallel with ScheduleBufferUpdate() from worker threads, but only
    /// one thread is allowed to call RenderThreadUpdate() at a time. The method must be called periodically
    /// to process pending buffer updates. If the method is not called, ScheduleBufferUpdate() may block indefinitely
    /// when there are no free pages available for new updates.
    VIRTUAL void METHOD(RenderThreadUpdate)(THIS_
                                            IDeviceContext* pContext) PURE;

    /// Schedules an asynchronous buffer update operation.
    ///
    /// \param [in] pContext      - If calling the method from the render thread, a pointer to the device context.
    ///                             If calling the method from a worker thread, this parameter must be null.
    /// \param [in] pDstBuffer    - Pointer to the destination buffer.
    /// \param [in] DstOffset     - Offset in the destination buffer.
    /// \param [in] NumBytes      - Number of bytes to copy.
    /// \param [in] pSrcData      - Pointer to the source data.
    /// \param [in] Callback      - Optional callback to be called when the GPU copy operation is scheduled for execution.
    /// \param [in] pCallbackData - Optional pointer to user data that will be passed to the callback.
    ///
    /// The method is thread-safe and can be called from multiple threads simultaneously with other calls to ScheduleBufferUpdate()
    /// and RenderThreadUpdate().
    /// 
    /// If the method is called from a worker thread, the pContext parameter must be null, and the render thread must periodically
    /// call RenderThreadUpdate() to process pending buffer updates. If RenderThreadUpdate() is not called, the method may block indefinitely
    /// when there are no free pages available for new updates.
    /// 
    /// If the method is called from the render thread, the pContext parameter must be a pointer to the device context used to create the
    /// GPU upload manager. If the method is called from the render thread with null pContext, it may never return.
    VIRTUAL void METHOD(ScheduleBufferUpdate)(THIS_
                                              IDeviceContext*               pContext,
                                              IBuffer*                      pDstBuffer,
                                              Uint32                        DstOffset,
                                              Uint32                        NumBytes,
                                              const void*                   pSrcData,
                                              GPUUploadEnqueuedCallbackType Callback      DEFAULT_VALUE(nullptr),
                                              void*                         pCallbackData DEFAULT_VALUE(nullptr)) PURE;

    /// Retrieves GPU upload manager statistics.
    ///
    /// The method must not be called concurrently with RenderThreadUpdate().
    VIRTUAL void METHOD(GetStats)(THIS_
                                  GPUUploadManagerStats REF Stats) CONST PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

// clang-format off

#    define IGPUUploadManager_RenderThreadUpdate(This, ...)   CALL_IFACE_METHOD(GPUUploadManager, RenderThreadUpdate, This, __VA_ARGS__)
#    define IGPUUploadManager_ScheduleBufferUpdate(This, ...) CALL_IFACE_METHOD(GPUUploadManager, ScheduleBufferUpdate, This, __VA_ARGS__)
#    define IGPUUploadManager_GetStats(This, ...)             CALL_IFACE_METHOD(GPUUploadManager, GetStats, This, __VA_ARGS__)

// clang-format on

#endif

#include "../../../Primitives/interface/DefineRefMacro.h"

/// Creates an instance of the GPU upload manager.
void DILIGENT_GLOBAL_FUNCTION(CreateGPUUploadManager)(const GPUUploadManagerCreateInfo REF CreateInfo,
                                                      IGPUUploadManager**                  ppManager);

#include "../../../Primitives/interface/UndefRefMacro.h"

DILIGENT_END_NAMESPACE // namespace Diligent
