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

#include <stdexcept>

#include "BasicPlatformDebug.h"
#include "FormatMessage.h"
#include "BasicFileSystem.h"

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
void LogError( const char *strFunctionName, const char *strFullFilePath, int Line, const FirstArgType& first, const RestArgsType&... RestArgs )
{
    std::string FileName;
    BasicFileSystem::SplitFilePath( strFullFilePath, nullptr, &FileName );
    Diligent::MsgStream ss;
    ss << "The following error occured in the " << strFunctionName << "() function (" << FileName << ", line " << Line << "):\n";
    Diligent::FormatMsg( ss, first, RestArgs... );
    auto strFullMessage = ss.str();
    OutputDebugMessage( bThrowException ? BasicPlatformDebug::DebugMessageSeverity::FatalError : BasicPlatformDebug::DebugMessageSeverity::Error, strFullMessage.c_str() );
    ThrowIf<bThrowException>(std::move(strFullMessage));
}

#define LOG_ERROR(...)\
do{                                       \
    LogError<false>(__FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__); \
}while(false)

#define LOG_ERROR_ONCE(...)\
do{                                     \
    static bool IsFirstTime = true;     \
    if(IsFirstTime)                     \
    {                                   \
        LogError<false>(__FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__); \
        IsFirstTime = false;            \
    }                                   \
}while(false)

#define LOG_ERROR_AND_THROW(...)\
do{                                     \
    LogError<true>(__FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__);\
}while(false)

#define LOG_DEBUG_MESSAGE(Severity, ...)\
do{                                     \
    Diligent::MsgStream ss;             \
    Diligent::FormatMsg( ss, ##__VA_ARGS__ );\
    OutputDebugMessage( Severity, ss.str().c_str() );\
}while(false)

#define LOG_ERROR_MESSAGE(...)    LOG_DEBUG_MESSAGE(BasicPlatformDebug::DebugMessageSeverity::Error,   ##__VA_ARGS__)
#define LOG_WARNING_MESSAGE(...)  LOG_DEBUG_MESSAGE(BasicPlatformDebug::DebugMessageSeverity::Warning, ##__VA_ARGS__)
#define LOG_INFO_MESSAGE(...)     LOG_DEBUG_MESSAGE(BasicPlatformDebug::DebugMessageSeverity::Info,    ##__VA_ARGS__)

#define LOG_DEBUG_MESSAGE_ONCE(Severity, ...)\
do{                                                \
    static bool IsFirstTime = true;                \
    if(IsFirstTime)                                \
    {                                              \
        LOG_DEBUG_MESSAGE(Severity, ##__VA_ARGS__);\
        IsFirstTime = false;                       \
    }                                              \
}while(false)

#define LOG_ERROR_MESSAGE_ONCE(...)    LOG_DEBUG_MESSAGE_ONCE(BasicPlatformDebug::DebugMessageSeverity::Error,   ##__VA_ARGS__)
#define LOG_WARNING_MESSAGE_ONCE(...)  LOG_DEBUG_MESSAGE_ONCE(BasicPlatformDebug::DebugMessageSeverity::Warning, ##__VA_ARGS__)
#define LOG_INFO_MESSAGE_ONCE(...)     LOG_DEBUG_MESSAGE_ONCE(BasicPlatformDebug::DebugMessageSeverity::Info,    ##__VA_ARGS__)
