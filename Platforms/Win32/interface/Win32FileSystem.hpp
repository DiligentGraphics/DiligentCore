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

#pragma once

#include <memory>
#include "../../Basic/interface/BasicFileSystem.hpp"
#include "../../Basic/interface/StandardFile.hpp"

class WindowsFile : public StandardFile
{
public:
    WindowsFile(const FileOpenAttribs& OpenAttribs);
};


struct WindowsFileSystem : public BasicFileSystem
{
public:
    static WindowsFile* OpenFile(const FileOpenAttribs& OpenAttribs);

    static inline Diligent::Char GetSlashSymbol() { return '\\'; }

    static bool FileExists(const Diligent::Char* strFilePath);
    static bool PathExists(const Diligent::Char* strPath);

    static bool CreateDirectory(const Diligent::Char* strPath);
    static void ClearDirectory(const Diligent::Char* strPath, bool Recursive = false);
    static void DeleteFile(const Diligent::Char* strPath);
    static void DeleteDirectory(const Diligent::Char* strPath);
    static bool IsDirectory(const Diligent::Char* strPath);

    static std::vector<std::unique_ptr<FindFileData>> Search(const Diligent::Char* SearchPattern);

    static std::string OpenFileDialog(const char* Title, const char* Filter);

    static std::string SaveFileDialog(const char* Title, const char* Filter);

    static std::string GetCurrentDirectory();


    /// Returns a relative path from one file or folder to another.

    /// \param [in]  strPathFrom     - Path that defines the start of the relative path.
    ///                                If this parameter is null, current directory will be used.
    /// \param [in]  IsFromDirectory - Indicates if strPathFrom is a directory.
    ///                                Ignored if strPathFrom is null (in which case current directory
    ///                                is used).
    /// \param [in]  strPathTo       - Path that defines the endpoint of the relative path.
    ///                                This parameter must not be null.
    /// \param [in]  IsToDirectory   - Indicates if strPathTo is a directory.
    /// \param [out] RelativePath    - Relative path from strPathFrom to strPathTo.
    ///                                If no relative path exists, strPathFrom will be returned.
    ///
    /// \return                        true if the relative path exists (i.e. strPathFrom and strPathTo
    ///                                have a common prefix), and false otherwise.
    static bool GetRelativePath(const Diligent::Char* strPathFrom,
                                bool                  IsFromDirectory,
                                const Diligent::Char* strPathTo,
                                bool                  IsToDirectory,
                                std::string&          RelativePath);
};
