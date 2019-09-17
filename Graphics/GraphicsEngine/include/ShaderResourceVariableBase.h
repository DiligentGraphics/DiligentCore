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

/// \file
/// Implementation of the Diligent::ShaderBase template class

#include <vector>

#include "ShaderResourceVariable.h"
#include "PipelineState.h"

namespace Diligent
{

template<typename TNameCompare>
SHADER_RESOURCE_VARIABLE_TYPE GetShaderVariableType(SHADER_TYPE                       ShaderStage,
                                                    SHADER_RESOURCE_VARIABLE_TYPE     DefaultVariableType,
                                                    const ShaderResourceVariableDesc* Variables,
                                                    Uint32                            NumVars,
                                                    TNameCompare                      NameCompare)
{
    for (Uint32 v = 0; v < NumVars; ++v)
    {
        const auto& CurrVarDesc = Variables[v];
        if ( ((CurrVarDesc.ShaderStages & ShaderStage) != 0) && NameCompare(CurrVarDesc.Name) )
        {
            return CurrVarDesc.Type;
        }
    }
    return DefaultVariableType;
}

inline SHADER_RESOURCE_VARIABLE_TYPE GetShaderVariableType(SHADER_TYPE                       ShaderStage,
                                                           const Char*                       Name,
                                                           SHADER_RESOURCE_VARIABLE_TYPE     DefaultVariableType,
                                                           const ShaderResourceVariableDesc* Variables,
                                                           Uint32                            NumVars)
{
    return GetShaderVariableType(ShaderStage, DefaultVariableType, Variables, NumVars, 
        [&](const char* VarName)
        {
            return strcmp(VarName, Name) == 0;
        }
    );
}

inline SHADER_RESOURCE_VARIABLE_TYPE GetShaderVariableType(SHADER_TYPE                       ShaderStage,
                                                           const Char*                       Name,
                                                           const PipelineResourceLayoutDesc& LayoutDesc)
{
    return GetShaderVariableType(ShaderStage, Name, LayoutDesc.DefaultVariableType, LayoutDesc.Variables, LayoutDesc.NumVariables);
}

inline SHADER_RESOURCE_VARIABLE_TYPE GetShaderVariableType(SHADER_TYPE                        ShaderStage,
                                                           const String&                      Name,
                                                           SHADER_RESOURCE_VARIABLE_TYPE      DefaultVariableType,
                                                           const ShaderResourceVariableDesc*  Variables,
                                                           Uint32                             NumVars)
{
    return GetShaderVariableType(ShaderStage, DefaultVariableType, Variables, NumVars, 
        [&](const char* VarName)
        {
            return Name.compare(VarName) == 0;
        }
    );
}

inline SHADER_RESOURCE_VARIABLE_TYPE GetShaderVariableType(SHADER_TYPE                       ShaderStage,
                                                           const String&                     Name,
                                                           const PipelineResourceLayoutDesc& LayoutDesc)
{
    return GetShaderVariableType(ShaderStage, Name, LayoutDesc.DefaultVariableType, LayoutDesc.Variables, LayoutDesc.NumVariables);
}

inline bool IsAllowedType(SHADER_RESOURCE_VARIABLE_TYPE VarType, Uint32 AllowedTypeBits)noexcept
{
    return ((1 << VarType) & AllowedTypeBits) != 0;
}

inline Uint32 GetAllowedTypeBits(const SHADER_RESOURCE_VARIABLE_TYPE* AllowedVarTypes, Uint32 NumAllowedTypes)noexcept
{
    if(AllowedVarTypes == nullptr)
        return 0xFFFFFFFF;

    Uint32 AllowedTypeBits = 0;
    for(Uint32 i=0; i < NumAllowedTypes; ++i)
        AllowedTypeBits |= 1 << AllowedVarTypes[i];
    return AllowedTypeBits;
}

/// Base implementation of a shader variable
struct ShaderVariableBase : public IShaderResourceVariable
{
    ShaderVariableBase(IObject& Owner) : 
        // Shader variables are always created as part of the shader, or 
        // shader resource binding, so we must provide owner pointer to 
        // the base class constructor
        m_Owner(Owner)
    {
    }

    IObject& GetOwner()
    {
        return m_Owner;
    }

    virtual IReferenceCounters* GetReferenceCounters()const override final
    {
        return m_Owner.GetReferenceCounters();
    }

    virtual Atomics::Long AddRef()override final
    {
        return m_Owner.AddRef();
    }

    virtual Atomics::Long Release()override final
    {
        return m_Owner.Release();
    }

    virtual void QueryInterface( const INTERFACE_ID& IID, IObject** ppInterface )override final
    {
        if( ppInterface == nullptr )
            return;

        *ppInterface = nullptr;
        if( IID == IID_ShaderResourceVariable || IID == IID_Unknown )
        {
            *ppInterface = this;
            (*ppInterface)->AddRef();
        }
    }

protected:
    IObject& m_Owner;
};

}
