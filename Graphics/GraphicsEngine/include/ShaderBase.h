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
/// Implementation of the Diligent::ShaderBase template class

#include "Shader.h"
#include "DeviceObjectBase.h"
#include "STDAllocator.h"
#include "PlatformMisc.h"
#include "EngineMemory.h"

namespace Diligent
{

inline SHADER_TYPE GetShaderTypeFromIndex( Int32 Index )
{
    return static_cast<SHADER_TYPE>(1 << Index);
}

inline Int32 GetShaderTypeIndex( SHADER_TYPE Type )
{
    Int32 ShaderIndex = PlatformMisc::GetLSB(Type);

#ifdef _DEBUG
    switch( Type )
    {
        case SHADER_TYPE_UNKNOWN: VERIFY_EXPR(ShaderIndex == -1); break;
        case SHADER_TYPE_VERTEX:  VERIFY_EXPR(ShaderIndex ==  0); break;
        case SHADER_TYPE_PIXEL:   VERIFY_EXPR(ShaderIndex ==  1); break;
        case SHADER_TYPE_GEOMETRY:VERIFY_EXPR(ShaderIndex ==  2); break;
        case SHADER_TYPE_HULL:    VERIFY_EXPR(ShaderIndex ==  3); break;
        case SHADER_TYPE_DOMAIN:  VERIFY_EXPR(ShaderIndex ==  4); break;
        case SHADER_TYPE_COMPUTE: VERIFY_EXPR(ShaderIndex ==  5); break;
        default: UNEXPECTED( "Unexpected shader type (", Type, ")" ); break;
    }
    VERIFY( Type == GetShaderTypeFromIndex(ShaderIndex), "Incorrect shader type index" );
#endif
    return ShaderIndex;
}

static const int VSInd = GetShaderTypeIndex(SHADER_TYPE_VERTEX);
static const int PSInd = GetShaderTypeIndex(SHADER_TYPE_PIXEL);
static const int GSInd = GetShaderTypeIndex(SHADER_TYPE_GEOMETRY);
static const int HSInd = GetShaderTypeIndex(SHADER_TYPE_HULL);
static const int DSInd = GetShaderTypeIndex(SHADER_TYPE_DOMAIN);
static const int CSInd = GetShaderTypeIndex(SHADER_TYPE_COMPUTE);

template<typename TNameCompare>
SHADER_VARIABLE_TYPE GetShaderVariableType(SHADER_VARIABLE_TYPE DefaultVariableType, const ShaderVariableDesc *VariableDesc, Uint32 NumVars, TNameCompare NameCompare)
{
    for (Uint32 v = 0; v < NumVars; ++v)
    {
        const auto &CurrVarDesc = VariableDesc[v];
        if ( NameCompare(CurrVarDesc.Name) )
        {
            return CurrVarDesc.Type;
        }
    }
    return DefaultVariableType;
}

inline SHADER_VARIABLE_TYPE GetShaderVariableType(const Char* Name, SHADER_VARIABLE_TYPE DefaultVariableType, const ShaderVariableDesc *VariableDesc, Uint32 NumVars)
{
    return GetShaderVariableType(DefaultVariableType, VariableDesc, NumVars, 
        [&](const char *VarName)
        {
            return strcmp(VarName, Name) == 0;
        }
    );
}

inline SHADER_VARIABLE_TYPE GetShaderVariableType(const Char* Name, const ShaderDesc& ShdrDesc)
{
    return GetShaderVariableType(Name, ShdrDesc.DefaultVariableType, ShdrDesc.VariableDesc, ShdrDesc.NumVariables);
}

inline SHADER_VARIABLE_TYPE GetShaderVariableType(const String& Name, SHADER_VARIABLE_TYPE DefaultVariableType, const ShaderVariableDesc *VariableDesc, Uint32 NumVars)
{
    return GetShaderVariableType(DefaultVariableType, VariableDesc, NumVars, 
        [&](const char *VarName)
        {
            return Name.compare(VarName) == 0;
        }
    );
}

inline SHADER_VARIABLE_TYPE GetShaderVariableType(const String& Name, const ShaderDesc& ShdrDesc)
{
    return GetShaderVariableType(Name, ShdrDesc.DefaultVariableType, ShdrDesc.VariableDesc, ShdrDesc.NumVariables);
}


/// Base implementation of a shader variable

struct ShaderVariableBase : public IShaderVariable
{
    ShaderVariableBase(IObject &Owner) : 
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

    virtual void QueryInterface( const INTERFACE_ID &IID, IObject **ppInterface )override final
    {
        if( ppInterface == nullptr )
            return;

        *ppInterface = nullptr;
        if( IID == IID_ShaderVariable || IID == IID_Unknown )
        {
            *ppInterface = this;
            (*ppInterface)->AddRef();
        }
    }

protected:
    IObject &m_Owner;
};

/// Implementation of a dummy shader variable that silently ignores all operations
struct DummyShaderVariable : ShaderVariableBase
{
    DummyShaderVariable(IObject &Owner) :
        ShaderVariableBase(Owner)
    {}

    virtual void Set( IDeviceObject *pObject )override final
    {
        // Ignore operation
        // Probably output warning
    }

