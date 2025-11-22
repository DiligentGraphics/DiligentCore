/*
 *  Copyright 2019-2025 Diligent Graphics LLC
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

#include "BasicFileSystem.hpp"

#include <algorithm>
#include <cstring>

#include "DebugUtilities.hpp"

namespace Diligent
{

String BasicFileSystem::m_strWorkingDirectory;

BasicFile::BasicFile(const FileOpenAttribs& OpenAttribs) :
    m_Path //
    {
        [](const Char* strPath) //
        {
            String Path = strPath != nullptr ? strPath : "";
            BasicFileSystem::CorrectSlashes(Path);
            return Path;
        }(OpenAttribs.strFilePath) //
    },
    m_OpenAttribs{m_Path.c_str(), OpenAttribs.AccessMode}
{
}

BasicFile::~BasicFile()
{
}

String BasicFile::GetOpenModeStr()
{
    std::string OpenModeStr;
    switch (m_OpenAttribs.AccessMode)
    {
        // clang-format off
        case EFileAccessMode::Read:            OpenModeStr += 'r'; break;
        case EFileAccessMode::Overwrite:       OpenModeStr += 'w'; break;
        case EFileAccessMode::Append:          OpenModeStr += 'a'; break;
        case EFileAccessMode::ReadUpdate:      OpenModeStr += "r+"; break;
        case EFileAccessMode::OverwriteUpdate: OpenModeStr += "w+"; break;
        case EFileAccessMode::AppendUpdate:    OpenModeStr += "a+"; break;
        // clang-format on
        default: break;
    }

    // Always open file in binary mode. Text mode is platform-specific
    OpenModeStr += 'b';

    return OpenModeStr;
}

BasicFile* BasicFileSystem::OpenFile(FileOpenAttribs& OpenAttribs)
{
    return nullptr;
}

void BasicFileSystem::ReleaseFile(BasicFile* pFile)
{
    if (pFile)
        delete pFile;
}

bool BasicFileSystem::FileExists(const Char* strFilePath)
{
    return false;
}

void BasicFileSystem::CorrectSlashes(String& Path, Char Slash)
{
    if (Slash != 0)
        DEV_CHECK_ERR(IsSlash(Slash), "Incorrect slash symbol");
    else
        Slash = SlashSymbol;
    Char RevSlashSym = (Slash == '\\') ? '/' : '\\';
    std::replace(Path.begin(), Path.end(), RevSlashSym, Slash);
}

void BasicFileSystem::GetPathComponents(const String& Path,
                                        String*       Directory,
                                        String*       FileName)
{
    auto LastSlashPos = Path.find_last_of("/\\");
    if (Directory)
    {
        if (LastSlashPos != String::npos)
            *Directory = Path.substr(0, LastSlashPos);
        else
            *Directory = "";
    }

    if (FileName)
    {
        if (LastSlashPos != String::npos)
            *FileName = Path.substr(LastSlashPos + 1);
        else
            *FileName = Path;
    }
}

bool BasicFileSystem::IsPathAbsolute(const Char* strPath)
{
    if (strPath == nullptr || strPath[0] == 0)
        return false;

#if PLATFORM_WIN32 || PLATFORM_UNIVERSAL_WINDOWS
    return ((strPath[1] == ':' && (strPath[2] == '\\' || strPath[2] == '/')) || // c:\Path or c:/Path
            (strPath[0] == '\\' && strPath[1] == '\\'));                        // \\?\Path
#elif PLATFORM_LINUX || PLATFORM_MACOS || PLATFORM_IOS || PLATFORM_TVOS || PLATFORM_ANDROID || PLATFORM_WEB
    return strPath[0] == '/';
#else
#    error Unknown platform.
#endif
}

template <typename StringType>
std::vector<StringType> SplitPath(const Char* Path, bool Simplify)
{
    std::vector<StringType> Components;

    // BasicFileSystem::IsSlash() does not get inlined by at least MSVC
    auto IsSlash = [](Char c) {
        return c == '/' || c == '\\';
    };

    // Estimate the number of components and reserve space in the vector
    {
        size_t CompnentCount = 1;
        for (const auto* c = Path; *c != '\0'; ++c)
        {
            if (IsSlash(c[0]) && !IsSlash(c[1]))
                ++CompnentCount;
        }
        Components.reserve(CompnentCount);
    }

    const auto* c = Path;
    while (*c != '\0')
    {
        while (IsSlash(*c))
            ++c;

        if (*c == '\0')
        {
            // a/
            break;
        }

        const auto* const CmpStart = c;
        while (*c != '\0' && !IsSlash(*c))
            ++c;

        if (Simplify)
        {
            if ((c - CmpStart) == 1 && CmpStart[0] == '.') // "."
            {
                // Skip /.
                continue;
            }
            else if ((c - CmpStart) == 2 && CmpStart[0] == '.' && CmpStart[1] == '.') // ".."
            {
                // Pop previous subdirectory if "/.." is found, but only if there is
                // no ".." already (e.g "../..")
                if (!Components.empty() && Components.back() != "..")
                {
                    Components.pop_back();
                    continue;
                }
            }
        }

        Components.emplace_back(CmpStart, c);
    }

    return Components;
}

std::vector<String> BasicFileSystem::SplitPath(const Char* Path, bool Simplify)
{
    return Diligent::SplitPath<String>(Path, Simplify);
}

std::string BasicFileSystem::BuildPathFromComponents(const std::vector<String>& Components, Char Slash)
{
    if (Slash != 0)
        DEV_CHECK_ERR(IsSlash(Slash), "Incorrect slash symbol");
    else
        Slash = SlashSymbol;

    std::string Path;
    for (const auto& Cmp : Components)
    {
        if (!Path.empty())
            Path += Slash;
        Path += Cmp;
    }

    return Path;
}

std::string BasicFileSystem::SimplifyPath(const Char* Path, Char Slash)
{
    if (Path == nullptr || Path[0] == '\0')
        return "";

    DEV_CHECK_ERR(Slash == 0 || IsSlash(Slash), "Incorrect slash symbol");
    Slash = IsSlash(Slash) ? Slash : SlashSymbol;

    std::string SimplifiedPath;
    const char* c = Path;

    if (Slash == WinSlash)
    {
        // Windows path
        if (c[1] == ':')
        {
            // Windows drive letter (e.g., C:)
            SimplifiedPath.push_back(*(c++)); // Drive letter
            SimplifiedPath.push_back(*(c++)); // ':'
        }
        else if (IsSlash(c[0]) && IsSlash(c[1]))
        {
            // Windows UNC path (e.g., \\Server\Share)
            SimplifiedPath.push_back(Slash); // First '\'
            SimplifiedPath.push_back(Slash); // Second '\'
            c += 2;
            // Copy server name
            while (*c != '\0' && !IsSlash(*c))
                SimplifiedPath.push_back(*(c++));
        }
    }
    else
    {
        // Unix path
        VERIFY_EXPR(Slash == UnixSlash);
        if (IsSlash(*c))
        {
            // Unix absolute path (e.g., /home/user)
            SimplifiedPath.push_back(Slash);
            ++c;
        }
    }

    const size_t RootLen = SimplifiedPath.length();

    Uint32 NumLeadingDirUps = 0;
    while (*c != '\0')
    {
        // Skip leading slashes
        while (IsSlash(*c))
            ++c;

        // Handle . and ..
        if (*c == '.')
        {
            if ((IsSlash(c[1]) || c[1] == '\0'))
            {
                // Skip /.
                c += (c[1] != '\0') ? 2 : 1;
                continue;
            }

            if (c[1] == '.' && (IsSlash(c[2]) || c[2] == '\0'))
            {
                // Handle /..
                c += (c[2] != '\0') ? 3 : 2;
                // Pop previous subdirectory unless it is a root
                if (SimplifiedPath.size() > RootLen)
                {
                    while (SimplifiedPath.size() > RootLen)
                    {
                        bool WasSlash = IsSlash(SimplifiedPath.back());
                        SimplifiedPath.pop_back();
                        if (WasSlash)
                            break;
                    }
                }
                else if (RootLen == 0)
                {
                    // Relative path - count leading ../
                    ++NumLeadingDirUps;
                }
                continue;
            }
        }

        if (*c == '\0')
            break;

        if (!SimplifiedPath.empty() && !IsSlash(SimplifiedPath.back()))
        {
            SimplifiedPath.push_back(Slash);
        }

        // Copy regular path component
        while (*c != '\0' && !IsSlash(*c))
        {
            SimplifiedPath.push_back(*(c++));
        }
    }

    if (NumLeadingDirUps > 0)
    {
        const bool IsPathEmpty = SimplifiedPath.empty();
        SimplifiedPath.insert(0, NumLeadingDirUps * 3 - (IsPathEmpty ? 1 : 0), '.');
        for (Uint32 i = 0; i < NumLeadingDirUps - (IsPathEmpty ? 1 : 0); ++i)
        {
            SimplifiedPath[i * 3 + 2] = Slash;
        }
    }

    return SimplifiedPath;
}


std::string BasicFileSystem::GetRelativePath(const Char* PathFrom,
                                             bool        IsFromDirectory,
                                             const Char* PathTo,
                                             bool /*IsToDirectory*/,
                                             Char Slash)
{
    DEV_CHECK_ERR(PathFrom != nullptr, "Source path must not be null");
    DEV_CHECK_ERR(PathTo != nullptr, "Destination path must not be null");
    if (Slash != 0)
        DEV_CHECK_ERR(IsSlash(Slash), "Incorrect slash symbol");
    else
        Slash = SlashSymbol;

    const auto FromPathComps = SplitPath(PathFrom, true);
    const auto ToPathComps   = SplitPath(PathTo, true);

    auto from_it = FromPathComps.begin();
    auto to_it   = ToPathComps.begin();
    while (from_it != FromPathComps.end() && to_it != ToPathComps.end() && *from_it == *to_it)
    {
        ++from_it;
        ++to_it;
    }
    if (from_it == FromPathComps.begin())
        return PathFrom; // No common prefix

    String RelPath;
    for (; from_it != FromPathComps.end(); ++from_it)
    {
        if (!IsFromDirectory && from_it + 1 == FromPathComps.end())
        {
            //                    from_it
            //                       V
            // from:    "common/from/file"
            // to:      "common/to"
            // RelPath: ".."
            break;
        }

        if (!RelPath.empty())
            RelPath.push_back(Slash);
        RelPath.append("..");
    }

    for (; to_it != ToPathComps.end(); ++to_it)
    {
        // IsToDirectory is in fact irrelevant
        if (!RelPath.empty())
            RelPath.push_back(Slash);
        RelPath.append(*to_it);
    }

    return RelPath;
}


std::string BasicFileSystem::FileDialog(const FileDialogAttribs& DialogAttribs)
{
    LOG_WARNING_MESSAGE("File dialog is not implemented on this platform");
    return "";
}

std::string BasicFileSystem::OpenFolderDialog(const char* Title)
{
    LOG_WARNING_MESSAGE("Open folder dialog is not implemented on this platform");
    return "";
}

} // namespace Diligent
