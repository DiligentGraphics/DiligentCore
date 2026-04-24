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

#include "CompositorServicesSession.hpp"

#import <Metal/Metal.h>
#include <CompositorServices/CompositorServices.h>
#include <ARKit/ARKit.h>
#include <simd/simd.h>
#include <cstring>

#include "RenderDeviceMtl.h"
#include "DeviceContextMtl.h"
#include "DebugUtilities.hpp"
#include "RefCntAutoPtr.hpp"
#include "ObjCWrapper.hpp"

namespace Diligent
{

namespace
{

float4x4 ConvertSimdMatrix(const simd_float4x4& M)
{
    float4x4 Out;
    std::memcpy(Out.m, M.columns, sizeof(float4x4));
    return Out;
}

} // namespace


struct CompositorServicesSession::Impl
{
    cp_layer_renderer_t              Renderer = nullptr;
    RefCntAutoPtr<IRenderDevice>     pDevice;
    RefCntAutoPtr<IDeviceContext>    pContext;
    RefCntAutoPtr<IRenderDeviceMtl>  pDeviceMtl;
    RefCntAutoPtr<IDeviceContextMtl> pContextMtl;

    ObjCWrapper<ar_world_tracking_configuration_t> Config;
    ObjCWrapper<ar_data_providers_t>               DataProviders;
    ObjCWrapper<ar_world_tracking_provider_t>      WorldTracking;
    ObjCWrapper<ar_session_t>                      ArSession;
    simd_float4x4                                  LastAnchor = matrix_identity_float4x4;
    bool                                           HasAnchor  = false;

    void StartWorldTracking()
    {
        Config.Attach(ar_world_tracking_configuration_create());
        if (!Config)
        {
            UNEXPECTED("Failed to create ar_world_tracking_configuration_t");
            return;
        }

        WorldTracking.Attach(ar_world_tracking_provider_create(Config));
        if (!WorldTracking)
        {
            UNEXPECTED("Failed to create ar_world_tracking_provider_t");
            Config.Release();
            return;
        }

        DataProviders.Attach(ar_data_providers_create());
        ar_data_providers_add_data_provider(DataProviders, WorldTracking);

        ArSession.Attach(ar_session_create());
        ar_session_run(ArSession, DataProviders);
    }

    void StopWorldTracking()
    {
        if (ArSession)
            ar_session_stop(ArSession);
        ArSession.Release();
        DataProviders.Release();
        WorldTracking.Release();
        Config.Release();
    }

    // Samples the device anchor predicted for the drawable's presentation
    // time and attaches it to the drawable. No-op if world tracking is
    // disabled or the session has not produced an anchor yet.
    void SetDrawableDeviceAnchor(cp_drawable_t Drawable)
    {
        if (Drawable == nullptr || WorldTracking == nullptr)
            return;

        cp_frame_timing_t Timing = cp_drawable_get_frame_timing(Drawable);
        if (Timing == nullptr)
            return;

        CFTimeInterval PresentationTime = cp_time_to_cf_time_interval(cp_frame_timing_get_presentation_time(Timing));

        // Anchor takes ownership of the +1-retained handle and releases it on
        // scope exit on every code path.
        ObjCWrapper<ar_device_anchor_t> Anchor{ar_device_anchor_create()};
        ar_device_anchor_query_status_t Status =
            ar_world_tracking_provider_query_device_anchor_at_timestamp(WorldTracking, PresentationTime, Anchor);

        if (Status != ar_device_anchor_query_status_success)
            return;

        LastAnchor = ar_anchor_get_origin_from_anchor_transform(Anchor);
        HasAnchor  = true;

        cp_drawable_set_device_anchor(Drawable, Anchor);
    }

