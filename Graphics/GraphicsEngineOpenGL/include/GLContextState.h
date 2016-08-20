/*     Copyright 2015-2016 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "GraphicsTypes.h"
#include "GLObjectWrapper.h"
#include "UniqueIdentifier.h"

namespace Diligent
{

class GLContextState
{
public:
    GLContextState(class RenderDeviceGLImpl *pDeviceGL);

    void SetProgram( const GLObjectWrappers::GLProgramObj &GLProgram );
    void SetPipeline( const GLObjectWrappers::GLPipelineObj &GLPipeline );
    void BindVAO( const GLObjectWrappers::GLVertexArrayObj &VAO );
    void BindFBO( const GLObjectWrappers::GLFrameBufferObj &FBO );
    void SetActiveTexture( Int32 Index );
    void BindTexture( Int32 Index, GLenum BindTarget, const GLObjectWrappers::GLTextureObj &Tex);
    void BindSampler( Uint32 Index, const GLObjectWrappers::GLSamplerObj &GLSampler);
    void BindImage( Uint32 Index, class TextureViewGLImpl *pTexView, GLint MipLevel, GLboolean IsLayered, GLint Layer, GLenum Access, GLenum Format );
    void EnsureMemoryBarrier(Uint32 RequiredBarriers, class AsyncWritableResource *pRes = nullptr);
    void SetPendingMemoryBarriers( Uint32 PendingBarriers );
    
    void EnableDepthTest( Bool bEnable );
    void EnableDepthWrites( Bool bEnable );
    void SetDepthFunc(COMPARISON_FUNCTION CmpFunc);
    void EnableStencilTest( Bool bEnable );
    void SetStencilWriteMask( Uint8 StencilWriteMask );
    void SetStencilRef( GLenum Face, Int32 Ref );
    void SetStencilFunc( GLenum Face, COMPARISON_FUNCTION Func, Int32 Ref, Uint32 Mask );
    void SetStencilOp( GLenum Face, STENCIL_OP StencilFailOp, STENCIL_OP StencilDepthFailOp, STENCIL_OP StencilPassOp );

    void SetFillMode( FILL_MODE FillMode );
    void SetCullMode( CULL_MODE CullMode );
    void SetFrontFace( Bool FrontCounterClockwise );
    void SetDepthBias( float DepthBias, float fSlopeScaledDepthBias );
    void SetDepthClamp( Bool bEnableDepthClamp );
    void EnableScissorTest( Bool bEnableScissorTest );

    void SetBlendFactors(const float *BlendFactors);
    void SetBlendState(const BlendStateDesc &BSDsc, Uint32 SampleMask);

    Bool GetDepthWritesEnabled(){ return m_DepthWritesEnableState; }
    Bool GetScissorTestEnabled(){ return m_RSState.ScissorTestEnable; }
    void GetColorWriteMask( Uint32 RTIndex, Uint32 &WriteMask, Bool &bIsIndependent );
    void SetColorWriteMask( Uint32 RTIndex, Uint32 WriteMask, Bool bIsIndependent );

private:
    // It is unsafe to use GL handle to keep track of bound objects
    // When an object is released, GL is free to reuse its handle for 
    // the new created objects.
    // Even using pointers is not safe as when an object is created,
    // the system can reuse the same address
    // The safest way is to keep global unique ID for all objects

    Diligent::UniqueIdentifier m_GLProgId;
    Diligent::UniqueIdentifier m_GLPipelineId;
    Diligent::UniqueIdentifier m_VAOId;
    Diligent::UniqueIdentifier m_FBOId;
    std::vector< Diligent::UniqueIdentifier > m_BoundTextures;
    std::vector< Diligent::UniqueIdentifier > m_BoundSamplers;
    struct BoundImageInfo
    {
        Diligent::UniqueIdentifier InterfaceID;
        GLint MipLevel;
        GLboolean IsLayered;
        GLint Layer;
        GLenum Access;
        GLenum Format;
        
        BoundImageInfo( Diligent::UniqueIdentifier _UniqueID = 0, 
                         GLint _MipLevel = 0,
                         GLboolean _IsLayered = 0,
                         GLint _Layer = 0,
                         GLenum _Access = 0,
                         GLenum _Format = 0) :
            InterfaceID  (_UniqueID ),
            MipLevel  (_MipLevel ),
            IsLayered (_IsLayered),
            Layer     (_Layer    ),
            Access    (_Access   ),
            Format    (_Format   )
        {}

        bool operator==(const BoundImageInfo &rhs)const
        {
            return  InterfaceID    == rhs.InterfaceID  &&
                    MipLevel    == rhs.MipLevel  &&
                    IsLayered   == rhs.IsLayered &&
                    Layer       == rhs.Layer     &&
                    Access      == rhs.Access    &&
                    Format      == rhs.Format;
        }
    };
    std::vector< BoundImageInfo > m_pBoundImages;
    Uint32 m_PendingMemoryBarriers;

    class EnableStateHelper
    {
    public:
        enum class ENABLE_STATE
        {
            UNKNOWN,
            ENABLED,
            DISABLED
        };

        EnableStateHelper() : m_EnableState( ENABLE_STATE::UNKNOWN ) {}
        bool operator == (bool bEnabled)const
        {
            return  bEnabled && m_EnableState == ENABLE_STATE::ENABLED ||
                   !bEnabled && m_EnableState == ENABLE_STATE::DISABLED;
        }
        bool operator != (bool bEnabled) const
        {
            return !(*this == bEnabled);
        }

        const EnableStateHelper& operator = (bool bEnabled)
        {
            m_EnableState = bEnabled ? ENABLE_STATE::ENABLED : ENABLE_STATE::DISABLED;
            return *this;
        }

        operator bool()const
        {
            return m_EnableState == ENABLE_STATE::ENABLED;
        }

    private:
        ENABLE_STATE m_EnableState;
    };
    EnableStateHelper m_DepthEnableState;
    EnableStateHelper m_DepthWritesEnableState;
    COMPARISON_FUNCTION m_DepthCmpFunc;
    EnableStateHelper m_StencilTestEnableState;
    Uint8 m_StencilReadMask;
    Uint8 m_StencilWriteMask;
    struct StencilOpState
    {
        COMPARISON_FUNCTION Func;
        STENCIL_OP          StencilFailOp;
        STENCIL_OP          StencilDepthFailOp;
        STENCIL_OP          StencilPassOp;
        Int32 Ref;
        Uint32 Mask;
        StencilOpState() :
            Func( COMPARISON_FUNC_UNKNOWN ),
            StencilFailOp(STENCIL_OP_UNDEFINED),
            StencilDepthFailOp( STENCIL_OP_UNDEFINED ),
            StencilPassOp( STENCIL_OP_UNDEFINED ),
            Ref( -1 ),
            Mask( -1 )
        {}
    }m_StencilOpState[2];

    struct RasterizerGLState
    {
        FILL_MODE FillMode;
        CULL_MODE CullMode;
        EnableStateHelper FrontCounterClockwise;
        float fDepthBias;
        float fSlopeScaledDepthBias;
        EnableStateHelper DepthClampEnable;
        EnableStateHelper ScissorTestEnable;
        RasterizerGLState() : 
            FillMode(FILL_MODE_UNDEFINED),
            CullMode(CULL_MODE_UNDEFINED),
            fDepthBias( std::numeric_limits<float>::max() ),
            fSlopeScaledDepthBias( std::numeric_limits<float>::max() )
        {}
    }m_RSState;

    struct ContextCaps
    {
        bool bFillModeSelectionSupported;
        GLint m_iMaxCombinedTexUnits;
        ContextCaps() :
            bFillModeSelectionSupported(True),
            m_iMaxCombinedTexUnits(0)
        {}
    }m_Caps;

    Uint32 m_ColorWriteMasks[MaxRenderTargets];
    EnableStateHelper m_bIndependentWriteMasks;
    Int32 m_iActiveTexture;
};

}
