/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "DynamicBuffer.hpp"
#include "TestingEnvironment.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

TEST(DynamicBufferTest, Create)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    TestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Dynamic buffer create test 0";
        BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        BuffDesc.uiSizeInBytes = 0;
        DynamicBuffer DynBuff{pDevice, BuffDesc};
        EXPECT_EQ(DynBuff.GetDesc(), BuffDesc);
        EXPECT_STREQ(DynBuff.GetDesc().Name, BuffDesc.Name);
        EXPECT_FALSE(DynBuff.PendingUpdate());

        auto* pBuffer = DynBuff.GetBuffer(nullptr, nullptr);
        EXPECT_FALSE(DynBuff.PendingUpdate());
        EXPECT_EQ(pBuffer, nullptr);
    }

    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Dynamic buffer create test 1";
        BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        BuffDesc.uiSizeInBytes = 256;
        DynamicBuffer DynBuff{pDevice, BuffDesc};
        EXPECT_EQ(DynBuff.GetDesc(), BuffDesc);
        EXPECT_FALSE(DynBuff.PendingUpdate());

        auto* pBuffer = DynBuff.GetBuffer(nullptr, nullptr);
        ASSERT_NE(pBuffer, nullptr);
        EXPECT_EQ(pBuffer->GetDesc(), BuffDesc);
    }

    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Dynamic buffer create test 2";
        BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        BuffDesc.uiSizeInBytes = 256;
        DynamicBuffer DynBuff{nullptr, BuffDesc};
        EXPECT_EQ(DynBuff.GetDesc(), BuffDesc);
        EXPECT_TRUE(DynBuff.PendingUpdate());

        auto* pBuffer = DynBuff.GetBuffer(pDevice, nullptr);
        EXPECT_FALSE(DynBuff.PendingUpdate());
        ASSERT_NE(pBuffer, nullptr);
        EXPECT_EQ(pBuffer->GetDesc(), BuffDesc);
    }
}

