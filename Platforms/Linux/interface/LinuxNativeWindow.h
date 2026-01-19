/*
 *  Copyright 2019-2026 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

/// \file Defines LinuxNativeWindow structure that contains platform-native window handles

#include "../../../Primitives/interface/CommonDefinitions.h"
#include "../../../Primitives/interface/BasicTypes.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

/// Platform-native handles for creating a Vulkan surface on Linux (XCB, Xlib, Wayland).
///
/// Surface creation selection (in order):
/// - **XCB** (`VK_USE_PLATFORM_XCB_KHR`): use `pXCBConnection` + `WindowId` (both valid) -> `vkCreateXcbSurfaceKHR`.
/// - **Xlib** (`VK_USE_PLATFORM_XLIB_KHR`): use `pDisplay` + `WindowId` (both valid) and no surface yet -> `vkCreateXlibSurfaceKHR`.
/// - **Wayland** (`VK_USE_PLATFORM_WAYLAND_KHR`): use `pDisplay` + `pWaylandSurface` (both valid) and no surface yet -> `vkCreateWaylandSurfaceKHR`.
///
/// Notes:
/// - Populate only the members for the active backend; leave others null/zero.
/// - pDisplay is backend-dependent: `Display*` (Xlib) or `wl_display*` (Wayland).
struct LinuxNativeWindow
{
    /// Native window ID for X11 backends (XCB/Xlib). Must be non-zero to create an X11 surface.
    Uint32 WindowId DEFAULT_INITIALIZER(0);

    /// Display handle: `Display*` (Xlib) or `wl_display*` (Wayland).
    void* pDisplay DEFAULT_INITIALIZER(nullptr);

    /// XCB connection handle: `xcb_connection_t*` (XCB only).
    void* pXCBConnection DEFAULT_INITIALIZER(nullptr);

    /// Wayland surface handle: `wl_surface*` (Wayland only).
    void* pWaylandSurface DEFAULT_INITIALIZER(nullptr);
};
DILIGENT_END_NAMESPACE // namespace Diligent
