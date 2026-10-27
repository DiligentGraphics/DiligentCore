/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include "BytecodeCache.h"
#include "DataBlobImpl.hpp"
#include "DefaultShaderSourceStreamFactory.h"
#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

TEST(BytecodeCacheTest, Basic)
{
    RefCntAutoPtr<IBytecodeCache> pCache;
    CreateBytecodeCache({RENDER_DEVICE_TYPE_VULKAN}, &pCache);
    ASSERT_NE(pCache, nullptr);

    {
        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.Desc.Name       = "TestName";
        ShaderCI.Source          = "SomeCode";

        RefCntAutoPtr<IDataBlob> pBytecodeSaved;
        RefCntAutoPtr<IDataBlob> pBytecodeLoaded;

        {
            const std::string Data{"TestString"};
            pBytecodeSaved = DataBlobImpl::Create(Data.length(), Data.c_str());
            pCache->AddBytecode(ShaderCI, pBytecodeSaved);
        }

        {
            RefCntAutoPtr<IDataBlob> pShaderDataBlob;
            pCache->Store(&pShaderDataBlob);
            pCache->Clear();
            EXPECT_TRUE(pCache->Load(pShaderDataBlob));
        }

        {
            pCache->GetBytecode(ShaderCI, &pBytecodeLoaded);
            ASSERT_NE(pBytecodeLoaded, nullptr);
        }

        EXPECT_EQ(pBytecodeSaved->GetSize(), pBytecodeLoaded->GetSize());
        EXPECT_EQ(memcmp(pBytecodeSaved->GetConstDataPtr(), pBytecodeLoaded->GetConstDataPtr(), pBytecodeLoaded->GetSize()), 0);
    }
}

TEST(BytecodeCacheTest, RemoveBytecode)
{
    RefCntAutoPtr<IBytecodeCache> pCache;
    CreateBytecodeCache({RENDER_DEVICE_TYPE_VULKAN}, &pCache);
    ASSERT_NE(pCache, nullptr);

    {
        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.Desc.Name       = "TestName";
        ShaderCI.Source          = "SomeCode";

        {
            const std::string        Data{"TestString"};
            RefCntAutoPtr<IDataBlob> pBytecode = DataBlobImpl::Create(Data.length(), Data.c_str());
            pCache->AddBytecode(ShaderCI, pBytecode);
        }

        {
            pCache->RemoveBytecode(ShaderCI);
        }

        {
            RefCntAutoPtr<IDataBlob> pBytecode;
            pCache->GetBytecode(ShaderCI, &pBytecode);
            EXPECT_EQ(pBytecode, nullptr);
        }
    }
}

TEST(BytecodeCacheTest, DoubleAdd)
{
    RefCntAutoPtr<IBytecodeCache> pCache;
    CreateBytecodeCache({RENDER_DEVICE_TYPE_VULKAN}, &pCache);
    ASSERT_NE(pCache, nullptr);

    {
        ShaderCreateInfo ShaderCI{};
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.Desc.Name       = "TestName";
        ShaderCI.Source          = "SomeCode";

        RefCntAutoPtr<IDataBlob> pBytecodeSaved;
        RefCntAutoPtr<IDataBlob> pBytecodeLoaded;
        {
            const std::string        Data{"TestString0"};
            RefCntAutoPtr<IDataBlob> pBytecode = DataBlobImpl::Create(Data.length(), Data.c_str());
            pCache->AddBytecode(ShaderCI, pBytecode);
        }

        {
            const std::string Data{"TestString1"};
            pBytecodeSaved = DataBlobImpl::Create(Data.length(), Data.c_str());
            pCache->AddBytecode(ShaderCI, pBytecodeSaved);
        }

        {
            pCache->GetBytecode(ShaderCI, &pBytecodeLoaded);
            ASSERT_NE(pBytecodeLoaded, nullptr);
        }

        EXPECT_EQ(pBytecodeSaved->GetSize(), pBytecodeLoaded->GetSize());
        EXPECT_EQ(memcmp(pBytecodeSaved->GetConstDataPtr(), pBytecodeLoaded->GetConstDataPtr(), pBytecodeLoaded->GetSize()), 0);
    }
}

