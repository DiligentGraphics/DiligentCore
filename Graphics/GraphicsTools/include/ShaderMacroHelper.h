/*     Copyright 2015-2016 Egor Yusov
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

#include <set>
#include <vector>
#include <sstream>
#include <iomanip>
#include "BasicTypes.h"
#include "DebugUtilities.h"
#include "Shader.h"

namespace Diligent
{

class ShaderMacroHelper
{
public:
    ShaderMacroHelper() : m_bIsFinalized(false) {}

    template<typename DefintionType>
	void AddShaderMacro( const Diligent::Char* Name, DefintionType Definition )
	{
        assert( !m_bIsFinalized );
		std::ostringstream ss;
		ss << Definition;
		AddShaderMacro<const Diligent::Char*>( Name, ss.str().c_str() );
	}
	
    void Finalize()
	{
		ShaderMacro LastMacro = {NULL, NULL};
		m_Macros.push_back(LastMacro);
        m_bIsFinalized = true;
	}

	operator const ShaderMacro* ()
	{
        assert( !m_Macros.size() || m_bIsFinalized );
        if( m_Macros.size() && !m_bIsFinalized )
            Finalize();
        return m_Macros.size() ? &m_Macros[0] : NULL;
	}
    
private:

	std::vector< ShaderMacro > m_Macros;
	std::set< std::string > m_DefinitionsPull;
    bool m_bIsFinalized;
};

template<>
inline void ShaderMacroHelper::AddShaderMacro( const Diligent::Char* Name, const Diligent::Char* Definition )
{
    VERIFY_EXPR( !m_bIsFinalized );
	ShaderMacro NewMacro = 
	{
		Name,
		m_DefinitionsPull.insert(Definition).first->c_str()
	};
	m_Macros.push_back(NewMacro);
}

template<>
inline void ShaderMacroHelper::AddShaderMacro( const Diligent::Char* Name, bool Definition )
{
    assert( !m_bIsFinalized );
	AddShaderMacro( Name, Definition ? "1" : "0");
}

template<>
inline void ShaderMacroHelper::AddShaderMacro( const Diligent::Char* Name, float Definition )
{
    assert( !m_bIsFinalized );
	std::ostringstream ss;
    
    // Make sure that when floating point represents integer, it is still
    // written as float: 1024.0, but not 1024. This is essnetial to
    // avoid type conversion issues in GLES.
    if( Definition == static_cast<float>(static_cast<int>(Definition)) )
        ss << std::fixed << std::setprecision( 1 );

	ss << Definition;
	AddShaderMacro<const Diligent::Char*>( Name, ss.str().c_str() );
}

}
