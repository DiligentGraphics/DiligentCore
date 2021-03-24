/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#include <stdio.h>
#include <unistd.h>
#include <cstdio>

#include "LinuxFileSystem.hpp"
#include "Errors.hpp"
#include "DebugUtilities.hpp"

LinuxFile* LinuxFileSystem::OpenFile(const FileOpenAttribs& OpenAttribs)
{
    LinuxFile* pFile = nullptr;
    try
    {
        pFile = new LinuxFile(OpenAttribs, LinuxFileSystem::GetSlashSymbol());
    }
    catch (const std::runtime_error& err)
    {
    }
    return pFile;
}


bool LinuxFileSystem::FileExists(const Diligent::Char* strFilePath)
{
    FileOpenAttribs OpenAttribs;
    OpenAttribs.strFilePath = strFilePath;
    BasicFile   DummyFile(OpenAttribs, LinuxFileSystem::GetSlashSymbol());
    const auto& Path   = DummyFile.GetPath(); // This is necessary to correct slashes
    FILE*       pFile  = fopen(Path.c_str(), "r");
    bool        Exists = (pFile != nullptr);
    if (Exists)
        fclose(pFile);
    return Exists;
}

bool LinuxFileSystem::PathExists(const Diligent::Char* strPath)
{
    UNSUPPORTED("Not implemented");
    return false;
}

bool LinuxFileSystem::CreateDirectory(const Diligent::Char* strPath)
{
    UNSUPPORTED("Not implemented");
    return false;
}

void LinuxFileSystem::ClearDirectory(const Diligent::Char* strPath)
{
    UNSUPPORTED("Not implemented");
}

void LinuxFileSystem::DeleteFile(const Diligent::Char* strPath)
{
    remove(strPath);
}

std::vector<std::unique_ptr<FindFileData>> LinuxFileSystem::Search(const Diligent::Char* SearchPattern)
{
    UNSUPPORTED("Not implemented");
    return std::vector<std::unique_ptr<FindFileData>>();
}
