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

#include "pch.h"
#include "BasicFileSystem.h"
#include "DebugUtilities.h"
#include <algorithm>

Diligent::String BasicFileSystem::m_strWorkingDirectory;

BasicFile::BasicFile( const FileOpenAttribs &OpenAttribs, Diligent::Char SlashSymbol ) : 
    m_OpenAttribs(OpenAttribs),
    m_Path( BasicFileSystem::GetFullPath(OpenAttribs.strFilePath) )
{
    BasicFileSystem::CorrectSlashes(m_Path, SlashSymbol);

    m_OpenAttribs.strFilePath = m_Path.c_str();
}

BasicFile::~BasicFile()
{
}

Diligent::String BasicFile::GetOpenModeStr()
{
    std::string OpenModeStr;
    switch(m_OpenAttribs.AccessMode)
    {
        case EFileAccessMode::Read:      OpenModeStr += 'r'; break;
        case EFileAccessMode::Overwrite: OpenModeStr += 'w'; break;
        case EFileAccessMode::Append:    OpenModeStr += 'a'; break;
        default:    break;
    }

    // Always open file in binary mode. Text mode is platform-specific
    OpenModeStr += 'b';

    return OpenModeStr;
}


std::string BasicFileSystem::GetFullPath( const Diligent::Char *strFilePath )
{
    std::string FullPath = m_strWorkingDirectory;
    auto len = FullPath.length();
    if( len>0 && FullPath[len - 1] != '\\' )
    {
        FullPath += '\\';
    }
    FullPath += strFilePath;
    return FullPath;
}

BasicFile* BasicFileSystem::OpenFile( FileOpenAttribs &OpenAttribs )
{
    return nullptr;
}

void BasicFileSystem::ReleaseFile( BasicFile * pFile)
{
    if( pFile )
        delete pFile;
}

bool BasicFileSystem::FileExists( const Diligent::Char *strFilePath )
{
    return false;
}

Diligent::Char BasicFileSystem::GetSlashSymbol()
{
    UNSUPPORTED( "Unsupported" );
    return 0;
}

void BasicFileSystem::CorrectSlashes( Diligent::String &Path, Diligent::Char SlashSymbol )
{
    VERIFY( SlashSymbol == '\\' || SlashSymbol == '/', 
            "Incorrect slash symbol" );
    Diligent::Char RevSlashSym = (SlashSymbol == '\\') ? '/' : '\\';
    std::replace( Path.begin(), Path.end(), RevSlashSym, SlashSymbol);
}

void BasicFileSystem::SplitFilePath(const Diligent::String& FullName,
                                     Diligent::String *Path, 
                                     Diligent::String *Name)
{
    auto LastSlashPos = FullName.find_last_of( "/\\" );
    if( Path )
    {
        if( LastSlashPos != Diligent::String::npos )
            *Path = FullName.substr( 0, LastSlashPos );
        else
            *Path = "";
    }

    if( Name )
    {
        if( LastSlashPos != Diligent::String::npos )
            *Name = FullName.substr( LastSlashPos+1 );
        else
            *Path = FullName;
    }
}
