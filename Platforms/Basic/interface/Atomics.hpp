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

#include <atomic>

namespace Diligent
{

/// Atomically updates Val to Candidate if Candidate is greater than the current value of Val.
/// Returns the value observed before the update (which may be greater than or equal to Candidate).
template <typename T>
T AtomicMax(std::atomic<T>&   Val,
            T                 Candidate,
            std::memory_order Success = std::memory_order_seq_cst,
            std::memory_order Failure = std::memory_order_relaxed)
{
    static_assert(std::atomic<T>::is_always_lock_free,
                  "AtomicMax requires a lock-free atomic type for performance.");
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_pointer_v<T>,
                  "AtomicMax only supports arithmetic or pointer types.");

    T Cur = Val.load(std::memory_order_relaxed);
    while (Cur < Candidate && !Val.compare_exchange_weak(Cur, Candidate, Success, Failure))
    {
        // Cur is updated to the latest value by compare_exchange_weak on failure
    }
    return Cur; // the value observed before we "won" or before it was already >= candidate
}

/// Atomically updates Val to Candidate if Candidate is less than the current value of Val.
/// Returns the value observed before the update (which may be less than or equal to Candidate).
template <typename T>
T AtomicMin(std::atomic<T>&   Val,
            T                 Candidate,
            std::memory_order Success = std::memory_order_seq_cst,
            std::memory_order Failure = std::memory_order_relaxed)
{
    static_assert(std::atomic<T>::is_always_lock_free,
                  "AtomicMax requires a lock-free atomic type for performance.");
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_pointer_v<T>,
                  "AtomicMax only supports arithmetic or pointer types.");

    T Cur = Val.load(std::memory_order_relaxed);
    while (Cur > Candidate && !Val.compare_exchange_weak(Cur, Candidate, Success, Failure))
    {
        // Cur is updated to the latest value by compare_exchange_weak on failure
    }
    return Cur; // the value observed before we "won" or before it was already <= candidate
}

} // namespace Diligent
