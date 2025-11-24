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

#include "pch.h"
#include "BasicFileStream.hpp"

namespace Diligent
{

RefCntAutoPtr<BasicFileStream> BasicFileStream::Create(const Char* Path, EFileAccessMode Access)
{
    if (Path == nullptr || Path[0] == '\0')
    {
        DEV_ERROR("Path must not be null or empty");
        return {};
    }

    return RefCntAutoPtr<BasicFileStream>{MakeNewRCObj<BasicFileStream>()(Path, Access)};
}

BasicFileStream::BasicFileStream(IReferenceCounters* pRefCounters,
                                 const Char*         Path,
                                 EFileAccessMode     Access /* = EFileAccessMode::Read*/) :
    TBase{pRefCounters},
    m_FileWrpr{Path, Access}
{
}

bool BasicFileStream::Read(void* Data, size_t Size)
{
    return m_FileWrpr->Read(Data, Size);
}

void BasicFileStream::ReadBlob(Diligent::IDataBlob* pData)
{
    m_FileWrpr->Read(pData);
}

bool BasicFileStream::Write(const void* Data, size_t Size)
{
    return m_FileWrpr->Write(Data, Size);
}

bool BasicFileStream::IsValid()
{
    return !!m_FileWrpr;
}

size_t BasicFileStream::GetSize()
{
    return m_FileWrpr->GetSize();
}

size_t BasicFileStream::GetPos()
{
    return m_FileWrpr->GetPos();
}

bool BasicFileStream::SetPos(size_t Offset, int Origin)
{
    return m_FileWrpr->SetPos(Offset, static_cast<FilePosOrigin>(Origin));
}

} // namespace Diligent
