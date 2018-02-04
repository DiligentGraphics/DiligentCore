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

#include "pch.h"

#include "GLContextAndroid.h"
#include "EngineGLAttribs.h"

#ifndef EGL_CONTEXT_MINOR_VERSION_KHR
#define EGL_CONTEXT_MINOR_VERSION_KHR 0x30FB
#endif

#ifndef EGL_CONTEXT_MAJOR_VERSION_KHR
#define EGL_CONTEXT_MAJOR_VERSION_KHR EGL_CONTEXT_CLIENT_VERSION
#endif

namespace Diligent
{
    bool GLContext::InitEGLSurface()
    {
        display_ = eglGetDisplay( EGL_DEFAULT_DISPLAY );
        if( display_ == EGL_NO_DISPLAY )
        {
            LOG_ERROR_AND_THROW( "No EGL display found" );
        }

        auto success = eglInitialize( display_, 0, 0 );
        if( !success )
        {
            LOG_ERROR_AND_THROW( "Failed to initialise EGL" );
        }

        /*
        * Here specify the attributes of the desired configuration.
        * Below, we select an EGLConfig with at least 8 bits per color
        * component compatible with on-screen windows
        */
        color_size_ = 8;
        depth_size_ = 24;
        const EGLint attribs[] = 
        { 
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, //Request opengl ES2.0
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT, 
            //EGL_COLORSPACE, EGL_COLORSPACE_sRGB, // does not work
            EGL_BLUE_SIZE,  color_size_, 
            EGL_GREEN_SIZE, color_size_,
            EGL_RED_SIZE,   color_size_, 
            EGL_ALPHA_SIZE, color_size_,
            EGL_DEPTH_SIZE, depth_size_,
            //EGL_SAMPLE_BUFFERS  , 1,
            //EGL_SAMPLES         , 4,
            EGL_NONE 
        };
                
        // Get a list of EGL frame buffer configurations that match specified attributes
        EGLint num_configs;
        success = eglChooseConfig( display_, attribs, &config_, 1, &num_configs );
        if( !success )
        {
            LOG_ERROR_AND_THROW( "Failed to choose config" );
        }

        if( !num_configs )
        {
            //Fall back to 16bit depth buffer
            depth_size_ = 16;
            const EGLint attribs[] = 
            { 
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, //Request opengl ES2.0
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT, 
                EGL_BLUE_SIZE,  color_size_, 
                EGL_GREEN_SIZE, color_size_,
                EGL_RED_SIZE,   color_size_,
                EGL_ALPHA_SIZE, color_size_,
                EGL_DEPTH_SIZE, depth_size_, 
                EGL_NONE 
            };
            success = eglChooseConfig( display_, attribs, &config_, 1, &num_configs );
            if( !success )
            {
                LOG_ERROR_AND_THROW( "Failed to choose 16-bit depth config" );
            }
        }

        if( !num_configs )
        {
            LOG_ERROR_AND_THROW( "Unable to retrieve EGL config" );
        }

        LOG_INFO_MESSAGE("Chosen EGL config: ", color_size_, " bit color, ", depth_size_, " bit depth");

        surface_ = eglCreateWindowSurface( display_, config_, window_, NULL );
        if( surface_ == EGL_NO_SURFACE )
        {
            LOG_ERROR_AND_THROW( "Failed to create EGLSurface" );
        }

        eglQuerySurface( display_, surface_, EGL_WIDTH, &screen_width_ );
        eglQuerySurface( display_, surface_, EGL_HEIGHT, &screen_height_ );

        /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
        * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
        * As soon as we picked a EGLConfig, we can safely reconfigure the
        * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
        EGLint format;
        eglGetConfigAttrib( display_, config_, EGL_NATIVE_VISUAL_ID, &format );
        ANativeWindow_setBuffersGeometry( window_, 0, 0, format );

        return true;
    }

    bool GLContext::InitEGLContext()
    {
        major_version_ = 3;
        minor_version_ = 1;

        const EGLint context_attribs[] = 
        { 
            EGL_CONTEXT_CLIENT_VERSION, major_version_,
            EGL_CONTEXT_MINOR_VERSION_KHR, minor_version_,
            EGL_NONE 
        };

        LOG_INFO_MESSAGE( "contextAttribs: ", context_attribs[0], ' ', context_attribs[1], '\n' );
        LOG_INFO_MESSAGE( "contextAttribs: ", context_attribs[2], ' ', context_attribs[3], '\n' );

        context_ = eglCreateContext( display_, config_, NULL, context_attribs );
        if( context_ == EGL_NO_CONTEXT )
        {
            LOG_ERROR_AND_THROW( "Failed to create EGLContext" );
        }

        if( eglMakeCurrent( display_, surface_, surface_, context_ ) == EGL_FALSE )
        {
            LOG_ERROR_AND_THROW( "Unable to eglMakeCurrent" );
        }

        context_valid_ = true;
        return true;
    }

