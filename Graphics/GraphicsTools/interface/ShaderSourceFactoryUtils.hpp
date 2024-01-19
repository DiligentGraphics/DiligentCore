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

#pragma once

/// \file
/// Defines c++ graphics engine utilities

#include "ShaderSourceFactoryUtils.h"

#include <vector>
#include <unordered_set>
#include <string>

#include "../../../Common/interface/RefCntAutoPtr.hpp"

namespace Diligent
{

/// C++ wrapper over MemoryShaderSourceFactoryCreateInfo.
struct MemoryShaderSourceFactoryCreateInfoX
{
    MemoryShaderSourceFactoryCreateInfoX() noexcept
    {}

    MemoryShaderSourceFactoryCreateInfoX(const MemoryShaderSourceFactoryCreateInfo& _Desc) :
        Desc{_Desc}
    {
        if (Desc.NumSources != 0)
            Sources.assign(Desc.pSources, Desc.pSources + Desc.NumSources);

        SyncDesc(true);
    }

    MemoryShaderSourceFactoryCreateInfoX(const std::initializer_list<MemoryShaderSourceFileInfo>& _Sources) :
        Sources{_Sources}
    {
        SyncDesc(true);
    }

    MemoryShaderSourceFactoryCreateInfoX(const MemoryShaderSourceFactoryCreateInfoX& _DescX) :
        MemoryShaderSourceFactoryCreateInfoX{static_cast<const MemoryShaderSourceFactoryCreateInfo&>(_DescX)}
    {}

    MemoryShaderSourceFactoryCreateInfoX& operator=(const MemoryShaderSourceFactoryCreateInfoX& _DescX)
    {
        MemoryShaderSourceFactoryCreateInfoX Copy{_DescX};
        std::swap(*this, Copy);
        return *this;
    }

    MemoryShaderSourceFactoryCreateInfoX(MemoryShaderSourceFactoryCreateInfoX&&) noexcept = default;
    MemoryShaderSourceFactoryCreateInfoX& operator=(MemoryShaderSourceFactoryCreateInfoX&&) noexcept = default;

    MemoryShaderSourceFactoryCreateInfoX& Add(const MemoryShaderSourceFileInfo& Elem)
    {
        Sources.push_back(Elem);
        auto& Name = Sources.back().Name;
        Name       = StringPool.emplace(Name).first->c_str();
        SyncDesc();
        return *this;
    }

    template <typename... ArgsType>
    MemoryShaderSourceFactoryCreateInfoX& Add(ArgsType&&... args)
    {
        const MemoryShaderSourceFileInfo Elem{std::forward<ArgsType>(args)...};
        return Add(Elem);
    }

    void Clear()
    {
        MemoryShaderSourceFactoryCreateInfoX EmptyDesc;
        std::swap(*this, EmptyDesc);
    }

    const MemoryShaderSourceFactoryCreateInfo& Get() const noexcept
    {
        return Desc;
    }

    Uint32 GetNumSources() const noexcept
    {
        return Desc.NumSources;
    }

    operator const MemoryShaderSourceFactoryCreateInfo&() const noexcept
    {
        return Desc;
    }

    const MemoryShaderSourceFileInfo& operator[](size_t Index) const noexcept
    {
        return Sources[Index];
    }

    MemoryShaderSourceFileInfo& operator[](size_t Index) noexcept
    {
        return Sources[Index];
    }

private:
    void SyncDesc(bool CopyStrings = false)
    {
        Desc.NumSources = static_cast<Uint32>(Sources.size());
        Desc.pSources   = Desc.NumSources > 0 ? Sources.data() : nullptr;

        if (CopyStrings)
        {
            for (auto& Source : Sources)
                Source.Name = StringPool.emplace(Source.Name).first->c_str();
        }
    }
    MemoryShaderSourceFactoryCreateInfo     Desc;
    std::vector<MemoryShaderSourceFileInfo> Sources;
    std::unordered_set<std::string>         StringPool;
};

inline RefCntAutoPtr<IShaderSourceInputStreamFactory> CreateMemoryShaderSourceFactory(const MemoryShaderSourceFactoryCreateInfo& CI)
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pFactory;
    CreateMemoryShaderSourceFactory(CI, &pFactory);
    return pFactory;
}

RefCntAutoPtr<IShaderSourceInputStreamFactory> CreateMemoryShaderSourceFactory(const std::initializer_list<MemoryShaderSourceFileInfo>& Sources)
{
    MemoryShaderSourceFactoryCreateInfoX CI{Sources};
    return CreateMemoryShaderSourceFactory(CI);
}

} // namespace Diligent
