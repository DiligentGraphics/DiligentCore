/*     Copyright 2015-2018 Egor Yusov
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

#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "Atomics.h"

namespace ThreadingTools
{

class LockFlag
{
public:
    enum {LOCK_FLAG_UNLOCKED = 0, LOCK_FLAG_LOCKED = 1};
    LockFlag(Atomics::Long InitFlag = LOCK_FLAG_UNLOCKED)
    {
        //m_Flag.store(InitFlag);
        m_Flag = InitFlag;
    }

    operator Atomics::Long()const{return m_Flag;}

private:
    friend class LockHelper;
    Atomics::AtomicLong m_Flag;
};
   
class LockHelper
{
public:
    
    LockHelper() :
        m_pLockFlag(nullptr)
    {
    }
    LockHelper(LockFlag &LockFlag) :
        m_pLockFlag(nullptr)
    {
        Lock(LockFlag);
    }

    LockHelper( LockHelper &&LockHelper ) :
        m_pLockFlag( std::move(LockHelper.m_pLockFlag) )
    {
        LockHelper.m_pLockFlag = nullptr;
    }

    const LockHelper& operator = (LockHelper &&LockHelper)
    {
        m_pLockFlag = std::move( LockHelper.m_pLockFlag );
        LockHelper.m_pLockFlag = nullptr;
        return *this;
    }

    ~LockHelper()
    {
        Unlock();
    }

    static bool UnsafeTryLock(LockFlag &LockFlag)
    {
        return Atomics::AtomicCompareExchange( LockFlag.m_Flag, 
                                               static_cast<Atomics::Long>( LockFlag::LOCK_FLAG_LOCKED ), 
                                               static_cast<Atomics::Long>( LockFlag::LOCK_FLAG_UNLOCKED) ) == LockFlag::LOCK_FLAG_UNLOCKED;
    }

    bool TryLock(LockFlag &LockFlag)
    {
        if( UnsafeTryLock( LockFlag) )
        {
            m_pLockFlag = &LockFlag;
            return true;
        }
        else 
            return false;
    }
    
    static void UnsafeLock(LockFlag &LockFlag)
    {
        while( !UnsafeTryLock( LockFlag ) )
            std::this_thread::yield();
    }

    void Lock(LockFlag &LockFlag)
    {
        VERIFY( m_pLockFlag == NULL, "Object already locked" );
        // Wait for the flag to become unlocked and lock it
        while( !TryLock( LockFlag ) )
            std::this_thread::yield();
    }

    static void UnsafeUnlock(LockFlag &LockFlag)
    {
        LockFlag.m_Flag = LockFlag::LOCK_FLAG_UNLOCKED;
    }

    void Unlock()
    {
        if( m_pLockFlag )
            UnsafeUnlock(*m_pLockFlag);
        m_pLockFlag = NULL;
    }

private:
    LockFlag *m_pLockFlag;
    LockHelper( const LockHelper &LockHelper );
    const LockHelper& operator = ( const LockHelper &LockHelper );
};

class Signal
{
public:
    Signal() 
    {
        m_SignalledValue = 0;
        m_NumThreadsAwaken = 0;
    }

    // http://en.cppreference.com/w/cpp/thread/condition_variable
    void Trigger(bool NotifyAll = false, int SignalValue = 1)
    {
        //  The thread that intends to modify the variable has to
        //  * acquire a std::mutex (typically via std::lock_guard)
        //  * perform the modification while the lock is held
        //  * execute notify_one or notify_all on the std::condition_variable (the lock does not need to be held for notification)        
        {
            // std::condition_variable works only with std::unique_lock<std::mutex>
            std::lock_guard<std::mutex> Lock(m_Mutex);
            VERIFY(SignalValue != 0, "Signal value must not be 0");
            VERIFY(m_SignalledValue == 0 && m_NumThreadsAwaken == 0, "Not all threads have been awaken since the signal was triggered last time, or the signal has not been reset");
            m_SignalledValue = SignalValue;
        }
        // Unlocking is done before notifying, to avoid waking up the waiting 
        // thread only to block again (see notify_one for details)
        if(NotifyAll)
            m_CondVar.notify_all();
        else
            m_CondVar.notify_one();
    }

    // WARNING!
    // If multiple threads are waiting for a signal in an infinite loop,
    // autoresetting the signal does not guarantee that one thread cannot 
    // go through the loop twice. In this case, every thread must wait for its 
    // own auto-reset signal or the threads must be blocked by another signal

    int Wait(bool AutoReset = false, int NumThreadsWaiting = 0)
    {
        //  Any thread that intends to wait on std::condition_variable has to
        //  * acquire a std::unique_lock<std::mutex>, on the SAME MUTEX as used to protect the shared variable
        //  * execute wait, wait_for, or wait_until. The wait operations atomically release the mutex 
        //    and suspend the execution of the thread.
        //  * When the condition variable is notified, a timeout expires, or a spurious wakeup occurs, 
        //    the thread is awakened, and the mutex is atomically reacquired: 
        //    - The thread should then check the condition and resume waiting if the wake up was spurious.
        std::unique_lock<std::mutex> Lock(m_Mutex);
        // It is safe to check m_SignalledValue since we are holding
        // the mutex
        if(m_SignalledValue == 0)
        {
            m_CondVar.wait(Lock, [&] {return m_SignalledValue != 0;} );
        }
        int SignalledValue = m_SignalledValue;
        // Count the number of threads awaken while holding the mutex
        ++m_NumThreadsAwaken;
        if (AutoReset)
        {
            VERIFY(NumThreadsWaiting > 0, "Number of waiting threads must not be 0 when auto resetting the signal");
            // Reset the signal while holding the mutex. If Trigger() is executed by another
            // thread, it will wait until we release the mutex
            if(m_NumThreadsAwaken == NumThreadsWaiting)
            {
                m_SignalledValue = 0;
                m_NumThreadsAwaken = 0;
            }
        }
        return SignalledValue;
    }

    void Reset()
    {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        m_SignalledValue = 0;
        m_NumThreadsAwaken = 0;
    }

    bool IsTriggered()const { return m_SignalledValue != 0; }

private:
    
    std::mutex m_Mutex;
    std::condition_variable m_CondVar;
    std::atomic_int m_SignalledValue;
    std::atomic_int m_NumThreadsAwaken;
    
    Signal(const Signal&) = delete;
    Signal& operator = (const Signal&) = delete;
};

}
