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

#include "AndroidFileSystem.h"
#include "Errors.h"
#include "DebugUtilities.h"
#include <JNIHelper.h>

using namespace ndk_helper;

AndroidFile::AndroidFile( const FileOpenAttribs &OpenAttribs ) : 
    BasicFile(OpenAttribs, AndroidFileSystem::GetSlashSymbol())/*,
    m_pFile(nullptr)*/
{
    //auto OpenModeStr = GetOpenModeStr();
    //
    //m_pFile = fopen( FileName, OpenModeStr.c_str());
    //if( m_pFile != nullptr )
    //{
    //    break;
    //}
    //else
    //{
    //    std::stringstream ErrSS;
    //    ErrSS<< "Failed to open file" << std::endl << FileName;
    //    LOG_ERROR_AND_THROW(ErrSS.str().c_str());
    //}

    // Workaround:
    // Try to read data from the file to make sure it exists
    auto FullPath = m_OpenAttribs.strFilePath;
    std::vector<Diligent::Uint8> Data;
    if( !JNIHelper::GetInstance()->ReadFile( FullPath, &Data ) )
    {
        LOG_ERROR_AND_THROW( "Failed to open file ", FullPath );
    }
}

AndroidFile::~AndroidFile()
{
    //if( m_pFile )
    //{
    //    fclose( m_pFile );
    //    m_pFile = nullptr;
    //}
}

void AndroidFile::Read( Diligent::IDataBlob *pData )
{
    auto FullPath = m_OpenAttribs.strFilePath;
    std::vector<Diligent::Uint8> Data;
    bool b = JNIHelper::GetInstance()->ReadFile( FullPath, &Data );
    if( b )
    {
        pData->Resize( Data.size() );
        memcpy( pData->GetDataPtr(), Data.data(), Data.size() );
    }
    else
    {
        LOG_ERROR_MESSAGE( "Unable to open file ", m_OpenAttribs.strFilePath, "\nFull path: ", FullPath );
    }
}

size_t AndroidFile::GetSize()
{
    UNSUPPORTED( "Not implemented" );

    return 0;
}

bool AndroidFile::Read( void *Data, size_t BufferSize )
{
    UNSUPPORTED( "Not implemented" );

    //VERIFY( m_pFile, "File not opened" );
    //auto OrigPos = ftell( m_pFile );
    //fseek( m_pFile, 0, SEEK_END );
    //auto FileSize = ftell( m_pFile );
    //fseek( m_pFile, 0, SEEK_SET );
    //Data.resize( FileSize );
    //auto ItemsRead = fread( Data.data(), FileSize, 1, m_pFile );
    //fseek( m_pFile, OrigPos, SEEK_SET );
    //return ItemsRead == 1;
    return false;
}

bool AndroidFile::Write( const void *Data, size_t BufferSize )
{
    UNSUPPORTED( "Not implemented" );

    return false;
}

size_t AndroidFile::GetPos()
{
    UNSUPPORTED( "Not implemented" );

    return 0;
}

void AndroidFile::SetPos(size_t Offset, FilePosOrigin Origin)
{
    UNSUPPORTED( "Not implemented" );
}


AndroidFile* AndroidFileSystem::OpenFile( const FileOpenAttribs &OpenAttribs )
{
    AndroidFile *pFile = nullptr;
    try
    {
        pFile = new AndroidFile( OpenAttribs );
    }
    catch( const std::runtime_error &err )
    {

    }

    return pFile;
}


bool AndroidFileSystem::FileExists( const Diligent::Char *strFilePath )
{
    FileOpenAttribs OpenAttribs;
    OpenAttribs.strFilePath = strFilePath;
    BasicFile DummyFile( OpenAttribs, AndroidFileSystem::GetSlashSymbol() );
    const auto& Path = DummyFile.GetPath();
    std::vector<Diligent::Uint8> Data;
    bool b = JNIHelper::GetInstance()->ReadFile( Path.c_str(), &Data );
    return b;
}

bool AndroidFileSystem::PathExists( const Diligent::Char *strPath )
{
    UNSUPPORTED( "Not implemented" );
    return false;
}
    
bool AndroidFileSystem::CreateDirectory( const Diligent::Char *strPath )
{
    UNSUPPORTED( "Not implemented" );
    return false;
}

void AndroidFileSystem::ClearDirectory( const Diligent::Char *strPath )
{
    UNSUPPORTED( "Not implemented" );
}

void AndroidFileSystem::DeleteFile( const Diligent::Char *strPath )
{
    UNSUPPORTED( "Not implemented" );
}
    
std::vector<std::unique_ptr<FindFileData>> AndroidFileSystem::Search(const Diligent::Char *SearchPattern)
{
    UNSUPPORTED( "Not implemented" );
    return std::vector<std::unique_ptr<FindFileData>>();
}
