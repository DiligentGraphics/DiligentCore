/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

/// \file
/// Implementation of the Diligent::TextureBase template class

#include <memory>

#include "Texture.h"
#include "GraphicsTypes.h"
#include "DeviceObjectBase.hpp"
#include "GraphicsAccessories.hpp"
#include "STDAllocator.hpp"
#include "FormatString.hpp"

namespace Diligent
{

struct CopyTextureAttribs;

/// Validates texture description and throws an exception in case of an error.
void ValidateTextureDesc(const TextureDesc& TexDesc) noexcept(false);

/// Validates and corrects texture view description; throws an exception in case of an error.
void ValidatedAndCorrectTextureViewDesc(const TextureDesc& TexDesc, TextureViewDesc& ViewDesc) noexcept(false);

/// Validates update texture command parameters.
void ValidateUpdateTextureParams(const TextureDesc& TexDesc, Uint32 MipLevel, Uint32 Slice, const Box& DstBox, const TextureSubResData& SubresData);

/// Validates copy texture command parameters.
void ValidateCopyTextureParams(const CopyTextureAttribs& CopyAttribs);

/// Validates map texture command parameters.
void ValidateMapTextureParams(const TextureDesc& TexDesc,
                              Uint32             MipLevel,
                              Uint32             ArraySlice,
                              MAP_TYPE           MapType,
                              Uint32             MapFlags,
                              const Box*         pMapRegion);

/// Base implementation of the ITexture interface

/// \tparam EngineImplTraits - Engine implementation type traits.
template <typename EngineImplTraits>
class TextureBase : public DeviceObjectBase<typename EngineImplTraits::TextureInterface, typename EngineImplTraits::RenderDeviceImplType, TextureDesc>
{
public:
    // Base interface this class inherits (ITextureD3D12, ITextureVk, etc.)
    using BaseInterface = typename EngineImplTraits::TextureInterface;

    // Render device implementation type (RenderDeviceD3D12Impl, RenderDeviceVkImpl, etc.).
    using RenderDeviceImplType = typename EngineImplTraits::RenderDeviceImplType;

    // Texture view implementation type (TextureViewD3D12Impl, TextureViewVkImpl, etc.).
    using TextureViewImplType = typename EngineImplTraits::TextureViewImplType;

    // Type of the texture view object allocator.
    using TexViewObjAllocatorType = typename EngineImplTraits::TexViewObjAllocatorType;

    using TDeviceObjectBase = DeviceObjectBase<BaseInterface, RenderDeviceImplType, TextureDesc>;

    /// \param pRefCounters        - Reference counters object that controls the lifetime of this texture.
    /// \param TexViewObjAllocator - Allocator that is used to allocate memory for the instances of the texture view object.
    ///                              This parameter is only used for debug purposes.
    /// \param pDevice             - Pointer to the device
    /// \param Desc                - Texture description
    /// \param bIsDeviceInternal   - Flag indicating if the texture is an internal device object and
    ///							     must not keep a strong reference to the device
    TextureBase(IReferenceCounters*      pRefCounters,
                TexViewObjAllocatorType& TexViewObjAllocator,
                RenderDeviceImplType*    pDevice,
                const TextureDesc&       Desc,
                bool                     bIsDeviceInternal = false) :
        TDeviceObjectBase(pRefCounters, pDevice, Desc, bIsDeviceInternal),
#ifdef DILIGENT_DEBUG
        m_dbgTexViewObjAllocator(TexViewObjAllocator),
#endif
        m_pDefaultSRV{nullptr, STDDeleter<TextureViewImplType, TexViewObjAllocatorType>(TexViewObjAllocator)},
        m_pDefaultRTV{nullptr, STDDeleter<TextureViewImplType, TexViewObjAllocatorType>(TexViewObjAllocator)},
        m_pDefaultDSV{nullptr, STDDeleter<TextureViewImplType, TexViewObjAllocatorType>(TexViewObjAllocator)},
        m_pDefaultUAV{nullptr, STDDeleter<TextureViewImplType, TexViewObjAllocatorType>(TexViewObjAllocator)}
    {
        if (this->m_Desc.MipLevels == 0)
        {
            // Compute the number of levels in the full mipmap chain
            if (this->m_Desc.Type == RESOURCE_DIM_TEX_1D ||
                this->m_Desc.Type == RESOURCE_DIM_TEX_1D_ARRAY)
            {
                this->m_Desc.MipLevels = ComputeMipLevelsCount(this->m_Desc.Width);
            }
            else if (this->m_Desc.Type == RESOURCE_DIM_TEX_2D ||
                     this->m_Desc.Type == RESOURCE_DIM_TEX_2D_ARRAY ||
                     this->m_Desc.Type == RESOURCE_DIM_TEX_CUBE ||
                     this->m_Desc.Type == RESOURCE_DIM_TEX_CUBE_ARRAY)
            {
                this->m_Desc.MipLevels = ComputeMipLevelsCount(this->m_Desc.Width, this->m_Desc.Height);
            }
            else if (this->m_Desc.Type == RESOURCE_DIM_TEX_3D)
            {
                this->m_Desc.MipLevels = ComputeMipLevelsCount(this->m_Desc.Width, this->m_Desc.Height, this->m_Desc.Depth);
            }
            else
            {
                UNEXPECTED("Unknown texture type");
            }
        }

        Uint64 DeviceQueuesMask = pDevice->GetCommandQueueMask();
        DEV_CHECK_ERR((this->m_Desc.ImmediateContextMask & DeviceQueuesMask) != 0,
                      "No bits in the immediate context mask (0x", std::hex, this->m_Desc.ImmediateContextMask,
                      ") correspond to one of ", pDevice->GetCommandQueueCount(), " available software command queues");
        this->m_Desc.ImmediateContextMask &= DeviceQueuesMask;

        if ((this->m_Desc.BindFlags & BIND_INPUT_ATTACHMENT) != 0)
            this->m_Desc.BindFlags |= BIND_SHADER_RESOURCE;

        // Validate correctness of texture description
        ValidateTextureDesc(this->m_Desc);
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_Texture, TDeviceObjectBase)

