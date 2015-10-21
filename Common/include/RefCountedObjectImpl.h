/*     Copyright 2015 Egor Yusov
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

namespace Diligent
{

/// Base class for reference counting objects
template<typename Base>
class RefCountedObject : public Base
{
public:
    RefCountedObject(IObject *pOwner = nullptr) : 
        m_pRefCounters(nullptr)
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
        // WARNING! m_pRefCounters pointer might be expired in scenarios like this:
        //
        //    A ==sp==> B ---wp---> A
        //    
        //    RefCounters_A.ReleaseStrongRef(){ // NumStrongRef == 0, NumWeakRef == 1
        //      delete A{
        //        A.~dtor(){
        //            B.~dtor(){
        //                wpA.ReleaseWeakRef(){ // NumStrongRef == 0, NumWeakRef == 0
        //                    delete RefCounters_A;
        //        ...
        //        VERIFY( m_pRefCounters->GetNumStrongRefs() == 0 // Access violation!  

        // This also may happen if one thread is executing ReleaseStrongRef(), while
        // another one is simultaneously running ReleaseWeakRef().

        //VERIFY( m_pRefCounters->GetNumStrongRefs() == 0,
        //        "There remain strong references to the object being destroyed" );
    };

    virtual IReferenceCounters* GetReferenceCounters()const override
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


private:

    class RefCountersImpl : public IReferenceCounters
    {
    public:
        static RefCountersImpl* Create( RefCountedObject *pOwner )
        {
            return new RefCountersImpl( pOwner );
        }

        virtual Atomics::Long AddStrongRef()override
        {
            VERIFY( m_pObject, "Attempting to increment strong reference counter for a destroyed object!" );
            return Atomics::AtomicIncrement(m_lNumStrongReferences);
        }

        virtual Atomics::Long ReleaseStrongRef()override
        {
            // If the object is about to be destroyed, we must assure that no other 
            // thread is accessing the ENTIRE REFERENCE COUNTERS OBJECT at the same time. 
            // (Protecting only the pointer is not sufficient!)
            // The problem may arise if a weak pointer in some other thread is trying to 
            // obtain access to the object. 
            // The safest way is to always protect the entire function:
            ThreadingTools::LockHelper Lock(m_LockFlag);

            // It is unsafe to not always lock.
            // For instance, locking if there is only one strong reference left
            // if( m_lNumStrongReferences == 1 )
            //    Lock.Lock( m_LockFlag );
            // may fail. Suppose the following scenario:
            //
            //             This thread           |             Another thread
            //                                   |  1. Start releasing another strong  
            //                                   |     reference to this object
            // 1. Read m_lNumStrongReferences==2 |  2. Read m_lNumStrongReferences==2
            //    No lock acquired               |  3. Decrement the counter,
            //                                   |     m_lNumStrongReferences==1
            // 2. Decrement the counter,         |
            //    m_lNumStrongReferences==0,     |
            //    and the object will be         |
            //    destroyed without locking      |


            // Likewise locking if there is at least one weak reference
            // if( m_lNumWeakReferences > 0 )
            //    Lock.Lock( m_LockFlag );
            // may also fail. Suppose the following scenario:
            //
            //             This thread           |             Another thread
            //                                   |  
            // 1. Read m_lNumWeakReferences==0   |   1. Start creating weak reference 
            //    No lock acquired               |      from another strong reference object
            //                                   |   2. Call AddWeakRef(),
            //                                   |      m_lNumWeakReferences==1
            //                                   |   3. Call Release() on the original strong
            //                                   |      referenceo bject, m_lNumStrongReferences==1
            //                                   |   4. Start creating another strong reference
            //                                   |      from the weak pointer, acquired lock and
            //                                   |      read m_pObject
            // 2. Destroy the object             |  
            //                                   |   5. Attempt to create strong reference from
            //                                   |      invalidated pointer

            // Both situations are unlikely to happen. However, they show that
            // conditional locking is not safe. There might be other more probable
            // situations
            auto RefCount = Atomics::AtomicDecrement(m_lNumStrongReferences);
            VERIFY( RefCount >= 0, "Inconsistent call to ReleaseStrongRef()" );
            if( RefCount == 0 )
            {
                // Locking the object here is also not safe as antoher thread
                // may be running GetObject(). If it obtains the lock first, it will 
                // get the pointer to the object which will then be destroyed by this 
                // thread
                
                VERIFY(m_pObject, "Object pointer is null, which means it has already been destroyed");
                // There are no more STRONG references to the object and it is about to be 
                // destroyed. There could be weak references, so reference counters 
                // can remain alive after the object itself is destroyed.

                // Note that since reference counters are locked, no weak pointers can access
                // m_pObject while the object is being deleted.

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

                // Note that this is the only place where m_pObject member can be modified
                // after the reference counters object has been created
                m_pObject = nullptr;
                // The object is now detached from the reference counters and it is if
                // it was destroyed since no one can obtain access to it.

                // It is essentially important to check the number of weak references
                // while the object is locked. Otherwise reference counters object
                // may be destroyed twice if ReleaseWeakRef() is executed by other thread:
                //
                //             This thread             |    Another thread - ReleaseWeakRef()
                //                                     |           
                // 1. Decrement m_lNumStrongReferences,|   1. Decrement m_lNumWeakReferences,
                //    m_lNumStrongReferences==0        |      m_lNumWeakReferences == 0
                //                                     |
                // 2. Destroy the object               |   2. Destroy the object
                //
                bool bDestroyThis = m_lNumWeakReferences == 0;

                // We must explicitly unlock the object now to avoid deadlocks. Also, 
                // if this is deleted, this->m_LockFlag will expire, which will cause 
                // Lock.~LockHelper() to crash
                Lock.Unlock();

                // Destroy referenced object
                delete pObj;

                // Note that this may be destroyed here already, 
                // see comments in ~RefCountedObject()
                if( bDestroyThis )
                    delete this;
            }

            return RefCount;
        }

        virtual Atomics::Long AddWeakRef()override
        {
            return Atomics::AtomicIncrement(m_lNumWeakReferences);
        }

        virtual Atomics::Long ReleaseWeakRef()override
        {
            ThreadingTools::LockHelper Lock(m_LockFlag);
            // It is essentially important to check the number of references
            // while the object is locked. Otherwise reference counters object
            // may be destroyed twice if ReleaseStrongRef() is executed by other 
            // thread.
            auto NumWeakReferences = Atomics::AtomicDecrement(m_lNumWeakReferences);
            VERIFY( NumWeakReferences >= 0, "Inconsistent call to ReleaseWeakRef()" );
            if( NumWeakReferences == 0 && m_lNumStrongReferences == 0 )
            {
                // There are no more references to the ref counters object.
                // We can safely unlock it and destroy.
                // If we do not unlock it, this->m_LockFlag will expire, 
                // which will cause Lock.~LockHelper() to crash.
                Lock.Unlock();
                delete this;
            }
            return NumWeakReferences;
        }

        virtual void GetObject( class IObject **ppObject )override
        {
            // We need to lock the object before accessing it to prevent
            // deletion of the referenced object in another thread.
            // The thread which is about to delete the object locks it and 
            // decrements the reference counter only after it gets exclusive access.
            // So if we obtain mutex first, we will increase the reference counter
            // before the other thread decrements it. Thus the object will not 
            // be deleted.
            ThreadingTools::LockHelper Lock(m_LockFlag);
            if( m_pObject )
            {
                // QueryInterface() must not lock the object, or a deadlock happens.
                // The only other two methods that lock the object are ReleaseStrongRef()
                // and ReleaseWeakRef(), which are never called by QueryInterface()
                m_pObject->QueryInterface(Diligent::IID_Unknown, ppObject);
            }
        }

        virtual Atomics::Long GetNumStrongRefs()const override
        {
            return m_lNumStrongReferences;
        }

        virtual Atomics::Long GetNumWeakRefs()const override
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
        RefCountersImpl(const RefCountersImpl&);
        RefCountersImpl(RefCountersImpl&&);
        RefCountersImpl& operator = (const RefCountersImpl&);
        RefCountersImpl& operator = (RefCountersImpl&&);

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
};

}
