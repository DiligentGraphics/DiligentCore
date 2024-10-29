/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "FileWrapper.hpp"
#include "DataBlobImpl.hpp"

namespace Diligent
{

bool FileWrapper::ReadWholeFile(const char* FilePath, std::vector<Uint8>& Data, bool Silent)
{
    if (FilePath == nullptr)
    {
        DEV_ERROR("File path must not be null");
        return false;
    }

    FileWrapper File{FilePath, EFileAccessMode::Read};
    if (!File)
    {
        if (!Silent)
        {
            LOG_ERROR_MESSAGE("Failed to open file '", FilePath, "'.");
        }
        return false;
    }

    const size_t Size = File->GetSize();
    Data.resize(Size);
    if (Size > 0)
    {
        if (!File->Read(Data.data(), Size))
        {
            if (!Silent)
            {
                LOG_ERROR_MESSAGE("Failed to read file '", FilePath, "'.");
            }
            return false;
        }
    }

    return true;
}

bool FileWrapper::ReadWholeFile(const char* FilePath, IDataBlob** ppData, bool Silent)
{
    if (ppData == nullptr)
    {
        DEV_ERROR("Data pointer must not be null");
        return false;
    }

    DEV_CHECK_ERR(*ppData == nullptr, "Data pointer is not null. This may result in memory leak.");

    FileWrapper File{FilePath, EFileAccessMode::Read};
    if (!File)
    {
        if (!Silent)
        {
            LOG_ERROR_MESSAGE("Failed to open file '", FilePath, "'.");
        }
        return false;
    }

    RefCntAutoPtr<DataBlobImpl> pData = DataBlobImpl::Create();
    if (!File->Read(pData))
    {
        if (!Silent)
        {
            LOG_ERROR_MESSAGE("Failed to read file '", FilePath, "'.");
        }
        return false;
    }

    *ppData = pData.Detach();
    return true;
}

} // namespace Diligent
