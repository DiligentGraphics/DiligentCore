/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include "VertexPoolX.hpp"
#include "gtest/gtest.h"
#include <memory>

using namespace Diligent;

namespace
{

TEST(VertexPoolXTest, VertexPoolDescX)
{
    constexpr VertexPoolElementDesc Elements[] = {
        VertexPoolElementDesc{16u, BIND_VERTEX_BUFFER, USAGE_DEFAULT, BUFFER_MODE_UNDEFINED, CPU_ACCESS_NONE},
        VertexPoolElementDesc{32u, BIND_SHADER_RESOURCE, USAGE_DYNAMIC, BUFFER_MODE_STRUCTURED, CPU_ACCESS_WRITE},
    };

    VertexPoolDesc Ref;

    {
        VertexPoolDescX DescX;
        EXPECT_EQ(Ref, DescX);
        {
            auto Name = std::make_unique<std::string>("Test Name");
            DescX.SetName(Name->c_str());
        }
        EXPECT_NE(Ref, DescX);
        Ref.Name = "Test Name";
        EXPECT_EQ(Ref, DescX);

        DescX.SetVertexCount(1024);
        EXPECT_NE(Ref, DescX);
        Ref.VertexCount = 1024;
        EXPECT_EQ(Ref, DescX);

        DescX.AddElement(Elements[0]);
        EXPECT_NE(Ref, DescX);
        Ref.pElements   = Elements;
        Ref.NumElements = 1;
        EXPECT_EQ(Ref, DescX);

        DescX.AddElement(Elements[1]);
        EXPECT_NE(Ref, DescX);
        Ref.NumElements = 2;
        EXPECT_EQ(Ref, DescX);
    }

    {
        VertexPoolDescX DescX{Ref};
        EXPECT_EQ(Ref, DescX);
    }

    {
        VertexPoolDescX DescX;
        DescX = Ref;
        EXPECT_EQ(Ref, DescX);
    }
}

TEST(VertexPoolXTest, VertexPoolCreateInfoX)
{
    VertexPoolCreateInfo Ref;

    constexpr VertexPoolElementDesc Elements[] = {
        VertexPoolElementDesc{16u, BIND_VERTEX_BUFFER, USAGE_DEFAULT, BUFFER_MODE_UNDEFINED, CPU_ACCESS_NONE},
        VertexPoolElementDesc{32u, BIND_SHADER_RESOURCE, USAGE_DYNAMIC, BUFFER_MODE_STRUCTURED, CPU_ACCESS_WRITE},
    };

    {
        VertexPoolCreateInfoX CIX;
        EXPECT_EQ(Ref, CIX);
        {
            auto Name = std::make_unique<std::string>("Test Name");
            CIX.SetName(Name->c_str());
        }
        EXPECT_NE(Ref, CIX);
        Ref.Desc.Name = "Test Name";
        EXPECT_EQ(Ref, CIX);

        CIX.SetVertexCount(1024);
        EXPECT_NE(Ref, CIX);
        Ref.Desc.VertexCount = 1024;
        EXPECT_EQ(Ref, CIX);

        CIX.AddElement(Elements[0]);
        EXPECT_NE(Ref, CIX);
        Ref.Desc.pElements   = Elements;
        Ref.Desc.NumElements = 1;
        EXPECT_EQ(Ref, CIX);

        CIX.AddElement(Elements[1]);
        EXPECT_NE(Ref, CIX);
        Ref.Desc.NumElements = 2;
        EXPECT_EQ(Ref, CIX);

        CIX.SetExtraVertexCount(256);
        EXPECT_NE(Ref, CIX);
        Ref.ExtraVertexCount = 256;
        EXPECT_EQ(Ref, CIX);


        CIX.SetMaxVertexCount(2048);
        EXPECT_NE(Ref, CIX);
        Ref.MaxVertexCount = 2048;
        EXPECT_EQ(Ref, CIX);

        CIX.SetDisableDebugValidation(true);
        EXPECT_NE(Ref, CIX);
        Ref.DisableDebugValidation = true;
        EXPECT_EQ(Ref, CIX);
    }

    {
        VertexPoolCreateInfoX CIX{Ref};
        EXPECT_EQ(Ref, CIX);
    }

    {
        VertexPoolCreateInfoX CIX;
        CIX = Ref;
        EXPECT_EQ(Ref, CIX);
    }
}

} // namespace
