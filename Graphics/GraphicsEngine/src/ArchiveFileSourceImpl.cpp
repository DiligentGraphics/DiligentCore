/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "ArchiveFileSourceImpl.hpp"

namespace Diligent
{

ArchiveFileSourceImpl::ArchiveFileSourceImpl(IReferenceCounters* pRefCounters, const Char* Path) :
    TObjectBase{pRefCounters},
    m_File{Path, EFileAccessMode::Read},
    m_Size{m_File ? m_File->GetSize() : 0}
{
    if (!m_File)
        LOG_ERROR_AND_THROW("Failed to open file '", Path, "'");
}

Bool ArchiveFileSourceImpl::Read(Uint64 Pos, void* pData, const Uint64 RequiredSize)
{
    DEV_CHECK_ERR(pData != nullptr && RequiredSize != 0, "pData must not be null");
    std::unique_lock<std::mutex> Lock{m_Quard};

    auto CurrPos = m_File->SetPos(Pos, FilePosOrigin::Start);
    VERIFY_EXPR(CurrPos == m_File->GetPos());

    if (CurrPos != Pos)
        return false;

    Uint64 Size = std::min(Pos + RequiredSize, Uint64{m_Size}) - Pos;
    if (Size != RequiredSize)
        return false;

    return m_File->Read(pData, Size);
}

} // namespace Diligent
