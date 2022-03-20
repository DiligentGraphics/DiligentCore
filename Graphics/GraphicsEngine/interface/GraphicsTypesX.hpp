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

/// \file
/// C++ struct wrappers for basic types.

#include <vector>
#include <utility>
#include <string>
#include <unordered_set>

#include "RenderPass.h"
#include "InputLayout.h"
#include "Framebuffer.h"
#include "PipelineResourceSignature.h"
#include "../../../Platforms/Basic/interface/DebugUtilities.hpp"

namespace Diligent
{

/// C++ wrapper over Diligent::SubpassDesc struct.
struct SubpassDescX
{
    SubpassDescX() noexcept
    {}

    SubpassDescX(const SubpassDesc& _Desc) :
        Desc{_Desc}
    {
        auto CopyAttachments = [](auto*& pAttachments, Uint32 Count, auto& Attachments) {
            if (Count != 0)
            {
                VERIFY_EXPR(pAttachments != nullptr);
                Attachments.assign(pAttachments, pAttachments + Count);
                pAttachments = Attachments.data();
            }
            else
            {
                pAttachments = nullptr;
            }
        };
        CopyAttachments(Desc.pInputAttachments, Desc.InputAttachmentCount, Inputs);
        CopyAttachments(Desc.pRenderTargetAttachments, Desc.RenderTargetAttachmentCount, RenderTargets);
        if (Desc.pResolveAttachments != nullptr)
            CopyAttachments(Desc.pResolveAttachments, Desc.RenderTargetAttachmentCount, Resolves);
        CopyAttachments(Desc.pPreserveAttachments, Desc.PreserveAttachmentCount, Preserves);

        SetDepthStencil(Desc.pDepthStencilAttachment);
        SetShadingRate(Desc.pShadingRateAttachment);
    }

    SubpassDescX(const SubpassDescX& _DescX) :
        SubpassDescX{static_cast<const SubpassDesc&>(_DescX)}
    {}

    SubpassDescX& operator=(const SubpassDescX& _DescX)
    {
        SubpassDescX Copy{_DescX};
        Swap(Copy);
        return *this;
    }

    SubpassDescX(SubpassDescX&&) = default;
    SubpassDescX& operator=(SubpassDescX&&) = default;

    SubpassDescX& AddRenderTarget(const AttachmentReference& RenderTarget, const AttachmentReference* pResolve = nullptr)
    {
        RenderTargets.push_back(RenderTarget);
        Desc.pRenderTargetAttachments    = RenderTargets.data();
        Desc.RenderTargetAttachmentCount = static_cast<Uint32>(RenderTargets.size());

        if (pResolve != nullptr)
        {
            VERIFY_EXPR(Resolves.size() < RenderTargets.size());
            while (Resolves.size() + 1 < RenderTargets.size())
                Resolves.push_back(AttachmentReference{ATTACHMENT_UNUSED, RESOURCE_STATE_UNKNOWN});
            Resolves.push_back(*pResolve);
            VERIFY_EXPR(Resolves.size() == RenderTargets.size());
            Desc.pResolveAttachments = Resolves.data();
        }

        return *this;
    }

    SubpassDescX& AddInput(const AttachmentReference& Input)
    {
        Inputs.push_back(Input);
        Desc.pInputAttachments    = Inputs.data();
        Desc.InputAttachmentCount = static_cast<Uint32>(Inputs.size());
        return *this;
    }

    SubpassDescX& AddPreserve(Uint32 Preserve)
    {
        Preserves.push_back(Preserve);
        Desc.pPreserveAttachments    = Preserves.data();
        Desc.PreserveAttachmentCount = static_cast<Uint32>(Preserves.size());
        return *this;
    }

    SubpassDescX& SetDepthStencil(const AttachmentReference* pDepthStencilAttachment)
    {
        DepthStencil                 = (pDepthStencilAttachment != nullptr) ? *pDepthStencilAttachment : AttachmentReference{};
        Desc.pDepthStencilAttachment = (pDepthStencilAttachment != nullptr) ? &DepthStencil : nullptr;
        return *this;
    }

