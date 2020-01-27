/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

#include "Win32FileSystem.hpp"
#include "Errors.hpp"
#include "../../Common/interface/StringTools.hpp"

// We can't use namespace Diligent before #including <Windows.h> because Diligent::INTERFACE_ID will confilct with windows InterfaceID
//using namespace Diligent;

// Windows.h defines CreateDirectory and DeleteFile as macros.
// So we need to do some tricks to avoid name mess.
static bool CreateDirectoryImpl(const Diligent::Char* strPath);

bool WindowsFileSystem::CreateDirectory(const Diligent::Char* strPath)
{
    return CreateDirectoryImpl(strPath);
}

static void DeleteFileImpl(const Diligent::Char* strPath);

void WindowsFileSystem::DeleteFile(const Diligent::Char* strPath)
{
    DeleteFileImpl(strPath);
}

#include <Windows.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

using namespace Diligent;

static std::vector<wchar_t> UTF8ToUTF16(LPCSTR lpUTF8)
{
    // When last parameter is 0, the function returns the required buffer size, in characters,
    // including any terminating null character.
    auto                 nChars = MultiByteToWideChar(CP_UTF8, 0, lpUTF8, -1, NULL, 0);
    std::vector<wchar_t> wstr(nChars);
    MultiByteToWideChar(CP_UTF8, 0, lpUTF8, -1, wstr.data(), nChars);
    return wstr;
}

WindowsFile::WindowsFile(const FileOpenAttribs& OpenAttribs) :
    StandardFile(OpenAttribs, WindowsFileSystem::GetSlashSymbol())
{
    VERIFY_EXPR(m_pFile == nullptr);
    auto OpenModeStr = WidenString(GetOpenModeStr());

    for (;;)
    {
        auto    UTF16FilePath = UTF8ToUTF16(m_OpenAttribs.strFilePath);
        errno_t err           = _wfopen_s(&m_pFile, UTF16FilePath.data(), OpenModeStr.c_str());
        if (err == 0)
        {
            break;
        }
        else if (err == ENFILE || // Too many files open in system
                 err == EMFILE)   // Too many open files
        {
            // No more file descriptors are available: we have to wait
            //g_SystemMetricsStream << "Failed to open file " << FileName;
            //g_SystemMetricsStream << "\nWaiting 50 ms...\n";
            Sleep(50);
            continue;
        }
        else
        {
            char errstr[128];
            strerror_s(errstr, _countof(errstr), err);
            LOG_ERROR_AND_THROW("Failed to open file ", m_OpenAttribs.strFilePath,
                                "\nThe following error occured: ", errstr);
        }
    }
}

WindowsFile* WindowsFileSystem::OpenFile(const FileOpenAttribs& OpenAttribs)
{
    WindowsFile* pFile = nullptr;
    try
    {
        pFile = new WindowsFile(OpenAttribs);
    }
    catch (const std::runtime_error& /*err*/)
    {
    }

    return pFile;
}

bool WindowsFileSystem::FileExists(const Char* strFilePath)
{
    FileOpenAttribs OpenAttribs;
    OpenAttribs.strFilePath = strFilePath;
    BasicFile   DummyFile(OpenAttribs, WindowsFileSystem::GetSlashSymbol());
    const auto& Path          = DummyFile.GetPath(); // This is necessary to correct slashes
    auto        UTF16FilePath = UTF8ToUTF16(Path.c_str());
    FILE*       pFile         = nullptr;
    auto        err           = _wfopen_s(&pFile, UTF16FilePath.data(), L"r");
    bool        Exists        = (err == 0);
    if (Exists && pFile)
        fclose(pFile);
    return Exists;
}

static bool CreateDirectoryImpl(const Char* strPath)
{
    // Test all parent directories
    std::string            DirectoryPath = strPath;
    std::string::size_type SlashPos      = std::wstring::npos;
    const auto             SlashSym      = WindowsFileSystem::GetSlashSymbol();
    WindowsFileSystem::CorrectSlashes(DirectoryPath, SlashSym);

    do
    {
        SlashPos = DirectoryPath.find(SlashSym, (SlashPos != std::string::npos) ? SlashPos + 1 : 0);

        std::string ParentDir = (SlashPos != std::wstring::npos) ? DirectoryPath.substr(0, SlashPos) : DirectoryPath;
        if (!WindowsFileSystem::PathExists(ParentDir.c_str()))
        {
            // If there is no directory, create it
            if (!::CreateDirectoryA(ParentDir.c_str(), NULL))
                return false;
        }
    } while (SlashPos != std::string::npos);

    return true;
}

