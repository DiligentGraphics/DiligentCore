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
/// Defines Diligent::IArchiveBuilderFactory interface

#include "../../../Primitives/interface/Object.h"
#include "ArchiveBuilder.h"
#include "Shader.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {F20B91EB-BDE3-4615-81CC-F720AA32410E}
static const INTERFACE_ID IID_ArchiveBuilderFactory =
    {0xf20b91eb, 0xbde3, 0x4615, {0x81, 0xcc, 0xf7, 0x20, 0xaa, 0x32, 0x41, 0xe}};

#define DILIGENT_INTERFACE_NAME IArchiveBuilderFactory
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IArchiveBuilderFactoryInclusiveMethods \
    IObjectInclusiveMethods;                   \
    IArchiveBuilderFactoryMethods ArchiveBuilderFactory

// clang-format off

DILIGENT_BEGIN_INTERFACE(IArchiveBuilderFactory, IObject)
{
    // AZ TODO
    VIRTUAL void METHOD(CreateArchiveBuilder)(THIS_
                                              IArchiveBuilder** ppBuilder) PURE;
    
    // AZ TODO
    VIRTUAL void METHOD(CreateShader)(THIS_
                                      const ShaderCreateInfo REF ShaderCI,
                                      Uint32                     DeviceBits,
                                      IShader**                  ppShader) PURE;
    
    // AZ TODO
    VIRTUAL void METHOD(CreateRenderPass)(THIS_
                                          const RenderPassDesc REF Desc,
                                          IRenderPass**            ppRenderPass) PURE;
    
    // AZ TODO
    VIRTUAL void METHOD(CreatePipelineResourceSignature)(THIS_
                                                         const PipelineResourceSignatureDesc REF Desc,
                                                         Uint32                                  DeviceBits,
                                                         IPipelineResourceSignature**            ppSignature) PURE;
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

#    define IArchiveBuilderFactory_CreateArchiveBuilder(This, ...)             CALL_IFACE_METHOD(ArchiveBuilderFactory, CreateArchiveBuilder,            This, __VA_ARGS__)
#    define IArchiveBuilderFactory_CreateShader(This, ...)                     CALL_IFACE_METHOD(ArchiveBuilderFactory, CreateShader,                    This, __VA_ARGS__)
#    define IArchiveBuilderFactory_CreateRenderPass(This, ...)                 CALL_IFACE_METHOD(ArchiveBuilderFactory, CreateRenderPass,                This, __VA_ARGS__)
#    define IArchiveBuilderFactory_CreatePipelineResourceSignature(This, ...)  CALL_IFACE_METHOD(ArchiveBuilderFactory, CreatePipelineResourceSignature, This, __VA_ARGS__)

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