    /// Implementation of ITexture::CreateView(); calls CreateViewInternal() virtual function that
    /// creates texture view for the specific engine implementation.
    virtual void DILIGENT_CALL_TYPE CreateView(const struct TextureViewDesc& ViewDesc, ITextureView** ppView) override
    {
        DEV_CHECK_ERR(ViewDesc.ViewType != TEXTURE_VIEW_UNDEFINED, "Texture view type is not specified");
        if (ViewDesc.ViewType == TEXTURE_VIEW_SHADER_RESOURCE)
            DEV_CHECK_ERR(this->m_Desc.BindFlags & BIND_SHADER_RESOURCE, "Attempting to create SRV for texture '", this->m_Desc.Name, "' that was not created with BIND_SHADER_RESOURCE flag.");
        else if (ViewDesc.ViewType == TEXTURE_VIEW_UNORDERED_ACCESS)
            DEV_CHECK_ERR(this->m_Desc.BindFlags & BIND_UNORDERED_ACCESS, "Attempting to create UAV for texture '", this->m_Desc.Name, "' that was not created with BIND_UNORDERED_ACCESS flag.");
        else if (ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET)
            DEV_CHECK_ERR(this->m_Desc.BindFlags & BIND_RENDER_TARGET, "Attempting to create RTV for texture '", this->m_Desc.Name, "' that was not created with BIND_RENDER_TARGET flag.");
        else if (ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL)
            DEV_CHECK_ERR(this->m_Desc.BindFlags & BIND_DEPTH_STENCIL, "Attempting to create DSV for texture '", this->m_Desc.Name, "' that was not created with BIND_DEPTH_STENCIL flag.");
        else
            UNEXPECTED("Unexpected texture view type.");

        CreateViewInternal(ViewDesc, ppView, false);
    }

    /// Creates default texture views.

