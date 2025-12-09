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

#pragma once

/// \file
/// Declaration of Diligent::PipelineLayoutVk class

#include <array>

#include "VulkanUtilities/ObjectWrappers.hpp"

namespace Diligent
{

class RenderDeviceVkImpl;
class PipelineResourceSignatureVkImpl;

/// Push constant information extracted from shaders or selected from inline constants
struct PushConstantInfoVk
{
    VkShaderStageFlags StageFlags     = 0;
    Uint32             Size           = 0;
    Uint32             SignatureIndex = ~0u; // Index of the signature containing the push constant (~0u if none)
    Uint32             ResourceIndex  = ~0u; // Resource index within the signature (~0u if none)
};

static constexpr Uint32 INVALID_PUSH_CONSTANT_INDEX = ~0u;

/// Implementation of the Diligent::PipelineLayoutVk class
class PipelineLayoutVk
{
public:
    PipelineLayoutVk();
    ~PipelineLayoutVk();

    void Create(RenderDeviceVkImpl*                            pDeviceVk,
                RefCntAutoPtr<PipelineResourceSignatureVkImpl> ppSignatures[],
                Uint32                                         SignatureCount,
                const PushConstantInfoVk&                      PushConstant = {}) noexcept(false);
    void Release(RenderDeviceVkImpl* pDeviceVkImpl, Uint64 CommandQueueMask);

    VkPipelineLayout GetVkPipelineLayout() const { return m_VkPipelineLayout; }

    // Returns the index of the first descriptor set used by the resource signature at the given bind index
    Uint32 GetFirstDescrSetIndex(Uint32 Index) const
    {
        VERIFY_EXPR(Index <= m_DbgMaxBindIndex);
        return m_FirstDescrSetIndex[Index];
    }

    // Returns true if this pipeline layout has push constants
    bool HasPushConstants() const { return m_PushConstantSize > 0; }

    // Returns the size of push constants in bytes
    Uint32 GetPushConstantSize() const { return m_PushConstantSize; }

    // Returns the shader stage flags for push constants
    VkShaderStageFlags GetPushConstantStageFlags() const { return m_PushConstantStageFlags; }

    // Returns the signature index containing the push constant resource
    // Returns INVALID_PUSH_CONSTANT_INDEX if no push constant is selected
    Uint32 GetPushConstantSignatureIndex() const { return m_PushConstantSignatureIndex; }

    // Returns the resource index within the signature for push constant
    // Returns INVALID_PUSH_CONSTANT_INDEX if no push constant is selected
    Uint32 GetPushConstantResourceIndex() const { return m_PushConstantResourceIndex; }

private:
    VulkanUtilities::PipelineLayoutWrapper m_VkPipelineLayout;

    using FirstDescrSetIndexArrayType = std::array<Uint8, MAX_RESOURCE_SIGNATURES>;
    // Index of the first descriptor set, for every resource signature.
    FirstDescrSetIndexArrayType m_FirstDescrSetIndex = {};

    // The total number of descriptor sets used by this pipeline layout
    // (Maximum is MAX_RESOURCE_SIGNATURES * 2)
    Uint8 m_DescrSetCount = 0;

    // Push constant size in bytes
    Uint32 m_PushConstantSize = 0;

    // Shader stages that use push constants
    VkShaderStageFlags m_PushConstantStageFlags = 0;

    // Index of the signature containing the push constant resource
    Uint32 m_PushConstantSignatureIndex = INVALID_PUSH_CONSTANT_INDEX;

    // Resource index within the signature for push constant
    Uint32 m_PushConstantResourceIndex = INVALID_PUSH_CONSTANT_INDEX;

#ifdef DILIGENT_DEBUG
    Uint32 m_DbgMaxBindIndex = 0;
#endif
};

} // namespace Diligent
