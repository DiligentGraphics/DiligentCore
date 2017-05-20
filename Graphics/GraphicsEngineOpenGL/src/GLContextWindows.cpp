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

#include "pch.h"

#include "GLContextWindows.h"
#include "DeviceCaps.h"
#include "GLTypeConversions.h"

namespace Diligent
{

    void APIENTRY openglCallbackFunction( GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        void* userParam )
    {
        std::stringstream MessageSS;

        MessageSS << std::endl << "OPENGL DEBUG MESSAGE: " << message << std::endl;
        MessageSS << "Type: ";
        switch( type ) {
        case GL_DEBUG_TYPE_ERROR:
            MessageSS << "ERROR";
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            MessageSS << "DEPRECATED_BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            MessageSS << "UNDEFINED_BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            MessageSS << "PORTABILITY";
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            MessageSS << "PERFORMANCE";
            break;
        case GL_DEBUG_TYPE_OTHER:
            MessageSS << "OTHER";
            break;
        }
        MessageSS << std::endl;

        MessageSS << "Severity: ";
        switch( severity ){
        case GL_DEBUG_SEVERITY_LOW:
            MessageSS << "LOW";
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            MessageSS << "MEDIUM";
            break;
        case GL_DEBUG_SEVERITY_HIGH:
            MessageSS << "HIGH";
            break;
        }
        MessageSS << std::endl;

        //MessageSS << "Id: "<< id << std::endl;

        OutputDebugStringA( MessageSS.str().c_str() );
    }

