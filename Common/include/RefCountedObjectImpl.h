/*     Copyright 2015-2016 Egor Yusov
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

/// \file
/// Implementation of the template base class for reference counting objects

#include "Object.h"
#include "Atomics.h"
#include "DebugUtilities.h"
#include "LockHelper.h"
#include "ValidatedCast.h"
#include "MemoryAllocator.h"

namespace Diligent
{

class IMemoryAllocator;
/// Base class for reference counting objects
template<typename Base, typename TObjectAllocator = IMemoryAllocator>
class RefCountedObject : public Base
{
public:
    RefCountedObject(IObject *pOwner = nullptr, TObjectAllocator *pObjAllocator = nullptr) : 
        m_pRefCounters(nullptr),
        m_pAllocator(pObjAllocator)
    {
        if( pOwner )
        {
            auto *pRefCounters = pOwner->GetReferenceCounters();
            VERIFY(pRefCounters, "Reference counters are not initialized in the owner object");
            m_pRefCounters = ValidatedCast<IReferenceCounters>( pRefCounters );
        }
        else
        {
            m_pRefCounters = RefCountersImpl::Create(this);
        }
    };

    virtual ~RefCountedObject()
    {
        // m_pRefCounters is set to null before executing delete this.
        //
        // WARNING! If m_pRefCounters was not set to null, it still may be expired in scenarios like this:
        //
        //    A ==sp==> B ---wp---> A
        //    
        //    RefCounters_A.ReleaseStrongRef(){ // NumStrongRef == 0, NumWeakRef == 1
        //      RefCounters_A.m_pObject = nullptr;
        //      bDestroyThis = (m_lNumWeakReferences == 0) == false;
        //      delete A{
        //        A.~dtor(){
        //            B.~dtor(){
        //                wpA.ReleaseWeakRef(){ // NumStrongRef == 0, NumWeakRef == 0, m_pObject==nullptr
        //                    delete RefCounters_A;
        //        ...
        //        VERIFY( m_pRefCounters->GetNumStrongRefs() == 0 // Access violation!  

        // This also may happen if one thread is executing ReleaseStrongRef(), while
        // another one is simultaneously running ReleaseWeakRef().

        //VERIFY( m_pRefCounters->GetNumStrongRefs() == 0,
        //        "There remain strong references to the object being destroyed" );
    };

    virtual IReferenceCounters* GetReferenceCounters()const override final
    {
        return m_pRefCounters;
    }

    virtual Atomics::Long AddRef()override
    {
        return m_pRefCounters->AddStrongRef();
    }

    virtual Atomics::Long Release()override
    {
        return m_pRefCounters->ReleaseStrongRef();
    }

    void* operator new(size_t Size)
    {
        return new Uint8[Size];
    }

    void operator delete(void *ptr)
    {
        delete[] reinterpret_cast<Uint8*>(ptr);
    }

    void* operator new(size_t Size, TObjectAllocator &Allocator, const Char* dbgDescription, const char* dbgFileName, const  Int32 dbgLineNumber)
    {
        return Allocator.Allocate(Size, dbgDescription, dbgFileName, dbgLineNumber);
    }

    void operator delete(void *ptr, TObjectAllocator &Allocator, const Char* dbgDescription, const char* dbgFileName, const  Int32 dbgLineNumber)
    {
        return Allocator.Free(ptr);
    }

private:

    class RefCountersImpl : public IReferenceCounters
    {
    public:
        static RefCountersImpl* Create( RefCountedObject *pOwner )
        {
            return new RefCountersImpl( pOwner );
        }

        virtual Atomics::Long AddStrongRef()override final
        {
            VERIFY( m_pObject, "Attempting to increment strong reference counter for a destroyed object!" );
            return Atomics::AtomicIncrement(m_lNumStrongReferences);
        }

        virtual Atomics::Long ReleaseStrongRef()override final
        {
            // Decrement strong reference counter without acquiring the lock. 
            auto RefCount = Atomics::AtomicDecrement(m_lNumStrongReferences);
            VERIFY( RefCount >= 0, "Inconsistent call to ReleaseStrongRef()" );
            if( RefCount == 0 )
            {
                // Since RefCount==0, there are no more strong references and the only place 
                // where strong ref counter can be incremented is from GetObject().

                // There is a serious risk: if several threads get to this point,
                // then <this> may already be destroyed and m_LockFlag expired. 
                // Consider the following scenario:
                //                                      |
                //             This thread              |             Another thread
                //                                      |
                //                      m_lNumStrongReferences == 1
                //                      m_lNumWeakReferences == 1
                //                                      |  
                // 1. Decrement m_lNumStrongReferences  |   
                //    Read RefCount==0, no lock acquired|      
                //                                      |   1. Run GetObject()
                //                                      |      - acquire the lock
                //                                      |      - increment m_lNumStrongReferences
                //                                      |      - release the lock
                //                                      |   
                //                                      |   2. Run ReleaseWeakRef()
                //                                      |      - decrement m_lNumWeakReferences
                //                                      |  
                //                                      |   3. Run ReleaseStrongRef()
                //                                      |      - decrement m_lNumStrongReferences
                //                                      |      - read RefCount==0
                //
                //         Both threads will get to this point. The first one will destroy <this>
                //         The second one will read expired m_LockFlag

                //  IT IS CRUCIALLY IMPORTANT TO ASSURE THAT ONLY ONE THREAD WILL EVER
                //  EXECUTE THIS CODE

                // The sloution is to atomically increment strong ref counter in GetObject().
                // There are two possible scenarios depending on who first increments the counter:


                //                                                     Scenario I
                //
                //             This thread              |      Another thread - GetObject()         |   One more thread - GetObject()
                //                                      |                                           |
                //                       m_lNumStrongReferences == 1                                |
                //                                      |                                           |
                //                                      |   1. Acquire the lock                     |
                // 1. Decrement m_lNumStrongReferences  |                                           |   1. Wait for the lock
                // 2. Read RefCount==0                  |   2. Increment m_lNumStrongReferences     |
                // 3. Start destroying the object       |   3. Read StrongRefCnt == 1               |
                // 4. Wait for the lock                 |   4. DO NOT return the reference          |
                //                                      |      to the object                        |
                //                                      |   5. Decrement m_lNumStrongReferences     |
                // _  _  _  _  _  _  _  _  _  _  _  _  _|   6. Release the lock _  _  _  _  _  _  _ |_  _  _  _  _  _  _  _  _  _  _  _  _  _
                //                                      |                                           |   2. Acquire the lock  
                //                                      |                                           |   3. Increment m_lNumStrongReferences
                //                                      |                                           |   4. Read StrongRefCnt == 1
                //                                      |                                           |   5. DO NOT return the reference 
                //                                      |                                           |      to the object
                //                                      |                                           |   6. Decrement m_lNumStrongReferences         
                //  _  _  _  _  _  _  _  _  _  _  _  _  | _  _  _  _  _  _  _  _  _  _  _  _  _  _  | _ 7. Release the lock _  _  _  _  _  _
                // 5. Acquire the lock                  |                                           |
                //   - m_lNumStrongReferences==0        |                                           |
                // 6. DESTROY the object                |                                           |
                //                                      |                                           |

                //  GetObject() MUST BE SERIALIZED for this to work properly!


                //                                   Scenario II
                //
                //             This thread              |      Another thread - GetObject()
                //                                      |
                //                       m_lNumStrongReferences == 1
                //                                      |
                //                                      |   1. Acquire the lock  
                //                                      |   2. Increment m_lNumStrongReferences
                // 1. Decrement m_lNumStrongReferences  |
                // 2. Read RefCount>0                   |   
                // 3. DO NOT destroy the object         |   3. Read StrongRefCnt > 1 (while m_lNumStrongReferences == 1)
                //                                      |   4. Return the reference to the object
                //                                      |       - Increment m_lNumStrongReferences
                //                                      |   5. Decrement m_lNumStrongReferences

#ifdef _DEBUG
                Atomics::Long NumStrongRefs = m_lNumStrongReferences;
                VERIFY( NumStrongRefs == 0 || NumStrongRefs == 1, "Num strong references (", NumStrongRefs, ") is expected to be 0 or 1" );
#endif

                // Acquire the lock.
                ThreadingTools::LockHelper Lock(m_LockFlag);

                // GetObject() first acquires the lock, and only then increments and 
                // decrements the ref counter. If it reads 1 after incremeting the counter,
                // it does not return the reference to the object and decrements the counter.
                // If we acquired the lock, GetObject() will not start until we are done
                VERIFY_EXPR( m_lNumStrongReferences == 0 && m_pObject != nullptr )
                
                // Extra caution
                if(m_lNumStrongReferences == 0 && m_pObject != nullptr)
                {
                    // We cannot destroy the object while reference counters are locked as this will  
                    // cause a deadlock in cases like this:
                    //
                    //    A ==sp==> B ---wp---> A
                    //    
                    //    RefCounters_A.Lock();
                    //    delete A{
                    //      A.~dtor(){
                    //          B.~dtor(){
                    //              wpA.ReleaseWeakRef(){
                    //                  RefCounters_A.Lock(); // Deadlock
                    //

                    // So we store the pointer to the object and destory it after unlocking the
                    // reference counters
                    auto *pObj = m_pObject;
                
                    // In a multithreaded environment, reference counters object may 
                    // be destroyed at any time while m_pObject->~dtor() is running.
                    // NOTE: m_pObject may not be the only object referencing m_pRefCounters.
                    //       All objects that are owned by m_pObject will point to the same 
                    //       reference counters object.
                    m_pObject->m_pRefCounters = nullptr;

                    // Note that this is the only place where m_pObject member is modified
                    // after the ref counters object has been created
                    m_pObject = nullptr;
                    // The object is now detached from the reference counters and it is if
                    // it was destroyed since no one can obtain access to it.


                    // It is essentially important to check the number of weak references
                    // while the object is locked. Otherwise reference counters object
                    // may be destroyed twice if ReleaseWeakRef() is executed by other thread:
                    //
                    //             This thread             |    Another thread - ReleaseWeakRef()
                    //                                     |           
                    // 1. Decrement m_lNumStrongReferences,|   
                    //    m_lNumStrongReferences==0,       |   
                    //    acquire the lock, destroy        |
                    //    the obj, release the lock        |
                    //    m_lNumWeakReferences == 1        | 
                    //                                     |   1. Aacquire the lock, 
                    //                                     |      decrement m_lNumWeakReferences,
                    //                                     |      m_lNumWeakReferences == 0, m_pObject == nullptr
                    //                                     |
                    // 2. Read m_lNumWeakReferences == 0   |
                    // 3. Destroy the ref counters obj     |   2. Destroy the ref counters obj
                    //
                    bool bDestroyThis = m_lNumWeakReferences == 0;
                    // ReleaseWeakRef() decrements m_lNumWeakReferences, and checks it for
                    // null only after acquiring the lock. So if m_lNumWeakReferences==0, no 
                    // weak reference-related code may be running


                    // We must explicitly unlock the object now to avoid deadlocks. Also, 
                    // if this is deleted, this->m_LockFlag will expire, which will cause 
                    // Lock.~LockHelper() to crash
                    Lock.Unlock();

                    // Destroy referenced object
                    //m_Delete(pObj);
                    if (pObj->m_pAllocator)
                    {
                        auto *pAllocator = pObj->m_pAllocator;
                        pObj->~RefCountedObject();
                        pAllocator->Free(pObj);
                    }
                    else
                    {
                        delete pObj;
                    }

                    // Note that <this> may be destroyed here already, 
                    // see comments in ~RefCountedObject()
                    if( bDestroyThis )
                        delete this;
                }
            }

            return RefCount;
        }

        virtual Atomics::Long AddWeakRef()override final
        {
            return Atomics::AtomicIncrement(m_lNumWeakReferences);
        }

        virtual Atomics::Long ReleaseWeakRef()override final
        {
            // All access to m_pObject must be atomic!
            ThreadingTools::LockHelper Lock(m_LockFlag);
            // It is essentially important to check the number of weak references
            // while the object is locked. Otherwise reference counters object
            // may be destroyed twice if ReleaseStrongRef() is executed by other 
            // thread.
            auto NumWeakReferences = Atomics::AtomicDecrement(m_lNumWeakReferences);
            VERIFY( NumWeakReferences >= 0, "Inconsistent call to ReleaseWeakRef()" );

            // There is one special case when we must not destroy the ref counters object even
            // when NumWeakReferences == 0 && m_lNumStrongReferences == 0 :
            //
            //             This thread             |    Another thread - ReleaseStrongRef()
            //                                     |
            // 1. Lock the object                  |
            //                                     |           
            // 2. Decrement m_lNumWeakReferences,  |   1. Decrement m_lNumStrongReferences,
            //    m_lNumWeakReferences==0          |      RefCount == 0
            //                                     |
            //                                     |   2. Start waiting for the lock to destroy
            //                                     |      the object, m_pObject != nullptr
            // 3. Do not destroy reference         |
            //    counters, unlock                 |
            //                                     |   3. Acquire the lock, 
            //                                     |      destroy the object, 
            //                                     |      read m_lNumWeakReferences==0 
            //                                     |      destroy the reference counters
            //
            if( NumWeakReferences == 0 && /*m_lNumStrongReferences == 0 &&*/ m_pObject == nullptr )
            {
                // m_pObject is set to null atomically. If it is not null, ReleaseStrongRef()
                // will take care of it. 
                // Access to m_pObject and decrementing m_lNumWeakReferences is atomic. Since we acquired the lock, 
                // no other thread can change either of them. 
                // Access to m_lNumStrongReferences is NOT PROTECTED by lock.

                // There are no more references to the ref counters object and the object itself
                // is already destroyed.
                // We can safely unlock it and destroy.
                // If we do not unlock it, this->m_LockFlag will expire, 
                // which will cause Lock.~LockHelper() to crash.
                Lock.Unlock();
                delete this;
            }
            return NumWeakReferences;
        }

        virtual void GetObject( class IObject **ppObject )override final
        {
            if( m_pObject == nullptr)
                return; // Early exit

            // It is essential to INCREMENT REF COUNTER while object IS LOCKED to make sure that 
            // StrongRefCnt > 1 guarantees that the object is alive.

            // If other thread started deleting the object in ReleaseStrongRef(), then m_lNumStrongReferences==0
            // We must make sure only one thread is allowed to increment the counter to guarantee that if StrongRefCnt > 1,
            // there is at least one real strong reference left. Otherwise the following scenario may occur:
            //
            //                                      m_lNumStrongReferences == 1
            //                                                                              
            //    Thread 1 - ReleaseStrongRef()    |     Thread 2 - GetObject()        |     Thread 3 - GetObject()
            //                                     |                                   |
            //  - Decrement m_lNumStrongReferences | -Increment m_lNumStrongReferences | -Increment m_lNumStrongReferences
            //  - Read RefCount == 0               | -Read StrongRefCnt==1             | -Read StrongRefCnt==2 
            //    Destroy the object               |                                   | -Return reference to the soon
            //                                     |                                   |  to expire object
            //     
            ThreadingTools::LockHelper Lock(m_LockFlag);

            auto StrongRefCnt = Atomics::AtomicIncrement(m_lNumStrongReferences);
            
            // Checking if m_pObject != nullptr is not reliable:
            //
            //           This thread                    |       Another thread - 
            //                                          |                                         
            //   1. Acquire the lock                    |                                         
            //                                          |    1. Decrement m_lNumStrongReferences  
            //   2. Increment m_lNumStrongReferences    |    2. Test RefCount==0                  
            //   3. Read StrongRefCnt == 1              |    3. Start destroying the object       
            //      m_pObject != nullptr                |
            //   4. DO NOT return the reference to      |    4. Wait for the lock, m_pObject != nullptr                 
            //      the object                          |
            //   5. Decrement m_lNumStrongReferences    |
            //                                          |    5. Destroy the object

            if( m_pObject && StrongRefCnt > 1 )
            {
                // QueryInterface() must not lock the object, or a deadlock happens.
                // The only other two methods that lock the object are ReleaseStrongRef()
                // and ReleaseWeakRef(), which are never called by QueryInterface()
                m_pObject->QueryInterface(Diligent::IID_Unknown, ppObject);
            }
            Atomics::AtomicDecrement(m_lNumStrongReferences);
        }

        virtual Atomics::Long GetNumStrongRefs()const override final
        {
            return m_lNumStrongReferences;
        }

        virtual Atomics::Long GetNumWeakRefs()const override final
        {
            return m_lNumWeakReferences;
        }

    private:
        RefCountersImpl(RefCountedObject *pOwner) : 
            m_pObject(pOwner)
        {
            VERIFY(m_pObject, "Owner must not be null");
            m_lNumStrongReferences = 0;
            m_lNumWeakReferences = 0;
        }

        ~RefCountersImpl()
        {
            VERIFY( m_lNumStrongReferences == 0 && m_lNumWeakReferences == 0,
                    "There exist outstanding references to the object being destroyed" );
        }

        // No copies/moves
        RefCountersImpl(const RefCountersImpl&) = delete;
        RefCountersImpl(RefCountersImpl&&) = delete;
        RefCountersImpl& operator = (const RefCountersImpl&) = delete;
        RefCountersImpl& operator = (RefCountersImpl&&) = delete;

        // It is crucially important that the type of the pointer
        // is RefCountedObject and not IObject, since the latter
        // does not have virtual dtor.
        RefCountedObject *m_pObject;
        Atomics::AtomicLong m_lNumStrongReferences;
        Atomics::AtomicLong m_lNumWeakReferences;
        ThreadingTools::LockFlag m_LockFlag;
    };
    
    // Reference counters pointer cannot be of type RefCountersImpl*, 
    // because when an owner pointer is provided to RefCountedObject(), 
    // the type of pOwner->GetReferenceCounters() may not be convertible
    // to RefCountedObject<Base>::RefCountersImpl*.
    IReferenceCounters *m_pRefCounters;
    TObjectAllocator *m_pAllocator;
};

}
