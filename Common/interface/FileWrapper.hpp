/*
 *  Copyright 2019-2024 Diligent Graphics LLC
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

#include <vector>

#include "../../Primitives/interface/Errors.hpp"
#include "../../Primitives/interface/DataBlob.h"
#include "../../Platforms/Basic/interface/DebugUtilities.hpp"
#include "../../Platforms/interface/FileSystem.hpp"

namespace Diligent
{

class FileWrapper
{
public:
    FileWrapper() :
        m_pFile{nullptr}
    {}

    explicit FileWrapper(const Char*     Path,
                         EFileAccessMode Access = EFileAccessMode::Read) :
        m_pFile{nullptr}
    {
        FileOpenAttribs OpenAttribs(Path, Access);
        Open(OpenAttribs);
    }

    ~FileWrapper()
    {
        Close();
    }

    void Open(const FileOpenAttribs& OpenAttribs)
    {
        VERIFY(!m_pFile, "Another file already attached");
        Close();
        m_pFile = FileSystem::OpenFile(OpenAttribs);
    }

    CFile* Detach()
    {
        CFile* pFile = m_pFile;
        m_pFile      = nullptr;
        return pFile;
    }

    void Attach(CFile* pFile)
    {
        VERIFY(!m_pFile, "Another file already attached");
        Close();
        m_pFile = pFile;
    }

    void Close()
    {
        if (m_pFile)
            FileSystem::ReleaseFile(m_pFile);
        m_pFile = nullptr;
    }

    operator CFile*() { return m_pFile; }
    CFile* operator->() { return m_pFile; }

    explicit operator bool() const { return m_pFile != nullptr; }

    static bool ReadWholeFile(const char* FilePath, std::vector<Uint8>& Data, bool Silent = false);
    static bool ReadWholeFile(const char* FilePath, IDataBlob** ppData, bool Silent = false);
    static bool WriteFile(const char* FilePath, const void* Data, size_t Size, bool Silent = false);

private:
    FileWrapper(const FileWrapper&);
    const FileWrapper& operator=(const FileWrapper&);

    CFile* m_pFile;
};

} // namespace Diligent
