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

#include "ShaderResourceVariableD3D.h"

/// \file
/// Declaration of Diligent::ShaderVariableD3DBase class

namespace Diligent
{
    template<typename TShaderResourceLayout>
    struct ShaderVariableD3DBase : public IShaderResourceVariableD3D
    {
        ShaderVariableD3DBase(TShaderResourceLayout&            ParentResLayout,
                              const D3DShaderResourceAttribs&   Attribs,
                              SHADER_RESOURCE_VARIABLE_TYPE     VariableType) : 
            m_ParentResLayout   (ParentResLayout),
            m_Attribs           (Attribs),
            m_VariableType      (VariableType)
        {
        }

        virtual IReferenceCounters* GetReferenceCounters()const override final
        {
            return m_ParentResLayout.GetOwner().GetReferenceCounters();
        }

        virtual Atomics::Long AddRef()override final
        {
            return m_ParentResLayout.GetOwner().AddRef();
        }

        virtual Atomics::Long Release()override final
        {
            return m_ParentResLayout.GetOwner().Release();
        }

        void QueryInterface( const INTERFACE_ID& IID, IObject** ppInterface )override final
        {
            if( ppInterface == nullptr )
                return;

            *ppInterface = nullptr;
            if( IID == IID_ShaderResourceVariableD3D || IID == IID_ShaderResourceVariable || IID == IID_Unknown )
            {
                *ppInterface = this;
                (*ppInterface)->AddRef();
            }
        }        

        virtual SHADER_RESOURCE_VARIABLE_TYPE GetType()const override final
        {
            return m_VariableType;
        }

        virtual ShaderResourceDesc GetResourceDesc()const override final
        {
            return GetHLSLResourceDesc();
        }

        virtual HLSLShaderResourceDesc GetHLSLResourceDesc()const override final
        {
            return m_Attribs.GetHLSLResourceDesc();
        }

        virtual Uint32 GetIndex()const override final
        {
            return m_ParentResLayout.GetVariableIndex(*this);
        }

        const D3DShaderResourceAttribs&     m_Attribs;

    protected:
        TShaderResourceLayout&              m_ParentResLayout;
        const SHADER_RESOURCE_VARIABLE_TYPE m_VariableType;
    };
}
