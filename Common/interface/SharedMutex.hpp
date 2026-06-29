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

/// \file
/// Defines shared mutex type.

#include <mutex>
#include <shared_mutex>

namespace Threading
{

#if defined(__MINGW32__) || defined(__MINGW64__)

// MinGW libstdc++ std::shared_mutex has been observed to not serialize
// std::unique_lock<std::shared_mutex> writers reliably. Use a regular mutex
// for correctness; shared/read locking degrades to exclusive locking.
// See https://sourceforge.net/p/mingw-w64/bugs/1011
#    define DILIGENT_SHARED_MUTEX_USES_EXCLUSIVE_LOCKING 1

inline constexpr bool SharedMutexSupportsConcurrentReaders = false;

class SharedMutex
{
public:
    SharedMutex() = default;

    // clang-format off
    SharedMutex           (const SharedMutex&) = delete;
    SharedMutex& operator=(const SharedMutex&) = delete;
    // clang-format on

    void lock()
    {
        m_Mutex.lock();
    }

    bool try_lock()
    {
        return m_Mutex.try_lock();
    }

    void unlock()
    {
        m_Mutex.unlock();
    }

    void lock_shared()
    {
        m_Mutex.lock();
    }

    bool try_lock_shared()
    {
        return m_Mutex.try_lock();
    }

    void unlock_shared()
    {
        m_Mutex.unlock();
    }

private:
    std::mutex m_Mutex;
};


#else

inline constexpr bool SharedMutexSupportsConcurrentReaders = true;

using SharedMutex = std::shared_mutex;

#endif

} // namespace Threading
