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

#pragma once

#include "../../Platforms/interface/Intrinsics.hpp"

#if DILIGENT_NEON_SUPPORTED

namespace Diligent
{

namespace BasicMathDetail
{

inline float32x4_t MultiplyMatrixRowNEON(const float* const ARow,
                                         const float32x4_t  bRow0,
                                         const float32x4_t  bRow1,
                                         const float32x4_t  bRow2,
                                         const float32x4_t  bRow3)
{
    float32x4_t Result = vmulq_n_f32(bRow0, ARow[0]);
    Result             = vmlaq_n_f32(Result, bRow1, ARow[1]);
    Result             = vmlaq_n_f32(Result, bRow2, ARow[2]);
    Result             = vmlaq_n_f32(Result, bRow3, ARow[3]);
    return Result;
}

inline void MultiplyMatrix4x4NEON(const float* const A, const float* const B, float* const Result)
{
    const float32x4_t bRow0 = vld1q_f32(B + 0);
    const float32x4_t bRow1 = vld1q_f32(B + 4);
    const float32x4_t bRow2 = vld1q_f32(B + 8);
    const float32x4_t bRow3 = vld1q_f32(B + 12);

    vst1q_f32(Result + 0, MultiplyMatrixRowNEON(A + 0, bRow0, bRow1, bRow2, bRow3));
    vst1q_f32(Result + 4, MultiplyMatrixRowNEON(A + 4, bRow0, bRow1, bRow2, bRow3));
    vst1q_f32(Result + 8, MultiplyMatrixRowNEON(A + 8, bRow0, bRow1, bRow2, bRow3));
    vst1q_f32(Result + 12, MultiplyMatrixRowNEON(A + 12, bRow0, bRow1, bRow2, bRow3));
}

} // namespace BasicMathDetail

} // namespace Diligent

#endif // DILIGENT_NEON_SUPPORTED