TEST(DynamicBufferTest, Resize)
{
    auto* pEnv     = TestingEnvironment::GetInstance();
    auto* pDevice  = pEnv->GetDevice();
    auto* pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Dynamic buffer resize test 0";
        BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        BuffDesc.uiSizeInBytes = 256;
        DynamicBuffer DynBuff{nullptr, BuffDesc};
        EXPECT_TRUE(DynBuff.PendingUpdate());

        BuffDesc.uiSizeInBytes = 512;
        DynBuff.Resize(nullptr, nullptr, BuffDesc.uiSizeInBytes);
        EXPECT_TRUE(DynBuff.PendingUpdate());

        auto* pBuffer = DynBuff.GetBuffer(pDevice, nullptr);
        EXPECT_EQ(DynBuff.GetVersion(), Uint32{1});
        EXPECT_FALSE(DynBuff.PendingUpdate());
        ASSERT_NE(pBuffer, nullptr);
        EXPECT_EQ(pBuffer->GetDesc(), BuffDesc);
    }

    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Dynamic buffer resize test 1";
        BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        BuffDesc.uiSizeInBytes = 256;
        DynamicBuffer DynBuff{pDevice, BuffDesc};
        EXPECT_FALSE(DynBuff.PendingUpdate());

        BuffDesc.uiSizeInBytes = 1024;
        DynBuff.Resize(nullptr, nullptr, 1024);
        EXPECT_EQ(DynBuff.GetDesc(), BuffDesc);
        EXPECT_TRUE(DynBuff.PendingUpdate());

        BuffDesc.uiSizeInBytes = 512;
        DynBuff.Resize(pDevice, nullptr, BuffDesc.uiSizeInBytes);
        EXPECT_EQ(DynBuff.GetDesc(), BuffDesc);
        EXPECT_TRUE(DynBuff.PendingUpdate());

        auto* pBuffer = DynBuff.GetBuffer(nullptr, pContext);
        EXPECT_FALSE(DynBuff.PendingUpdate());
        EXPECT_EQ(pBuffer->GetDesc(), BuffDesc);
        ASSERT_NE(pBuffer, nullptr);
    }

    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Dynamic buffer resize test 2";
        BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        BuffDesc.uiSizeInBytes = 256;
        DynamicBuffer DynBuff{pDevice, BuffDesc};
        EXPECT_NE(DynBuff.GetBuffer(nullptr, nullptr), nullptr);

        BuffDesc.uiSizeInBytes = 512;
        DynBuff.Resize(pDevice, pContext, BuffDesc.uiSizeInBytes);
        EXPECT_FALSE(DynBuff.PendingUpdate());
        EXPECT_EQ(DynBuff.GetVersion(), Uint32{1});


        auto* pBuffer = DynBuff.GetBuffer(nullptr, nullptr);
        ASSERT_NE(pBuffer, nullptr);
        EXPECT_EQ(pBuffer->GetDesc(), BuffDesc);

        BuffDesc.uiSizeInBytes = 128;
        DynBuff.Resize(pDevice, pContext, BuffDesc.uiSizeInBytes);
        EXPECT_FALSE(DynBuff.PendingUpdate());
        EXPECT_EQ(DynBuff.GetVersion(), Uint32{2});

        pBuffer = DynBuff.GetBuffer(nullptr, nullptr);
        ASSERT_NE(pBuffer, nullptr);
        EXPECT_EQ(pBuffer->GetDesc(), BuffDesc);
    }

    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Dynamic buffer resize test 3";
        BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        BuffDesc.uiSizeInBytes = 256;
        DynamicBuffer DynBuff{pDevice, BuffDesc};
        EXPECT_FALSE(DynBuff.PendingUpdate());

        BuffDesc.uiSizeInBytes = 1024;
        DynBuff.Resize(pDevice, nullptr, 1024);
        EXPECT_TRUE(DynBuff.PendingUpdate());
        EXPECT_EQ(DynBuff.GetVersion(), Uint32{1});

        DynBuff.Resize(nullptr, pContext, BuffDesc.uiSizeInBytes);
        EXPECT_FALSE(DynBuff.PendingUpdate());
        EXPECT_EQ(DynBuff.GetVersion(), Uint32{1});

        auto* pBuffer = DynBuff.GetBuffer(nullptr, nullptr);
        EXPECT_FALSE(DynBuff.PendingUpdate());
        EXPECT_EQ(pBuffer->GetDesc(), BuffDesc);
        ASSERT_NE(pBuffer, nullptr);
    }

    {
        BufferDesc BuffDesc;
        BuffDesc.Name          = "Dynamic buffer resize test 4";
        BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        BuffDesc.uiSizeInBytes = 256;
        DynamicBuffer DynBuff{pDevice, BuffDesc};
        EXPECT_NE(DynBuff.GetBuffer(nullptr, nullptr), nullptr);

        DynBuff.Resize(nullptr, nullptr, 1024);

        BuffDesc.uiSizeInBytes = 0;
        DynBuff.Resize(nullptr, nullptr, BuffDesc.uiSizeInBytes);
        EXPECT_FALSE(DynBuff.PendingUpdate());
        EXPECT_EQ(DynBuff.GetDesc(), BuffDesc);

        auto* pBuffer = DynBuff.GetBuffer(pDevice, nullptr);
        EXPECT_EQ(pBuffer, nullptr);

        DynBuff.Resize(pDevice, pContext, 512);
        EXPECT_FALSE(DynBuff.PendingUpdate());

        DynBuff.Resize(pDevice, nullptr, 1024);
        DynBuff.Resize(nullptr, nullptr, 0);
        EXPECT_FALSE(DynBuff.PendingUpdate());
        pBuffer = DynBuff.GetBuffer(pDevice, nullptr);
        EXPECT_EQ(pBuffer, nullptr);
    }
}

} // namespace
