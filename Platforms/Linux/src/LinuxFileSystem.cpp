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

#include "LinuxFileSystem.h"
#include "Errors.h"
#include "DebugUtilities.h"

LinuxFile::LinuxFile( const FileOpenAttribs &OpenAttribs ) : 
    BasicFile(OpenAttribs, LinuxFileSystem::GetSlashSymbol())/*,
    m_pFile(nullptr)*/
{
    //auto OpenModeStr = GetOpenModeStr();

    //for (;; )
    //{
    //    errno_t err = fopen_s(&m_pFile, m_OpenAttribs.strFilePath, OpenModeStr.c_str());
    //    if (err == 0)
    //    {
    //        break;
    //    }
    //    else if (err == ENFILE || // Too many files open in system 
    //        err == EMFILE)  // Too many open files 
    //    {
    //        // No more file descriptors are available: we have to wait
    //        //g_SystemMetricsStream << "Failed to open file " << FileName;
    //        //g_SystemMetricsStream << "\nWaiting 50 ms...\n";
    //        //Sleep(50);
    //        continue;
    //    }
    //    else
    //    {
    //        char errstr[128];
    //        strerror_s(errstr, _countof(errstr), err);
    //        LOG_ERROR_AND_THROW("Failed to open file ", m_OpenAttribs.strFilePath,
    //            "\nThe following error occured: ", errstr);
    //    }
    //}
}

LinuxFile::~LinuxFile()
{
    //if( m_pFile )
    //{
    //    fclose( m_pFile );
    //    m_pFile = nullptr;
    //}
}

void LinuxFile::Read( Diligent::IDataBlob *pData )
{
#if 0
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
#endif
    UNSUPPORTED("Not implemented")
}

size_t LinuxFile::GetSize()
{
    UNSUPPORTED( "Not implemented" );

    return 0;
}

bool LinuxFile::Read( void *Data, size_t BufferSize )
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

bool LinuxFile::Write( const void *Data, size_t BufferSize )
{
    UNSUPPORTED( "Not implemented" );

    return false;
}

size_t LinuxFile::GetPos()
{
    UNSUPPORTED( "Not implemented" );

    return 0;
}

void LinuxFile::SetPos(size_t Offset, FilePosOrigin Origin)
{
    UNSUPPORTED( "Not implemented" );
}


LinuxFile* LinuxFileSystem::OpenFile( const FileOpenAttribs &OpenAttribs )
{
    LinuxFile *pFile = nullptr;
#if 0
    try
    {
        pFile = new LinuxFile( OpenAttribs );
    }
    catch( const std::runtime_error &err )
    {

    }
#endif
    UNSUPPORTED("Not implemented")
    return pFile;
}


bool LinuxFileSystem::FileExists( const Diligent::Char *strFilePath )
{
#if 0
    FileOpenAttribs OpenAttribs;
    OpenAttribs.strFilePath = strFilePath;
    BasicFile DummyFile( OpenAttribs, LinuxFileSystem::GetSlashSymbol() );
    const auto& Path = DummyFile.GetPath();
    std::vector<Diligent::Uint8> Data;
    bool b = JNIHelper::GetInstance()->ReadFile( Path.c_str(), &Data );
    return b;
#endif
    UNSUPPORTED("Not implemented")
    return false;
}

bool LinuxFileSystem::PathExists( const Diligent::Char *strPath )
{
    UNSUPPORTED( "Not implemented" );
    return false;
}
    
bool LinuxFileSystem::CreateDirectory( const Diligent::Char *strPath )
{
    UNSUPPORTED( "Not implemented" );
    return false;
}

void LinuxFileSystem::ClearDirectory( const Diligent::Char *strPath )
{
    UNSUPPORTED( "Not implemented" );
}

void LinuxFileSystem::DeleteFile( const Diligent::Char *strPath )
{
    UNSUPPORTED( "Not implemented" );
}
    
std::vector<std::unique_ptr<FindFileData>> LinuxFileSystem::Search(const Diligent::Char *SearchPattern)
{
    UNSUPPORTED( "Not implemented" );
    return std::vector<std::unique_ptr<FindFileData>>();
}
