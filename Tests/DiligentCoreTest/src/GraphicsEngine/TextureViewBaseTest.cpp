/*
 *  Copyright 2023 Diligent Graphics LLC
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

#include "TextureView.h"
#include "GraphicsTypesOutputInserters.hpp"
#include "FastRand.hpp"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

TEST(TextureComponentMappingTest, OperatorMultiply)
{
    auto Test = [](const std::string& lhs, const std::string& rhs, const std::string& res) {
        TextureComponentMapping Mapping1;
        EXPECT_TRUE(TextureComponentMappingFromString(lhs, Mapping1));
        TextureComponentMapping Mapping2;
        EXPECT_TRUE(TextureComponentMappingFromString(rhs, Mapping2));
        TextureComponentMapping Res;
        EXPECT_TRUE(TextureComponentMappingFromString(res, Res));
        EXPECT_EQ(Mapping1 * Mapping2, Res) << Mapping1 << " * " << Mapping2 << " = " << Res;
    };

    Test("rgba", "rrrr", "rrrr");
    Test("rgba", "gggg", "gggg");
    Test("rgba", "bbbb", "bbbb");
    Test("rgba", "aaaa", "aaaa");
    Test("rgba", "0000", "0000");
    Test("rgba", "1111", "1111");

    Test("rrrr", "rgba", "rrrr");
    Test("gggg", "rgba", "gggg");
    Test("bbbb", "rgba", "bbbb");
    Test("aaaa", "rgba", "aaaa");
    Test("0000", "rgba", "0000");
    Test("1111", "rgba", "1111");

    Test("rgba", "rgba", "rgba");
    Test("rgba", "abgr", "abgr");
    Test("rrr1", "bbbb", "rrrr");

    for (Uint32 swizzle1 = 1; swizzle1 < TEXTURE_COMPONENT_SWIZZLE_COUNT; ++swizzle1)
    {
        const auto Swizzle1 = static_cast<TEXTURE_COMPONENT_SWIZZLE>(swizzle1);
        for (Uint32 swizzle2 = 1; swizzle2 < TEXTURE_COMPONENT_SWIZZLE_COUNT; ++swizzle2)
        {
            const auto              Swizzle2 = static_cast<TEXTURE_COMPONENT_SWIZZLE>(swizzle2);
            TextureComponentMapping Mapping1{Swizzle1, Swizzle1, Swizzle1, Swizzle1};
            TextureComponentMapping Mapping2{Swizzle2, Swizzle2, Swizzle2, Swizzle2};

            const auto RefSwizzle = Swizzle2 == TEXTURE_COMPONENT_SWIZZLE_ONE || Swizzle2 == TEXTURE_COMPONENT_SWIZZLE_ZERO ?
                Swizzle2 :
                Swizzle1;

            TextureComponentMapping RefMapping{
                RefSwizzle == TEXTURE_COMPONENT_SWIZZLE_R ? TEXTURE_COMPONENT_SWIZZLE_IDENTITY : RefSwizzle,
                RefSwizzle == TEXTURE_COMPONENT_SWIZZLE_G ? TEXTURE_COMPONENT_SWIZZLE_IDENTITY : RefSwizzle,
                RefSwizzle == TEXTURE_COMPONENT_SWIZZLE_B ? TEXTURE_COMPONENT_SWIZZLE_IDENTITY : RefSwizzle,
                RefSwizzle == TEXTURE_COMPONENT_SWIZZLE_A ? TEXTURE_COMPONENT_SWIZZLE_IDENTITY : RefSwizzle,
            };

            EXPECT_EQ(Mapping1 * Mapping2, RefMapping) << Mapping1 << " * " << Mapping2 << " = " << RefMapping;
        }
    }

    Test("ab01", "barg", "01ab");
    Test("ab01", "ba1g", "011b");
    Test("gba1", "barg", "a1gb");

    FastRandInt rnd{0, 0, static_cast<int>(TEXTURE_COMPONENT_SWIZZLE_COUNT - 1)};
    for (size_t i = 0; i < 2048; ++i)
    {
        TextureComponentMapping Mapping1{
            static_cast<TEXTURE_COMPONENT_SWIZZLE>(rnd()),
            static_cast<TEXTURE_COMPONENT_SWIZZLE>(rnd()),
            static_cast<TEXTURE_COMPONENT_SWIZZLE>(rnd()),
            static_cast<TEXTURE_COMPONENT_SWIZZLE>(rnd()),
        };
        TextureComponentMapping Mapping2{
            static_cast<TEXTURE_COMPONENT_SWIZZLE>(rnd()),
            static_cast<TEXTURE_COMPONENT_SWIZZLE>(rnd()),
            static_cast<TEXTURE_COMPONENT_SWIZZLE>(rnd()),
            static_cast<TEXTURE_COMPONENT_SWIZZLE>(rnd()),
        };
        TextureComponentMapping Res = Mapping1 * Mapping2;

        for (Uint32 c = 0; c < 4; ++c)
        {
            if (Mapping1[c] == TEXTURE_COMPONENT_SWIZZLE_IDENTITY)
                Mapping1[c] = static_cast<TEXTURE_COMPONENT_SWIZZLE>(c + TEXTURE_COMPONENT_SWIZZLE_R);
            if (Mapping2[c] == TEXTURE_COMPONENT_SWIZZLE_IDENTITY)
                Mapping2[c] = static_cast<TEXTURE_COMPONENT_SWIZZLE>(c + TEXTURE_COMPONENT_SWIZZLE_R);
        }

        auto CombineSwizzle = [&Mapping1](TEXTURE_COMPONENT_SWIZZLE Swizzle) {
            VERIFY_EXPR(Swizzle != TEXTURE_COMPONENT_SWIZZLE_IDENTITY);
            return (Swizzle == TEXTURE_COMPONENT_SWIZZLE_ONE || Swizzle == TEXTURE_COMPONENT_SWIZZLE_ZERO) ?
                Swizzle :
                Mapping1[Swizzle - TEXTURE_COMPONENT_SWIZZLE_R];
        };
        TextureComponentMapping Ref{
            CombineSwizzle(Mapping2.R),
            CombineSwizzle(Mapping2.G),
            CombineSwizzle(Mapping2.B),
            CombineSwizzle(Mapping2.A),
        };

        EXPECT_EQ(Res, Ref) << Mapping1 << " * " << Mapping2;
    }
}

} // namespace
