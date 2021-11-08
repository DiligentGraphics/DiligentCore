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

#include "DearchiverBase.hpp"
#include "ArchiveFileImpl.hpp"
#include "ArchiveMemoryImpl.hpp"

namespace Diligent
{

bool DearchiverBase::VerifyUnpackPipelineState(const PipelineStateUnpackInfo& DeArchiveInfo, IPipelineState** ppPSO)
{
    DEV_CHECK_ERR(ppPSO != nullptr, "ppPSO must not be null");
    DEV_CHECK_ERR(DeArchiveInfo.pArchive != nullptr, "pArchive must not be null");
    DEV_CHECK_ERR(DeArchiveInfo.Name != nullptr, "Name must not be null");
    DEV_CHECK_ERR(DeArchiveInfo.pDevice != nullptr, "pDevice must not be null");
    DEV_CHECK_ERR(DeArchiveInfo.PipelineType <= PIPELINE_TYPE_LAST, "PipelineType must be valid");

    if (!ppPSO ||
        !DeArchiveInfo.pArchive ||
        !DeArchiveInfo.Name ||
        !DeArchiveInfo.pDevice ||
        DeArchiveInfo.PipelineType > PIPELINE_TYPE_LAST)
        return false;

    return true;
}

bool DearchiverBase::VerifyUnpackResourceSignature(const ResourceSignatureUnpackInfo& DeArchiveInfo, IPipelineResourceSignature** ppSignature)
{
    DEV_CHECK_ERR(ppSignature != nullptr, "ppSignature must not be null");
    DEV_CHECK_ERR(DeArchiveInfo.pArchive != nullptr, "pArchive must not be null");
    DEV_CHECK_ERR(DeArchiveInfo.Name != nullptr, "Name must not be null");
    DEV_CHECK_ERR(DeArchiveInfo.pDevice != nullptr, "pDevice must not be null");

    if (!ppSignature ||
        !DeArchiveInfo.pArchive ||
        !DeArchiveInfo.Name ||
        !DeArchiveInfo.pDevice)
        return false;

    return true;
}

bool DearchiverBase::VerifyUnpackRenderPass(const RenderPassUnpackInfo& DeArchiveInfo, IRenderPass** ppRP)
{
    DEV_CHECK_ERR(ppRP != nullptr, "ppRP must not be null");
    DEV_CHECK_ERR(DeArchiveInfo.pArchive != nullptr, "pArchive must not be null");
    DEV_CHECK_ERR(DeArchiveInfo.Name != nullptr, "Name must not be null");
    DEV_CHECK_ERR(DeArchiveInfo.pDevice != nullptr, "pDevice must not be null");

    if (!ppRP ||
        !DeArchiveInfo.pArchive ||
        !DeArchiveInfo.Name ||
        !DeArchiveInfo.pDevice)
        return false;

    return true;
}

} // namespace Diligent
