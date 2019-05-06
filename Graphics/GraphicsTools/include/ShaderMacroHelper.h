/*     Copyright 2015-2019 Egor Yusov
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

//--------------------------------------------------------------------------------------
// Copyright 2013 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------
#pragma once

#include <set>
#include <vector>
#include <sstream>
#include <iomanip>
#include "../../GraphicsEngine/interface/Shader.h"
#include "../../../Platforms/Basic/interface/DebugUtilities.h"


namespace Diligent
{

class ShaderMacroHelper
{
public:

    template<typename DefintionType>
	void AddShaderMacro( const Diligent::Char* Name, DefintionType Definition )
	{
		std::ostringstream ss;
		ss << Definition;
		AddShaderMacro<const Diligent::Char*>( Name, ss.str().c_str() );
	}
	
    void Finalize()
	{
        if (!m_bIsFinalized)
        {
            m_Macros.emplace_back(nullptr, nullptr);
            m_bIsFinalized = true;
        }
	}

    void Reopen()
    {
        if (m_bIsFinalized)
        {
            VERIFY_EXPR(m_Macros.size() > 0 && m_Macros.back().Name == nullptr && m_Macros.back().Definition == nullptr);
            m_Macros.pop_back();
            m_bIsFinalized = false;
        }
    }

    void Clear()
    {
        m_Macros.clear();
        m_DefinitionsPool.clear();
        m_bIsFinalized = false;
    }

	operator const ShaderMacro* ()
	{
        if (m_Macros.size() > 0 && !m_bIsFinalized)
            Finalize();
        return m_Macros.size() ? m_Macros.data() : nullptr;
	}
    
private:

	std::vector< ShaderMacro > m_Macros;
	std::set< std::string > m_DefinitionsPool;
    bool m_bIsFinalized = false;
};

template<>
inline void ShaderMacroHelper::AddShaderMacro( const Diligent::Char* Name, const Diligent::Char* Definition )
{
    Reopen();
    auto *PooledDefinition = m_DefinitionsPool.insert(Definition).first->c_str();
	m_Macros.emplace_back(Name, PooledDefinition);
}

template<>
inline void ShaderMacroHelper::AddShaderMacro( const Diligent::Char* Name, bool Definition )
{
	AddShaderMacro( Name, Definition ? "1" : "0");
}

template<>
inline void ShaderMacroHelper::AddShaderMacro( const Diligent::Char* Name, float Definition )
{
	std::ostringstream ss;
    
    // Make sure that when floating point represents integer, it is still
    // written as float: 1024.0, but not 1024. This is essnetial to
    // avoid type conversion issues in GLES.
    if( Definition == static_cast<float>(static_cast<int>(Definition)) )
        ss << std::fixed << std::setprecision( 1 );

	ss << Definition;
	AddShaderMacro<const Diligent::Char*>( Name, ss.str().c_str() );
}

template<>
inline void ShaderMacroHelper::AddShaderMacro( const Diligent::Char* Name, Uint32 Definition )
{
    // Make sure that uint constants have the 'u' suffix to avoid problems in GLES.
	std::ostringstream ss;
	ss << Definition << 'u';
	AddShaderMacro<const Diligent::Char*>( Name, ss.str().c_str() );
}

}