    virtual void SetArray(IDeviceObject* const* ppObjects, Uint32 FirstElement, Uint32 NumElements)override final
    {
        // Ignore operation
        // Probably output warning
    }
};

/// Template class implementing base functionality for a shader object

/// \tparam BaseInterface - base interface that this class will inheret 
///                         (Diligent::IShaderD3D11, Diligent::IShaderD3D12 or Diligent::IShaderGL).
/// \tparam RenderDeviceBaseInterface - base interface for the render device
///                                     (Diligent::IRenderDeviceD3D11, Diligent::IRenderDeviceD3D12, Diligent::IRenderDeviceGL, or Diligent::IRenderDeviceGLES).
template<class BaseInterface, class RenderDeviceBaseInterface>
class ShaderBase : public DeviceObjectBase<BaseInterface, ShaderDesc>
{
public:
    typedef DeviceObjectBase<BaseInterface, ShaderDesc> TDeviceObjectBase;

    /// \param pRefCounters - reference counters object that controls the lifetime of this shader.
	/// \param pDevice - pointer to the device.
	/// \param ShdrDesc - shader description.
	/// \param bIsDeviceInternal - flag indicating if the shader is an internal device object and 
	///							   must not keep a strong reference to the device.
    ShaderBase( IReferenceCounters *pRefCounters, IRenderDevice *pDevice, const ShaderDesc& ShdrDesc, bool bIsDeviceInternal = false ) :
        TDeviceObjectBase( pRefCounters, pDevice, ShdrDesc, bIsDeviceInternal ),
        m_DummyShaderVar(*this),
        m_VariablesDesc(ShdrDesc.NumVariables, ShaderVariableDesc(), STD_ALLOCATOR_RAW_MEM(ShaderVariableDesc, GetRawAllocator(), "Allocator for vector<ShaderVariableDesc>") ),
        m_StringPool(ShdrDesc.NumVariables + ShdrDesc.NumStaticSamplers, String(), STD_ALLOCATOR_RAW_MEM(String, GetRawAllocator(), "Allocator for vector<String>")),
        m_StaticSamplers(ShdrDesc.NumStaticSamplers, StaticSamplerDesc(), STD_ALLOCATOR_RAW_MEM(StaticSamplerDesc, GetRawAllocator(), "Allocator for vector<StaticSamplerDesc>") )
    {
        auto Str = m_StringPool.begin();
        if(this->m_Desc.VariableDesc)
        {
            for (Uint32 v = 0; v < this->m_Desc.NumVariables; ++v, ++Str)
            {
                m_VariablesDesc[v] = this->m_Desc.VariableDesc[v];
                VERIFY(m_VariablesDesc[v].Name != nullptr, "Variable name not provided");
                *Str = m_VariablesDesc[v].Name;
                m_VariablesDesc[v].Name = Str->c_str();
            }
            this->m_Desc.VariableDesc = m_VariablesDesc.data();
        }
        if(this->m_Desc.StaticSamplers)
        {
            for (Uint32 s = 0; s < this->m_Desc.NumStaticSamplers; ++s, ++Str)
            {
                m_StaticSamplers[s] = this->m_Desc.StaticSamplers[s];
                VERIFY(m_StaticSamplers[s].TextureName != nullptr, "Static sampler texture name not provided");
                *Str = m_StaticSamplers[s].TextureName;
                m_StaticSamplers[s].TextureName = Str->c_str();
#ifdef DEVELOPMENT
                const auto &BorderColor = m_StaticSamplers[s].Desc.BorderColor;
                if( !( (BorderColor[0] == 0 && BorderColor[1] == 0 && BorderColor[2] == 0 && BorderColor[3] == 0) ||
                       (BorderColor[0] == 0 && BorderColor[1] == 0 && BorderColor[2] == 0 && BorderColor[3] == 1) ||
                       (BorderColor[0] == 1 && BorderColor[1] == 1 && BorderColor[2] == 1 && BorderColor[3] == 1) ) )
                {
                    LOG_WARNING_MESSAGE("Static sampler for variable \"", *Str , "\" specifies border color (", BorderColor[0], ", ", BorderColor[1], ", ",  BorderColor[2], ", ",  BorderColor[3], "). D3D12 static samplers only allow transparent black (0,0,0,0), opaque black (0,0,0,1) or opaque white (1,1,1,1) as border colors");
                }
#endif
            }
            this->m_Desc.StaticSamplers = m_StaticSamplers.data();
        }

        VERIFY_EXPR(Str == m_StringPool.end());
    }

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE( IID_Shader, TDeviceObjectBase )
    
protected:
    DummyShaderVariable m_DummyShaderVar; ///< Dummy shader variable

    /// Shader variable descriptions
    std::vector<ShaderVariableDesc, STDAllocatorRawMem<ShaderVariableDesc> > m_VariablesDesc;
    /// String pool that is used to hold copies of variable names and static sampler names
    std::vector<String, STDAllocatorRawMem<String> > m_StringPool;
    /// Static sampler descriptions
    std::vector<StaticSamplerDesc, STDAllocatorRawMem<StaticSamplerDesc> > m_StaticSamplers;
};

}
