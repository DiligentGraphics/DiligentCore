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

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "RefCntAutoPtr.h"
#include "SwapChain.h"
#include "GraphicsTypesOutputInserters.h"

#include "gtest/gtest.h"

namespace Diligent
{

class TestingEnvironment final : public ::testing::Environment
{
public:
    TestingEnvironment(DeviceType deviceType, ADAPTER_TYPE AdapterType);
    ~TestingEnvironment() override;

    // Override this to define how to set up the environment.
    void SetUp() override final;

    // Override this to define how to tear down the environment.
    void TearDown() override final;

    void Reset();
    void ReleaseResources();

    class ScopedReset
    {
    public:
        ScopedReset() = default;
        ~ScopedReset()
        {
            if (m_pTheEnvironment)
                m_pTheEnvironment->Reset();
        }
    };

    class ScopedReleaseResources
    {
    public:
        ScopedReleaseResources() = default;
        ~ScopedReleaseResources()
        {
            if (m_pTheEnvironment)
                m_pTheEnvironment->ReleaseResources();
        }
    };

    IRenderDevice*  GetDevice() { return m_pDevice; }
    IDeviceContext* GetDeviceContext() { return m_pDeviceContext; }

    static TestingEnvironment* GetInstance() { return m_pTheEnvironment; }

    RefCntAutoPtr<ITexture> CreateTexture(const char* Name, TEXTURE_FORMAT Fmt, BIND_FLAGS BindFlags, Uint32 Width, Uint32 Height);

private:
    const DeviceType m_DeviceType;

    static TestingEnvironment* m_pTheEnvironment;

    RefCntAutoPtr<IRenderDevice>  m_pDevice;
    RefCntAutoPtr<IDeviceContext> m_pDeviceContext;
    RefCntAutoPtr<ISwapChain>     m_pSwapChain;
};

} // namespace Diligent
