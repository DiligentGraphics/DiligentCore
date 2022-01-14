/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include "Serializer.hpp"
#include "DynamicLinearAllocator.hpp"
#include "DeviceObjectArchiveBase.hpp"

namespace Diligent
{

template <SerializerMode Mode>
struct PSOSerializer
{
    template <typename T>
    using ConstQual = typename Serializer<Mode>::template ConstQual<T>;

    using TPRSNames        = DeviceObjectArchiveBase::TPRSNames;
    using ShaderIndexArray = DeviceObjectArchiveBase::ShaderIndexArray;

    static void SerializeImmutableSampler(Serializer<Mode>&                Ser,
                                          ConstQual<ImmutableSamplerDesc>& SampDesc);

    static void SerializePRSDesc(Serializer<Mode>&                         Ser,
                                 ConstQual<PipelineResourceSignatureDesc>& Desc,
                                 DynamicLinearAllocator*                   Allocator);

    static void SerializePRSInternalData(Serializer<Mode>&                                 Ser,
                                         ConstQual<PipelineResourceSignatureInternalData>& InternalData,
                                         DynamicLinearAllocator*                           Allocator);

    static void SerializePSOCreateInfo(Serializer<Mode>&                   Ser,
                                       ConstQual<PipelineStateCreateInfo>& CreateInfo,
                                       ConstQual<TPRSNames>&               PRSNames,
                                       DynamicLinearAllocator*             Allocator);

    static void SerializePSOCreateInfo(Serializer<Mode>&                           Ser,
                                       ConstQual<GraphicsPipelineStateCreateInfo>& CreateInfo,
                                       ConstQual<TPRSNames>&                       PRSNames,
                                       DynamicLinearAllocator*                     Allocator,
                                       ConstQual<const char*>&                     RenderPassName);

    static void SerializePSOCreateInfo(Serializer<Mode>&                          Ser,
                                       ConstQual<ComputePipelineStateCreateInfo>& CreateInfo,
                                       ConstQual<TPRSNames>&                      PRSNames,
                                       DynamicLinearAllocator*                    Allocator);

    static void SerializePSOCreateInfo(Serializer<Mode>&                       Ser,
                                       ConstQual<TilePipelineStateCreateInfo>& CreateInfo,
                                       ConstQual<TPRSNames>&                   PRSNames,
                                       DynamicLinearAllocator*                 Allocator);

    static void SerializePSOCreateInfo(Serializer<Mode>&                                         Ser,
                                       ConstQual<RayTracingPipelineStateCreateInfo>&             CreateInfo,
                                       ConstQual<TPRSNames>&                                     PRSNames,
                                       DynamicLinearAllocator*                                   Allocator,
                                       const std::function<void(Uint32&, ConstQual<IShader*>&)>& ShaderToIndex);

    static void SerializeRenderPassDesc(Serializer<Mode>&          Ser,
                                        ConstQual<RenderPassDesc>& RPDesc,
                                        DynamicLinearAllocator*    Allocator);

    static void SerializeShaders(Serializer<Mode>&            Ser,
                                 ConstQual<ShaderIndexArray>& Shaders,
                                 DynamicLinearAllocator*      Allocator);
};

DECL_TRIVIALLY_SERIALIZABLE(BlendStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(RasterizerStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(DepthStencilStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(SampleDesc);

} // namespace Diligent