    SubpassDescX& SetDepthStencil(const AttachmentReference& DepthStencilAttachment)
    {
        return SetDepthStencil(&DepthStencilAttachment);
    }

    SubpassDescX& SetShadingRate(const ShadingRateAttachment* pShadingRateAttachment)
    {
        ShadingRate                 = (pShadingRateAttachment != nullptr) ? *pShadingRateAttachment : ShadingRateAttachment{};
        Desc.pShadingRateAttachment = (pShadingRateAttachment != nullptr) ? &ShadingRate : nullptr;
        return *this;
    }

    SubpassDescX& SetShadingRate(const ShadingRateAttachment& ShadingRateAttachment)
    {
        return SetShadingRate(&ShadingRateAttachment);
    }

    void ClearInputs()
    {
        Inputs.clear();
        Desc.InputAttachmentCount = 0;
        Desc.pInputAttachments    = nullptr;
    }

    void ClearRenderTargets()
    {
        RenderTargets.clear();
        Resolves.clear();
        Desc.RenderTargetAttachmentCount = 0;
        Desc.pRenderTargetAttachments    = nullptr;
        Desc.pResolveAttachments         = nullptr;
    }

    void ClearPreserves()
    {
        Preserves.clear();
        Desc.PreserveAttachmentCount = 0;
        Desc.pPreserveAttachments    = nullptr;
    }

    const SubpassDesc& Get() const
    {
        return Desc;
    }

    operator const SubpassDesc&() const
    {
        return Desc;
    }

    bool operator==(const SubpassDesc& RHS) const
    {
        return static_cast<const SubpassDesc&>(*this) == RHS;
    }
    bool operator!=(const SubpassDesc& RHS) const
    {
        return !(static_cast<const SubpassDesc&>(*this) == RHS);
    }

    bool operator==(const SubpassDescX& RHS) const
    {
        return *this == static_cast<const SubpassDesc&>(RHS);
    }
    bool operator!=(const SubpassDescX& RHS) const
    {
        return *this != static_cast<const SubpassDesc&>(RHS);
    }

    void Clear()
    {
        SubpassDescX CleanDesc;
        Swap(CleanDesc);
    }

    void Swap(SubpassDescX& Other)
    {
        std::swap(*this, Other);

        if (Desc.pDepthStencilAttachment != nullptr)
            Desc.pDepthStencilAttachment = &DepthStencil;
        if (Desc.pShadingRateAttachment != nullptr)
            Desc.pShadingRateAttachment = &ShadingRate;

        if (Other.Desc.pDepthStencilAttachment != nullptr)
            Other.Desc.pDepthStencilAttachment = &Other.DepthStencil;
        if (Other.Desc.pShadingRateAttachment != nullptr)
            Other.Desc.pShadingRateAttachment = &Other.ShadingRate;
    }

private:
    SubpassDesc Desc;

    std::vector<AttachmentReference> Inputs;
    std::vector<AttachmentReference> RenderTargets;
    std::vector<AttachmentReference> Resolves;
    std::vector<Uint32>              Preserves;

    AttachmentReference   DepthStencil;
    ShadingRateAttachment ShadingRate;
};


/// C++ wrapper over Diligent::RenderPassDesc.
struct RenderPassDescX
{
    RenderPassDescX() noexcept
    {}

    RenderPassDescX(const RenderPassDesc& _Desc) :
        Desc{_Desc}
    {
        if (Desc.AttachmentCount != 0)
        {
            Attachments.assign(Desc.pAttachments, Desc.pAttachments + Desc.AttachmentCount);
            Desc.pAttachments = Attachments.data();
        }

        if (Desc.SubpassCount != 0)
        {
            Subpasses.assign(Desc.pSubpasses, Desc.pSubpasses + Desc.SubpassCount);
            Desc.pSubpasses = Subpasses.data();
        }

        if (Desc.DependencyCount != 0)
        {
            Dependencies.assign(Desc.pDependencies, Desc.pDependencies + Desc.DependencyCount);
            Desc.pDependencies = Dependencies.data();
        }
    }

    RenderPassDescX(const RenderPassDescX& _DescX) :
        RenderPassDescX{static_cast<const RenderPassDesc&>(_DescX)}
    {}

