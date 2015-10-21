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
/// Declaration of Diligent::BufferD3D11Impl class

#include "BufferD3D11.h"
#include "RenderDeviceD3D11.h"
#include "BufferBase.h"
#include "BufferViewD3D11Impl.h"

namespace Diligent
{

/// Implementation of the Diligent::IBufferD3D11 interface
class BufferD3D11Impl : public BufferBase<IBufferD3D11, BufferViewD3D11Impl>
{
public:
    typedef BufferBase<IBufferD3D11, BufferViewD3D11Impl> TBufferBase;
    BufferD3D11Impl(class RenderDeviceD3D11Impl *pDeviceD3D11, const BufferDesc& BuffDesc, const BufferData &BuffData = BufferData());
    ~BufferD3D11Impl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;

    virtual void UpdateData( IDeviceContext *pContext, Uint32 Offset, Uint32 Size, const PVoid pData )override;
    virtual void CopyData( IDeviceContext *pContext, IBuffer *pSrcBuffer, Uint32 SrcOffset, Uint32 DstOffset, Uint32 Size )override;
    virtual void Map( IDeviceContext *pContext, MAP_TYPE MapType, Uint32 MapFlags, PVoid &pMappedData )override;
    virtual void Unmap( IDeviceContext *pContext )override;
    virtual ID3D11Buffer *GetD3D11Buffer()override{ return m_pd3d11Buffer; }

private:
    virtual void CreateViewInternal( const struct BufferViewDesc &ViewDesc, IBufferView **ppView, bool bIsDefaultView )override;

    void CreateUAV( struct BufferViewDesc &UAVDesc, ID3D11UnorderedAccessView **ppD3D11UAV );
    void CreateSRV( struct BufferViewDesc &SRVDesc, ID3D11ShaderResourceView **ppD3D11SRV );

    friend class DeviceContextD3D11Impl;
    Diligent::CComPtr<ID3D11Buffer> m_pd3d11Buffer; ///< D3D11 buffer object
};

}
