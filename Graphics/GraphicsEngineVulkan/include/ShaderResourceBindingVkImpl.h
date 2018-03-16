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
/// Declaration of Diligent::ShaderResourceBindingVkImpl class

#include "ShaderResourceBindingVk.h"
#include "RenderDeviceVk.h"
#include "ShaderResourceBindingBase.h"
#include "ShaderBase.h"
#include "ShaderResourceCacheVk.h"
#include "ShaderResourceLayoutVk.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;
/// Implementation of the Diligent::IShaderResourceBindingVk interface
class ShaderResourceBindingVkImpl : public ShaderResourceBindingBase<IShaderResourceBindingVk>
{
public:
    typedef ShaderResourceBindingBase<IShaderResourceBindingVk> TBase;
    ShaderResourceBindingVkImpl(IReferenceCounters *pRefCounters, class PipelineStateVkImpl *pPSO, bool IsPSOInternal);
    ~ShaderResourceBindingVkImpl();

    virtual void QueryInterface( const Diligent::INTERFACE_ID &IID, IObject **ppInterface )override;

    virtual void BindResources(Uint32 ShaderFlags, IResourceMapping *pResMapping, Uint32 Flags)override;

    virtual IShaderVariable *GetVariable(SHADER_TYPE ShaderType, const char *Name)override;

/*    ShaderResourceLayoutVk& GetResourceLayout(SHADER_TYPE ResType)
    {
        auto ShaderInd = GetShaderTypeIndex(ResType);
        auto ResLayoutInd = m_ResourceLayoutIndex[ShaderInd];
        VERIFY(ResLayoutInd >= 0, "Shader resource layout is not initialized");
        VERIFY_EXPR(ResLayoutInd < (Int32)m_NumShaders);
        return m_pResourceLayouts[ResLayoutInd];
    }
    ShaderResourceCacheVk& GetResourceCache(){return m_ShaderResourceCache;}

#ifdef VERIFY_SHADER_BINDINGS
    void dbgVerifyResourceBindings(const PipelineStateVkImpl *pPSO);
#endif

    bool StaticResourcesInitialized()const{return m_bStaticResourcesInitialized;}
    void InitializeStaticResources(const PipelineStateVkImpl *pPSO);

private:

    ShaderResourceCacheVk m_ShaderResourceCache;
    ShaderResourceLayoutVk* m_pResourceLayouts = nullptr;
    // Resource layout index in m_ResourceLayouts[] array for every shader stage
    Int8 m_ResourceLayoutIndex[6] = {-1, -1, -1, -1, -1, -1};
    bool m_bStaticResourcesInitialized = false;
    Uint32 m_NumShaders = 0;
*/
};

}
