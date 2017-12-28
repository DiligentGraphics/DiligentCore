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

#include "FormatMessage.h"
#include "BasicPlatformDebug.h"

#ifdef _DEBUG

#include <typeinfo>

// This function is only requried to ensure that Message argument passed to the macro
// is actually string and not something else
inline void EnsureStr( const char* ){}

#define ASSERTION_FAILED(Message, ...)\
do{                                         \
    EnsureStr(Message);                     \
    Diligent::MsgStream ms;                 \
    Diligent::FormatMsg( ms, Message, ##__VA_ARGS__);\
    DebugAssertionFailed( ms.str().c_str(), __FUNCTION__, __FILE__, __LINE__); \
}while(false)

#   define VERIFY(Expr, Message, ...)\
    do{                              \
        EnsureStr(Message);          \
        if( !(Expr) )                \
        {                            \
            ASSERTION_FAILED(Message, ##__VA_ARGS__);\
        }                            \
    }while(false)

#   define UNEXPECTED   ASSERTION_FAILED
#   define UNSUPPORTED  ASSERTION_FAILED

#   define VERIFY_EXPR(Expr) VERIFY(Expr, "Debug exression failed:\n", #Expr)


template<typename DstType, typename SrcType>
void CheckDynamicType( SrcType *pSrcPtr )
{
    VERIFY(pSrcPtr == nullptr || dynamic_cast<DstType*> (pSrcPtr) != nullptr, "Dynamic type cast failed. Src typeid: \'", typeid(*pSrcPtr).name(), "\' Dst typeid: \'", typeid(DstType).name(), '\'');
}
#   define CHECK_DYNAMIC_TYPE(DstType, pSrcPtr) do{ CheckDynamicType<DstType>(pSrcPtr); }while(false)


#else

#   define CHECK_DYNAMIC_TYPE(...){}
#   define VERIFY(...){}
#   define UNEXPECTED(...){}
#   define UNSUPPORTED(...){}
#   define VERIFY_EXPR(...){}

#endif