    GLContext::GLContext( const ContextInitInfo &Info, DeviceCaps &DeviceCaps ) :
        m_SwapChainAttribs(Info.SwapChainAttribs),
		m_Context(0),
		m_WindowHandleToDeviceContext(0)
    {
		Int32 MajorVersion = 0, MinorVersion = 0;
		if(Info.pNativeWndHandle != nullptr)
		{
			HWND hWnd = reinterpret_cast<HWND>(Info.pNativeWndHandle);
			RECT rc;
			GetClientRect( hWnd, &rc );
			m_SwapChainAttribs.Width = rc.right - rc.left;
			m_SwapChainAttribs.Height = rc.bottom - rc.top;

			// See http://www.opengl.org/wiki/Tutorial:_OpenGL_3.1_The_First_Triangle_(C%2B%2B/Win)
			//     http://www.opengl.org/wiki/Creating_an_OpenGL_Context_(WGL)
			PIXELFORMATDESCRIPTOR pfd;
			memset( &pfd, 0, sizeof( PIXELFORMATDESCRIPTOR ) );
			pfd.nSize = sizeof( PIXELFORMATDESCRIPTOR );
			pfd.nVersion = 1;
			pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
			pfd.iPixelType = PFD_TYPE_RGBA;
			pfd.cColorBits = 32;
			pfd.cDepthBits = 32;
			pfd.iLayerType = PFD_MAIN_PLANE;


			m_WindowHandleToDeviceContext = GetDC( hWnd );
			int nPixelFormat = ChoosePixelFormat( m_WindowHandleToDeviceContext, &pfd );

			if( nPixelFormat == 0 )
				LOG_ERROR_AND_THROW( "Invalid Pixel Format" );

			BOOL bResult = SetPixelFormat( m_WindowHandleToDeviceContext, nPixelFormat, &pfd );
			if( !bResult )
				LOG_ERROR_AND_THROW( "Failed to set Pixel Format" );

			// Create standard OpenGL (2.1) rendering context which will be used only temporarily, 
			HGLRC tempContext = wglCreateContext( m_WindowHandleToDeviceContext );
			// and make it current
			wglMakeCurrent( m_WindowHandleToDeviceContext, tempContext );

			// Initialize GLEW
			GLenum err = glewInit();
			if( GLEW_OK != err )
				LOG_ERROR_AND_THROW( "Failed to initialize GLEW" );

        
			if( wglewIsSupported( "WGL_ARB_create_context" ) == 1 )
			{
				MajorVersion = 4;
				MinorVersion = 4;
				// Setup attributes for a new OpenGL rendering context
				int attribs[] =
				{
					WGL_CONTEXT_MAJOR_VERSION_ARB, MajorVersion,
					WGL_CONTEXT_MINOR_VERSION_ARB, MinorVersion,
					WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
					GL_CONTEXT_PROFILE_MASK, GL_CONTEXT_CORE_PROFILE_BIT,
					0, 0
				};

	#ifdef _DEBUG
				attribs[5] |= WGL_CONTEXT_DEBUG_BIT_ARB;
	#endif 

				// Create new rendering context
				// In order to create new OpenGL rendering context we have to call function wglCreateContextAttribsARB(), 
				// which is an OpenGL function and requires OpenGL to be active when it is called. 
				// The only way is to create an old context, activate it, and while it is active create a new one. 
				// Very inconsistent, but we have to live with it!
				m_Context = wglCreateContextAttribsARB( m_WindowHandleToDeviceContext, 0, attribs );

				// Delete tempContext
				wglMakeCurrent( NULL, NULL );
				wglDeleteContext( tempContext );
				// 
				wglMakeCurrent( m_WindowHandleToDeviceContext, m_Context );
				wglSwapIntervalEXT( 0 );
			}
			else
			{       //It's not possible to make a GL 3.x context. Use the old style context (GL 2.1 and before)
				m_Context = tempContext;
			}
		}
		else
		{
			auto CurrentCtx = wglGetCurrentContext();
			m_WindowHandleToDeviceContext = wglGetCurrentDC();
			if (CurrentCtx != 0)
			{
				LOG_INFO_MESSAGE("Attaching to existing OpenGL context")
			}
			else
			{
				LOG_ERROR_AND_THROW("No current GL context found! Provide non-null handle to a native Window to create a GL context")
			}
			
			// Initialize GLEW
			GLenum err = glewInit();
			if( GLEW_OK != err )
				LOG_ERROR_AND_THROW( "Failed to initialize GLEW" );
		}

        //Checking GL version
        const GLubyte *GLVersionString = glGetString( GL_VERSION );

        //Or better yet, use the GL3 way to get the version number
        glGetIntegerv( GL_MAJOR_VERSION, &MajorVersion );
        glGetIntegerv( GL_MINOR_VERSION, &MinorVersion );

        if( glDebugMessageCallback )
        {
            glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
            glDebugMessageCallback( openglCallbackFunction, nullptr );
            GLuint unusedIds = 0;
            glDebugMessageControl( GL_DONT_CARE,
                GL_DONT_CARE,
                GL_DONT_CARE,
                0,
                &unusedIds,
                true );
        }

        // Under the standard filtering rules for cubemaps, filtering does not work across faces of the cubemap. 
        // This results in a seam across the faces of a cubemap. This was a hardware limitation in the past, but 
        // modern hardware is capable of interpolating across a cube face boundary.
        // GL_TEXTURE_CUBE_MAP_SEAMLESS is not defined in OpenGLES
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        if( glGetError() != GL_NO_ERROR )
            LOG_ERROR_MESSAGE("Failed to enable seamless cubemap filtering");

        DeviceCaps.DevType = DeviceType::OpenGL;
        DeviceCaps.MajorVersion = MajorVersion;
        DeviceCaps.MinorVersion = MinorVersion;
        bool IsGL43OrAbove = MajorVersion >= 5 || MajorVersion == 4 && MinorVersion >= 3;
        auto &TexCaps = DeviceCaps.TexCaps;
        TexCaps.bTexture2DMSSupported      = IsGL43OrAbove;
        TexCaps.bTexture2DMSArraySupported = IsGL43OrAbove;
        TexCaps.bTextureViewSupported      = IsGL43OrAbove;
        TexCaps.bCubemapArraysSupported    = IsGL43OrAbove;
        DeviceCaps.bMultithreadedResourceCreationSupported = False;
    }

    GLContext::~GLContext()
    {
		// Do not destroy context if it was create by the app.
        if( m_Context )
		{
			wglMakeCurrent( m_WindowHandleToDeviceContext, 0 );
            wglDeleteContext( m_Context );
		}
    }

    void GLContext::SwapBuffers()
    {
        ::SwapBuffers( m_WindowHandleToDeviceContext );
    }
}
