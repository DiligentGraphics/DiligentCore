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
/// Super resolution upscaler variant IDs

#include "InterfaceID.h"

namespace Diligent
{

// {7B3A8D2E-1F4C-4E9A-B5D0-6C8E2F1A3B5D}
static constexpr INTERFACE_ID VariantId_DLSS =
    {0x7b3a8d2e, 0x1f4c, 0x4e9a, {0xb5, 0xd0, 0x6c, 0x8e, 0x2f, 0x1a, 0x3b, 0x5d}};

// {C4D70001-A1B2-4C3D-8E9F-0A1B2C3D4E5F}
static constexpr INTERFACE_ID VariantId_MetalFXSpatial =
    {0xc4d70001, 0xa1b2, 0x4c3d, {0x8e, 0x9f, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f}};

// {C4D70002-A1B2-4C3D-8E9F-0A1B2C3D4E5F}
static constexpr INTERFACE_ID VariantId_MetalFXTemporal =
    {0xc4d70002, 0xa1b2, 0x4c3d, {0x8e, 0x9f, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f}};

// {F5A10001-B2C3-4D5E-9F01-2A3B4C5D6E7F}
static constexpr INTERFACE_ID VariantId_FSRSpatial =
    {0xf5a10001, 0xb2c3, 0x4d5e, {0x9f, 0x01, 0x2a, 0x3b, 0x4c, 0x5d, 0x6e, 0x7f}};

} // namespace Diligent
