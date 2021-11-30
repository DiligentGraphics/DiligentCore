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
    Ser(Desc.NumResources,
        Desc.NumImmutableSamplers,
        Desc.BindingIndex,
        Desc.UseCombinedTextureSamplers,
        Desc.CombinedSamplerSuffix);
    // skip Name
    // skip SRBAllocationGranularity

    auto* pResources = PSOSerializer_ArrayHelper<Mode>::Create(Desc.Resources, Desc.NumResources, Allocator);
    for (Uint32 r = 0; r < Desc.NumResources; ++r)
    {
        // Serialize PipelineResourceDesc
        auto& ResDesc = pResources[r];
        Ser(ResDesc.Name,
            ResDesc.ShaderStages,
            ResDesc.ArraySize,
            ResDesc.ResourceType,
            ResDesc.VarType,
            ResDesc.Flags);
    }

    auto* pImmutableSamplers = PSOSerializer_ArrayHelper<Mode>::Create(Desc.ImmutableSamplers, Desc.NumImmutableSamplers, Allocator);
    for (Uint32 s = 0; s < Desc.NumImmutableSamplers; ++s)
    {
        // Serialize ImmutableSamplerDesc
        auto& SampDesc = pImmutableSamplers[s];
        SerializeImmutableSampler(Ser, SampDesc);
    }

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

    // instead of ppResourceSignatures
    for (Uint32 i = 0; i < CreateInfo.ResourceSignaturesCount; ++i)
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
        Ser(InputLayout.NumElements);
        auto* pLayoutElements = PSOSerializer_ArrayHelper<Mode>::Create(InputLayout.LayoutElements, InputLayout.NumElements, Allocator);
        for (Uint32 i = 0; i < InputLayout.NumElements; ++i)
        {
            // Serialize LayoutElement
            auto& Elem = pLayoutElements[i];
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
        }
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
    {
        Ser(CreateInfo.GeneralShaderCount);
        auto* pGeneralShaders = PSOSerializer_ArrayHelper<Mode>::Create(CreateInfo.pGeneralShaders, CreateInfo.GeneralShaderCount, Allocator);
        for (Uint32 i = 0; i < CreateInfo.GeneralShaderCount; ++i)
        {
            auto&  Group       = pGeneralShaders[i];
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
        }
    }
    //  Serialize RayTracingTriangleHitShaderGroup
    {
        Ser(CreateInfo.TriangleHitShaderCount);
        auto* pTriangleHitShaders = PSOSerializer_ArrayHelper<Mode>::Create(CreateInfo.pTriangleHitShaders, CreateInfo.TriangleHitShaderCount, Allocator);
        for (Uint32 i = 0; i < CreateInfo.TriangleHitShaderCount; ++i)
        {
            auto&  Group                 = pTriangleHitShaders[i];
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
        }
    }
    //  Serialize RayTracingProceduralHitShaderGroup
    {
        Ser(CreateInfo.ProceduralHitShaderCount);
        auto* pProceduralHitShaders = PSOSerializer_ArrayHelper<Mode>::Create(CreateInfo.pProceduralHitShaders, CreateInfo.ProceduralHitShaderCount, Allocator);
        for (Uint32 i = 0; i < CreateInfo.ProceduralHitShaderCount; ++i)
        {
            auto&  Group                   = pProceduralHitShaders[i];
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
        }
    }

    // skip shaders - they are device specific

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
    // Serialize RenderPassDesc
    Ser(RPDesc.AttachmentCount,
        RPDesc.SubpassCount,
        RPDesc.DependencyCount);

    auto* pAttachments = PSOSerializer_ArrayHelper<Mode>::Create(RPDesc.pAttachments, RPDesc.AttachmentCount, Allocator);
    for (Uint32 i = 0; i < RPDesc.AttachmentCount; ++i)
    {
        // Serialize RenderPassAttachmentDesc
        auto& Attachment = pAttachments[i];
        Ser(Attachment.Format,
            Attachment.SampleCount,
            Attachment.LoadOp,
            Attachment.StoreOp,
            Attachment.StencilLoadOp,
            Attachment.StencilStoreOp,
            Attachment.InitialState,
            Attachment.FinalState);
    }

    auto* pSubpasses = PSOSerializer_ArrayHelper<Mode>::Create(RPDesc.pSubpasses, RPDesc.SubpassCount, Allocator);
    for (Uint32 i = 0; i < RPDesc.SubpassCount; ++i)
    {
        // Serialize SubpassDesc
        auto& Subpass                   = pSubpasses[i];
        bool  HasResolveAttachments     = Subpass.pResolveAttachments != nullptr;
        bool  HasDepthStencilAttachment = Subpass.pDepthStencilAttachment != nullptr;
        bool  HasShadingRateAttachment  = Subpass.pShadingRateAttachment != nullptr;

        Ser(Subpass.InputAttachmentCount,
            Subpass.RenderTargetAttachmentCount,
            Subpass.PreserveAttachmentCount,
            HasResolveAttachments,
            HasDepthStencilAttachment,
            HasShadingRateAttachment);

        auto* pInputAttachments = PSOSerializer_ArrayHelper<Mode>::Create(Subpass.pInputAttachments, Subpass.InputAttachmentCount, Allocator);
        for (Uint32 j = 0; j < Subpass.InputAttachmentCount; ++j)
        {
            auto& InputAttach = pInputAttachments[j];
            Ser(InputAttach.AttachmentIndex,
                InputAttach.State);
        }

        auto* pRenderTargetAttachments = PSOSerializer_ArrayHelper<Mode>::Create(Subpass.pRenderTargetAttachments, Subpass.RenderTargetAttachmentCount, Allocator);
        for (Uint32 j = 0; j < Subpass.RenderTargetAttachmentCount; ++j)
        {
            auto& RTAttach = pRenderTargetAttachments[j];
            Ser(RTAttach.AttachmentIndex,
                RTAttach.State);
        }

        auto* pPreserveAttachments = PSOSerializer_ArrayHelper<Mode>::Create(Subpass.pPreserveAttachments, Subpass.PreserveAttachmentCount, Allocator);
        for (Uint32 j = 0; j < Subpass.PreserveAttachmentCount; ++j)
        {
            auto& Attach = pPreserveAttachments[j];
            Ser(Attach);
        }

        if (HasResolveAttachments)
        {
            auto* pResolveAttachments = PSOSerializer_ArrayHelper<Mode>::Create(Subpass.pResolveAttachments, Subpass.RenderTargetAttachmentCount, Allocator);
            for (Uint32 j = 0; j < Subpass.RenderTargetAttachmentCount; ++j)
            {
                auto& ResAttach = pResolveAttachments[j];
                Ser(ResAttach.AttachmentIndex,
                    ResAttach.State);
            }
        }
        if (HasDepthStencilAttachment)
        {
            auto* pDepthStencilAttachment = PSOSerializer_ArrayHelper<Mode>::Create(Subpass.pDepthStencilAttachment, 1, Allocator);
            Ser(pDepthStencilAttachment->AttachmentIndex,
                pDepthStencilAttachment->State);
        }
        if (HasShadingRateAttachment)
        {
            auto* pShadingRateAttachment = PSOSerializer_ArrayHelper<Mode>::Create(Subpass.pShadingRateAttachment, 1, Allocator);
            Ser(pShadingRateAttachment->Attachment.AttachmentIndex,
                pShadingRateAttachment->Attachment.State,
                pShadingRateAttachment->TileSize);
        }
    }

    auto* pDependencies = PSOSerializer_ArrayHelper<Mode>::Create(RPDesc.pDependencies, RPDesc.DependencyCount, Allocator);
    for (Uint32 i = 0; i < RPDesc.DependencyCount; ++i)
    {
        // Serialize SubpassDependencyDesc
        auto& Dep = pDependencies[i];
        Ser(Dep.SrcSubpass,
            Dep.DstSubpass,
            Dep.SrcStageMask,
            Dep.DstStageMask,
            Dep.SrcAccessMask,
            Dep.DstAccessMask);
    }

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
