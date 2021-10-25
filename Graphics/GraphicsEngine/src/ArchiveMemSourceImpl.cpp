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

#include "ArchiveMemSourceImpl.hpp"
#include "EngineMemory.h"

namespace Diligent
{

ArchiveMemSourceImpl::ArchiveMemSourceImpl(IReferenceCounters* pRefCounters, IDataBlob* pBlob) :
    TObjectBase{pRefCounters},
    m_pBlob{pBlob},
    m_pData{pBlob ? static_cast<const Uint8*>(pBlob->GetDataPtr()) : nullptr},
    m_Size{pBlob ? pBlob->GetSize() : 0}
{
    if (m_pBlob == nullptr || m_pData == nullptr || m_Size == 0)
        LOG_ERROR_AND_THROW("pBlob must not be null and Size must not be zero");
}

ArchiveMemSourceImpl::~ArchiveMemSourceImpl()
{
}

Bool ArchiveMemSourceImpl::Read(Uint64 Pos, void* pData, const Uint64 RequiredSize)
{
    DEV_CHECK_ERR(pData != nullptr && RequiredSize != 0, "pData must not be null");

    m_Pos = static_cast<size_t>(std::min(Pos, Uint64{m_Size}));
    if (m_Pos != Pos)
        return false;

    Uint64 Size = std::min(m_Pos + RequiredSize, Uint64{m_Size}) - m_Pos;
    std::memcpy(pData, m_pData + m_Pos, static_cast<size_t>(Size));
    m_Pos += static_cast<size_t>(Size);
    return Size == RequiredSize;
}

} // namespace Diligent
