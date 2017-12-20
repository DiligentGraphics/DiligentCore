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

#include "LinuxDebug.h"
#include "FormatMessage.h"
#include "FileSystem.h"
//#include <Linux/log.h>
#include <csignal>
#include <cassert>

void LinuxDebug :: AssertionFailed( const Diligent::Char *Message, const char *Function, const char *File, int Line )
{
    assert(false);
    //std::string FileName;
    //FileSystem::SplitFilePath( File, nullptr, &FileName );
    //std::stringstream msgss;
    //Diligent::FormatMsg( msgss, "\nDebug assertion failed in ", Function, "(), file ", FileName, ", line ", Line, ":\n", Message);
    //auto FullMsg = msgss.str();
    //OutputDebugMessage( DebugMessageSeverity::Error, FullMsg.c_str() );

    //raise( SIGTRAP );
};


void LinuxDebug::OutputDebugMessage( DebugMessageSeverity Severity, const Diligent::Char *Message )
{
    assert(false);
    //static const Linux_LogPriority Priorities[] = { Linux_LOG_INFO, Linux_LOG_WARN, Linux_LOG_ERROR, Linux_LOG_FATAL };
    //__Linux_log_print( Priorities[static_cast<int>(Severity)], "Graphics Engine", "%s", Message );
}