    void GLContext::AttachToCurrentEGLContext()
    {
        if( eglGetCurrentContext() == EGL_NO_CONTEXT )
        {
            LOG_ERROR_AND_THROW( "Failed to attach to EGLContext: no active context" );
        }
        context_valid_ = true;
        glGetIntegerv( GL_MAJOR_VERSION, &major_version_ );
        glGetIntegerv( GL_MINOR_VERSION, &minor_version_ );
    }

    GLContext::NativeGLContextType GLContext::GetCurrentNativeGLContext()
    {
        return eglGetCurrentContext();
    }

    void GLContext::InitGLES()
    {
        if( gles_initialized_ )
            return;
        //
        //Initialize OpenGL ES 3 if available
        //
        const char* versionStr = (const char*)glGetString( GL_VERSION );
        LOG_INFO_MESSAGE( "GL Version: ", versionStr, '\n' );
        
        LoadGLFunctions();

        // When GL_FRAMEBUFFER_SRGB is enabled, and if the destination image is in the sRGB colorspace
        // then OpenGL will assume the shader's output is in the linear RGB colorspace. It will therefore 
        // convert the output from linear RGB to sRGB.
        // Any writes to images that are not in the sRGB format should not be affected.
        // Thus this setting should be just set once and left that way
        glEnable(GL_FRAMEBUFFER_SRGB);
        if( glGetError() != GL_NO_ERROR )
            LOG_ERROR_MESSAGE("Failed to enable SRGB framebuffers");

        gles_initialized_ = true;
    }

    bool GLContext::Init( ANativeWindow* window )
    {
        if( egl_context_initialized_ )
            return true;

        //
        //Initialize EGL
        //
        window_ = window;
        if (window != nullptr)
        {
            InitEGLSurface();
            InitEGLContext();
        }
        else
        {
            AttachToCurrentEGLContext();
        }
        InitGLES();

        egl_context_initialized_ = true;

        return true;
    }

