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

#if DILIGENT_SSE_SUPPORTED

namespace Diligent
{

namespace BasicMathDetail
{

inline __m128 MultiplyMatrixRowSSE(const __m128 aRow,
                                   const __m128 bRow0,
                                   const __m128 bRow1,
                                   const __m128 bRow2,
                                   const __m128 bRow3)
{
    __m128 Result = _mm_mul_ps(_mm_shuffle_ps(aRow, aRow, _MM_SHUFFLE(0, 0, 0, 0)), bRow0);
    Result        = _mm_add_ps(Result, _mm_mul_ps(_mm_shuffle_ps(aRow, aRow, _MM_SHUFFLE(1, 1, 1, 1)), bRow1));
    Result        = _mm_add_ps(Result, _mm_mul_ps(_mm_shuffle_ps(aRow, aRow, _MM_SHUFFLE(2, 2, 2, 2)), bRow2));
    Result        = _mm_add_ps(Result, _mm_mul_ps(_mm_shuffle_ps(aRow, aRow, _MM_SHUFFLE(3, 3, 3, 3)), bRow3));
    return Result;
}

inline void MultiplyMatrix4x4SSE(const float* const A, const float* const B, float* const Result)
{
    const __m128 bRow0 = _mm_loadu_ps(B + 0);
    const __m128 bRow1 = _mm_loadu_ps(B + 4);
    const __m128 bRow2 = _mm_loadu_ps(B + 8);
    const __m128 bRow3 = _mm_loadu_ps(B + 12);

    const __m128 aRow0 = _mm_loadu_ps(A + 0);
    const __m128 aRow1 = _mm_loadu_ps(A + 4);
    const __m128 aRow2 = _mm_loadu_ps(A + 8);
    const __m128 aRow3 = _mm_loadu_ps(A + 12);

    _mm_storeu_ps(Result + 0, MultiplyMatrixRowSSE(aRow0, bRow0, bRow1, bRow2, bRow3));
    _mm_storeu_ps(Result + 4, MultiplyMatrixRowSSE(aRow1, bRow0, bRow1, bRow2, bRow3));
    _mm_storeu_ps(Result + 8, MultiplyMatrixRowSSE(aRow2, bRow0, bRow1, bRow2, bRow3));
    _mm_storeu_ps(Result + 12, MultiplyMatrixRowSSE(aRow3, bRow0, bRow1, bRow2, bRow3));
}

} // namespace BasicMathDetail

} // namespace Diligent

#endif // DILIGENT_SSE_SUPPORTED
