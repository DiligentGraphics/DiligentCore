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

#include "DynamicTextureAtlas.h"

#include <thread>

#include "TestingEnvironment.hpp"
#include "gtest/gtest.h"
#include "FastRand.hpp"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

TEST(DynamicTextureAtlas, Create)
{
    auto* const pEnv     = TestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto* const pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    DynamicTextureAtlasCreateInfo CI;
    CI.ExtraSliceCount    = 2;
    CI.TextureGranularity = 16;
    CI.Desc.Format        = TEX_FORMAT_RGBA8_UNORM;
    CI.Desc.Name          = "Dynamic Texture Atlas Test";
    CI.Desc.Type          = RESOURCE_DIM_TEX_2D_ARRAY;
    CI.Desc.BindFlags     = BIND_SHADER_RESOURCE;
    CI.Desc.Width         = 512;
    CI.Desc.Height        = 512;
    CI.Desc.ArraySize     = 0;

    {
        RefCntAutoPtr<IDynamicTextureAtlas> pAtlas;
        CreateDynamicTextureAtlas(nullptr, CI, &pAtlas);

        auto* pTexture = pAtlas->GetTexture(nullptr, nullptr);
        EXPECT_EQ(pTexture, nullptr);

        RefCntAutoPtr<ITextureAtlasSuballocation> pSuballoc;
        pAtlas->Allocate(128, 128, &pSuballoc);
        EXPECT_TRUE(pSuballoc);

        pTexture = pAtlas->GetTexture(pDevice, pContext);
        EXPECT_NE(pTexture, nullptr);
    }

    CI.Desc.ArraySize = 2;
    {
        RefCntAutoPtr<IDynamicTextureAtlas> pAtlas;
        CreateDynamicTextureAtlas(nullptr, CI, &pAtlas);

        auto* pTexture = pAtlas->GetTexture(pDevice, pContext);
        EXPECT_NE(pTexture, nullptr);
    }

    {
        RefCntAutoPtr<IDynamicTextureAtlas> pAtlas;
        CreateDynamicTextureAtlas(pDevice, CI, &pAtlas);

        auto* pTexture = pAtlas->GetTexture(pDevice, pContext);
        EXPECT_NE(pTexture, nullptr);

        RefCntAutoPtr<ITextureAtlasSuballocation> pSuballoc;
        pAtlas->Allocate(128, 128, &pSuballoc);
        EXPECT_TRUE(pSuballoc);

        // Release atlas first
        pAtlas.Release();
        pSuballoc.Release();
    }
}

TEST(DynamicTextureAtlas, Allocate)
{
    auto* const pEnv     = TestingEnvironment::GetInstance();
    auto* const pDevice  = pEnv->GetDevice();
    auto* const pContext = pEnv->GetDeviceContext();

    TestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    DynamicTextureAtlasCreateInfo CI;
    CI.ExtraSliceCount    = 2;
    CI.TextureGranularity = 16;
    CI.Desc.Format        = TEX_FORMAT_RGBA8_UNORM;
    CI.Desc.Name          = "Dynamic Texture Atlas Test";
    CI.Desc.Type          = RESOURCE_DIM_TEX_2D_ARRAY;
    CI.Desc.BindFlags     = BIND_SHADER_RESOURCE;
    CI.Desc.Width         = 512;
    CI.Desc.Height        = 512;
    CI.Desc.ArraySize     = 1;

    RefCntAutoPtr<IDynamicTextureAtlas> pAtlas;
    CreateDynamicTextureAtlas(pDevice, CI, &pAtlas);

#ifdef _DEBUG
    constexpr size_t NumIterations = 8;
#else
    constexpr size_t NumIterations = 32;
#endif
    for (size_t i = 0; i < NumIterations; ++i)
    {
        const size_t NumThreads = std::max(4u, std::thread::hardware_concurrency());

        const size_t NumAllocations = i * 8;

        std::vector<std::vector<RefCntAutoPtr<ITextureAtlasSuballocation>>> pSubAllocations(NumThreads);
        for (auto& Allocs : pSubAllocations)
            Allocs.resize(NumAllocations);

        {
            std::vector<std::thread> Threads(NumThreads);
            for (size_t t = 0; t < Threads.size(); ++t)
            {
                Threads[t] = std::thread{
                    [&](size_t thread_id) //
                    {
                        FastRandInt rnd{static_cast<unsigned int>(thread_id), 4, 64};

                        auto& Allocs = pSubAllocations[thread_id];
                        for (auto& Alloc : Allocs)
                        {
                            Uint32 Width  = static_cast<Uint32>(rnd());
                            Uint32 Height = static_cast<Uint32>(rnd());
                            pAtlas->Allocate(Width, Height, &Alloc);
                            ASSERT_TRUE(Alloc);
                            EXPECT_EQ(Alloc->GetSize().x, Width);
                            EXPECT_EQ(Alloc->GetSize().y, Height);
                        }
                    },
                    t //
                };
            }

            for (auto& Thread : Threads)
                Thread.join();
        }

        auto* pTexture = pAtlas->GetTexture(pDevice, pContext);
        EXPECT_NE(pTexture, nullptr);

        {
            std::vector<std::thread> Threads(NumThreads);
            for (size_t t = 0; t < Threads.size(); ++t)
            {
                Threads[t] = std::thread{
                    [&](size_t thread_id) //
                    {
                        auto& Allocs = pSubAllocations[thread_id];
                        for (auto& Alloc : Allocs)
                            Alloc.Release();
                    },
                    t //
                };
            }

            for (auto& Thread : Threads)
                Thread.join();
        }
    }
}

} // namespace
