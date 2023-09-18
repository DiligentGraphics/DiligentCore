/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

#include <array>
#include <algorithm>
#include "ColorConversion.h"

namespace Diligent
{

namespace
{

class LinearToGammaMap
{
public:
    LinearToGammaMap() noexcept
    {
        for (Uint32 i = 0; i < m_ToGamma.size(); ++i)
        {
            m_ToGamma[i] = LinearToGamma(static_cast<float>(i) / 255.f);
        }
    }

    float operator[](Uint8 x) const
    {
        return m_ToGamma[x];
    }

private:
    std::array<float, 256> m_ToGamma;
};

class GammaToLinearMap
{
public:
    GammaToLinearMap() noexcept
    {
        for (Uint32 i = 0; i < m_ToLinear.size(); ++i)
        {
            m_ToLinear[i] = GammaToLinear(static_cast<float>(i) / 255.f);
        }
    }

    float operator[](Uint8 x) const
    {
        return m_ToLinear[x];
    }

private:
    std::array<float, 256> m_ToLinear;
};

} // namespace

float LinearToGamma(Uint8 x)
{
    static const LinearToGammaMap map;
    return map[x];
}

float GammaToLinear(Uint8 x)
{
    static const GammaToLinearMap map;
    return map[x];
}

} // namespace Diligent
