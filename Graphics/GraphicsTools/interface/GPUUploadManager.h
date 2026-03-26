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

    /// Maximum number of pages that the manager should maintain.
    ///
    /// Note the manager may temporarily exceed this limit in certain scenarios (for example, if large
    /// update is scheduled while the maximum is already reached), but it will reduce the number of
    /// pages to the maximum as soon as possible.
    Uint32 MaxPageCount DEFAULT_INITIALIZER(64);
};
typedef struct GPUUploadManagerCreateInfo GPUUploadManagerCreateInfo;


/// Callback function type for writing data to a staging buffer.
/// This callback is invoked by ScheduleBufferUpdate() when the manager needs to write data to a staging buffer page.
/// The callback is expected to write the data to the provided destination pointer.
/// \param [in] pDstData   - Pointer to the staging buffer memory where the data should be written.
/// \param [in] NumBytes   - Number of bytes to write. This is the same number of bytes specified in ScheduleBufferUpdateInfo::NumBytes.
/// \param [in] pUserData  - User-provided pointer passed to ScheduleBufferUpdate().
///
/// \warning Reentrancy / thread-safety:
///          The callback is executed from inside IGPUUploadManager::ScheduleBufferUpdate().
///          The callback MUST NOT call back into the same IGPUUploadManager instance.
typedef void (*WriteStagingDataCallbackType)(void* pDstData, Uint32 NumBytes, void* pUserData);


/// Callback function type for GPU upload enqueued callback.
///
/// This callback is invoked on the render thread when the copy command for the update
/// has been enqueued into the device context command stream (i.e. the copy is *scheduled*,
/// but may not have executed on the GPU yet).
///
/// If the copy operation has not been scheduled by the time the manager is destroyed,
/// the callback will be invoked with a null buffer pointer, allowing the application
/// to clean up any resources associated with the copy operation.
///
/// \warning Reentrancy / thread-safety:
///          The callback is executed from inside IGPUUploadManager::RenderThreadUpdate().
///          The callback MUST NOT call back into the same IGPUUploadManager instance
///          (e.g. ScheduleBufferUpdate(), RenderThreadUpdate(), GetStats()), and MUST NOT
///          perform actions that may synchronously trigger RenderThreadUpdate() or otherwise
///          re-enter the manager, as this may lead to deadlocks, unbounded recursion, or
///          inconsistent internal state.
///
///          If follow-up work is required, the callback should only enqueue work to be
///          processed later (e.g. push a task into a user-owned queue) and return promptly.
///
/// \param [in] pDstBuffer  - Destination buffer passed to ScheduleBufferUpdate().
/// \param [in] DstOffset   - Destination offset passed to ScheduleBufferUpdate().
/// \param [in] NumBytes    - Number of bytes passed to ScheduleBufferUpdate().
/// \param [in] pUserData   - User-provided pointer passed to ScheduleBufferUpdate().
typedef void (*GPUUploadEnqueuedCallbackType)(IBuffer* pDstBuffer,
                                              Uint32   DstOffset,
                                              Uint32   NumBytes,
                                              void*    pUserData);


/// Callback function type for copying buffer data.
/// This callback is invoked on the render thread when the manager needs to perform the copy operation for a buffer update.
/// The callback is expected to perform the copy operation itself, using the provided parameters, and schedule it for execution on the GPU.
///
/// \param [in] pContext   - Device context to use for scheduling the copy operation.
/// \param [in] pSrcBuffer - Source buffer containing the data to copy. The buffer is guaranteed to be valid for the duration of the callback.
/// \param [in] SrcOffset  - Offset in the source buffer where the data to copy starts.
/// \param [in] NumBytes   - Number of bytes to copy.
/// \param [in] pUserData  - User-provided pointer passed to ScheduleBufferUpdate().
///
/// If the copy operation was not scheduled by the time the manager is destroyed,
/// the callback will be called with a null device context pointer so that the application
/// can clean up any resources associated with the copy operation.
///
/// \warning Reentrancy / thread-safety:
///          The callback is executed from inside IGPUUploadManager::RenderThreadUpdate().
///          The callback MUST NOT call back into the same IGPUUploadManager instance
///          (e.g. ScheduleBufferUpdate(), RenderThreadUpdate(), GetStats()), and MUST NOT
///          perform actions that may synchronously trigger RenderThreadUpdate() or otherwise
///          re-enter the manager, as this may lead to deadlocks, unbounded recursion, or
///          inconsistent internal state.
///
///          If follow-up work is required, the callback should only enqueue work to be
///          processed later (e.g. push a task into a user-owned queue) and return promptly.
typedef void (*CopyStagingBufferCallbackType)(IDeviceContext* pContext,
                                              IBuffer*        pSrcBuffer,
                                              Uint32          SrcOffset,
                                              Uint32          NumBytes,
                                              void*           pUserData);


