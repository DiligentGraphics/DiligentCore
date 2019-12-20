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

#include "StringTools.h"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

TEST(Common_StringTools, StreqSuff)
{
    EXPECT_TRUE(StreqSuff("abc_def", "abc", "_def"));
    EXPECT_TRUE(!StreqSuff("abc", "abc", "_def"));
    EXPECT_TRUE(!StreqSuff("ab", "abc", "_def"));
    EXPECT_TRUE(!StreqSuff("abc_de", "abc", "_def"));
    EXPECT_TRUE(!StreqSuff("abc_def", "ab", "_def"));
    EXPECT_TRUE(!StreqSuff("abc_def", "abd", "_def"));
    EXPECT_TRUE(!StreqSuff("abc_def", "abc", "_de"));
    EXPECT_TRUE(!StreqSuff("abc", "abc", "_def"));
    EXPECT_TRUE(!StreqSuff("abc_def", "", "_def"));
    EXPECT_TRUE(!StreqSuff("abc_def", "", ""));

    EXPECT_TRUE(StreqSuff("abc", "abc", "_def", true));
    EXPECT_TRUE(!StreqSuff("abc", "abc_", "_def", true));
    EXPECT_TRUE(!StreqSuff("abc_", "abc", "_def", true));
    EXPECT_TRUE(StreqSuff("abc", "abc", nullptr, true));
    EXPECT_TRUE(StreqSuff("abc", "abc", nullptr, false));
    EXPECT_TRUE(!StreqSuff("ab", "abc", nullptr, true));
    EXPECT_TRUE(!StreqSuff("abc", "ab", nullptr, false));
}

} // namespace
