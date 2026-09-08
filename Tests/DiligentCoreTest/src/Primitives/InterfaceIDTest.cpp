/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include "InterfaceID.h"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

const INTERFACE_ID Ids[] = {
    {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 1}},
    {0, 0, 0, {0, 0, 0, 0, 0, 0, 1, 0}},
    {0, 0, 0, {0, 0, 0, 0, 0, 1, 0, 0}},
    {0, 0, 0, {0, 0, 0, 0, 1, 0, 0, 0}},
    {0, 0, 0, {0, 0, 0, 1, 0, 0, 0, 0}},
    {0, 0, 0, {0, 0, 1, 0, 0, 0, 0, 0}},
    {0, 0, 0, {0, 1, 0, 0, 0, 0, 0, 0}},
    {0, 0, 0, {1, 0, 0, 0, 0, 0, 0, 0}},
    {0, 0, 1, {0, 0, 0, 0, 0, 0, 0, 0}},
    {0, 1, 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {1, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}},
};

constexpr size_t IdCount = sizeof(Ids) / sizeof(Ids[0]);

TEST(Primitives_InterfaceID, SelfComparison)
{
    for (const INTERFACE_ID& Id : Ids)
    {
        EXPECT_TRUE(Id == Id);
        EXPECT_FALSE(Id != Id);
        EXPECT_FALSE(Id < Id);
    }
}

TEST(Primitives_InterfaceID, Equal)
{
    const INTERFACE_ID SameValue = Ids[0];
    EXPECT_TRUE(Ids[0] == SameValue);

    for (size_t Index = 1; Index < IdCount; ++Index)
    {
        EXPECT_FALSE(Ids[0] == Ids[Index]);
        EXPECT_FALSE(Ids[Index] == Ids[0]);
    }
}

TEST(Primitives_InterfaceID, NotEqual)
{
    const INTERFACE_ID SameValue = Ids[0];
    EXPECT_FALSE(Ids[0] != SameValue);

    for (size_t Index = 1; Index < IdCount; ++Index)
    {
        EXPECT_TRUE(Ids[0] != Ids[Index]);
        EXPECT_TRUE(Ids[Index] != Ids[0]);
    }
}

TEST(Primitives_InterfaceID, Less)
{
    for (size_t Lhs = 0; Lhs < IdCount; ++Lhs)
    {
        for (size_t Rhs = 0; Rhs < IdCount; ++Rhs)
        {
            EXPECT_EQ(Ids[Lhs] < Ids[Rhs], Lhs < Rhs);
        }
    }
}

} // namespace