void WindowsFileSystem::ClearDirectory(const Char* strPath, bool Recursive)
{
    WIN32_FIND_DATAA ffd;
    HANDLE           hFind = INVALID_HANDLE_VALUE;

    // Find the first file in the directory.
    std::string Directory(strPath);
    if (Directory.length() > 0 && Directory.back() != GetSlashSymbol())
        Directory.push_back(GetSlashSymbol());

    auto SearchPattern = Directory + "*";
    hFind              = FindFirstFileA(SearchPattern.c_str(), &ffd);

    if (INVALID_HANDLE_VALUE == hFind)
    {
        LOG_ERROR_MESSAGE("FindFirstFile failed with error code ", GetLastError());
        return;
    }

    // List all the files in the directory
    do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (Recursive)
            {
                // Skip '.' and anything that begins with '..'
                if (!((ffd.cFileName[0] == '.' && ffd.cFileName[1] == 0) || (ffd.cFileName[0] == '.' && ffd.cFileName[1] == '.')))
                {
                    auto SubDirName = Directory + ffd.cFileName;
                    ClearDirectory(SubDirName.c_str(), Recursive);

                    if (RemoveDirectoryA(SubDirName.c_str()) == FALSE)
                    {
                        LOG_ERROR_MESSAGE("Failed to remove directory '", SubDirName, "'. Error code: ", GetLastError());
                    }
                }
            }
        }
        else
        {
            auto FileName = Directory + ffd.cFileName;
            DeleteFileImpl(FileName.c_str());
        }
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);
}


static void DeleteFileImpl(const Char* strPath)
{
    if (SetFileAttributesA(strPath, FILE_ATTRIBUTE_NORMAL) == FALSE)
    {
        LOG_WARNING_MESSAGE("Failed to set FILE_ATTRIBUTE_NORMAL for file '", strPath, "'. Error code: ", GetLastError());
    }

    if (DeleteFileA(strPath) == FALSE)
    {
        LOG_ERROR_MESSAGE("Failed to delete file '", strPath, "'. Error code: ", GetLastError());
    }
}

void WindowsFileSystem::DeleteDirectory(const Diligent::Char* strPath)
{
    ClearDirectory(strPath, true);

    if (RemoveDirectoryA(strPath) == FALSE)
    {
        LOG_ERROR_MESSAGE("Failed to remove directory '", strPath, "'. Error code: ", GetLastError());
    }
}


bool WindowsFileSystem::PathExists(const Char* strPath)
{
    return PathFileExistsA(strPath) != FALSE;
}

struct WndFindFileData : public FindFileData
{
    virtual const Char* Name() const override { return ffd.cFileName; }

    virtual bool IsDirectory() const override { return (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }

    WIN32_FIND_DATAA ffd;

    WndFindFileData(const WIN32_FIND_DATAA& _ffd) :
        ffd{_ffd} {}
};

std::vector<std::unique_ptr<FindFileData>> WindowsFileSystem::Search(const Char* SearchPattern)
{
    std::vector<std::unique_ptr<FindFileData>> SearchRes;

    WIN32_FIND_DATAA ffd;
    // Find the first file in the directory.
    auto hFind = FindFirstFileA(SearchPattern, &ffd);

    if (INVALID_HANDLE_VALUE == hFind)
    {
        return SearchRes;
    }

    // List all the files in the directory
    do
    {
        SearchRes.emplace_back(new WndFindFileData(ffd));
    } while (FindNextFileA(hFind, &ffd) != 0);

    auto dwError = GetLastError();
    if (dwError != ERROR_NO_MORE_FILES)
    {
        //ErrorHandler(TEXT("FindFirstFile"));
    }

    FindClose(hFind);

    return SearchRes;
}

std::string WindowsFileSystem::OpenFileDialog(const char* Title, const char* Filter)
{
    std::string   FileName;
    char          buffer[1024] = {};
    OPENFILENAMEA ofn          = {};
    ofn.lStructSize            = sizeof(ofn);
    ofn.lpstrFilter            = Filter;
    ofn.lpstrFile              = buffer;
    ofn.nMaxFile               = _countof(buffer);
    ofn.lpstrTitle             = Title;
    ofn.Flags                  = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn))
    {
        FileName = buffer;
    }
    return FileName;
}

bool WindowsFileSystem::IsDirectory(const Diligent::Char* strPath)
{
    if (!PathExists(strPath))
    {
        LOG_WARNING_MESSAGE("Path '", strPath, "' does not exist. Use PathExists function to check if path exists.");
        return false;
    }

    return (GetFileAttributesA(strPath) & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
