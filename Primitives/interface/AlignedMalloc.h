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

#include <stdlib.h>

#ifdef USE_CRT_MALLOC_DBG
#    include <crtdbg.h>
#endif

#if PLATFORM_ANDROID && __ANDROID_API__ < 28
#    define USE_ALIGNED_MALLOC_FALLBACK 1
#endif

#ifdef ALIGNED_MALLOC
#    undef ALIGNED_MALLOC
#endif
#ifdef ALIGNED_FREE
#    undef ALIGNED_FREE
#endif

#if defined(_MSC_VER) && defined(USE_CRT_MALLOC_DBG)
#    define ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber) _aligned_malloc_dbg(Size, Alignment, dbgFileName, dbgLineNumber)
#    define ALIGNED_FREE(Ptr)                                           _aligned_free(Ptr)
#elif defined(_MSC_VER) || defined(__MINGW64__) || defined(__MINGW32__)
#    define ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber) _aligned_malloc(Size, Alignment)
#    define ALIGNED_FREE(Ptr)                                           _aligned_free(Ptr)
#elif defined(USE_ALIGNED_MALLOC_FALLBACK)
#    define ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber) AllocateAlignedFallback(Size, Alignment)
#    define ALIGNED_FREE(Ptr)                                           FreeAlignedFallback(Ptr)
#else
#    define ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber) aligned_alloc(Alignment, ((Size) + (Alignment)-1) & ~((Alignment)-1))
#    define ALIGNED_FREE(Ptr)                                           free(Ptr)
#endif

namespace Diligent
{

void* AllocateAlignedFallback(size_t Size, size_t Alignment);
void  FreeAlignedFallback(void* Ptr);

} // namespace Diligent
