/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include <map>
#include "ShaderToolsCommon.hpp"

namespace Diligent
{

// Source-level information that cannot be recovered reliably from Tint-generated WGSL.
// Binding points identify resources independently of generated variable and type names.
struct WGSLShaderReflection
{
    struct BindingPoint
    {
        Uint32 Group   = 0;
        Uint32 Binding = 0;

        bool operator<(const BindingPoint& Rhs) const
        {
            return Group < Rhs.Group || (Group == Rhs.Group && Binding < Rhs.Binding);
        }
    };
    struct Resource
    {
        std::string                            Name;
        std::unique_ptr<ShaderCodeBufferDescX> UniformBuffer;
    };
    std::map<BindingPoint, Resource> Resources;

    // Tint preserves override IDs through the IR rewrite, but may rename their
    // identifiers. Keep the application-facing names independently of those names.
    std::map<Uint32, std::string> SpecializationConstants;

    Resource* Find(Uint32 Group, Uint32 Binding)
    {
        auto It = Resources.find({Group, Binding});
        return It != Resources.end() ? &It->second : nullptr;
    }

    const Resource* Find(Uint32 Group, Uint32 Binding) const
    {
        auto It = Resources.find({Group, Binding});
        return It != Resources.end() ? &It->second : nullptr;
    }
};

// Keep metadata with saved WGSL so GetBytecode(), archives, and compilation-free
// clients retain the original reflection. The payload contains no SPIR-V bytecode.
void AppendWGSLShaderReflection(std::string& WGSL, const WGSLShaderReflection& Reflection);

// Missing metadata is valid for native WGSL and shaders produced by older versions.
// Returns false for an unsupported version or malformed metadata.
bool ParseWGSLShaderReflection(const std::string& WGSL, WGSLShaderReflection& Reflection);

} // namespace Diligent
