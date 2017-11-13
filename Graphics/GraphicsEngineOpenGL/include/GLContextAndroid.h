/*     Copyright 2015-2017 Egor Yusov
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

#include <EGL/egl.h>
#include <android\native_window.h>

namespace Diligent
{
    struct ContextInitInfo
    {
        SwapChainDesc SwapChainAttribs;
        void *pNativeWndHandle = nullptr;
    };

    class GLContext
    {
    public:
        typedef EGLContext NativeGLContextType;

        GLContext( const ContextInitInfo &Info, DeviceCaps &DeviceCaps );
        ~GLContext();

        bool Init( ANativeWindow* window );

        void SwapBuffers();

        bool Invalidate();

        void Suspend();
        EGLint Resume( ANativeWindow* window );

        const SwapChainDesc& GetSwapChainDesc(){ return SwapChainAttribs_; }
        
        NativeGLContextType GetCurrentNativeGLContext();

    private:
        //EGL configurations
        ANativeWindow* window_ = nullptr;
        EGLDisplay display_ = EGL_NO_DISPLAY;
        EGLSurface surface_ = EGL_NO_SURFACE;
        EGLContext context_ = EGL_NO_CONTEXT;
        EGLConfig config_;

        //Screen parameters
        int32_t color_size_ = 0;
        int32_t depth_size_ = 0;
        int32_t major_version_ = 0;
        int32_t minor_version_ = 0;

        //Flags
        bool gles_initialized_ = false;
        bool egl_context_initialized_ = false;
        bool context_valid_ = false;

        SwapChainDesc SwapChainAttribs_;

        void InitGLES();
        void Terminate();
        bool InitEGLSurface();
        bool InitEGLContext();
        void AttachToCurrentEGLContext();
        void FillDeviceCaps( DeviceCaps &DeviceCaps );
    };
}
