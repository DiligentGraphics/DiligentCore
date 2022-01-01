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
struct PSOSerializer_ArrayHelper
{
    template <typename T>
    static const T* Create(const T*                SrcArray,
                           Uint32                  Count,
                           DynamicLinearAllocator* Allocator)
    {
        VERIFY_EXPR(Allocator == nullptr);
        VERIFY_EXPR((SrcArray != nullptr) == (Count != 0));
        return SrcArray;
    }
};

template <>
struct PSOSerializer_ArrayHelper<SerializerMode::Read>
{
    template <typename T>
    static T* Create(const T*&               DstArray,
                     Uint32                  Count,
                     DynamicLinearAllocator* Allocator)
    {
        VERIFY_EXPR(Allocator != nullptr);
        VERIFY_EXPR(DstArray == nullptr);
        auto* pArray = Allocator->ConstructArray<T>(Count);
        DstArray     = pArray;
        return pArray;
    }
};


template <SerializerMode Mode>
struct PSOSerializer
{
    template <typename T>
    using TQual = typename Serializer<Mode>::template TQual<T>;

    using TPRSNames        = DeviceObjectArchiveBase::TPRSNames;
    using ShaderIndexArray = DeviceObjectArchiveBase::ShaderIndexArray;


    template <typename ArrayElemType, typename CountType, typename ArrayElemSerializerType>
    static void SerializeArray(Serializer<Mode>&       Ser,
                               DynamicLinearAllocator* Allocator,
                               ArrayElemType&          Elements,
                               CountType&              Count,
                               ArrayElemSerializerType ElemSerializer);

    template <typename ArrayElemType, typename CountType>
    static void SerializeArrayRaw(Serializer<Mode>&       Ser,
                                  DynamicLinearAllocator* Allocator,
                                  ArrayElemType&          Elements,
                                  CountType&              Count);

    static void SerializeImmutableSampler(Serializer<Mode>&            Ser,
                                          TQual<ImmutableSamplerDesc>& SampDesc);

    static void SerializePRSDesc(Serializer<Mode>&                               Ser,
                                 TQual<PipelineResourceSignatureDesc>&           Desc,
                                 TQual<PipelineResourceSignatureSerializedData>& Serialized,
                                 DynamicLinearAllocator*                         Allocator);

    static void SerializePSOCreateInfo(Serializer<Mode>&               Ser,
                                       TQual<PipelineStateCreateInfo>& CreateInfo,
                                       TQual<TPRSNames>&               PRSNames,
                                       DynamicLinearAllocator*         Allocator);

    static void SerializePSOCreateInfo(Serializer<Mode>&                       Ser,
                                       TQual<GraphicsPipelineStateCreateInfo>& CreateInfo,
                                       TQual<TPRSNames>&                       PRSNames,
                                       DynamicLinearAllocator*                 Allocator,
                                       TQual<const char*>&                     RenderPassName);

    static void SerializePSOCreateInfo(Serializer<Mode>&                      Ser,
                                       TQual<ComputePipelineStateCreateInfo>& CreateInfo,
                                       TQual<TPRSNames>&                      PRSNames,
                                       DynamicLinearAllocator*                Allocator);

    static void SerializePSOCreateInfo(Serializer<Mode>&                   Ser,
                                       TQual<TilePipelineStateCreateInfo>& CreateInfo,
                                       TQual<TPRSNames>&                   PRSNames,
                                       DynamicLinearAllocator*             Allocator);

    static void SerializePSOCreateInfo(Serializer<Mode>&                                     Ser,
                                       TQual<RayTracingPipelineStateCreateInfo>&             CreateInfo,
                                       TQual<TPRSNames>&                                     PRSNames,
                                       DynamicLinearAllocator*                               Allocator,
                                       const std::function<void(Uint32&, TQual<IShader*>&)>& ShaderToIndex);

    static void SerializeRenderPassDesc(Serializer<Mode>&       Ser,
                                        TQual<RenderPassDesc>&  RPDesc,
                                        DynamicLinearAllocator* Allocator);

    static void SerializeShaders(Serializer<Mode>&        Ser,
                                 TQual<ShaderIndexArray>& Shaders,
                                 DynamicLinearAllocator*  Allocator);
};

template <SerializerMode Mode>
template <typename ArrayElemType, typename CountType, typename ArrayElemSerializerType>
void PSOSerializer<Mode>::SerializeArray(Serializer<Mode>&       Ser,
                                         DynamicLinearAllocator* Allocator,
                                         ArrayElemType&          Elements,
                                         CountType&              Count,
                                         ArrayElemSerializerType ElemSerializer)
{
    Ser(Count);
    auto* pElements = PSOSerializer_ArrayHelper<Mode>::Create(Elements, Count, Allocator);
    for (Uint32 i = 0; i < Count; ++i)
    {
        ElemSerializer(Ser, pElements[i]);
    }
}

template <SerializerMode Mode>
template <typename ArrayElemType, typename CountType>
void PSOSerializer<Mode>::SerializeArrayRaw(Serializer<Mode>&       Ser,
                                            DynamicLinearAllocator* Allocator,
                                            ArrayElemType&          Elements,
                                            CountType&              Count)
{
    SerializeArray(Ser, Allocator, Elements, Count,
                   [](Serializer<Mode>& Ser,
                      auto&             Elem) //
                   {
                       Ser(Elem);
                   });
}

DECL_TRIVIALLY_SERIALIZABLE(BlendStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(RasterizerStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(DepthStencilStateDesc);
DECL_TRIVIALLY_SERIALIZABLE(SampleDesc);

} // namespace Diligent
