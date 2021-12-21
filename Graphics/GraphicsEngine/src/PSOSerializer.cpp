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

#include "PSOSerializer.hpp"

namespace Diligent
{

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializeImmutableSampler(
    Serializer<Mode>&            Ser,
    TQual<ImmutableSamplerDesc>& SampDesc)
{
    Ser(SampDesc.SamplerOrTextureName,
        SampDesc.ShaderStages,
        SampDesc.Desc.Name,
        SampDesc.Desc.MinFilter,
        SampDesc.Desc.MagFilter,
        SampDesc.Desc.MipFilter,
        SampDesc.Desc.AddressU,
        SampDesc.Desc.AddressV,
        SampDesc.Desc.AddressW,
        SampDesc.Desc.Flags,
        SampDesc.Desc.MipLODBias,
        SampDesc.Desc.MaxAnisotropy,
        SampDesc.Desc.ComparisonFunc,
        SampDesc.Desc.BorderColor,
        SampDesc.Desc.MinLOD,
        SampDesc.Desc.MaxLOD);

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(ImmutableSamplerDesc) == 72, "Did you add a new member to ImmutableSamplerDesc? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializePRSDesc(
    Serializer<Mode>&                               Ser,
    TQual<PipelineResourceSignatureDesc>&           Desc,
    TQual<PipelineResourceSignatureSerializedData>& Serialized,
    DynamicLinearAllocator*                         Allocator)
{
    // Serialize PipelineResourceSignatureDesc
    Ser(Desc.BindingIndex,
        Desc.UseCombinedTextureSamplers,
        Desc.CombinedSamplerSuffix);
    // skip Name
    // skip SRBAllocationGranularity

    SerializeArray(Ser, Allocator, Desc.Resources, Desc.NumResources,
                   [](Serializer<Mode>&            Ser,
                      TQual<PipelineResourceDesc>& ResDesc) //
                   {
                       Ser(ResDesc.Name,
                           ResDesc.ShaderStages,
                           ResDesc.ArraySize,
                           ResDesc.ResourceType,
                           ResDesc.VarType,
                           ResDesc.Flags);
                   });

    SerializeArray(Ser, Allocator, Desc.ImmutableSamplers, Desc.NumImmutableSamplers, SerializeImmutableSampler);

    // Serialize PipelineResourceSignatureSerializedData
    Ser(Serialized.ShaderStages,
        Serialized.StaticResShaderStages,
        Serialized.PipelineType,
        Serialized.StaticResStageIndex);

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(PipelineResourceSignatureDesc) == 56, "Did you add a new member to PipelineResourceSignatureDesc? Please add serialization here.");
    static_assert(sizeof(PipelineResourceDesc) == 24, "Did you add a new member to PipelineResourceDesc? Please add serialization here.");
    static_assert(sizeof(PipelineResourceSignatureSerializedData) == 16, "Did you add a new member to PipelineResourceSignatureSerializedData? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializePSOCreateInfo(
    Serializer<Mode>&               Ser,
    TQual<PipelineStateCreateInfo>& CreateInfo,
    TQual<TPRSNames>&               PRSNames,
    DynamicLinearAllocator*         Allocator)
{
    // Serialize PipelineStateCreateInfo
    //   Serialize PipelineStateDesc
    Ser(CreateInfo.PSODesc.PipelineType);
    Ser(CreateInfo.ResourceSignaturesCount,
        CreateInfo.Flags);
    // skip SRBAllocationGranularity
    // skip ImmediateContextMask
    // skip pPSOCache

    auto& ResourceLayout = CreateInfo.PSODesc.ResourceLayout;
    Ser(ResourceLayout.DefaultVariableType, ResourceLayout.DefaultVariableMergeStages);
    SerializeArray(Ser, Allocator, ResourceLayout.Variables, ResourceLayout.NumVariables,
                   [](Serializer<Mode>&                  Ser,
                      TQual<ShaderResourceVariableDesc>& VarDesc) //
                   {
                       Ser(VarDesc.Name,
                           VarDesc.ShaderStages,
                           VarDesc.Type,
                           VarDesc.Flags);
                   });

    SerializeArray(Ser, Allocator, ResourceLayout.ImmutableSamplers, ResourceLayout.NumImmutableSamplers, SerializeImmutableSampler);

    // instead of ppResourceSignatures
    for (Uint32 i = 0; i < std::max(CreateInfo.ResourceSignaturesCount, 1u); ++i)
    {
        Ser(PRSNames[i]);
    }

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(ShaderResourceVariableDesc) == 24, "Did you add a new member to ShaderResourceVariableDesc? Please add serialization here.");
    static_assert(sizeof(PipelineStateCreateInfo) == 96, "Did you add a new member to PipelineStateCreateInfo? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializePSOCreateInfo(
    Serializer<Mode>&                       Ser,
    TQual<GraphicsPipelineStateCreateInfo>& CreateInfo,
    TQual<TPRSNames>&                       PRSNames,
    DynamicLinearAllocator*                 Allocator,
    TQual<const char*>&                     RenderPassName)
{
    SerializePSOCreateInfo(Ser, static_cast<TQual<PipelineStateCreateInfo>&>(CreateInfo), PRSNames, Allocator);

    // Serialize GraphicsPipelineDesc
    Ser(CreateInfo.GraphicsPipeline.BlendDesc,
        CreateInfo.GraphicsPipeline.SampleMask,
        CreateInfo.GraphicsPipeline.RasterizerDesc,
        CreateInfo.GraphicsPipeline.DepthStencilDesc);

    //   Serialize InputLayoutDesc
    {
        auto& InputLayout = CreateInfo.GraphicsPipeline.InputLayout;
        SerializeArray(Ser, Allocator, InputLayout.LayoutElements, InputLayout.NumElements,
                       [](Serializer<Mode>&     Ser,
                          TQual<LayoutElement>& Elem) //
                       {
                           Ser(Elem.HLSLSemantic,
                               Elem.InputIndex,
                               Elem.BufferSlot,
                               Elem.NumComponents,
                               Elem.ValueType,
                               Elem.IsNormalized,
                               Elem.RelativeOffset,
                               Elem.Stride,
                               Elem.Frequency,
                               Elem.InstanceDataStepRate);
                       });
    }

    Ser(CreateInfo.GraphicsPipeline.PrimitiveTopology,
        CreateInfo.GraphicsPipeline.NumViewports,
        CreateInfo.GraphicsPipeline.NumRenderTargets,
        CreateInfo.GraphicsPipeline.SubpassIndex,
        CreateInfo.GraphicsPipeline.ShadingRateFlags,
        CreateInfo.GraphicsPipeline.RTVFormats,
        CreateInfo.GraphicsPipeline.DSVFormat,
        CreateInfo.GraphicsPipeline.SmplDesc,
        RenderPassName); // for CreateInfo.GraphicsPipeline.pRenderPass

    // skip NodeMask
    // skip shaders - they are device specific

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(GraphicsPipelineStateCreateInfo) == 344, "Did you add a new member to GraphicsPipelineStateCreateInfo? Please add serialization here.");
    static_assert(sizeof(LayoutElement) == 40, "Did you add a new member to LayoutElement? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializePSOCreateInfo(
    Serializer<Mode>&                      Ser,
    TQual<ComputePipelineStateCreateInfo>& CreateInfo,
    TQual<TPRSNames>&                      PRSNames,
    DynamicLinearAllocator*                Allocator)
{
    SerializePSOCreateInfo(Ser, static_cast<TQual<PipelineStateCreateInfo>&>(CreateInfo), PRSNames, Allocator);

    // skip shaders - they are device specific

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(ComputePipelineStateCreateInfo) == 104, "Did you add a new member to ComputePipelineStateCreateInfo? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializePSOCreateInfo(
    Serializer<Mode>&                   Ser,
    TQual<TilePipelineStateCreateInfo>& CreateInfo,
    TQual<TPRSNames>&                   PRSNames,
    DynamicLinearAllocator*             Allocator)
{
    SerializePSOCreateInfo(Ser, static_cast<TQual<PipelineStateCreateInfo>&>(CreateInfo), PRSNames, Allocator);

    // Serialize TilePipelineDesc
    Ser(CreateInfo.TilePipeline.NumRenderTargets,
        CreateInfo.TilePipeline.SampleCount,
        CreateInfo.TilePipeline.RTVFormats);

    // skip shaders - they are device specific

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(TilePipelineStateCreateInfo) == 128, "Did you add a new member to TilePipelineStateCreateInfo? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializePSOCreateInfo(
    Serializer<Mode>&                                     Ser,
    TQual<RayTracingPipelineStateCreateInfo>&             CreateInfo,
    TQual<TPRSNames>&                                     PRSNames,
    DynamicLinearAllocator*                               Allocator,
    const std::function<void(Uint32&, TQual<IShader*>&)>& ShaderToIndex)
{
    const bool IsReading = (Allocator != nullptr);
    const bool IsWriting = !IsReading;

    SerializePSOCreateInfo(Ser, static_cast<TQual<PipelineStateCreateInfo>&>(CreateInfo), PRSNames, Allocator);

    // Serialize RayTracingPipelineDesc
    Ser(CreateInfo.RayTracingPipeline.ShaderRecordSize,
        CreateInfo.RayTracingPipeline.MaxRecursionDepth);

    // Serialize RayTracingPipelineStateCreateInfo
    Ser(CreateInfo.pShaderRecordName,
        CreateInfo.MaxAttributeSize,
        CreateInfo.MaxPayloadSize);

    //  Serialize RayTracingGeneralShaderGroup
    SerializeArray(Ser, Allocator, CreateInfo.pGeneralShaders, CreateInfo.GeneralShaderCount,
                   [&](Serializer<Mode>&                    Ser,
                       TQual<RayTracingGeneralShaderGroup>& Group) //
                   {
                       Uint32 ShaderIndex = ~0u;
                       if (IsWriting)
                       {
                           ShaderToIndex(ShaderIndex, Group.pShader);
                       }
                       Ser(Group.Name, ShaderIndex);
                       VERIFY_EXPR(ShaderIndex != ~0u);
                       if (IsReading)
                       {
                           ShaderToIndex(ShaderIndex, Group.pShader);
                       }
                   });

    //  Serialize RayTracingTriangleHitShaderGroup
    SerializeArray(Ser, Allocator, CreateInfo.pTriangleHitShaders, CreateInfo.TriangleHitShaderCount,
                   [&](Serializer<Mode>&                        Ser,
                       TQual<RayTracingTriangleHitShaderGroup>& Group) //
                   {
                       Uint32 ClosestHitShaderIndex = ~0u;
                       Uint32 AnyHitShaderIndex     = ~0u;
                       if (IsWriting)
                       {
                           ShaderToIndex(ClosestHitShaderIndex, Group.pClosestHitShader);
                           ShaderToIndex(AnyHitShaderIndex, Group.pAnyHitShader);
                       }
                       Ser(Group.Name, ClosestHitShaderIndex, AnyHitShaderIndex);
                       VERIFY_EXPR(ClosestHitShaderIndex != ~0u);
                       if (IsReading)
                       {
                           ShaderToIndex(ClosestHitShaderIndex, Group.pClosestHitShader);
                           ShaderToIndex(AnyHitShaderIndex, Group.pAnyHitShader);
                       }
                   });

    //  Serialize RayTracingProceduralHitShaderGroup
    SerializeArray(Ser, Allocator, CreateInfo.pProceduralHitShaders, CreateInfo.ProceduralHitShaderCount,
                   [&](Serializer<Mode>&                          Ser,
                       TQual<RayTracingProceduralHitShaderGroup>& Group) //
                   {
                       Uint32 IntersectionShaderIndex = ~0u;
                       Uint32 ClosestHitShaderIndex   = ~0u;
                       Uint32 AnyHitShaderIndex       = ~0u;
                       if (IsWriting)
                       {
                           ShaderToIndex(IntersectionShaderIndex, Group.pIntersectionShader);
                           ShaderToIndex(ClosestHitShaderIndex, Group.pClosestHitShader);
                           ShaderToIndex(AnyHitShaderIndex, Group.pAnyHitShader);
                       }
                       Ser(Group.Name, IntersectionShaderIndex, ClosestHitShaderIndex, AnyHitShaderIndex);
                       VERIFY_EXPR(IntersectionShaderIndex != ~0u);
                       if (IsReading)
                       {
                           ShaderToIndex(IntersectionShaderIndex, Group.pIntersectionShader);
                           ShaderToIndex(ClosestHitShaderIndex, Group.pClosestHitShader);
                           ShaderToIndex(AnyHitShaderIndex, Group.pAnyHitShader);
                       }
                   });

    // skip shaders - they are device-specific

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(RayTracingPipelineStateCreateInfo) == 168, "Did you add a new member to RayTracingPipelineStateCreateInfo? Please add serialization here.");
    static_assert(sizeof(RayTracingGeneralShaderGroup) == 16, "Did you add a new member to RayTracingGeneralShaderGroup? Please add serialization here.");
    static_assert(sizeof(RayTracingTriangleHitShaderGroup) == 24, "Did you add a new member to RayTracingTriangleHitShaderGroup? Please add serialization here.");
    static_assert(sizeof(RayTracingProceduralHitShaderGroup) == 32, "Did you add a new member to RayTracingProceduralHitShaderGroup? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializeRenderPassDesc(
    Serializer<Mode>&       Ser,
    TQual<RenderPassDesc>&  RPDesc,
    DynamicLinearAllocator* Allocator)
{
    SerializeArray(Ser, Allocator, RPDesc.pAttachments, RPDesc.AttachmentCount,
                   [](Serializer<Mode>&                Ser,
                      TQual<RenderPassAttachmentDesc>& Attachment) //
                   {
                       Ser(Attachment.Format,
                           Attachment.SampleCount,
                           Attachment.LoadOp,
                           Attachment.StoreOp,
                           Attachment.StencilLoadOp,
                           Attachment.StencilStoreOp,
                           Attachment.InitialState,
                           Attachment.FinalState);
                   });

    SerializeArray(Ser, Allocator, RPDesc.pSubpasses, RPDesc.SubpassCount,
                   [&Allocator](Serializer<Mode>&   Ser,
                                TQual<SubpassDesc>& Subpass) //
                   {
                       auto SerializeAttachmentRef = [](Serializer<Mode>&           Ser,
                                                        TQual<AttachmentReference>& AttachRef) //
                       {
                           Ser(AttachRef.AttachmentIndex,
                               AttachRef.State);
                       };
                       SerializeArray(Ser, Allocator, Subpass.pInputAttachments, Subpass.InputAttachmentCount, SerializeAttachmentRef);
                       SerializeArray(Ser, Allocator, Subpass.pRenderTargetAttachments, Subpass.RenderTargetAttachmentCount, SerializeAttachmentRef);

                       // Note: when reading, ResolveAttachCount, DepthStencilAttachCount, and ShadingRateAttachCount will be overwritten
                       Uint32 ResolveAttachCount = Subpass.pResolveAttachments != nullptr ? Subpass.RenderTargetAttachmentCount : 0;
                       SerializeArray(Ser, Allocator, Subpass.pResolveAttachments, ResolveAttachCount, SerializeAttachmentRef);

                       Uint32 DepthStencilAttachCount = Subpass.pDepthStencilAttachment != nullptr ? 1 : 0;
                       SerializeArray(Ser, Allocator, Subpass.pDepthStencilAttachment, DepthStencilAttachCount, SerializeAttachmentRef);

                       SerializeArray(Ser, Allocator, Subpass.pPreserveAttachments, Subpass.PreserveAttachmentCount,
                                      [](Serializer<Mode>& Ser,
                                         TQual<Uint32>&    Attach) //
                                      {
                                          Ser(Attach);
                                      });

                       Uint32 ShadingRateAttachCount = Subpass.pShadingRateAttachment != nullptr ? 1 : 0;
                       SerializeArray(Ser, Allocator, Subpass.pShadingRateAttachment, ShadingRateAttachCount,
                                      [](Serializer<Mode>&             Ser,
                                         TQual<ShadingRateAttachment>& SRAttachment) //
                                      {
                                          Ser(SRAttachment.Attachment.AttachmentIndex,
                                              SRAttachment.Attachment.State,
                                              SRAttachment.TileSize);
                                      });
                   });

    SerializeArray(Ser, Allocator, RPDesc.pDependencies, RPDesc.DependencyCount,
                   [](Serializer<Mode>&             Ser,
                      TQual<SubpassDependencyDesc>& Dep) //
                   {
                       Ser(Dep.SrcSubpass,
                           Dep.DstSubpass,
                           Dep.SrcStageMask,
                           Dep.DstStageMask,
                           Dep.SrcAccessMask,
                           Dep.DstAccessMask);
                   });

#if defined(_MSC_VER) && defined(_WIN64)
    static_assert(sizeof(RenderPassDesc) == 56, "Did you add a new member to RenderPassDesc? Please add serialization here.");
    static_assert(sizeof(RenderPassAttachmentDesc) == 16, "Did you add a new member to RenderPassAttachmentDesc? Please add serialization here.");
    static_assert(sizeof(SubpassDesc) == 72, "Did you add a new member to SubpassDesc? Please add serialization here.");
    static_assert(sizeof(SubpassDependencyDesc) == 24, "Did you add a new member to SubpassDependencyDesc? Please add serialization here.");
    static_assert(sizeof(ShadingRateAttachment) == 16, "Did you add a new member to ShadingRateAttachment? Please add serialization here.");
    static_assert(sizeof(AttachmentReference) == 8, "Did you add a new member to AttachmentReference? Please add serialization here.");
#endif
}

template <SerializerMode Mode>
void PSOSerializer<Mode>::SerializeShaders(
    Serializer<Mode>&        Ser,
    TQual<ShaderIndexArray>& Shaders,
    DynamicLinearAllocator*  Allocator)
{
    Ser(Shaders.Count);

    auto* pIndices = PSOSerializer_ArrayHelper<Mode>::Create(Shaders.pIndices, Shaders.Count, Allocator);
    for (Uint32 i = 0; i < Shaders.Count; ++i)
        Ser(pIndices[i]);
}

template struct PSOSerializer<SerializerMode::Read>;
template struct PSOSerializer<SerializerMode::Write>;
template struct PSOSerializer<SerializerMode::Measure>;

} // namespace Diligent
