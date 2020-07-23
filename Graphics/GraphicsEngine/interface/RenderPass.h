/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

// clang-format off

/// \file
/// Definition of the Diligent::IRenderPass interface and related data structures

#include "DeviceObject.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {B818DEC7-174D-447A-A8E4-94D21C57B40A}
static const struct INTERFACE_ID IID_RenderPass =
    { 0xb818dec7, 0x174d, 0x447a, { 0xa8, 0xe4, 0x94, 0xd2, 0x1c, 0x57, 0xb4, 0xa } };


/// Render pass attachment description.
struct RenderPassAttachmentDesc
{
    int Dummy;
};
typedef struct RenderPassAttachmentDesc RenderPassAttachmentDesc;


/// Render pass subpass decription.
struct SubpassDesc
{
    int Dummy;
};
typedef struct SubpassDesc SubpassDesc;


/// Subpass dependency description
struct SubpassDependencyDesc
{
    int Dummy;
};
typedef struct SubpassDependencyDesc SubpassDependencyDesc;

/// Render pass description
struct RenderPassDesc DILIGENT_DERIVE(DeviceObjectAttribs)

    /// The number of attachments.
    Uint32                           AttachmentCount    DEFAULT_INITIALIZER(0);

    /// Pointer to the array of subpass attachments, see Diligent::RenderPassAttachmentDesc.
    const RenderPassAttachmentDesc*  pAttachments       DEFAULT_INITIALIZER(nullptr);

    /// The number of subpasses.
    Uint32                           SubpassCount       DEFAULT_INITIALIZER(0);

    /// Pointer to the array of subpass descriptions, see Diligent::SubpassDesc.
    const SubpassDesc*               pSubpasses         DEFAULT_INITIALIZER(nullptr);

    /// The number of subpass dependencies.
    Uint32                           DependencyCount    DEFAULT_INITIALIZER(0);

    /// The array of subpass dependencies, see Diligent::SubpassDependencyDesc.
    const SubpassDependencyDesc*     pDependencies      DEFAULT_INITIALIZER(nullptr);
};
typedef struct RenderPassDesc RenderPassDesc;


#if DILIGENT_CPP_INTERFACE

/// Render pass interface

/// Render pass  has no methods.
class IRenderPass : public IDeviceObject
{
};

#else

struct IRenderPass;

//  C requires that a struct or union has at least one member
//struct IRenderPassMethods
//{
//};

struct IRenderPassVtbl
{
    struct IObjectMethods       Object;
    struct IDeviceObjectMethods DeviceObject;
    //struct IRenderPassMethods  RenderPass;
};

typedef struct IRenderPass
{
    struct IRenderPassVtbl* pVtbl;
} IRenderPass;

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
