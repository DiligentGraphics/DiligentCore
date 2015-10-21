/*     Copyright 2015 Egor Yusov
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
/// Debug utilities

#include "DeviceObject.h"
#include "ShaderD3D11Impl.h"

#ifdef _DEBUG
#   define VERIFY_RESOURCE_ARRAYS
#endif

namespace Diligent
{
#ifdef VERIFY_RESOURCE_ARRAYS

    /// Debug function that verifies that cached engine objects and d3d11 shader resource views in two provided arrays are consistent
    void dbgVerifyResourceArrays( const std::vector< ShaderD3D11Impl::BoundSRV > &SRVs, const std::vector<ID3D11ShaderResourceView*> d3d11SRVs, IShader *pShader );

    /// Debug function that verifies that cached engine objects and d3d11 unordered access views in two provided arrays are consistent
    void dbgVerifyResourceArrays( const std::vector< ShaderD3D11Impl::BoundUAV > &UAVs, const std::vector<ID3D11UnorderedAccessView*> d3d11UAVs, IShader *pShader );

    /// Debug function that verifies that cached engine objects and d3d11 constant buffers in two provided arrays are consistent
    void dbgVerifyResourceArrays( const std::vector< ShaderD3D11Impl::BoundCB > &CBs, const std::vector<ID3D11Buffer*> d3d11CBs, IShader *pShader );

    /// Debug function that verifies that cached engine objects and d3d11 samplers in two provided arrays are consistent
    void dbgVerifyResourceArrays( const std::vector< ShaderD3D11Impl::BoundSampler >&Samplers, const std::vector<ID3D11SamplerState*> d3d11Samplers, IShader *pShader );

#else

    #define dbgVerifyResourceArrays(...)

#endif
}
