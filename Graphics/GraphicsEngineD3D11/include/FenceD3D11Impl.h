/*     Copyright 2015-2018 Egor Yusov
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
/// Declaration of Diligent::FenceD3D11Impl class

#include <deque>
#include "FenceD3D11.h"
#include "RenderDeviceD3D11.h"
#include "FenceBase.h"
#include "RenderDeviceD3D11Impl.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Implementation of the Diligent::IFenceD3D11 interface
class FenceD3D11Impl final : public FenceBase<IFenceD3D11, RenderDeviceD3D11Impl>
{
public:
    using TFenceBase = FenceBase<IFenceD3D11, RenderDeviceD3D11Impl>;

    FenceD3D11Impl(IReferenceCounters*    pRefCounters,
                   RenderDeviceD3D11Impl* pDevice,
                   const FenceDesc&       Desc);
    ~FenceD3D11Impl();

    virtual Uint64 GetCompletedValue()override final;

    /// Resets the fence to the specified value. 
    virtual void Reset(Uint64 Value)override final;

    void AddPendingQuery(CComPtr<ID3D11DeviceContext> pCtx, CComPtr<ID3D11Query> pQuery, Uint64 Value)
    {
        m_PendingQueries.emplace_back(std::move(pCtx), std::move(pQuery), Value);
    }

private:
    struct PendingFenceData
    {
        CComPtr<ID3D11DeviceContext> pd3d11Ctx;
        CComPtr<ID3D11Query>         pd3d11Query;
        Uint64                       Value;

        PendingFenceData(CComPtr<ID3D11DeviceContext> pCtx, CComPtr<ID3D11Query> pQuery, Uint64 _Value) : 
            pd3d11Ctx  (std::move(pCtx)),
            pd3d11Query(std::move(pQuery)),
            Value      (_Value)
        {}
    };
    std::deque< PendingFenceData > m_PendingQueries;
    volatile Uint64 m_LastCompletedFenceValue = 0;
};

}
