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

/// \file
/// CompositorServices session wrapper for visionOS.

/// `CompositorServicesSession` encapsulates the visionOS frame protocol (frame query, update,
/// timing wait, drawable iteration, present) and the optional ARKit world-tracking session,
/// exposing a small, idiomatic C++ API on top of the C/Objective-C `CompositorServices`
/// framework.

#include <functional>
#include <memory>

#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"
#include "../../../Primitives/interface/BasicTypes.h"
#include "../../../Common/interface/BasicMath.hpp"
#include "../../../Common/interface/RefCntAutoPtr.hpp"

namespace Diligent
{

/// High-level wrapper around the visionOS CompositorServices frame protocol.

/// The session owns:
/// - a `cp_layer_renderer_t` received from the SwiftUI `CompositorLayer` entry point;
/// - an optional ARKit world-tracking session that supplies device anchors for each drawable;
/// - references to the Diligent device and immediate context used for wrapping drawable
///   textures and presenting drawables.
///
/// `cp_layer_renderer_t` and `cp_drawable_t` are exposed as opaque `void*` pointers so that
/// consumers do not need to include `<CompositorServices/CompositorServices.h>` and can be
/// compiled as plain C++
class CompositorServicesSession
{
public:
    /// Initializes the session and, optionally, starts the ARKit world-tracking session.

    /// \param[in] pRenderer           - Layer renderer (`cp_layer_renderer_t`) from the SwiftUI
    ///                                  `CompositorLayer` entry point, passed as an opaque
    ///                                  pointer. Must not be null.
    /// \param[in] pDevice             - Diligent render device. Must not be null.
    /// \param[in] pContext            - Diligent immediate context. Must not be null.
    /// \param[in] EnableWorldTracking - When `true`, an ARKit world-tracking session is started
    ///                                  and used to attach device anchors to every drawable.
    ///                                  When `false`, the scene is rigidly attached to the
    ///                                  headset.
    CompositorServicesSession(void*           pRenderer,
                              IRenderDevice*  pDevice,
                              IDeviceContext* pContext,
                              bool            EnableWorldTracking = true);

    ~CompositorServicesSession();

    // clang-format off
    CompositorServicesSession           (const CompositorServicesSession&)  = delete;
    CompositorServicesSession           (      CompositorServicesSession&&) = delete;
    CompositorServicesSession& operator=(const CompositorServicesSession&)  = delete;
    CompositorServicesSession& operator=(      CompositorServicesSession&&) = delete;
    // clang-format on

    /// Drives a single CompositorServices frame from start to finish.

    /// Handles the full frame protocol mandated by visionOS:
    /// 1. Queries the next frame from the layer renderer.
    /// 2. Wraps app-side state updates in `cp_frame_start_update` / `cp_frame_end_update`
    ///    by invoking `Update`.
    /// 3. Predicts timing after the update phase and waits until the compositor's optimal
    ///    input time.
    /// 4. Enters the submission phase, iterates over all drawables, attaches a device anchor
    ///    (when world tracking is enabled) and invokes `RenderDrawable` for each.
    /// 5. Ends the submission phase.
    ///
    /// All null / cancelled-frame paths are handled internally.
    ///
    /// `RenderDrawable` receives an opaque `cp_drawable_t` pointer and is responsible for
    /// setting up render targets, drawing the scene and calling `PresentDrawable` on the
    /// session.
    ///
    /// Either callback may be empty.
    ///
    /// \param[in] Update         - Callback invoked during the update phase of the frame.
    /// \param[in] RenderDrawable - Callback invoked once per drawable during the submission
    ///                             phase of the frame.
    void RenderFrame(std::function<void()>                Update,
                     std::function<void(void* pDrawable)> RenderDrawable);

    /// Returns the number of views (eyes) in a drawable (typically 2 for stereo rendering).

    /// \param[in] pDrawable - Opaque `cp_drawable_t` pointer.
    Uint32 GetViewCount(void* pDrawable) const;

    /// Wraps the Metal color texture of a drawable view as a Diligent texture.

    /// \param[in] pDrawable - Opaque `cp_drawable_t` pointer.
    /// \param[in] ViewIndex - Zero-based view index.
    ///
    /// \return     A reference to the Diligent texture, or null on failure.
    RefCntAutoPtr<ITexture> GetColorSwapchainImage(void* pDrawable, Uint32 ViewIndex);

    /// Wraps the Metal depth texture of a drawable view as a Diligent texture.

    /// \param[in] pDrawable - Opaque `cp_drawable_t` pointer.
    /// \param[in] ViewIndex - Zero-based view index.
    ///
    /// \return     A reference to the Diligent texture, or null on failure.
    RefCntAutoPtr<ITexture> GetDepthSwapchainImage(void* pDrawable, Uint32 ViewIndex);

    /// Returns the reverse-Z projection matrix for a view in a drawable.

    /// The compositor mandates reverse-Z depth: the matrix maps the near plane to 1 and the
    /// far plane to 0. Clear the depth buffer to 0 and use `COMPARISON_FUNC_GREATER_EQUAL`
    /// in the PSO.
    ///
    /// \param[in] pDrawable - Opaque `cp_drawable_t` pointer.
    /// \param[in] ViewIndex - Zero-based view index.
    /// \param[in] NearZ     - Near clipping plane distance.
    /// \param[in] FarZ      - Far clipping plane distance.
    float4x4 GetProjectionMatrix(void* pDrawable, Uint32 ViewIndex, float NearZ, float FarZ) const;

    /// Returns the world-to-eye view matrix for a view in a drawable.

    /// When world tracking is enabled and the ARKit session has produced at least one anchor,
    /// the matrix accounts for the device pose in the world. Otherwise it yields a drawable-
    /// local view matrix (scene rigidly attached to the headset).
    ///
    /// \param[in] pDrawable - Opaque `cp_drawable_t` pointer.
    /// \param[in] ViewIndex - Zero-based view index.
    float4x4 GetViewMatrix(void* pDrawable, Uint32 ViewIndex) const;

    /// Encodes a present command for the drawable into the immediate context's Metal command
    /// buffer.

    /// Call after issuing all draw calls for the drawable, before `Flush`.
    ///
    /// \param[in] pDrawable - Opaque `cp_drawable_t` pointer.
    void PresentDrawable(void* pDrawable);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Diligent
