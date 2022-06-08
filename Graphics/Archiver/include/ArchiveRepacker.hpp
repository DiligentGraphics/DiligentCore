/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include "DeviceObjectArchive.hpp"
#include "FileStream.h"

namespace Diligent
{

class ArchiveRepacker
{
public:
    using DeviceType = DeviceObjectArchive::DeviceType;

    explicit ArchiveRepacker(IArchive* pArchive);

    void RemoveDeviceData(DeviceType Dev) noexcept(false);
    void AppendDeviceData(const ArchiveRepacker& Src, DeviceType Dev) noexcept(false);
    void Serialize(IFileStream* pStream) noexcept(false);
    bool Validate() const;
    void Print() const;

private:
    using ArchiveHeader          = DeviceObjectArchive::ArchiveHeader;
    using BlockOffsetType        = DeviceObjectArchive::BlockOffsetType;
    using ChunkHeader            = DeviceObjectArchive::ChunkHeader;
    using ChunkType              = DeviceObjectArchive::ChunkType;
    using FileOffsetAndSize      = DeviceObjectArchive::ArchiveRegion;
    using DataHeaderBase         = DeviceObjectArchive::DataHeaderBase;
    using RPDataHeader           = DeviceObjectArchive::RPDataHeader;
    using ShadersDataHeader      = DeviceObjectArchive::ShadersDataHeader;
    using NameToArchiveRegionMap = DeviceObjectArchive::NameToArchiveRegionMap;
    using ArchiveBlock           = DeviceObjectArchive::ArchiveBlock;

    static constexpr auto HeaderMagicNumber = DeviceObjectArchive::HeaderMagicNumber;
    static constexpr auto HeaderVersion     = DeviceObjectArchive::HeaderVersion;
    static constexpr auto InvalidOffset     = DeviceObjectArchive::DataHeaderBase::InvalidOffset;

    std::unique_ptr<DeviceObjectArchive> m_pArchive;
};

} // namespace Diligent