/// Structure describing a buffer update operation to be scheduled by IGPUUploadManager::ScheduleBufferUpdate().
struct ScheduleBufferUpdateInfo
{
    /// If calling ScheduleBufferUpdate() from the render thread, a pointer to the device context.
    /// If calling ScheduleBufferUpdate() from a worker thread, this parameter must be null.
    IDeviceContext* pContext DEFAULT_INITIALIZER(nullptr);

    /// Pointer to the destination buffer to update.
    /// If CopyBuffer callback is provided, this parameter will be ignored
    /// (though the manager will still keep a reference to the buffer until the copy operation is scheduled),
    /// and the callback must perform the copy operation itself.
    /// Otherwise, this buffer will be used as the destination for the copy operation
    IBuffer* pDstBuffer DEFAULT_INITIALIZER(nullptr);

    /// Offset in the destination buffer where the update will be applied.
    /// If CopyBuffer callback is provided, this parameter will be ignored, and the callback must
    /// perform the copy operation itself.
    /// Otherwise, this offset will be used as the destination offset for the copy operation.
    Uint32 DstOffset DEFAULT_INITIALIZER(0);

    /// Number of bytes to copy from the source data to the destination buffer.
    Uint32 NumBytes DEFAULT_INITIALIZER(0);

    /// Pointer to the source data to copy to the destination buffer.
    /// The manager makes an internal copy of the source data, so the memory pointed to by this
    /// parameter can be safely released or reused after the method returns.
    /// If WriteDataCallback callback is provided, this parameter will be ignored, and the callback must
    /// write the source data to the staging buffer when requested by the manager.
    const void* pSrcData DEFAULT_INITIALIZER(nullptr);

    /// Optional callback to write data to a staging buffer. If provided, the pSrcData parameter is ignored,
    /// and the manager will call the callback with a pointer to the staging buffer memory when it needs to
    /// write data to a staging buffer page.
    /// The callback will be called from ScheduleBufferUpdate().
    WriteStagingDataCallbackType WriteDataCallback DEFAULT_INITIALIZER(nullptr);

    /// Optional pointer to user data that will be passed to the WriteStagingDataCallback.
    void* pWriteDataCallbackUserData DEFAULT_INITIALIZER(nullptr);

    /// Optional callback to perform the copy operation. If this parameter is null, the manager will perform the copy
    /// from the source data to the destination buffer using its internal staging buffer and copy command.
    /// If the callback is provided, it must perform the copy operation itself. The manager will pass the
    /// necessary parameters to the callback.
    CopyStagingBufferCallbackType CopyBuffer DEFAULT_INITIALIZER(nullptr);

    /// Optional pointer to user data that will be passed to the CopyBuffer callback.
    void* pCopyBufferData DEFAULT_INITIALIZER(nullptr);

    /// Optional callback to be called when the GPU copy operation is scheduled for execution.
    /// If CopyBuffer is provided, the callback will not be called, and the CopyBuffer callback is expected
    /// to perform any necessary follow-up actions after scheduling the copy operation.
    GPUUploadEnqueuedCallbackType UploadEnqueued DEFAULT_INITIALIZER(nullptr);

    /// Optional pointer to user data that will be passed to the callback.
    void* pUploadEnqueuedData DEFAULT_INITIALIZER(nullptr);

#if DILIGENT_CPP_INTERFACE
    ScheduleBufferUpdateInfo() noexcept = default;

