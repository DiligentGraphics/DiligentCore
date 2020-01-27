/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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
/// Definition of the Diligent::IPipeplineStateD3D11 interface

#include "../../GraphicsEngine/interface/PipelineState.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {3EA6E3F4-9966-47FC-8CE8-0EB3E2273061}
static const struct INTERFACE_ID IID_PipelineStateD3D11 =
    {0x3ea6e3f4, 0x9966, 0x47fc, {0x8c, 0xe8, 0xe, 0xb3, 0xe2, 0x27, 0x30, 0x61}};

#if DILIGENT_CPP_INTERFACE

/// Exposes Direct3D11-specific functionality of a pipeline state object.
class IPipelineStateD3D11 : public IPipelineState
{
public:
    /// Returns a pointer to the ID3D11BlendState interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11BlendState* GetD3D11BlendState() = 0;


    /// Returns a pointer to the ID3D11RasterizerState interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11RasterizerState* GetD3D11RasterizerState() = 0;


    /// Returns a pointer to the ID3D11DepthStencilState interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11DepthStencilState* GetD3D11DepthStencilState() = 0;

    /// Returns a pointer to the ID3D11InputLayout interface of the internal Direct3D11 object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11InputLayout* GetD3D11InputLayout() = 0;

    /// Returns a pointer to the ID3D11VertexShader interface of the internal vertex shader object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11VertexShader* GetD3D11VertexShader() = 0;

    /// Returns a pointer to the ID3D11PixelShader interface of the internal pixel shader object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11PixelShader* GetD3D11PixelShader() = 0;


    /// Returns a pointer to the ID3D11GeometryShader interface of the internal geometry shader object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11GeometryShader* GetD3D11GeometryShader() = 0;

    /// Returns a pointer to the ID3D11DomainShader interface of the internal domain shader object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11DomainShader* GetD3D11DomainShader() = 0;

    /// Returns a pointer to the ID3D11HullShader interface of the internal hull shader object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11HullShader* GetD3D11HullShader() = 0;

    /// Returns a pointer to the ID3D11ComputeShader interface of the internal compute shader object.

    /// The method does *NOT* call AddRef() on the returned interface,
    /// so Release() must not be called.
    virtual ID3D11ComputeShader* GetD3D11ComputeShader() = 0;
};

#else

// clang-format off

struct IPipelineStateD3D11;

struct IPipelineStateD3D11Methods
{
    ID3D11BlendState*       (*GetD3D11BlendState)       (struct IPipelineStateD3D11*);
    ID3D11RasterizerState*  (*GetD3D11RasterizerState)  (struct IPipelineStateD3D11*);
    ID3D11DepthStencilState*(*GetD3D11DepthStencilState)(struct IPipelineStateD3D11*);
    ID3D11InputLayout*      (*GetD3D11InputLayout)      (struct IPipelineStateD3D11*);
    ID3D11VertexShader*     (*GetD3D11VertexShader)     (struct IPipelineStateD3D11*);
    ID3D11PixelShader*      (*GetD3D11PixelShader)      (struct IPipelineStateD3D11*);
    ID3D11GeometryShader*   (*GetD3D11GeometryShader)   (struct IPipelineStateD3D11*);
    ID3D11DomainShader*     (*GetD3D11DomainShader)     (struct IPipelineStateD3D11*);
    ID3D11HullShader*       (*GetD3D11HullShader)       (struct IPipelineStateD3D11*);
    ID3D11ComputeShader*    (*GetD3D11ComputeShader)    (struct IPipelineStateD3D11*);
};

// clang-format on

struct IPipelineStateD3D11Vtbl
{
    struct IObjectMethods             Object;
    struct IDeviceObjectMethods       DeviceObject;
    struct IPipelineStateMethods      PipelineState;
    struct IPipelineStateD3D11Methods PipelineStateD3D11;
};

struct IPipelineStateD3D11
{
    struct IPipelineStateD3D11Vtbl* pVtbl;
};

// clang-format off

#    define IPipelineStateD3D11_GetD3D11BlendState(This)        (This)->pVtbl->PipelineStateD3D11.GetD3D11BlendState       ((struct IPipelineStateD3D11*)(This))
#    define IPipelineStateD3D11_GetD3D11RasterizerState(This)   (This)->pVtbl->PipelineStateD3D11.GetD3D11RasterizerState  ((struct IPipelineStateD3D11*)(This))
#    define IPipelineStateD3D11_GetD3D11DepthStencilState(This) (This)->pVtbl->PipelineStateD3D11.GetD3D11DepthStencilState((struct IPipelineStateD3D11*)(This))
#    define IPipelineStateD3D11_GetD3D11InputLayout(This)       (This)->pVtbl->PipelineStateD3D11.GetD3D11InputLayout      ((struct IPipelineStateD3D11*)(This))
#    define IPipelineStateD3D11_GetD3D11VertexShader(This)      (This)->pVtbl->PipelineStateD3D11.GetD3D11VertexShader     ((struct IPipelineStateD3D11*)(This))
#    define IPipelineStateD3D11_GetD3D11PixelShader(This)       (This)->pVtbl->PipelineStateD3D11.GetD3D11PixelShader      ((struct IPipelineStateD3D11*)(This))
#    define IPipelineStateD3D11_GetD3D11GeometryShader(This)    (This)->pVtbl->PipelineStateD3D11.GetD3D11GeometryShader   ((struct IPipelineStateD3D11*)(This))
#    define IPipelineStateD3D11_GetD3D11DomainShader(This)      (This)->pVtbl->PipelineStateD3D11.GetD3D11DomainShader     ((struct IPipelineStateD3D11*)(This))
#    define IPipelineStateD3D11_GetD3D11HullShader(This)        (This)->pVtbl->PipelineStateD3D11.GetD3D11HullShader       ((struct IPipelineStateD3D11*)(This))
#    define IPipelineStateD3D11_GetD3D11ComputeShader(This)     (This)->pVtbl->PipelineStateD3D11.GetD3D11ComputeShader    ((struct IPipelineStateD3D11*)(This))

// clang-format on

#endif

DILIGENT_END_NAMESPACE // namespace Diligent
