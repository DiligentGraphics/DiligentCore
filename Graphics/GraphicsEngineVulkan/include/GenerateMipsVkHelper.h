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

 /// \file
/// Implementation of mipmap generation routines

#include <array>
#include <unordered_map>
#include "VulkanUtilities/VulkanLogicalDevice.h"
#include "VulkanUtilities/VulkanCommandBuffer.h"

namespace Diligent
{
    class RenderDeviceVkImpl;
    class TextureViewVkImpl;
    class DeviceContextVkImpl;

    class GenerateMipsVkHelper
    {
    public:
        GenerateMipsVkHelper(RenderDeviceVkImpl& DeviceVkImpl);
        
        GenerateMipsVkHelper             (const GenerateMipsVkHelper&)  = delete;
        GenerateMipsVkHelper             (      GenerateMipsVkHelper&&) = delete;
        GenerateMipsVkHelper& operator = (const GenerateMipsVkHelper&)  = delete;
        GenerateMipsVkHelper& operator = (      GenerateMipsVkHelper&&) = delete;

        void GenerateMips(TextureViewVkImpl& TexView, DeviceContextVkImpl& Ctx, IShaderResourceBinding& SRB);
        void CreateSRB(IShaderResourceBinding** ppSRB);

    private:
        std::array<RefCntAutoPtr<IPipelineState>, 4>  CreatePSOs(TEXTURE_FORMAT Fmt);
        std::array<RefCntAutoPtr<IPipelineState>, 4>& FindPSOs  (TEXTURE_FORMAT Fmt);

        RenderDeviceVkImpl& m_DeviceVkImpl;

        std::mutex m_PSOMutex;
	    std::unordered_map< TEXTURE_FORMAT, std::array<RefCntAutoPtr<IPipelineState>, 4> > m_PSOHash;
        static void GetGlImageFormat(const TextureFormatAttribs& FmtAttribs, std::array<char, 16>& GlFmt);
        RefCntAutoPtr<IBuffer> m_ConstantsCB;
    };
}
