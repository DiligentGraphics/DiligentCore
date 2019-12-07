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

#include "GraphicsAccessories.h"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

TEST(GraphicsAccessories_GraphicsAccessories, GetFilterTypeLiteralName)
{
#define TEST_FILTER_TYPE_ENUM(ENUM_VAL, ShortName)                          \
    {                                                                       \
        EXPECT_STREQ(GetFilterTypeLiteralName(ENUM_VAL, true), #ENUM_VAL);  \
        EXPECT_STREQ(GetFilterTypeLiteralName(ENUM_VAL, false), ShortName); \
    }

    // clang-format off
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_UNKNOWN,                "unknown");
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_POINT,                  "point");
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_LINEAR,                 "linear");
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_ANISOTROPIC,            "anisotropic");
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_COMPARISON_POINT,       "comparison point");
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_COMPARISON_LINEAR,      "comparison linear");
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_COMPARISON_ANISOTROPIC, "comparison anisotropic");
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_MINIMUM_POINT,          "minimum point");
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_MINIMUM_LINEAR,         "minimum linear");
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_MINIMUM_ANISOTROPIC,    "minimum anisotropic");
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_MAXIMUM_POINT,          "maximum point");
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_MAXIMUM_LINEAR,         "maximum linear");
    TEST_FILTER_TYPE_ENUM(FILTER_TYPE_MAXIMUM_ANISOTROPIC,    "maximum anisotropic");
    // clang-format on
#undef TEST_FILTER_TYPE_ENUM
}

TEST(GraphicsAccessories_GraphicsAccessories, GetTextureAddressModeLiteralName)
{
#define TEST_TEX_ADDRESS_MODE_ENUM(ENUM_VAL, ShortName)                             \
    {                                                                               \
        EXPECT_STREQ(GetTextureAddressModeLiteralName(ENUM_VAL, true), #ENUM_VAL);  \
        EXPECT_STREQ(GetTextureAddressModeLiteralName(ENUM_VAL, false), ShortName); \
    }

    // clang-format off
    TEST_TEX_ADDRESS_MODE_ENUM(TEXTURE_ADDRESS_UNKNOWN,     "unknown");
    TEST_TEX_ADDRESS_MODE_ENUM(TEXTURE_ADDRESS_WRAP,        "wrap");
    TEST_TEX_ADDRESS_MODE_ENUM(TEXTURE_ADDRESS_MIRROR,      "mirror");
    TEST_TEX_ADDRESS_MODE_ENUM(TEXTURE_ADDRESS_CLAMP,       "clamp");
    TEST_TEX_ADDRESS_MODE_ENUM(TEXTURE_ADDRESS_BORDER,      "border");
    TEST_TEX_ADDRESS_MODE_ENUM(TEXTURE_ADDRESS_MIRROR_ONCE, "mirror once");
    // clang-format on
#undef TEST_TEX_ADDRESS_MODE_ENUM
}

TEST(GraphicsAccessories_GraphicsAccessories, GetComparisonFunctionLiteralName)
{
#define TEST_COMPARISON_FUNC_ENUM(ENUM_VAL, ShortName)                              \
    {                                                                               \
        EXPECT_STREQ(GetComparisonFunctionLiteralName(ENUM_VAL, true), #ENUM_VAL);  \
        EXPECT_STREQ(GetComparisonFunctionLiteralName(ENUM_VAL, false), ShortName); \
    }

    // clang-format off
    TEST_COMPARISON_FUNC_ENUM(COMPARISON_FUNC_UNKNOWN,       "unknown");
    TEST_COMPARISON_FUNC_ENUM(COMPARISON_FUNC_NEVER,         "never");
    TEST_COMPARISON_FUNC_ENUM(COMPARISON_FUNC_LESS,          "less");
    TEST_COMPARISON_FUNC_ENUM(COMPARISON_FUNC_EQUAL,         "equal");
    TEST_COMPARISON_FUNC_ENUM(COMPARISON_FUNC_LESS_EQUAL,    "less equal");
    TEST_COMPARISON_FUNC_ENUM(COMPARISON_FUNC_GREATER,       "greater");
    TEST_COMPARISON_FUNC_ENUM(COMPARISON_FUNC_NOT_EQUAL,     "not equal");
    TEST_COMPARISON_FUNC_ENUM(COMPARISON_FUNC_GREATER_EQUAL, "greater equal");
    TEST_COMPARISON_FUNC_ENUM(COMPARISON_FUNC_ALWAYS,        "always");
    // clang-format on
#undef TEST_TEX_ADDRESS_MODE_ENUM
}

TEST(GraphicsAccessories_GraphicsAccessories, GetBlendFactorLiteralName)
{
#define TEST_BLEND_FACTOR_ENUM(ENUM_VAL)                              \
    {                                                                 \
        EXPECT_STREQ(GetBlendFactorLiteralName(ENUM_VAL), #ENUM_VAL); \
    }

    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_UNDEFINED);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_ZERO);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_ONE);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_SRC_COLOR);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_INV_SRC_COLOR);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_SRC_ALPHA);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_INV_SRC_ALPHA);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_DEST_ALPHA);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_INV_DEST_ALPHA);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_DEST_COLOR);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_INV_DEST_COLOR);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_SRC_ALPHA_SAT);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_BLEND_FACTOR);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_INV_BLEND_FACTOR);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_SRC1_COLOR);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_INV_SRC1_COLOR);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_SRC1_ALPHA);
    TEST_BLEND_FACTOR_ENUM(BLEND_FACTOR_INV_SRC1_ALPHA);
#undef TEST_TEX_ADDRESS_MODE_ENUM
}

TEST(GraphicsAccessories_GraphicsAccessories, GetBlendOperationLiteralName)
{
#define TEST_BLEND_OP_ENUM(ENUM_VAL)                                     \
    {                                                                    \
        EXPECT_STREQ(GetBlendOperationLiteralName(ENUM_VAL), #ENUM_VAL); \
    }

    TEST_BLEND_OP_ENUM(BLEND_OPERATION_UNDEFINED);
    TEST_BLEND_OP_ENUM(BLEND_OPERATION_ADD);
    TEST_BLEND_OP_ENUM(BLEND_OPERATION_SUBTRACT);
    TEST_BLEND_OP_ENUM(BLEND_OPERATION_REV_SUBTRACT);
    TEST_BLEND_OP_ENUM(BLEND_OPERATION_MIN);
    TEST_BLEND_OP_ENUM(BLEND_OPERATION_MAX);
#undef TEST_BLEND_OP_ENUM
}

} // namespace
