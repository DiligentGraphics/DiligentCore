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

#include <vector>

#include "../../../Common/interface/RefCntAutoPtr.hpp"
#include "../../GraphicsEngine/interface/Buffer.h"
#include "../../GraphicsEngine/interface/Texture.h"
#include "GraphicsUtilities.h"

namespace Diligent
{

/// Helper class that facilitates resource management.
class ResourceRegistry
{
public:
    using ResourceIdType = Uint32;

    ResourceRegistry()  = default;
    ~ResourceRegistry() = default;

    explicit ResourceRegistry(size_t ResourceCount) :
        m_Resources(ResourceCount)
    {}

    void SetSize(size_t ResourceCount)
    {
        m_Resources.resize(ResourceCount);
    }

    void Insert(ResourceIdType Id, IDeviceObject* pObject)
    {
        DEV_CHECK_ERR(Id < m_Resources.size(), "Resource index is out of range");
        m_Resources[Id] = pObject;
    }

    bool IsInitialized(ResourceIdType Id) const
    {
        DEV_CHECK_ERR(Id < m_Resources.size(), "Resource index is out of range");
        return m_Resources[Id] != nullptr;
    }

    ITexture* GetTexture(ResourceIdType Id) const
    {
        DEV_CHECK_ERR(Id < m_Resources.size(), "Resource index is out of range");
        DEV_CHECK_ERR(RefCntAutoPtr<ITexture>(m_Resources[Id], IID_Texture), "Resource is not a texture");
        return StaticCast<ITexture*>(m_Resources[Id]);
    }

    IBuffer* GetBuffer(ResourceIdType Id) const
    {
        DEV_CHECK_ERR(Id < m_Resources.size(), "Resource index is out of range");
        DEV_CHECK_ERR(RefCntAutoPtr<IBuffer>(m_Resources[Id], IID_Buffer), "Resource is not a buffer");
        return StaticCast<IBuffer*>(m_Resources[Id]);
    }

    ITextureView* GetTextureSRV(ResourceIdType Id) const
    {
        DEV_CHECK_ERR(Id < m_Resources.size(), "Resource index is out of range");
        return GetTextureDefaultSRV(m_Resources[Id]);
    }

    ITextureView* GetTextureRTV(ResourceIdType Id) const
    {
        DEV_CHECK_ERR(Id < m_Resources.size(), "Resource index is out of range");
        return GetTextureDefaultRTV(m_Resources[Id]);
    }

    ITextureView* GetTextureDSV(ResourceIdType Id) const
    {
        DEV_CHECK_ERR(Id < m_Resources.size(), "Resource index is out of range");
        return GetTextureDefaultDSV(m_Resources[Id]);
    }

    ITextureView* GetTextureUAV(ResourceIdType Id) const
    {
        DEV_CHECK_ERR(Id < m_Resources.size(), "Resource index is out of range");
        return GetTextureDefaultUAV(m_Resources[Id]);
    }

    IBufferView* GetBufferSRV(ResourceIdType Id) const
    {
        DEV_CHECK_ERR(Id < m_Resources.size(), "Resource index is out of range");
        return GetBufferDefaultSRV(m_Resources[Id]);
    }

    IBufferView* GetBufferUAV(ResourceIdType Id) const
    {
        DEV_CHECK_ERR(Id < m_Resources.size(), "Resource index is out of range");
        return GetBufferDefaultUAV(m_Resources[Id]);
    }

private:
    std::vector<RefCntAutoPtr<IDeviceObject>> m_Resources;
};

} // namespace Diligent