    RenderPassDescX& operator=(const RenderPassDescX& _DescX)
    {
        RenderPassDescX Copy{_DescX};
        std::swap(*this, Copy);
        return *this;
    }

    RenderPassDescX(RenderPassDescX&&) = default;
    RenderPassDescX& operator=(RenderPassDescX&&) = default;


    RenderPassDescX& AddAttachment(const RenderPassAttachmentDesc& Attachment)
    {
        Attachments.push_back(Attachment);
        Desc.AttachmentCount = static_cast<Uint32>(Attachments.size());
        Desc.pAttachments    = Attachments.data();
        return *this;
    }

    RenderPassDescX& AddSubpass(const SubpassDesc& Subpass)
    {
        Subpasses.push_back(Subpass);
        Desc.SubpassCount = static_cast<Uint32>(Subpasses.size());
        Desc.pSubpasses   = Subpasses.data();
        return *this;
    }

    RenderPassDescX& AddDependency(const SubpassDependencyDesc& Dependency)
    {
        Dependencies.push_back(Dependency);
        Desc.DependencyCount = static_cast<Uint32>(Dependencies.size());
        Desc.pDependencies   = Dependencies.data();
        return *this;
    }


    void ClearAttachments()
    {
        Attachments.clear();
        Desc.AttachmentCount = 0;
        Desc.pAttachments    = nullptr;
    }

    void ClearSubpasses()
    {
        Subpasses.clear();
        Desc.SubpassCount = 0;
        Desc.pSubpasses   = nullptr;
    }

    void ClearDependencies()
    {
        Dependencies.clear();
        Desc.DependencyCount = 0;
        Desc.pDependencies   = nullptr;
    }

    const RenderPassDesc& Get() const
    {
        return Desc;
    }

    operator const RenderPassDesc&() const
    {
        return Desc;
    }

    bool operator==(const RenderPassDesc& RHS) const
    {
        return static_cast<const RenderPassDesc&>(*this) == RHS;
    }
    bool operator!=(const RenderPassDesc& RHS) const
    {
        return !(static_cast<const RenderPassDesc&>(*this) == RHS);
    }

    bool operator==(const RenderPassDescX& RHS) const
    {
        return *this == static_cast<const RenderPassDesc&>(RHS);
    }
    bool operator!=(const RenderPassDescX& RHS) const
    {
        return *this != static_cast<const RenderPassDesc&>(RHS);
    }

    void Clear()
    {
        RenderPassDescX CleanDesc;
        std::swap(*this, CleanDesc);
    }

private:
    RenderPassDesc Desc;

    std::vector<RenderPassAttachmentDesc> Attachments;
    std::vector<SubpassDesc>              Subpasses;
    std::vector<SubpassDependencyDesc>    Dependencies;
};

/// C++ wrapper over InputLayoutDesc.
struct InputLayoutDescX
{
    InputLayoutDescX() noexcept
    {}

    InputLayoutDescX(const InputLayoutDesc& _Desc) :
        Desc{_Desc}
    {
        if (Desc.NumElements != 0)
        {
            Elements.assign(Desc.LayoutElements, Desc.LayoutElements + Desc.NumElements);
            Desc.LayoutElements = Elements.data();
        }
    }

    InputLayoutDescX(const std::initializer_list<LayoutElement>& _Elements) :
        Elements{_Elements}
    {
        Desc.NumElements    = static_cast<Uint32>(Elements.size());
        Desc.LayoutElements = !Elements.empty() ? Elements.data() : nullptr;
    }

    InputLayoutDescX(const InputLayoutDescX& _DescX) :
        InputLayoutDescX{static_cast<const InputLayoutDesc&>(_DescX)}
    {}

    InputLayoutDescX& operator=(const InputLayoutDescX& _DescX)
    {
        InputLayoutDescX Copy{_DescX};
        std::swap(*this, Copy);
        return *this;
    }

    InputLayoutDescX(InputLayoutDescX&&) = default;
    InputLayoutDescX& operator=(InputLayoutDescX&&) = default;

