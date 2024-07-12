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
 *  In no event and under no legal theory, whether in tort (including neWebGPUigence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly neWebGPUigent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include <webgpu/webgpu.h>

#include "ArchiverImpl.hpp"
#include "Archiver_Inc.hpp"

#include "RenderDeviceWebGPUImpl.hpp"
#include "PipelineResourceSignatureWebGPUImpl.hpp"
#include "PipelineStateWebGPUImpl.hpp"
#include "ShaderWebGPUImpl.hpp"
#include "DeviceObjectArchiveWebGPU.hpp"
#include "SerializedPipelineStateImpl.hpp"

namespace Diligent
{

template <>
struct SerializedResourceSignatureImpl::SignatureTraits<PipelineResourceSignatureWebGPUImpl>
{
    static constexpr DeviceType Type = DeviceType::WebGPU;

    template <SerializerMode Mode>
    using PRSSerializerType = PRSSerializerWebGPU<Mode>;
};

template <typename CreateInfoType>
void SerializedPipelineStateImpl::PatchShadersWebGPU(const CreateInfoType& CreateInfo) noexcept(false)
{
    UNSUPPORTED("Not yet implemented");
}

INSTANTIATE_PATCH_SHADER_METHODS(PatchShadersWebGPU)
INSTANTIATE_DEVICE_SIGNATURE_METHODS(PipelineResourceSignatureWebGPUImpl)

void SerializationDeviceImpl::GetPipelineResourceBindingsWebGPU(const PipelineResourceBindingAttribs& Info,
                                                                std::vector<PipelineResourceBinding>& ResourceBindings)
{
    UNSUPPORTED("Not yet implemented");
}

void SerializedShaderImpl::CreateShaderWebGPU(IReferenceCounters*     pRefCounters,
                                              const ShaderCreateInfo& ShaderCI,
                                              IDataBlob**             ppCompilerOutput) noexcept(false)
{
    UNSUPPORTED("Not yet implemented");
}

} // namespace Diligent
