/*     Copyright 2015-2019 Egor Yusov
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

namespace Diligent
{

// Helper class to facilitate texture copying by rendering
class TexRegionRender
{
public:
    TexRegionRender(class RenderDeviceGLImpl*   pDeviceGL);
    void SetStates(class DeviceContextGLImpl*   pCtxGL);
    void RestoreStates(class DeviceContextGLImpl*   pCtxGL);
    void Render(class DeviceContextGLImpl*  pCtxGL,
                ITextureView*               pSrcSRV,
                RESOURCE_DIMENSION          TexType,
                TEXTURE_FORMAT              TexFormat,
                Int32                       DstToSrcXOffset, 
                Int32                       DstToSrcYOffset,
                Int32                       SrcZ,
                Int32                       SrcMipLevel);

private:
    RefCntAutoPtr<IShader> m_pVertexShader;
    RefCntAutoPtr<IShader> m_pFragmentShaders[RESOURCE_DIM_NUM_DIMENSIONS * 3];
    RefCntAutoPtr<IBuffer> m_pConstantBuffer;
    RefCntAutoPtr<IPipelineState>         m_pPSO[RESOURCE_DIM_NUM_DIMENSIONS * 3];
    RefCntAutoPtr<IShaderResourceBinding> m_pSRB;
    IShaderVariable*                      m_pSrcTexVar = nullptr;

    RefCntAutoPtr<IPipelineState> m_pOrigPSO;
    Uint32                      m_OrigStencilRef              = 0;
    float                       m_OrigBlendFactors[4]         = {};
    Uint32                      m_NumRenderTargets            = 0;
    ITextureView*               m_pOrigRTVs[MaxRenderTargets] = {};
    RefCntAutoPtr<ITextureView> m_pOrigDSV;
    std::vector<Viewport>       m_OrigViewports;
};

}
