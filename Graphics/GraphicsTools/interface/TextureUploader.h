/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

#include "../../GraphicsEngine/interface/RenderDevice.h"
#include "../../GraphicsEngine/interface/DeviceContext.h"

namespace Diligent
{

// clang-format off
struct UploadBufferDesc
{
    Uint32         Width       = 0;
    Uint32         Height      = 0;
    Uint32         Depth       = 1;
    Uint32         MipLevels   = 1;
    Uint32         ArraySize   = 1;
    TEXTURE_FORMAT Format      = TEX_FORMAT_UNKNOWN;

    bool operator == (const UploadBufferDesc &rhs) const
    {
        return Width  == rhs.Width  && 
                Height == rhs.Height &&
                Depth  == rhs.Depth  &&
                Format == rhs.Format;
    }
};
// clang-format on

class IUploadBuffer : public IObject
{
public:
    virtual void                     WaitForCopyScheduled()                  = 0;
    virtual MappedTextureSubresource GetMappedData(Uint32 Mip, Uint32 Slice) = 0;
    virtual const UploadBufferDesc&  GetDesc() const                         = 0;
};

struct TextureUploaderDesc
{
};

struct TextureUploaderStats
{
    Uint32 NumPendingOperations = 0;
};

class ITextureUploader : public IObject
{
public:
    virtual void RenderThreadUpdate(IDeviceContext* pContext) = 0;

    virtual void AllocateUploadBuffer(const UploadBufferDesc& Desc,
                                      bool                    IsRenderThread,
                                      IUploadBuffer**         ppBuffer) = 0;
    virtual void ScheduleGPUCopy(ITexture*      pDstTexture,
                                 Uint32         ArraySlice,
                                 Uint32         MipLevel,
                                 IUploadBuffer* pUploadBuffer)  = 0;
    virtual void RecycleBuffer(IUploadBuffer* pUploadBuffer)    = 0;

    virtual TextureUploaderStats GetStats() = 0;
};

void CreateTextureUploader(IRenderDevice* pDevice, const TextureUploaderDesc& Desc, ITextureUploader** ppUploader);

} // namespace Diligent
