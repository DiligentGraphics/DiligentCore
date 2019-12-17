/*     Copyright 2019 Diligent Graphics LLC
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

#include "TestingEnvironment.h"

#ifndef NOMINMAX
#    define NOMINMAX
#endif

#include <d3d11.h>
#include <atlcomcli.h>

namespace Diligent
{

namespace Testing
{

HRESULT CompileD3DShader(const char*             Source,
                         LPCSTR                  strFunctionName,
                         const D3D_SHADER_MACRO* pDefines,
                         LPCSTR                  profile,
                         ID3DBlob**              ppBlobOut);

class TestingEnvironmentD3D11 final : public TestingEnvironment
{
public:
    TestingEnvironmentD3D11(DeviceType deviceType, ADAPTER_TYPE AdapterType, const SwapChainDesc& SCDesc);

    ID3D11Device* GetD3D11Device()
    {
        return m_pd3d11Device;
    }

    ID3D11DeviceContext* GetD3D11Context()
    {
        return m_pd3d11Context;
    }

    ID3D11RasterizerState* GetNoCullRS()
    {
        return m_pd3d11NoCullRS;
    }

    ID3D11DepthStencilState* GetDisableDepthDSS()
    {
        return m_pd3d11DisableDepthDSS;
    }

    ID3D11BlendState* GetDefaultBS()
    {
        return m_pd3d11DefaultBS;
    }

    CComPtr<ID3D11VertexShader> CreateVertexShader(const char*             Source,
                                                   LPCSTR                  strFunctionName = "main",
                                                   const D3D_SHADER_MACRO* pDefines        = nullptr,
                                                   LPCSTR                  profile         = "vs_5_0");

    CComPtr<ID3D11PixelShader> CreatePixelShader(const char*             Source,
                                                 LPCSTR                  strFunctionName = "main",
                                                 const D3D_SHADER_MACRO* pDefines        = nullptr,
                                                 LPCSTR                  profile         = "ps_5_0");

    CComPtr<ID3D11GeometryShader> CreateGeometryShader(const char*             Source,
                                                       LPCSTR                  strFunctionName = "main",
                                                       const D3D_SHADER_MACRO* pDefines        = nullptr,
                                                       LPCSTR                  profile         = "gs_5_0");

    CComPtr<ID3D11DomainShader> CreateDomainShader(const char*             Source,
                                                   LPCSTR                  strFunctionName = "main",
                                                   const D3D_SHADER_MACRO* pDefines        = nullptr,
                                                   LPCSTR                  profile         = "ds_5_0");

    CComPtr<ID3D11HullShader> CreateHullShader(const char*             Source,
                                               LPCSTR                  strFunctionName = "main",
                                               const D3D_SHADER_MACRO* pDefines        = nullptr,
                                               LPCSTR                  profile         = "gs_5_0");

    static TestingEnvironmentD3D11* GetInstance() { return ValidatedCast<TestingEnvironmentD3D11>(TestingEnvironment::GetInstance()); }

private:
    CComPtr<ID3D11Device>        m_pd3d11Device;
    CComPtr<ID3D11DeviceContext> m_pd3d11Context;

    CComPtr<ID3D11RasterizerState>   m_pd3d11NoCullRS;
    CComPtr<ID3D11DepthStencilState> m_pd3d11DisableDepthDSS;
    CComPtr<ID3D11BlendState>        m_pd3d11DefaultBS;
};

} // namespace Testing

} // namespace Diligent
