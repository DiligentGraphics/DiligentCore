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
/// Declaration of Diligent::ShaderResourceBindingD3D11Impl class

#include "ShaderResourceBindingD3D11.h"
#include "RenderDeviceD3D11.h"
#include "ShaderResourceBindingBase.h"
#include "ShaderResourceCacheD3D11.h"
#include "ShaderResourceLayoutD3D11.h"
#include "STDAllocator.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;
/// Implementation of the Diligent::IShaderResourceBindingD3D11 interface
class ShaderResourceBindingD3D11Impl : public ShaderResourceBindingBase<IShaderResourceBindingD3D11>
{
public:
    typedef ShaderResourceBindingBase<IShaderResourceBindingD3D11> TBase;
    ShaderResourceBindingD3D11Impl(IReferenceCounters*           pRefCounters,
                                   class PipelineStateD3D11Impl* pPSO,
                                   bool                          IsInternal);
    ~ShaderResourceBindingD3D11Impl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject** ppInterface )override final;

    virtual void BindResources(Uint32 ShaderFlags, IResourceMapping* pResMapping, Uint32 Flags)override final;

    virtual IShaderVariable *GetVariable(SHADER_TYPE ShaderType, const char *Name)override final;

    ShaderResourceCacheD3D11 &GetResourceCache(Uint32 Ind){VERIFY_EXPR(Ind < m_NumActiveShaders); return m_pBoundResourceCaches[Ind];}
    ShaderResourceLayoutD3D11 &GetResourceLayout(Uint32 Ind){VERIFY_EXPR(Ind < m_NumActiveShaders); return m_pResourceLayouts[Ind];}

    void BindStaticShaderResources();
    inline bool IsStaticResourcesBound(){return m_bIsStaticResourcesBound;}

    Uint32 GetNumActiveShaders()
    {
        return static_cast<Uint32>(m_NumActiveShaders);
    }

    Int32 GetActiveShaderTypeIndex(Uint32 s){return m_ShaderTypeIndex[s];}

private:
    // The caches are indexed by the shader order in the PSO, not shader index
    ShaderResourceCacheD3D11* m_pBoundResourceCaches = nullptr;
    ShaderResourceLayoutD3D11* m_pResourceLayouts    = nullptr;
    
    Int8 m_ShaderTypeIndex[6] = {};

    // Resource layout index in m_ResourceLayouts[] array for every shader stage
    Int8 m_ResourceLayoutIndex[6];
    Uint8 m_NumActiveShaders = 0;
    
    bool m_bIsStaticResourcesBound = false;
};

}