    InputLayoutDescX& Add(const LayoutElement& Elem)
    {
        Elements.push_back(Elem);
        Desc.NumElements    = static_cast<Uint32>(Elements.size());
        Desc.LayoutElements = Elements.data();
        return *this;
    }

    template <typename... ArgsType>
    InputLayoutDescX& Add(ArgsType&&... args)
    {
        Elements.emplace_back(std::forward<ArgsType>(args)...);
        Desc.NumElements    = static_cast<Uint32>(Elements.size());
        Desc.LayoutElements = Elements.data();
        return *this;
    }

    void Clear()
    {
        Elements.clear();
        Desc.NumElements    = 0;
        Desc.LayoutElements = nullptr;
    }

    const InputLayoutDesc& Get() const
    {
        return Desc;
    }

    operator const InputLayoutDesc&() const
    {
        return Desc;
    }

    bool operator==(const InputLayoutDesc& RHS) const
    {
        return static_cast<const InputLayoutDesc&>(*this) == RHS;
    }
    bool operator!=(const InputLayoutDesc& RHS) const
    {
        return !(static_cast<const InputLayoutDesc&>(*this) == RHS);
    }

    bool operator==(const InputLayoutDescX& RHS) const
    {
        return *this == static_cast<const InputLayoutDesc&>(RHS);
    }
    bool operator!=(const InputLayoutDescX& RHS) const
    {
        return *this != static_cast<const InputLayoutDesc&>(RHS);
    }

private:
    InputLayoutDesc            Desc;
    std::vector<LayoutElement> Elements;
};


template <typename BaseType>
struct DeviceObjectAttribsX : BaseType
{
    DeviceObjectAttribsX() noexcept
    {}

    DeviceObjectAttribsX(const BaseType& Base) :
        BaseType{Base},
        NameCopy{Base.Name != nullptr ? Base.Name : ""}
    {
        this->Name = NameCopy.c_str();
    }

    DeviceObjectAttribsX(const DeviceObjectAttribsX& Other) noexcept :
        BaseType{Other},
        NameCopy{Other.NameCopy}
    {
        this->Name = NameCopy.c_str();
    }

    DeviceObjectAttribsX(DeviceObjectAttribsX&& Other) noexcept :
        BaseType{std::move(Other)},
        NameCopy{std::move(Other.NameCopy)}
    {
        this->Name = NameCopy.c_str();
    }

    DeviceObjectAttribsX& operator=(const DeviceObjectAttribsX& Other) noexcept
    {
        static_cast<BaseType&>(*this) = Other;

        NameCopy   = Other.NameCopy;
        this->Name = NameCopy.c_str();

        return *this;
    }

    DeviceObjectAttribsX& operator=(const DeviceObjectAttribsX&& Other) noexcept
    {
        static_cast<BaseType&>(*this) = std::move(Other);

        NameCopy   = std::move(Other.NameCopy);
        this->Name = NameCopy.c_str();

        return *this;
    }

    void SetName(const char* NewName)
    {
        NameCopy   = NewName != nullptr ? NewName : "";
        this->Name = NameCopy.c_str();
    }

private:
    std::string NameCopy;
};

/// C++ wrapper over FramebufferDesc.
struct FramebufferDescX : DeviceObjectAttribsX<FramebufferDesc>
{
    using TBase = DeviceObjectAttribsX<FramebufferDesc>;

    FramebufferDescX() noexcept
    {}

    FramebufferDescX(const FramebufferDesc& _Desc) :
        TBase{_Desc}
    {
        if (AttachmentCount != 0)
        {
            Attachments.assign(ppAttachments, ppAttachments + AttachmentCount);
            ppAttachments = Attachments.data();
        }
    }

    FramebufferDescX(const FramebufferDescX& _DescX) :
        FramebufferDescX{static_cast<const FramebufferDesc&>(_DescX)}
    {}

    FramebufferDescX& operator=(const FramebufferDescX& _DescX)
    {
        FramebufferDescX Copy{_DescX};
        std::swap(*this, Copy);
        return *this;
    }

    FramebufferDescX(FramebufferDescX&& RHS) = default;
    FramebufferDescX& operator=(FramebufferDescX&&) = default;

