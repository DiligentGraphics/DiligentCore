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

#include <stdexcept>
#include <string>
#include <iostream>

#include "DebugOutput.h"
#include "FormatMessage.h"

namespace Diligent
{

template<bool>
void ThrowIf(std::string &&)
{
}

template<>
inline void ThrowIf<true>(std::string &&msg)
{
    throw std::runtime_error( std::move(msg) );
}

template<bool bThrowException, typename FirstArgType, typename... RestArgsType>
void LogError( const char *Function, const char *FullFilePath, int Line, const FirstArgType& first, const RestArgsType&... RestArgs )
{
    std::string FileName(FullFilePath);
    auto LastSlashPos = FileName.find_last_of("/\\");
    if(LastSlashPos != std::string::npos)
        FileName.erase(0, LastSlashPos+1);
    Diligent::MsgStream ss;
    Diligent::FormatMsg( ss, first, RestArgs... );
    auto Msg = ss.str();
    if(DebugMessageCallback != nullptr)
    {
        DebugMessageCallback( bThrowException ? DebugMessageSeverity::FatalError : DebugMessageSeverity::Error, Msg.c_str(), Function, FileName.c_str(), Line);
    }
    else
    {
        // No callback set - output to cerr
        std::cerr << "Diligent Engine: " << (bThrowException ? "Fatal Error" : "Error") << " in " << Function << "() (" << FileName << ", " << Line << "): " << Msg << '\n';
    }
    ThrowIf<bThrowException>(std::move(Msg));
}

}



#define LOG_ERROR(...)\
do{                                       \
    Diligent::LogError<false>(__FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__); \
}while(false)


#define LOG_ERROR_ONCE(...)\
do{                                     \
    static bool IsFirstTime = true;     \
    if(IsFirstTime)                     \
    {                                   \
        LOG_ERROR(##__VA_ARGS__);       \
        IsFirstTime = false;            \
    }                                   \
}while(false)


#define LOG_ERROR_AND_THROW(...)\
do{                                     \
    Diligent::LogError<true>(__FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__);\
}while(false)


#define LOG_DEBUG_MESSAGE(Severity, ...)\
do{                                               \
    Diligent::MsgStream _msg_ss;                  \
    Diligent::FormatMsg( _msg_ss, ##__VA_ARGS__ );\
    if(Diligent::DebugMessageCallback != nullptr) Diligent::DebugMessageCallback( Severity, _msg_ss.str().c_str(), nullptr, nullptr, 0 );\
}while(false)

#define LOG_ERROR_MESSAGE(...)    LOG_DEBUG_MESSAGE(Diligent::DebugMessageSeverity::Error,   ##__VA_ARGS__)
#define LOG_WARNING_MESSAGE(...)  LOG_DEBUG_MESSAGE(Diligent::DebugMessageSeverity::Warning, ##__VA_ARGS__)
#define LOG_INFO_MESSAGE(...)     LOG_DEBUG_MESSAGE(Diligent::DebugMessageSeverity::Info,    ##__VA_ARGS__)


#define LOG_DEBUG_MESSAGE_ONCE(Severity, ...)\
do{                                                \
    static bool IsFirstTime = true;                \
    if(IsFirstTime)                                \
    {                                              \
        LOG_DEBUG_MESSAGE(Severity, ##__VA_ARGS__);\
        IsFirstTime = false;                       \
    }                                              \
}while(false)

#define LOG_ERROR_MESSAGE_ONCE(...)    LOG_DEBUG_MESSAGE_ONCE(Diligent::DebugMessageSeverity::Error,   ##__VA_ARGS__)
#define LOG_WARNING_MESSAGE_ONCE(...)  LOG_DEBUG_MESSAGE_ONCE(Diligent::DebugMessageSeverity::Warning, ##__VA_ARGS__)
#define LOG_INFO_MESSAGE_ONCE(...)     LOG_DEBUG_MESSAGE_ONCE(Diligent::DebugMessageSeverity::Info,    ##__VA_ARGS__)


#define CHECK(Expr, Severity, ...)\
do{                               \
    if( !(Expr) )                 \
    {                             \
        LOG_DEBUG_MESSAGE(Severity, ##__VA_ARGS__);\
    }                             \
}while(false)

#define CHECK_ERR(Expr, ...)  CHECK(Expr, Diligent::DebugMessageSeverity::Error,   ##__VA_ARGS__)
#define CHECK_WARN(Expr, ...) CHECK(Expr, Diligent::DebugMessageSeverity::Warning, ##__VA_ARGS__)
#define CHECK_INFO(Expr, ...) CHECK(Expr, Diligent::DebugMessageSeverity::Info,    ##__VA_ARGS__)

#define CHECK_THROW(Expr, ...)\
do{                               \
    if( !(Expr) )                 \
    {                             \
        LOG_ERROR_AND_THROW(##__VA_ARGS__);\
    }                             \
}while(false)


#ifdef DEVELOPMENT

#define DEV_CHECK_ERR(Expr, ...)  CHECK_ERR (Expr, ##__VA_ARGS__)
#define DEV_CHECK_WARN(Expr, ...) CHECK_WARN(Expr, ##__VA_ARGS__)
#define DEV_CHECK_INFO(Expr, ...) CHECK_INFO(Expr, ##__VA_ARGS__)

#else

#define DEV_CHECK_ERR (Expr, ...) do{}while(false)
#define DEV_CHECK_WARN(Expr, ...) do{}while(false)
#define DEV_CHECK_INFO(Expr, ...) do{}while(false)

#endif
