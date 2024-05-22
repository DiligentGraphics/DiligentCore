/*
 *  Copyright 2019-2024 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#include "WinHPreface.h"
#include <d3dcommon.h>
#include "WinHPostface.h"

#include <functional>

#include "ShaderBase.hpp"
#include "ThreadPool.h"
#include "RefCntAutoPtr.hpp"

/// \file
/// Base implementation of a D3D shader

namespace Diligent
{

class IDXCompiler;

CComPtr<ID3DBlob> CompileD3DBytecode(const ShaderCreateInfo&        ShaderCI,
                                     const ShaderVersion            ShaderModel,
                                     IDXCompiler*                   DxCompiler,
                                     IDataBlob**                    ppCompilerOutput,
                                     std::function<void(ID3DBlob*)> InitResources) noexcept(false);

/// Base implementation of a D3D shader
template <typename EngineImplTraits>
class ShaderD3DBase : public ShaderBase<EngineImplTraits>
{
public:
    using RenderDeviceImplType = typename EngineImplTraits::RenderDeviceImplType;

    ShaderD3DBase(IReferenceCounters*        pRefCounters,
                  RenderDeviceImplType*      pDevice,
                  const ShaderDesc&          Desc,
                  const RenderDeviceInfo&    DeviceInfo,
                  const GraphicsAdapterInfo& AdapterInfo,
                  bool                       bIsDeviceInternal = false) :
        ShaderBase<EngineImplTraits>{pRefCounters, pDevice, Desc, DeviceInfo, AdapterInfo, bIsDeviceInternal}
    {}

    virtual void DILIGENT_CALL_TYPE GetBytecode(const void** ppBytecode,
                                                Uint64&      Size) const override final
    {
        DEV_CHECK_ERR(this->m_Status.load() > SHADER_STATUS_COMPILING, "Shader resources are not available until compilation is complete. Use GetStatus() to check the shader status.");
        if (m_pShaderByteCode)
        {
            *ppBytecode = m_pShaderByteCode->GetBufferPointer();
            Size        = m_pShaderByteCode->GetBufferSize();
        }
        else
        {
            *ppBytecode = nullptr;
            Size        = 0;
        }
    }

    ID3DBlob* GetD3DBytecode() const
    {
        return m_pShaderByteCode;
    }

protected:
    RefCntAutoPtr<IAsyncTask> Initialize(const ShaderCreateInfo&        ShaderCI,
                                         const ShaderVersion            ShaderModel,
                                         IDXCompiler*                   DxCompiler,
                                         IDataBlob**                    ppCompilerOutput,
                                         IThreadPool*                   pAsyncCompilationThreadPool,
                                         std::function<void(ID3DBlob*)> InitResources)
    {
        this->m_Status.store(SHADER_STATUS_COMPILING);
        if (pAsyncCompilationThreadPool == nullptr || (ShaderCI.CompileFlags & SHADER_COMPILE_FLAG_ASYNCHRONOUS) == 0 || ShaderCI.ByteCode != nullptr)
        {
            m_pShaderByteCode = CompileD3DBytecode(ShaderCI, ShaderModel, DxCompiler, ppCompilerOutput, InitResources);
            this->m_Status.store(SHADER_STATUS_READY);
            return {};
        }
        else
        {
            return EnqueueAsyncWork(pAsyncCompilationThreadPool,
                                    [this,
                                     ShaderCI = ShaderCreateInfoWrapper{ShaderCI, GetRawAllocator()},
                                     ShaderModel,
                                     DxCompiler,
                                     ppCompilerOutput,
                                     InitResources](Uint32 ThreadId) //
                                    {
                                        try
                                        {
                                            m_pShaderByteCode = CompileD3DBytecode(ShaderCI, ShaderModel, DxCompiler, ppCompilerOutput, InitResources);
                                            this->m_Status.store(SHADER_STATUS_READY);
                                        }
                                        catch (...)
                                        {
                                            this->m_Status.store(SHADER_STATUS_FAILED);
                                        }
                                    });
        }
    }

protected:
    CComPtr<ID3DBlob> m_pShaderByteCode;
};

} // namespace Diligent