TEST(BytecodeCacheTest, Include)
{
    RefCntAutoPtr<IBytecodeCache> pCache;
    CreateBytecodeCache({RENDER_DEVICE_TYPE_VULKAN}, &pCache);
    ASSERT_NE(pCache, nullptr);

    ShaderCreateInfo ShaderCI{};
    ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
    ShaderCI.Desc.Name       = "TestName";
    ShaderCI.FilePath        = "IncludeBasicTest.hlsl";

    const std::string        Data{"TestString"};
    RefCntAutoPtr<IDataBlob> pReferenceBytecode = DataBlobImpl::Create(Data.length(), Data.c_str());

    {
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        CreateDefaultShaderSourceStreamFactory("shaders/BytecodeCache/IncludeTest0", &pShaderSourceFactory);
        ASSERT_NE(pShaderSourceFactory, nullptr);

        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        pCache->AddBytecode(ShaderCI, pReferenceBytecode);
    }

    {
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        CreateDefaultShaderSourceStreamFactory("shaders/BytecodeCache/IncludeTest1", &pShaderSourceFactory);
        ASSERT_NE(pShaderSourceFactory, nullptr);

        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        RefCntAutoPtr<IDataBlob> pBytecode;
        pCache->GetBytecode(ShaderCI, &pBytecode);

        EXPECT_EQ(pReferenceBytecode->GetSize(), pBytecode->GetSize());
        EXPECT_EQ(memcmp(pReferenceBytecode->GetConstDataPtr(), pBytecode->GetConstDataPtr(), pBytecode->GetSize()), 0);
    }

    {
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        CreateDefaultShaderSourceStreamFactory("shaders/BytecodeCache/IncludeTest2", &pShaderSourceFactory);
        ASSERT_NE(pShaderSourceFactory, nullptr);

        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        RefCntAutoPtr<IDataBlob> pBytecode;
        pCache->GetBytecode(ShaderCI, &pBytecode);
        EXPECT_EQ(pBytecode, nullptr);
    }
}

TEST(BytecodeCacheTest, DistinctMacroBoundaries)
{
    RefCntAutoPtr<IBytecodeCache> Cache;
    CreateBytecodeCache({RENDER_DEVICE_TYPE_VULKAN}, &Cache);
    ASSERT_NE(Cache, nullptr);

    ShaderCreateInfo CI;
    CI.Source                        = "void main() {}";
    const ShaderMacro First[]        = {{"A", "12"}};
    const ShaderMacro Second[]       = {{"A1", "2"}};
    const Uint32      FirstData      = 12;
    const Uint32      SecondData     = 2;
    auto              FirstBytecode  = DataBlobImpl::Create(sizeof(FirstData), &FirstData);
    auto              SecondBytecode = DataBlobImpl::Create(sizeof(SecondData), &SecondData);
    CI.Macros                        = {First, 1};
    Cache->AddBytecode(CI, FirstBytecode);
    CI.Macros = {Second, 1};
    Cache->AddBytecode(CI, SecondBytecode);

    RefCntAutoPtr<IDataBlob> Stored;
    Cache->Store(&Stored);
    Cache->Clear();
    ASSERT_TRUE(Cache->Load(Stored));
    RefCntAutoPtr<IDataBlob> Loaded;
    CI.Macros = {First, 1};
    Cache->GetBytecode(CI, &Loaded);
    ASSERT_NE(Loaded, nullptr);
    ASSERT_EQ(Loaded->GetSize(), sizeof(FirstData));
    EXPECT_EQ(memcmp(Loaded->GetConstDataPtr(), &FirstData, sizeof(FirstData)), 0);
    Loaded.Release();
    CI.Macros = {Second, 1};
    Cache->GetBytecode(CI, &Loaded);
    ASSERT_NE(Loaded, nullptr);
    ASSERT_EQ(Loaded->GetSize(), sizeof(SecondData));
    EXPECT_EQ(memcmp(Loaded->GetConstDataPtr(), &SecondData, sizeof(SecondData)), 0);
}

} // namespace
