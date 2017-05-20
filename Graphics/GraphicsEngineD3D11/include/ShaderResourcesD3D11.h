/*     Copyright 2015-2017 Egor Yusov
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
/// Declaration of Diligent::ShaderResourcesD3D11 class


//  ShaderResourcesD3D11 are created by ShaderD3D11Impl instances. They are then referenced by ShaderResourceLayoutD3D11 objects, which are in turn
//  created by instances of ShaderResourceBindingsD3D11Impl (and ShaderD3D11Impl too)
//
//    _________________
//   |                 |
//   | ShaderD3D11Impl |
//   |_________________|
//            |
//            |shared_ptr
//    ________V_____________                  _____________________________________________________________________
//   |                      |  unique_ptr    |        |           |           |           |           |            |
//   | ShaderResourcesD3D11 |--------------->|   CBs  |  TexSRVs  |  TexUAVs  |  BufSRVs  |  BufUAVs  |  Samplers  |
//   |______________________|                |________|___________|___________|___________|___________|____________|
//            A                                     A         A          A          A           A               A
//            |                                      \         \          \          \           \              |
//            |shared_ptr                            Ref       Ref        Ref        Ref         Ref            |
//    ________|__________________                  ____\_________\__________\__________\___________\_______     |
//   |                           |   unique_ptr   |        |           |           |           |           |    |
//   | ShaderResourceLayoutD3D11 |--------------->|   CBs  |  TexSRVs  |  TexUAVs  |  BufSRVs  |  BufUAVs  |    |
//   |___________________________|                |________|___________|___________|___________|___________|    |
//                                                               |                                             Ref 
//                                                               |______________________________________________|
//               
//
//  One ShaderResources instance can be referenced by multiple objects
//
//
//                                                                               ____<m_pResourceLayouts>___        ________________________________ 
//                                                                              |                           |      |                                |
//                                                                          ----| ShaderResourceLayoutD3D11 |<-----| ShaderResourceBindingD3D11Impl |
//                                                                         |    |___________________________|      |________________________________|
//                                                                         |
//                                                                         |
//    _________________                  ______________________            |     ____<m_pResourceLayouts>___        ________________________________          
//   |                 |  shared_ptr    |                      | shared_ptr|    |                           |      |                                |
//   | ShaderD3D11Impl |--------------->| ShaderResourcesD3D11 |<---------------| ShaderResourceLayoutD3D11 |<-----| ShaderResourceBindingD3D11Impl |
//   |_________________|                |______________________|           |    |___________________________|      |________________________________|
//              |                                   A                      |
//              V                                   |                      |
//   ____<m_StaticResLayout>____                    |                      |     ____<m_pResourceLayouts>___        ________________________________ 
//  |                           |   shared_ptr      |                      |    |                           |      |                                |
//  | ShaderResourceLayoutD3D11 |-------------------                        ----| ShaderResourceLayoutD3D11 |<-----| ShaderResourceBindingD3D11Impl |
//  |___________________________|                                               |___________________________|      |________________________________|
//
//


#include "ShaderResources.h"

namespace Diligent
{

/// Diligent::ShaderResources class
class ShaderResourcesD3D11 : public ShaderResources
{
public:
    // Loads shader resources from the compiled shader bytecode
    ShaderResourcesD3D11(class RenderDeviceD3D11Impl *pDeviceD3D11Impl, ID3DBlob *pShaderBytecode, const ShaderDesc &ShdrDesc);
    ~ShaderResourcesD3D11();

    __forceinline Int32 GetMaxCBBindPoint()     const{return m_MaxCBBindPoint;      }
    __forceinline Int32 GetMaxSRVBindPoint()    const{return m_MaxSRVBindPoint;     }
    __forceinline Int32 GetMaxSamplerBindPoint()const{return m_MaxSamplerBindPoint; }
    __forceinline Int32 GetMaxUAVBindPoint()    const{return m_MaxUAVBindPoint;     }

#ifdef VERIFY_SHADER_BINDINGS
    void dbgVerifyCommittedResources(ID3D11Buffer*              CommittedD3D11CBs[],
                                     ID3D11ShaderResourceView*  CommittedD3D11SRVs[],
                                     ID3D11Resource*            CommittedD3D11SRVResources[],
                                     ID3D11SamplerState*        CommittedD3D11Samplers[],
                                     ID3D11UnorderedAccessView* CommittedD3D11UAVs[],
                                     ID3D11Resource*            CommittedD3D11UAVResources[],
                                     class ShaderResourceCacheD3D11 &ResourceCache)const;
#endif

    const Char *GetShaderName()const{return m_ShaderName;}
    
    void InitStaticSamplers(class ShaderResourceCacheD3D11 &ResourceCache)const;

private:
    typedef Int8 MaxBindPointType;

    MaxBindPointType m_MaxCBBindPoint      = -1; // Max == 13
    MaxBindPointType m_MaxSRVBindPoint     = -1; // Max == 127
    MaxBindPointType m_MaxSamplerBindPoint = -1; // Max == 15
    MaxBindPointType m_MaxUAVBindPoint     = -1; // Max == 7
    Uint8            m_NumStaticSamplers   =  0; // Max == 16

    static const UINT MaxAllowedBindPoint = std::numeric_limits<MaxBindPointType>::max();
    static_assert(D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT-1 <= MaxAllowedBindPoint, "Not enough bits to represent max CB slot" );
    static_assert(D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT-1      <= MaxAllowedBindPoint, "Not enough bits to represent max SRV slot");
    static_assert(D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT-1             <= MaxAllowedBindPoint, "Not enough bits to represent max Sampler slot");
    static_assert(D3D11_PS_CS_UAV_REGISTER_COUNT-1                    <= MaxAllowedBindPoint, "Not enough bits to represent max UAV slot");

    // ShaderResourcesD3D11 is part of the ShaderD3D11Impl object, so we can simply
    // reference shader name without the need to copy it
    const Char* m_ShaderName;

    typedef std::pair<const D3DShaderResourceAttribs*, RefCntAutoPtr<ISampler> > StaticSamplerAttribs;
    StaticSamplerAttribs& GetStaticSampler(Uint32 n)
    {
        VERIFY_EXPR(n < m_NumStaticSamplers);
        return reinterpret_cast< StaticSamplerAttribs* >(m_StaticSamplers.get())[n];
    }

    // We have to use void, because we will be calling destructors manually
    std::unique_ptr< void, STDDeleterRawMem<void > > m_StaticSamplers;
};

}