    GLContext::GLContext( const EngineGLAttribs &InitAttribs, DeviceCaps &DeviceCaps ) :
        display_( EGL_NO_DISPLAY ),
        surface_( EGL_NO_SURFACE ),
        context_( EGL_NO_CONTEXT ),
        egl_context_initialized_( false ),
        gles_initialized_( false ),
        major_version_(0),
        minor_version_(0)
    {
        auto *NativeWindow = reinterpret_cast<ANativeWindow*>(InitAttribs.pNativeWndHandle);
        Init( NativeWindow );

        FillDeviceCaps(DeviceCaps);
#if 0
        // Creates table of supported extensions strings
        extensions.clear();
        string tmp;
        sint32 begin, end;
        tmp = string( (char*)glGetString( GL_EXTENSIONS ) );

        begin = 0;
        end = tmp.find( ' ', 0 );

        DEBUG_PRINT( _L( "Checking Extensions" ) );

        while( end != string::npos )
        {
            DEBUG_PRINT( (_L( "extension %s" )), tmp.substr( begin, end - begin ).c_str() );
            extensions.insert( extensions.end(), tmp.substr( begin, end - begin ) );
            begin = end + 1;
            end = tmp.find( ' ', begin );
        }

        if( supportExtension( "GL_INTEL_tessellation" ) )
        {
            glPatchParameteri = (PFNGLPATCHPARAMETERIPROC)eglGetProcAddress( "glPatchParameteri" );
            DEBUG_PRINT( _L( "%s = %p" ), "glPatchParameteri", (void*)glPatchParameteri );
            glPatchParameterfv = (PFNGLPATCHPARAMETERFVPROC)eglGetProcAddress( "glPatchParameterfv" );
            DEBUG_PRINT( _L( "%s = %p" ), "glPatchParameterfv", (void*)glPatchParameterfv );
        }
        //if(supportExtension("GL_INTEL_compute_shader"))
        {
            glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC)eglGetProcAddress( "glDispatchCompute" );
            DEBUG_PRINT( _L( "%s = %p" ), "glDispatchCompute", (void*)glDispatchCompute );
            glBindImageTexture = (PFNGLBINDIMAGETEXTUREPROC)eglGetProcAddress( "glBindImageTexture" );
            DEBUG_PRINT( _L( "%s = %p" ), "glBindImageTexture", (void*)glBindImageTexture );
        }
#endif
    }

    GLContext::~GLContext()
    {
        Terminate();
    }

    void GLContext::SwapBuffers()
    {
        bool b = eglSwapBuffers( display_, surface_ );
        if( !b )
        {
            EGLint err = eglGetError();
            if( err == EGL_BAD_SURFACE )
            {
                //Recreate surface
                InitEGLSurface();
                //return EGL_SUCCESS; //Still consider glContext is valid
            }
            else if( err == EGL_CONTEXT_LOST || err == EGL_BAD_CONTEXT )
            {
                //Context has been lost!!
                context_valid_ = false;
                Terminate();
                InitEGLContext();
            }
            //return err;
        }
        //return EGL_SUCCESS;
    }

    void GLContext::Terminate()
    {
        if( display_ != EGL_NO_DISPLAY )
        {
            eglMakeCurrent( display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
            if( context_ != EGL_NO_CONTEXT )
            {
                eglDestroyContext( display_, context_ );
            }

            if( surface_ != EGL_NO_SURFACE )
            {
                eglDestroySurface( display_, surface_ );
            }
            eglTerminate( display_ );
        }

        display_ = EGL_NO_DISPLAY;
        context_ = EGL_NO_CONTEXT;
        surface_ = EGL_NO_SURFACE;
        context_valid_ = false;
    }


    EGLint GLContext::Resume( ANativeWindow* window )
    {
        if( egl_context_initialized_ == false )
        {
            Init( window );
            return EGL_SUCCESS;
        }


        //Create surface
        window_ = window;
        surface_ = eglCreateWindowSurface( display_, config_, window_, NULL );
        int32_t new_screen_width  = 0;
        int32_t new_screen_height = 0;
        eglQuerySurface( display_, surface_, EGL_WIDTH, &new_screen_width );
        eglQuerySurface( display_, surface_, EGL_HEIGHT, &new_screen_height );

        if( new_screen_width != screen_width_ || new_screen_height != screen_height_ )
        {
            //Screen resized
            LOG_INFO_MESSAGE( "Screen resized\n" );
        }

        if( eglMakeCurrent( display_, surface_, surface_, context_ ) == EGL_TRUE )
            return EGL_SUCCESS;

        EGLint err = eglGetError();
        LOG_WARNING_MESSAGE( "Unable to eglMakeCurrent ", err, '\n' );

        if( err == EGL_CONTEXT_LOST )
        {
            //Recreate context
            LOG_INFO_MESSAGE( "Re-creating egl context\n" );
            InitEGLContext();
        }
        else
        {
            //Recreate surface
            Terminate();
            InitEGLSurface();
            InitEGLContext();
        }

        return err;

    }

    void GLContext::Suspend()
    {
        if( surface_ != EGL_NO_SURFACE )
        {
            eglDestroySurface( display_, surface_ );
            surface_ = EGL_NO_SURFACE;
        }
    }

    bool GLContext::Invalidate()
    {
        Terminate();

        egl_context_initialized_ = false;
        return true;
    }

    void GLContext::FillDeviceCaps( DeviceCaps &DeviceCaps )
    {
        DeviceCaps.DevType = DeviceType::OpenGLES;
        DeviceCaps.MajorVersion = major_version_;
        DeviceCaps.MinorVersion = minor_version_;
        bool IsGLES31OrAbove = (major_version_ >= 4 || (major_version_ == 3 && minor_version_ >= 1) );
        DeviceCaps.bSeparableProgramSupported      = IsGLES31OrAbove;
        DeviceCaps.bIndirectRenderingSupported     = IsGLES31OrAbove;

        auto &SamCaps = DeviceCaps.SamCaps;
        SamCaps.bBorderSamplingModeSupported   = GL_TEXTURE_BORDER_COLOR       && IsGLES31OrAbove;
        SamCaps.bAnisotropicFilteringSupported = GL_TEXTURE_MAX_ANISOTROPY_EXT && IsGLES31OrAbove;
        SamCaps.bLODBiasSupported              = GL_TEXTURE_LOD_BIAS           && IsGLES31OrAbove;

        auto &TexCaps = DeviceCaps.TexCaps;
        TexCaps.bTexture1DSupported            = False;     // Not supported in GLES 3.1
        TexCaps.bTexture1DArraySupported       = False;     // Not supported in GLES 3.1
        TexCaps.bTexture2DMSSupported          = IsGLES31OrAbove;
        TexCaps.bTexture2DMSArraySupported     = False;     // Not supported in GLES 3.1
        TexCaps.bTextureViewSupported          = False;     // Not supported in GLES 3.1
        TexCaps.bCubemapArraysSupported        = False;     // Not supported in GLES 3.1
        DeviceCaps.bMultithreadedResourceCreationSupported = False;
    }
}
