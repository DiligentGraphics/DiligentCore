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

#include "pch.h"

#include "RenderDeviceGLImpl.h"
#include "DeviceContextGLImpl.h"
#include "BufferViewGLImpl.h"
#include "BufferGLImpl.h"
#include "GLTypeConversions.h"

namespace Diligent
{
    BufferViewGLImpl::BufferViewGLImpl( IReferenceCounters *pRefCounters,
                                        RenderDeviceGLImpl *pDevice, 
                                        IDeviceContext *pContext,
                                        const BufferViewDesc& ViewDesc, 
                                        BufferGLImpl* pBuffer,
                                        bool bIsDefaultView) :
        TBuffViewBase(pRefCounters, pDevice, ViewDesc, pBuffer, bIsDefaultView ),
        m_GLTexBuffer(false)
    {
        if( (ViewDesc.ViewType == BUFFER_VIEW_SHADER_RESOURCE || ViewDesc.ViewType == BUFFER_VIEW_UNORDERED_ACCESS) && 
            (pBuffer->GetDesc().Mode == BUFFER_MODE_FORMATTED || pBuffer->GetDesc().Mode == BUFFER_MODE_RAW) )
        {
#ifdef _MSC_VER
#   pragma warning(push)
#   pragma warning(disable : 4127) // conditional expression is constant
#endif
            VERIFY( GL_TEXTURE_BUFFER != 0, "GL texture buffers are not supported");
#ifdef _MSC_VER
#   pragma warning(pop)
#endif

            auto *pContextGL = ValidatedCast<DeviceContextGLImpl>(pContext);
            auto &ContextState = pContextGL->GetContextState();

            m_GLTexBuffer.Create();
            ContextState.BindTexture(-1, GL_TEXTURE_BUFFER, m_GLTexBuffer );

            const auto &BuffFmt = ViewDesc.Format;
            GLenum GLFormat = 0;
            if(pBuffer->GetDesc().Mode == BUFFER_MODE_FORMATTED || BuffFmt.ValueType != VT_UNDEFINED)
                GLFormat = TypeToGLTexFormat( BuffFmt.ValueType, BuffFmt.NumComponents, BuffFmt.IsNormalized );
            else
            {
                GLFormat = GL_R32UI;
            }
            glTexBuffer( GL_TEXTURE_BUFFER, GLFormat, pBuffer->GetGLHandle() );
            CHECK_GL_ERROR_AND_THROW( "Failed to create texture buffer" );

            ContextState.BindTexture(-1, GL_TEXTURE_BUFFER, GLObjectWrappers::GLTextureObj(false) );
        }
    }

    IMPLEMENT_QUERY_INTERFACE( BufferViewGLImpl, IID_BufferViewGL, TBuffViewBase )
}
