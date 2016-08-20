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

#pragma once

#include "Errors.h"

/// \file
/// Declaration of Diligent::ComErrorDesc class

namespace Diligent
{

/// Helper class that provides description of a COM error
class ComErrorDesc
{
public:
    ComErrorDesc( HRESULT hr )
    {
        FormatMessageA( 
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            hr,
            MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
            m_Msg,
            _countof(m_Msg),
            NULL );
        auto nLen = strlen( m_Msg );
        if( nLen > 1 && m_Msg[nLen - 1] == '\n' ) 
        {
            m_Msg[nLen - 1] = 0;
            if( m_Msg[nLen - 2] == '\r' ) 
            {
                m_Msg[nLen - 2] = 0;
            }
        }
    }

    const char* Get(){ return m_Msg; }

private:
    char m_Msg[4096];
};

}


#define CHECK_D3D_RESULT_THROW(Expr, Message)\
{                           \
    HRESULT _hr_ = Expr;    \
    if(FAILED(_hr_))        \
    {                       \
        ComErrorDesc ErrDesc( _hr_ );    \
        LOG_ERROR_AND_THROW( Message, "\nHRESULT Desc: ", ErrDesc.Get());\
    }                       \
}

#define CHECK_D3D_RESULT_THROW_EX(Expr, ...)\
{                           \
    HRESULT _hr_ = Expr;    \
    if(FAILED(_hr_))        \
    {                       \
        Diligent::MsgStream ms;                \
        Diligent::FormatMsg(ms, __VA_ARGS__);  \
        ComErrorDesc ErrDesc( _hr_ );          \
        LOG_ERROR_AND_THROW( ms.str(), "\nHRESULT Desc: ", ErrDesc.Get());\
    }                       \
}