    ScheduleBufferUpdateInfo(IDeviceContext*               _pCtx,
                             IBuffer*                      _pDstBuf,
                             Uint32                        _DstOffs,
                             Uint32                        _NumBytes,
                             const void*                   _pSrcData,
                             GPUUploadEnqueuedCallbackType _UploadEnqueued      = nullptr,
                             void*                         _pUploadEnqueuedData = nullptr) noexcept :
        pContext{_pCtx},
        pDstBuffer{_pDstBuf},
        DstOffset{_DstOffs},
        NumBytes{_NumBytes},
        pSrcData{_pSrcData},
        UploadEnqueued{_UploadEnqueued},
        pUploadEnqueuedData{_pUploadEnqueuedData}
    {}

    ScheduleBufferUpdateInfo(IBuffer*                      _pDstBuf,
                             Uint32                        _DstOffs,
                             Uint32                        _NumBytes,
                             const void*                   _pSrcData,
                             GPUUploadEnqueuedCallbackType _UploadEnqueued      = nullptr,
                             void*                         _pUploadEnqueuedData = nullptr) noexcept :
        ScheduleBufferUpdateInfo{nullptr, _pDstBuf, _DstOffs, _NumBytes, _pSrcData, _UploadEnqueued, _pUploadEnqueuedData}
    {}

    ScheduleBufferUpdateInfo(IDeviceContext*               _pCtx,
                             Uint32                        _NumBytes,
                             const void*                   _pSrcData,
                             CopyStagingBufferCallbackType _CopyBuffer,
                             void*                         _pCopyBufferData = nullptr) noexcept :
        pContext{_pCtx},
        NumBytes{_NumBytes},
        pSrcData{_pSrcData},
        CopyBuffer{_CopyBuffer},
        pCopyBufferData{_pCopyBufferData}
    {}

    ScheduleBufferUpdateInfo(Uint32                        _NumBytes,
                             const void*                   _pSrcData,
                             CopyStagingBufferCallbackType _CopyBuffer,
                             void*                         _pCopyBufferData = nullptr) noexcept :
        ScheduleBufferUpdateInfo{nullptr, _NumBytes, _pSrcData, _CopyBuffer, _pCopyBufferData}
    {}
#endif
};
typedef struct ScheduleBufferUpdateInfo ScheduleBufferUpdateInfo;


/// GPU upload manager page bucket information.
struct GPUUploadManagerBucketInfo
{
    /// Page size in bytes.
    Uint32 PageSize DEFAULT_INITIALIZER(0);

    /// Number of pages currently in the manager.
    Uint32 NumPages DEFAULT_INITIALIZER(0);
};
typedef struct GPUUploadManagerBucketInfo GPUUploadManagerBucketInfo;


/// GPU upload manager statistics.
struct GPUUploadManagerStats
{
    /// The number of pages in the manager.
    Uint32 NumPages DEFAULT_INITIALIZER(0);

    /// The number of free pages that are ready to be written to.
    Uint32 NumFreePages DEFAULT_INITIALIZER(0);

    /// The number of pages that are currently being used by the GPU for copy operations.
    Uint32 NumInFlightPages DEFAULT_INITIALIZER(0);

    /// The peak number of pages that were created by the manager. This value can exceed the maximum page count,
    /// but only temporarily when the manager needs to create new pages to accommodate large updates.
    Uint32 PeakNumPages DEFAULT_INITIALIZER(0);

    /// The peak pending update size in bytes. This is the maximum total size of all pending buffer updates
    /// that could not be enqueued immediately due to lack of free pages.
    Uint32 PeakTotalPendingUpdateSize DEFAULT_INITIALIZER(0);

    /// Peak size of a single update in bytes.
    Uint32 PeakUpdateSize DEFAULT_INITIALIZER(0);

    /// The number of buckets in the manager. Each bucket corresponds to a specific page size.
    Uint32 NumBuckets DEFAULT_INITIALIZER(0);

    /// Information about each bucket. The array contains NumBuckets valid entries.
    /// The pointer is valid only until the next call to RenderThreadUpdate() or
    /// ScheduleBufferUpdate() with a non-null device context, which may change the number
    /// of buckets.
    const GPUUploadManagerBucketInfo* pBucketInfo DEFAULT_INITIALIZER(nullptr);
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
    /// \param [in] UpdateInfo - Structure describing the buffer update operation. See ScheduleBufferUpdateInfo for details.
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
                                              const ScheduleBufferUpdateInfo REF UpdateInfo) PURE;

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
