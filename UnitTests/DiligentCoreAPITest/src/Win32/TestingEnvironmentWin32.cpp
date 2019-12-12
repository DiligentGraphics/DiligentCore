/*     Copyright 2019 Diligent Graphics LLC
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

#include "TestingEnvironment.h"

#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <Windows.h>

namespace Diligent
{

TestingEnvironment::NativeWindow TestingEnvironment::CreateNativeWindow()
{
#ifdef UNICODE
    const auto* const WindowClassName = L"SampleApp";
#else
    const auto* const WindowClassName = "SampleApp";
#endif
    // Register window class
    HINSTANCE instance = NULL;

    WNDCLASSEX wcex = {sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, DefWindowProc,
                       0L, 0L, instance, NULL, NULL, NULL, NULL, WindowClassName, NULL};
    RegisterClassEx(&wcex);

    LONG WindowWidth  = 512;
    LONG WindowHeight = 512;
    RECT rc           = {0, 0, WindowWidth, WindowHeight};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND wnd = CreateWindowA("SampleApp", "Dummy Window",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, instance, NULL);
    if (wnd == NULL)
        LOG_ERROR_AND_THROW("Unable to create a window");

    NativeWindow NativeWnd;
    NativeWnd.NativeWindowHandle = wnd;

    return NativeWnd;
}

} // namespace Diligent
