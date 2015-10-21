/*     Copyright 2015 Egor Yusov
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

/// \file
/// Declaration of Diligent::ShaderD3D11Impl class

#include "ShaderD3D11.h"
#include "RenderDeviceD3D11.h"
#include "ShaderBase.h"

#ifdef _DEBUG
#   define VERIFY_SHADER_BINDINGS
#endif

namespace Diligent
{

class ResourceMapping;

/// Implementation of the Diligent::IShaderD3D11 interface
class ShaderD3D11Impl : public ShaderBase<IShaderD3D11, IRenderDeviceD3D11>
{
public:
    typedef ShaderBase<IShaderD3D11, IRenderDeviceD3D11> TShaderBase;

    ShaderD3D11Impl(class RenderDeviceD3D11Impl *pRenderDeviceD3D11, const ShaderCreationAttribs &ShaderCreationAttribs);
    ~ShaderD3D11Impl();
    
    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;

    virtual void BindResources( IResourceMapping* pResourceMapping, Uint32 Flags  )override;
    
    virtual IShaderVariable* GetShaderVariable( const Char* Name )override;

    /// Describes resources associated with the bound constant buffer
    struct BoundCB
    {
        /// Strong reference to the buffer
        Diligent::RefCntAutoPtr<IDeviceObject> pBuff;
        /// Strong referene to the D3D11 buffer interface
        Diligent::CComPtr<ID3D11Buffer> pd3d11Buff;
    };

    /// Describes resources associated with the bound sampler
    struct BoundSampler
    {
        /// Strong reference to the sampler
        Diligent::RefCntAutoPtr<IDeviceObject> pSampler;
        /// Strong referene to the D3D11 sampler state interface
        Diligent::CComPtr<ID3D11SamplerState> pd3d11Sampler;
    };

    /// Describes resources associated with the bound SRV
    struct BoundSRV
    {
        /// Strong reference to the resource bound as SRV
        Diligent::RefCntAutoPtr<IDeviceObject> pResource;
        /// Strong reference to the resource view
        Diligent::RefCntAutoPtr<IDeviceObject> pView;
        /// Strong referene to the D3D11 SRV interface
        Diligent::CComPtr<ID3D11ShaderResourceView> pd3d11View;
    };

    /// Describes resources associated with the bound UAV
    struct BoundUAV
    {
        /// Strong reference to the resource bound as UAV
		Diligent::RefCntAutoPtr<IDeviceObject> pResource;
        /// Strong reference to the resource view
		Diligent::RefCntAutoPtr<IDeviceObject> pView;
        /// Strong referene to the D3D11 UAV interface
        Diligent::CComPtr<ID3D11UnorderedAccessView> pd3d11View;
    };

    /// Returns a const reference to the zero-slot-based array of bound constant buffers
    const std::vector< BoundCB >&      GetBoundCBs()      { return m_BoundCBs; }
    /// Returns a const reference to the zero-slot-based array of bound samplers
    const std::vector< BoundSampler >& GetBoundSamplers() { return m_BoundSamplers; }
    /// Returns a const reference to the zero-slot-based array bound SRVs
    const std::vector< BoundSRV >&     GetBoundSRVs()     { return m_BoundSRVs; }
    /// Returns a const reference to the zero-slot-based array bound UAVs
    const std::vector< BoundUAV >&     GetBoundUAVs()     { return m_BoundUAVs; }

    virtual ID3D11DeviceChild* GetD3D11Shader()override{ return m_pShader; }

#ifdef VERIFY_SHADER_BINDINGS
    void dbgVerifyBindings();
#endif

private:
    void BindConstantBuffers( IResourceMapping* pResourceMapping, Uint32 Flags );
    
    void BindSRVsAndSamplers( IResourceMapping* pResourceMapping, Uint32 Flags );

    void BindUAVs( IResourceMapping* pResourceMapping, Uint32 Flags );

    static const String m_SamplerSuffix;

    struct D3D11ShaderVarBase : ShaderVariableBase
    {
        D3D11ShaderVarBase( ShaderD3D11Impl *pShader, const String& _Name, UINT _BindPoint ) :
            ShaderVariableBase( pShader ),
            Name(_Name),
            BindPoint(_BindPoint)
        {}

        String Name;
        UINT BindPoint;
        const String& GetName(){ return Name; }
    };

    struct ConstBuffBindInfo : D3D11ShaderVarBase
    {
        ConstBuffBindInfo( ShaderD3D11Impl *pShader, const String& _Name, UINT _BindPoint ) :
            D3D11ShaderVarBase( pShader, _Name, _BindPoint )
        {}
        virtual void Set(IDeviceObject *pObject)override;
        bool IsBound();
    };
    
    struct TexAndSamplerBindInfo : D3D11ShaderVarBase
    {
        TexAndSamplerBindInfo( ShaderD3D11Impl *pShader, const String& _TexName, UINT _TexBindPoint, const String &_SamplerName, UINT _SamplerBindPoint ) :
            D3D11ShaderVarBase( pShader, _TexName, _TexBindPoint),
            SamplerName(_SamplerName),
            SamplerBindPoint(_SamplerBindPoint)
        {}

        String SamplerName;
        UINT SamplerBindPoint;
        virtual void Set(IDeviceObject *pObject)override;
        bool IsBound();
    };

    struct UAVBindInfoBase : public D3D11ShaderVarBase
    {
        UAVBindInfoBase( ShaderD3D11Impl *pShader, const String& _Name, UINT _BindPoint, D3D_SHADER_INPUT_TYPE _Type ) :
            D3D11ShaderVarBase( pShader, _Name, _BindPoint),
            Type(_Type)
        {}

        bool IsBound();
        D3D_SHADER_INPUT_TYPE Type;
    };

    struct TexUAVBindInfo : UAVBindInfoBase
    {
        TexUAVBindInfo( ShaderD3D11Impl *pShader, const String& _Name, UINT _BindPoint, D3D_SHADER_INPUT_TYPE _Type  ) :
            UAVBindInfoBase( pShader, _Name, _BindPoint, _Type )
        {}

        virtual void Set(IDeviceObject *pObject)override;
    };

    struct BuffUAVBindInfo : UAVBindInfoBase
    {
        BuffUAVBindInfo( ShaderD3D11Impl *pShader, const String& _Name, UINT _BindPoint, D3D_SHADER_INPUT_TYPE _Type  ) :
            UAVBindInfoBase( pShader, _Name, _BindPoint, _Type )
        {}

        virtual void Set(IDeviceObject *pObject)override;
    };

    struct BuffSRVBindInfo : D3D11ShaderVarBase
    {
        BuffSRVBindInfo( ShaderD3D11Impl *pShader, const String &_Name, UINT _BindPoint ) :
            D3D11ShaderVarBase( pShader, _Name, _BindPoint)
        {}

        virtual void Set(IDeviceObject *pObject)override;
        bool IsBound();
    };

    void LoadShaderResources();

    friend class VertexDescD3D11Impl;
    friend class DeviceContextD3D11Impl;
    /// D3D11 shader
    Diligent::CComPtr<ID3D11DeviceChild> m_pShader;
    Diligent::CComPtr<ID3DBlob> m_pShaderByteCode;// Only stored for Vertex Shader

    std::vector<ConstBuffBindInfo> m_ConstanBuffers;
    std::vector<TexAndSamplerBindInfo> m_TexAndSamplers;
    std::vector<TexUAVBindInfo> m_TexUAVs;
    std::vector<BuffUAVBindInfo> m_BuffUAVs;
    std::vector<BuffSRVBindInfo> m_BuffSRVs;


    /// An array of bound constant buffers. The data always starts at 
    /// slot 0 even when no resource is actually bound to the slot.
    std::vector<BoundCB> m_BoundCBs;

    /// An array of bound samplers. The data always starts at 
    /// slot 0 even when no sampler is actually bound to the slot.
    std::vector<BoundSampler> m_BoundSamplers;

    /// An array of bound SRVs. The data always starts at 
    /// slot 0 even when no resource is actually bound to the slot.
    std::vector<BoundSRV> m_BoundSRVs;

    /// An array of bound UAVs. The data always starts at 
    /// slot 0 even when no resource is actually bound to the slot.
    std::vector<BoundUAV> m_BoundUAVs;

    /// Hash map to look up shader variables by name.
    std::unordered_map<Diligent::HashMapStringKey, IShaderVariable* > m_VariableHash;
};

}