    void CreateTextureFromDrawable(id<MTLTexture> mtlTexture,
                                   bool           IsDepth,
                                   ITexture**     ppTexture)
    {
        if (mtlTexture == nil)
        {
            UNEXPECTED("Failed to get Metal texture from CompositorServices drawable");
            return;
        }

        const RESOURCE_STATE InitialState = IsDepth ? RESOURCE_STATE_DEPTH_WRITE : RESOURCE_STATE_RENDER_TARGET;
        pDeviceMtl->CreateTextureFromMtlResource(mtlTexture, InitialState, ppTexture);
    }
};


CompositorServicesSession::CompositorServicesSession(void*           pRenderer,
                                                     IRenderDevice*  pDevice,
                                                     IDeviceContext* pContext,
                                                     bool            EnableWorldTracking) :
    m_Impl{std::make_unique<Impl>()}
{
    cp_layer_renderer_t Renderer = static_cast<cp_layer_renderer_t>(pRenderer);
    if (Renderer == nullptr || pDevice == nullptr || pContext == nullptr)
    {
        UNEXPECTED("Renderer, pDevice and pContext must not be null");
        return;
    }

    m_Impl->Renderer = Renderer;
    m_Impl->pDevice  = pDevice;
    m_Impl->pContext = pContext;

    m_Impl->pDeviceMtl  = RefCntAutoPtr<IRenderDeviceMtl>{pDevice, IID_RenderDeviceMtl};
    m_Impl->pContextMtl = RefCntAutoPtr<IDeviceContextMtl>{pContext, IID_DeviceContextMtl};
    if (!m_Impl->pDeviceMtl || !m_Impl->pContextMtl)
    {
        UNEXPECTED("Failed to query Metal device/context interfaces");
        return;
    }

    if (EnableWorldTracking)
        m_Impl->StartWorldTracking();
}

CompositorServicesSession::~CompositorServicesSession()
{
    if (m_Impl)
        m_Impl->StopWorldTracking();
}

void CompositorServicesSession::RenderFrame(std::function<void()>                Update,
                                             std::function<void(void* pDrawable)> RenderDrawable)
{
    if (m_Impl->Renderer == nullptr)
        return;

    cp_frame_t Frame = cp_layer_renderer_query_next_frame(m_Impl->Renderer);
    if (Frame == nullptr)
        return;

    // Update phase: head-pose-independent work only.
    cp_frame_start_update(Frame);
    if (Update)
        Update();
    cp_frame_end_update(Frame);

    // Predict timing after end_update so the compositor accounts for the
    // actual update duration. Null means the layer is paused/invalidated -
    // in that case the frame has been cancelled and it is invalid to access.
    cp_frame_timing_t Timing = cp_frame_predict_timing(Frame);
    if (Timing == nullptr)
        return;
    cp_time_wait_until(cp_frame_timing_get_optimal_input_time(Timing));

    cp_frame_start_submission(Frame);

    cp_drawable_array_t Drawables = cp_frame_query_drawables(Frame);
    const size_t        Count     = cp_drawable_array_get_count(Drawables);
    // A zero-count array means the frame was cancelled; calling
    // cp_frame_end_submission on it would trigger a compositor "BUG IN CLIENT"
    // log, so we early-out without ending the submission phase.
    if (Count == 0)
        return;

    for (size_t Idx = 0; Idx < Count; ++Idx)
    {
        cp_drawable_t Drawable = cp_drawable_array_get_drawable(Drawables, Idx);
        if (Drawable == nullptr)
            continue;

        m_Impl->SetDrawableDeviceAnchor(Drawable);

        if (RenderDrawable)
            RenderDrawable(static_cast<void*>(Drawable));
    }

    cp_frame_end_submission(Frame);
}

Uint32 CompositorServicesSession::GetViewCount(void* pDrawable) const
{
    cp_drawable_t Drawable = static_cast<cp_drawable_t>(pDrawable);
    if (Drawable == nullptr)
    {
        UNEXPECTED("Drawable must not be null");
        return 0;
    }
    return static_cast<Uint32>(cp_drawable_get_view_count(Drawable));
}

RefCntAutoPtr<ITexture> CompositorServicesSession::GetColorSwapchainImage(void* pDrawable, Uint32 ViewIndex)
{
    cp_drawable_t Drawable = static_cast<cp_drawable_t>(pDrawable);
    if (Drawable == nullptr)
    {
        UNEXPECTED("Drawable must not be null");
        return {};
    }
    RefCntAutoPtr<ITexture> pTexture;
    id<MTLTexture> mtlTexture = cp_drawable_get_color_texture(Drawable, ViewIndex);
    m_Impl->CreateTextureFromDrawable(mtlTexture, /*IsDepth = */ false, &pTexture);
    return pTexture;
}

RefCntAutoPtr<ITexture> CompositorServicesSession::GetDepthSwapchainImage(void* pDrawable, Uint32 ViewIndex)
{
    cp_drawable_t Drawable = static_cast<cp_drawable_t>(pDrawable);
    if (Drawable == nullptr)
    {
        UNEXPECTED("Drawable must not be null");
        return {};
    }
    RefCntAutoPtr<ITexture> pTexture;
    id<MTLTexture> mtlTexture = cp_drawable_get_depth_texture(Drawable, ViewIndex);
    m_Impl->CreateTextureFromDrawable(mtlTexture, /*IsDepth = */ true, &pTexture);
    return pTexture;
}

float4x4 CompositorServicesSession::GetProjectionMatrix(void* pDrawable, Uint32 ViewIndex,
                                                         float NearZ, float FarZ) const
{
    cp_drawable_t Drawable = static_cast<cp_drawable_t>(pDrawable);
    if (Drawable == nullptr)
    {
        UNEXPECTED("Drawable must not be null");
        return float4x4::Identity();
    }

    // visionOS 2+ mandates reverse-Z: the compositor reads depth as near=1/far=0.
    // cp_drawable_set_depth_range() expects (far, near) in that order.
    cp_drawable_set_depth_range(Drawable, simd_make_float2(FarZ, NearZ));

    return ConvertSimdMatrix(cp_drawable_compute_projection(
        Drawable,
        cp_axis_direction_convention_right_up_back,
        static_cast<size_t>(ViewIndex)));
}

float4x4 CompositorServicesSession::GetViewMatrix(void* pDrawable, Uint32 ViewIndex) const
{
    cp_drawable_t Drawable = static_cast<cp_drawable_t>(pDrawable);
    if (Drawable == nullptr)
    {
        UNEXPECTED("Drawable must not be null");
        return float4x4::Identity();
    }

    cp_view_t     View      = cp_drawable_get_view(Drawable, ViewIndex);
    simd_float4x4 Transform = cp_view_get_transform(View);

    // `cp_view_get_transform` returns the eye's pose in the drawable's local
    // coordinate space (relative to the device anchor). To obtain a world-from-
    // eye transform, compose it with the cached device anchor:
    //
    //     world_from_eye = anchor_transform * view_transform
    //
    // Without the anchor contribution the scene stays rigidly attached to the
    // headset and does not respond to head motion.
    if (m_Impl->HasAnchor)
        Transform = simd_mul(m_Impl->LastAnchor, Transform);

    return ConvertSimdMatrix(simd_inverse(Transform));
}

void CompositorServicesSession::PresentDrawable(void* pDrawable)
{
    cp_drawable_t Drawable = static_cast<cp_drawable_t>(pDrawable);
    if (Drawable == nullptr)
    {
        UNEXPECTED("Drawable must not be null");
        return;
    }

    id<MTLCommandBuffer> mtlCmdBuffer = m_Impl->pContextMtl->GetMtlCommandBuffer();
    if (mtlCmdBuffer == nil)
    {
        UNEXPECTED("Failed to get Metal command buffer from device context");
        return;
    }

    cp_drawable_encode_present(Drawable, mtlCmdBuffer);
}

} // namespace Diligent