    ///
    /// - Creates default shader resource view addressing the entire texture if Diligent::BIND_SHADER_RESOURCE flag is set.
    /// - Creates default render target view addressing the most detailed mip level if Diligent::BIND_RENDER_TARGET flag is set.
    /// - Creates default depth-stencil view addressing the most detailed mip level if Diligent::BIND_DEPTH_STENCIL flag is set.
    /// - Creates default unordered access view addressing the entire texture if Diligent::BIND_UNORDERED_ACCESS flag is set.
    ///
    /// The function calls CreateViewInternal().
    void CreateDefaultViews()
    {
        const auto& TexFmtAttribs = GetTextureFormatAttribs(this->m_Desc.Format);
        if (TexFmtAttribs.ComponentType == COMPONENT_TYPE_UNDEFINED)
        {
            // Cannot create default view for TYPELESS formats
            return;
        }

        auto CreateDefaultView = [&](TEXTURE_VIEW_TYPE ViewType) //
        {
            TextureViewDesc ViewDesc;
            ViewDesc.ViewType = ViewType;

            std::string ViewName;
            switch (ViewType)
            {
                case TEXTURE_VIEW_SHADER_RESOURCE:
                    if ((this->m_Desc.MiscFlags & MISC_TEXTURE_FLAG_GENERATE_MIPS) != 0)
                        ViewDesc.Flags |= TEXTURE_VIEW_FLAG_ALLOW_MIP_MAP_GENERATION;
                    ViewName = "Default SRV of texture '";
                    break;

                case TEXTURE_VIEW_RENDER_TARGET:
                    ViewName = "Default RTV of texture '";
                    break;

                case TEXTURE_VIEW_DEPTH_STENCIL:
                    ViewName = "Default DSV of texture '";
                    break;

                case TEXTURE_VIEW_UNORDERED_ACCESS:
                    ViewDesc.AccessFlags = UAV_ACCESS_FLAG_READ_WRITE;

                    ViewName = "Default UAV of texture '";
                    break;

                default:
                    UNEXPECTED("Unexpected texture type");
            }
            ViewName += this->m_Desc.Name;
            ViewName += '\'';
            ViewDesc.Name = ViewName.c_str();

            ITextureView* pView = nullptr;
            CreateViewInternal(ViewDesc, &pView, true);
            VERIFY(pView != nullptr, "Failed to create default view for texture '", this->m_Desc.Name, "'.");
            VERIFY(pView->GetDesc().ViewType == ViewType, "Unexpected view type.");

            return static_cast<TextureViewImplType*>(pView);
        };

        if (this->m_Desc.BindFlags & BIND_SHADER_RESOURCE)
        {
            m_pDefaultSRV.reset(CreateDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        }

        if (this->m_Desc.BindFlags & BIND_RENDER_TARGET)
        {
            m_pDefaultRTV.reset(CreateDefaultView(TEXTURE_VIEW_RENDER_TARGET));
        }

        if (this->m_Desc.BindFlags & BIND_DEPTH_STENCIL)
        {
            m_pDefaultDSV.reset(CreateDefaultView(TEXTURE_VIEW_DEPTH_STENCIL));
        }

        if (this->m_Desc.BindFlags & BIND_UNORDERED_ACCESS)
        {
            m_pDefaultUAV.reset(CreateDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
        }
    }

    virtual void DILIGENT_CALL_TYPE SetState(RESOURCE_STATE State) override final
    {
        this->m_State = State;
    }

    virtual RESOURCE_STATE DILIGENT_CALL_TYPE GetState() const override final
    {
        return this->m_State;
    }

    bool IsInKnownState() const
    {
        return this->m_State != RESOURCE_STATE_UNKNOWN;
    }

    bool CheckState(RESOURCE_STATE State) const
    {
        VERIFY((State & (State - 1)) == 0, "Single state is expected");
        VERIFY(IsInKnownState(), "Texture state is unknown");
        return (this->m_State & State) == State;
    }

    bool CheckAnyState(RESOURCE_STATE States) const
    {
        VERIFY(IsInKnownState(), "Texture state is unknown");
        return (this->m_State & States) != 0;
    }

    /// Implementation of ITexture::GetDefaultView().
    virtual ITextureView* DILIGENT_CALL_TYPE GetDefaultView(TEXTURE_VIEW_TYPE ViewType) override
    {
        switch (ViewType)
        {
            // clang-format off
            case TEXTURE_VIEW_SHADER_RESOURCE:  return m_pDefaultSRV.get();
            case TEXTURE_VIEW_RENDER_TARGET:    return m_pDefaultRTV.get();
            case TEXTURE_VIEW_DEPTH_STENCIL:    return m_pDefaultDSV.get();
            case TEXTURE_VIEW_UNORDERED_ACCESS: return m_pDefaultUAV.get();
            // clang-format on
            default: UNEXPECTED("Unknown view type"); return nullptr;
        }
    }

protected:
    /// Pure virtual function that creates texture view for the specific engine implementation.
    virtual void CreateViewInternal(const struct TextureViewDesc& ViewDesc, ITextureView** ppView, bool bIsDefaultView) = 0;

#ifdef DILIGENT_DEBUG
    TexViewObjAllocatorType& m_dbgTexViewObjAllocator;
#endif
    // WARNING! We cannot use ITextureView here, because ITextureView has no virtual dtor!
    /// Default SRV addressing the entire texture
    std::unique_ptr<TextureViewImplType, STDDeleter<TextureViewImplType, TexViewObjAllocatorType>> m_pDefaultSRV;
    /// Default RTV addressing the most detailed mip level
    std::unique_ptr<TextureViewImplType, STDDeleter<TextureViewImplType, TexViewObjAllocatorType>> m_pDefaultRTV;
    /// Default DSV addressing the most detailed mip level
    std::unique_ptr<TextureViewImplType, STDDeleter<TextureViewImplType, TexViewObjAllocatorType>> m_pDefaultDSV;
    /// Default UAV addressing the entire texture
    std::unique_ptr<TextureViewImplType, STDDeleter<TextureViewImplType, TexViewObjAllocatorType>> m_pDefaultUAV;

    RESOURCE_STATE m_State = RESOURCE_STATE_UNKNOWN;
};

} // namespace Diligent
