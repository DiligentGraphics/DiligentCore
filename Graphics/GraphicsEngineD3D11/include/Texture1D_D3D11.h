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
/// Declaration of Diligent::Texture1D_D3D11 class

#include "TextureBaseD3D11.h"

namespace Diligent
{

/// Implementation of a 1D texture
class Texture1D_D3D11 final : public TextureBaseD3D11
{
public:
    Texture1D_D3D11(IReferenceCounters*          pRefCounters, 
                    FixedBlockMemoryAllocator&   TexViewObjAllocator, 
                    class RenderDeviceD3D11Impl* pDeviceD3D11, 
                    const TextureDesc&           TexDesc, 
                    const TextureData&           InitData = TextureData());

    Texture1D_D3D11(IReferenceCounters*          pRefCounters, 
                    FixedBlockMemoryAllocator&   TexViewObjAllocator, 
                    class RenderDeviceD3D11Impl* pDeviceD3D11, 
                    ID3D11Texture1D*             pd3d11Texture);
    ~Texture1D_D3D11();

protected:
    virtual void CreateSRV( TextureViewDesc& SRVDesc, ID3D11ShaderResourceView**  ppD3D11SRV )override final;
    virtual void CreateRTV( TextureViewDesc& RTVDesc, ID3D11RenderTargetView**    ppD3D11RTV )override final;
    virtual void CreateDSV( TextureViewDesc& DSVDesc, ID3D11DepthStencilView**    ppD3D11DSV )override final;
    virtual void CreateUAV( TextureViewDesc& UAVDesc, ID3D11UnorderedAccessView** ppD3D11UAV )override final;
};

}