    FramebufferDescX& AddAttachment(ITextureView* pView)
    {
        Attachments.push_back(pView);
        AttachmentCount = static_cast<Uint32>(Attachments.size());
        ppAttachments   = Attachments.data();
        return *this;
    }

    void ClearAttachments()
    {
        Attachments.clear();
        AttachmentCount = 0;
        ppAttachments   = nullptr;
    }

    void Clear()
    {
        FramebufferDescX CleanDesc;
        std::swap(*this, CleanDesc);
    }

private:
    std::vector<ITextureView*> Attachments;
};

/// C++ wrapper over PipelineResourceSignatureDesc.
struct PipelineResourceSignatureDescX : DeviceObjectAttribsX<PipelineResourceSignatureDesc>
{
    using TBase = DeviceObjectAttribsX<PipelineResourceSignatureDesc>;

    PipelineResourceSignatureDescX() noexcept
    {}

    PipelineResourceSignatureDescX(const PipelineResourceSignatureDesc& _Desc) :
        TBase{_Desc}
    {
        SetCombinedSamplerSuffix(CombinedSamplerSuffix);
        if (NumResources != 0)
        {
            ResCopy.assign(Resources, Resources + NumResources);
            Resources = ResCopy.data();
            for (auto& Res : ResCopy)
                Res.Name = StringPool.emplace(Res.Name).first->c_str();
        }

        if (NumImmutableSamplers != 0)
        {
            ImtblSamCopy.assign(ImmutableSamplers, ImmutableSamplers + NumImmutableSamplers);
            ImmutableSamplers = ImtblSamCopy.data();
            for (auto& Sam : ImtblSamCopy)
                Sam.SamplerOrTextureName = StringPool.emplace(Sam.SamplerOrTextureName).first->c_str();
        }
    }

    PipelineResourceSignatureDescX(const PipelineResourceSignatureDescX& _DescX) :
        PipelineResourceSignatureDescX{static_cast<const PipelineResourceSignatureDesc&>(_DescX)}
    {}

    PipelineResourceSignatureDescX& operator=(const PipelineResourceSignatureDescX& _DescX)
    {
        PipelineResourceSignatureDescX Copy{_DescX};
        std::swap(*this, Copy);
        return *this;
    }

    PipelineResourceSignatureDescX(PipelineResourceSignatureDescX&& RHS) = default;
    PipelineResourceSignatureDescX& operator=(PipelineResourceSignatureDescX&&) = default;

    PipelineResourceSignatureDescX& AddResource(const PipelineResourceDesc& Res)
    {
        ResCopy.push_back(Res);
        auto& ResName = ResCopy.back().Name;
        ResName       = StringPool.emplace(ResName).first->c_str();

        NumResources = static_cast<Uint32>(ResCopy.size());
        Resources    = ResCopy.data();
        return *this;
    }

    PipelineResourceSignatureDescX& AddImmutableSampler(const ImmutableSamplerDesc& Res)
    {
        ImtblSamCopy.push_back(Res);
        auto& SamOrTexName = ImtblSamCopy.back().SamplerOrTextureName;
        SamOrTexName       = StringPool.emplace(SamOrTexName).first->c_str();

        NumImmutableSamplers = static_cast<Uint32>(ImtblSamCopy.size());
        ImmutableSamplers    = ImtblSamCopy.data();
        return *this;
    }

    void ClearResources()
    {
        ResCopy.clear();
        NumResources = 0;
        Resources    = nullptr;
    }

    void ClearImmutableSamplers()
    {
        ImtblSamCopy.clear();
        NumImmutableSamplers = 0;
        ImmutableSamplers    = nullptr;
    }

    void SetCombinedSamplerSuffix(const char* Suffix)
    {
        CombinedSamplerSuffix = Suffix != nullptr ?
            StringPool.emplace(Suffix).first->c_str() :
            "";
    }

    void Clear()
    {
        PipelineResourceSignatureDescX CleanDesc;
        std::swap(*this, CleanDesc);
    }

private:
    std::vector<PipelineResourceDesc> ResCopy;
    std::vector<ImmutableSamplerDesc> ImtblSamCopy;
    std::unordered_set<std::string>   StringPool;
};

} // namespace Diligent
