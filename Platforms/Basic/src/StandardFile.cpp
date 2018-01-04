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

#include "StandardFile.h"
#include "DebugUtilities.h"
#include "Errors.h"

StandardFile::StandardFile(const FileOpenAttribs &OpenAttribs, Diligent::Char SlashSymbol) : 
    BasicFile(OpenAttribs, SlashSymbol),
    m_pFile(nullptr)
{
#if defined(PLATFORM_LINUX)
    auto OpenModeStr = GetOpenModeStr();
    m_pFile = fopen(m_OpenAttribs.strFilePath, OpenModeStr.c_str());
    if (m_pFile == nullptr)
    {
        LOG_ERROR_AND_THROW("Failed to open file ", m_OpenAttribs.strFilePath,
            "\nThe following error occured: ", strerror(errno));
    }
#endif
}

StandardFile::~StandardFile()
{
    if (m_pFile)
    {
        fclose(m_pFile);
        m_pFile = nullptr;
    }
}

void StandardFile::Read(Diligent::IDataBlob *pData)
{
    VERIFY_EXPR(pData != nullptr);
    auto FileSize = GetSize();
    pData->Resize(FileSize);
    auto Res = Read(pData->GetDataPtr(), pData->GetSize());
    VERIFY(Res, "Failed to read ", FileSize, " bytes from file");
}

bool StandardFile::Read(void *Data, size_t BufferSize)
{
    VERIFY(m_pFile, "File is not opened");
    if (!m_pFile)
        return 0;
    auto BytesRead = fread(Data, 1, BufferSize, m_pFile);

    return BytesRead == BufferSize;
}

bool StandardFile::Write(const void *Data, size_t BufferSize)
{
    VERIFY(m_pFile, "File is not opened");
    if (!m_pFile)
        return 0;
    auto BytesWritten = fwrite(Data, 1, BufferSize, m_pFile);

    return BytesWritten == BufferSize;
}

size_t StandardFile::GetSize()
{
    auto OrigPos = ftell(m_pFile);
    fseek(m_pFile, 0, SEEK_END);
    auto FileSize = ftell(m_pFile);

    fseek(m_pFile, OrigPos, SEEK_SET);
    return FileSize;
}

size_t StandardFile::GetPos()
{
    VERIFY(m_pFile, "File is not opened");
    if (!m_pFile)
        return 0;

    return ftell(m_pFile);
}

void StandardFile::SetPos(size_t Offset, FilePosOrigin Origin)
{
    VERIFY(m_pFile, "File is not opened");
    if (!m_pFile)
        return;

    int orig = SEEK_SET;
    switch (Origin)
    {
    case FilePosOrigin::Start: orig = SEEK_SET; break;
    case FilePosOrigin::Curr:  orig = SEEK_CUR; break;
    case FilePosOrigin::End:   orig = SEEK_END; break;
    default: UNEXPECTED("Unknown origin");
    }

    fseek(m_pFile, static_cast<long>(Offset), orig);
}
