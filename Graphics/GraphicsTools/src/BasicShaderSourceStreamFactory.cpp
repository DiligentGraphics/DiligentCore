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
#include "BasicShaderSourceStreamFactory.h"
#include "RefCntAutoPtr.h"

namespace Diligent
{
    BasicShaderSourceStreamFactory::BasicShaderSourceStreamFactory( const Char *SearchDirectories )
    {
        while( SearchDirectories )
        {
            const char* Semicolon = strchr( SearchDirectories, ';' );
            String SearchPath;
            if( Semicolon == nullptr )
            {
                SearchPath = SearchDirectories;
                SearchDirectories = nullptr;
            }
            else
            {
                SearchPath = String( SearchDirectories, Semicolon );
                SearchDirectories = Semicolon + 1;
            }

            if( SearchPath.length() > 0 )
            {
                if( SearchPath.back() != '\\' && SearchPath.back() != '/' )
                    SearchPath.push_back( '\\' );
                m_SearchDirectories.push_back( SearchPath );
            }
        }
        m_SearchDirectories.push_back( "" );
    }

    void BasicShaderSourceStreamFactory::CreateInputStream( const Diligent::Char *Name, IFileStream **ppStream )
    {
        bool bFileCreated = false;
        Diligent::RefCntAutoPtr<BasicFileStream> pBasicFileStream;
        for( const auto &SearchDir : m_SearchDirectories )
        {
            String FullPath = SearchDir + ( (Name[0] == '\\' || Name[0] == '/') ? Name + 1 : Name);
            if( !FileSystem::FileExists( FullPath.c_str() ) )
                continue;
            pBasicFileStream = new BasicFileStream( FullPath.c_str(), EFileAccessMode::Read );
            if( pBasicFileStream->IsValid() )
            {
                bFileCreated = true;
                break;
            }
            else
            {
                pBasicFileStream.Release();
            }
        }
        if( bFileCreated )
        {
            pBasicFileStream->QueryInterface( IID_FileStream, reinterpret_cast<IObject**>(ppStream) );
        }
        else
        {
            *ppStream = nullptr;
            LOG_ERROR_AND_THROW( "Failed to create input stream for source file ", Name )
        }
    }
}