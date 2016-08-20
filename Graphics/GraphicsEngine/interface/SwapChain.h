/*     Copyright 2015-2016 Egor Yusov
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
/// Definition of the Diligent::ISwapChain interface and related data structures

#include "Object.h"

namespace Diligent
{

// {1C703B77-6607-4EEC-B1FE-15C82D3B4130}
static const Diligent::INTERFACE_ID IID_SwapChain =
{ 0x1c703b77, 0x6607, 0x4eec, { 0xb1, 0xfe, 0x15, 0xc8, 0x2d, 0x3b, 0x41, 0x30 } };

/// Swap chain interface

/// The swap chain is created by a platform-dependent function
class ISwapChain : public IObject
{
public:

    /// Presents a rendered image to the user.
    virtual void Present() = 0;

    /// Returns the swap chain desctription
    virtual const SwapChainDesc& GetDesc()const = 0;

    /// Changes the swap chain's back buffer size

    /// \param [in] NewWidth - New swap chain width, in pixels
    /// \param [in] NewHeight - New swap chain height, in pixels
    virtual void Resize( Uint32 NewWidth, Uint32 NewHeight ) = 0;
};

}
