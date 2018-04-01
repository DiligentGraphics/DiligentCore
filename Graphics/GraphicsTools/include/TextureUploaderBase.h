/*     Copyright 2015-2018 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "TextureUploader.h"
#include "../../../Common/interface/ObjectBase.h"
#include "../../../Common/interface/HashUtils.h"
#include "../../../Common/interface/RefCntAutoPtr.h"

namespace std
{
    template<>
    struct hash<Diligent::UploadBufferDesc>
    {
        size_t operator()(const Diligent::UploadBufferDesc &Desc) const
        {
            return Diligent::ComputeHash(Desc.Width, Desc.Height, Desc.Depth, static_cast<Diligent::Int32>(Desc.Format));
        }
    };
}

namespace Diligent
{
    class UploadBufferBase : public ObjectBase<IUploadBuffer>
    {
    public:
        UploadBufferBase(IReferenceCounters *pRefCounters, const UploadBufferDesc &Desc) : 
            ObjectBase<IUploadBuffer>(pRefCounters),
            m_Desc(Desc)
        {
        }

        virtual void* GetDataPtr()   override final { return m_pData; }
        virtual size_t GetRowStride()  const override final{ return m_RowStride;  }
        virtual size_t GetDepthStride()const override final{ return m_DepthStride;  }
        virtual const UploadBufferDesc& GetDesc()const override final{ return m_Desc; }

    protected:
        const UploadBufferDesc m_Desc;
        void* m_pData = 0;
        size_t m_RowStride = 0;
        size_t m_DepthStride = 0;
    };

    class TextureUploaderBase : public ObjectBase<ITextureUploader>
    {
    public:
        TextureUploaderBase(IReferenceCounters *pRefCounters, IRenderDevice *pDevice, const TextureUploaderDesc Desc) :
            ObjectBase<ITextureUploader>(pRefCounters),
            m_pDevice(pDevice)
        {}

    protected:
        RefCntAutoPtr<IRenderDevice> m_pDevice;
    };

}
