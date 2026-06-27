/*
 *  Copyright 2019-2026 Diligent Graphics LLC
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

#pragma once

/// \file
/// Implementation of the template base class for reference counting objects

#include <stdlib.h>
#include <atomic>
#include <new>
#include <type_traits>

#include "../../Primitives/interface/Object.h"
#include "../../Primitives/interface/MemoryAllocator.h"
#include "../../Primitives/interface/AlignedMalloc.h"
#include "../../Platforms/Basic/interface/DebugUtilities.hpp"
#include "SpinLock.hpp"
#include "Cast.hpp"

namespace Diligent
{

template <typename T>
class RefCntWeakPtr;

// This class controls the lifetime of a refcounted object
// NB: RefCountersImpl can't be final, see https://github.com/DiligentGraphics/DiligentCore/issues/704.
class RefCountersImpl : public IReferenceCounters
{
    struct ObjectRecord
    {
        void* pObject    = nullptr;
        void* pAllocator = nullptr;

        void (*Destroy)(void* pObject, void* pAllocator)                             = nullptr;
        void (*Query)(void* pObject, const INTERFACE_ID& iid, IObject** ppInterface) = nullptr;

        explicit operator bool() const noexcept
        {
            return pObject != nullptr && Destroy != nullptr && Query != nullptr;
        }

        void DestroyObject() const
        {
            Destroy(pObject, pAllocator);
        }

        void QueryInterface(const INTERFACE_ID& iid, IObject** ppInterface) const
        {
            Query(pObject, iid, ppInterface);
        }
    };
    static_assert(std::is_trivially_copyable<ObjectRecord>::value, "ObjectRecord must be trivially copyable");

public:
    inline virtual ReferenceCounterValueType AddStrongRef() override final
    {
        VERIFY(m_ObjectState.load() == ObjectState::Alive, "Attempting to increment strong reference counter for a destroyed or not initialized object!");
        VERIFY(m_ObjectRecord, "Object record is not initialized");
        return m_NumStrongReferences.fetch_add(+1) + 1;
    }

    template <class TPreObjectDestroy>
    inline ReferenceCounterValueType ReleaseStrongRef(TPreObjectDestroy&& PreObjectDestroy)
    {
        static_assert(noexcept(PreObjectDestroy()), "PreObjectDestroy must be noexcept");

        VERIFY(m_ObjectState.load() == ObjectState::Alive, "Attempting to decrement strong reference counter for an object that is not alive");
        VERIFY(m_ObjectRecord, "Object record is not initialized");

        // Decrement strong reference counter without acquiring the lock.
        const ReferenceCounterValueType RefCount = m_NumStrongReferences.fetch_add(-1) - 1;
        VERIFY(RefCount >= 0, "Inconsistent call to ReleaseStrongRef()");
        if (RefCount == 0)
        {
            PreObjectDestroy();
            TryDestroyObject();
        }

        return RefCount;
    }

    inline virtual ReferenceCounterValueType ReleaseStrongRef() override final
    {
        return ReleaseStrongRef([]() noexcept {});
    }

    inline virtual ReferenceCounterValueType AddWeakRef() override final
    {
        return m_NumWeakReferences.fetch_add(+1) + 1;
    }

    inline virtual ReferenceCounterValueType ReleaseWeakRef() override final
    {
        // The method must be serialized!
        std::unique_lock<Threading::SpinLock> Guard{m_Lock};

        const ReferenceCounterValueType NumWeakReferences = m_NumWeakReferences.fetch_add(-1) - 1;
        VERIFY(NumWeakReferences >= 0, "Inconsistent call to ReleaseWeakRef()");

        // The object owns an implicit weak reference between Attach() and the end
        // of object destruction. Before Attach(), MakeNewRCObj owns the counters;
        // after object destruction, the last weak release owns deleting them.
        if (NumWeakReferences == 0 && m_ObjectState.load() == ObjectState::Destroyed)
        {
            VERIFY_EXPR(m_NumStrongReferences.load() == 0);
            VERIFY(!m_ObjectRecord, "Object record must be null");
            Guard.unlock();
            SelfDestroy();
        }
        return NumWeakReferences;
    }

    inline virtual void QueryObject(struct IObject** ppObject) override final
    {
        if (ppObject == nullptr)
            return;

        *ppObject = nullptr;

        if (m_ObjectState.load() != ObjectState::Alive)
            return; // Early exit

        // It is essential to INCREMENT REF COUNTER while HOLDING THE LOCK to make sure that
        // StrongRefCnt > 1 guarantees that the object is alive.

        // If other thread started deleting the object in ReleaseStrongRef(), then m_NumStrongReferences==0
        // We must make sure only one thread is allowed to increment the counter to guarantee that if StrongRefCnt > 1,
        // there is at least one real strong reference left. Otherwise the following scenario may occur:
        //
        //                                      m_NumStrongReferences == 1
        //
        //    Thread 1 - ReleaseStrongRef()    |    Thread 2 - QueryObject()       |    Thread 3 - QueryObject()
        //                                     |                                   |
        //  - Decrement m_NumStrongReferences  | -Increment m_NumStrongReferences  | -Increment m_NumStrongReferences
        //  - Read RefCount == 0               | -Read StrongRefCnt==1             | -Read StrongRefCnt==2
        //    Destroy the object               |                                   | -Return reference to the soon
        //                                     |                                   |  to expire object
        //
        ObjectRecord ObjRecord;

        if (TryAddStrongRefFromWeak(&ObjRecord))
        {
            struct TemporaryStrongRefGuard
            {
                RefCountersImpl& RefCounters;

                ~TemporaryStrongRefGuard()
                {
                    // QueryInterface() runs outside m_Lock, so this temporary reference
                    // may become the last strong reference if all real references are
                    // released before QueryInterface() adds a new one or if it throws.
                    RefCounters.ReleaseStrongRef();
                }
            } TempStrongRef{*this};

            // The temporary strong reference keeps the object record alive.
            // QueryInterface() is virtual object code and must run outside m_Lock.
            ObjRecord.QueryInterface(IID_Unknown, ppObject);
        }
    }

    inline virtual ReferenceCounterValueType GetNumStrongRefs() const override final
    {
        return m_NumStrongReferences.load();
    }

    inline virtual ReferenceCounterValueType GetNumWeakRefs() const override final
    {
        return m_NumWeakReferences.load();
    }

private:
    template <typename AllocatorType, typename ObjectType>
    friend class MakeNewRCObj;
    template <typename T>
    friend class RefCntWeakPtr;

    RefCountersImpl() noexcept
    {
    }

    // Attempts to obtain a strong reference while the caller only owns a weak
    // reference. This uses the same serialized speculative increment rule as
    // QueryObject(): increment first, then require the new count to be greater
    // than one while the object is still alive.
    bool TryAddStrongRefFromWeak() noexcept
    {
        return TryAddStrongRefFromWeak(nullptr);
    }

    bool TryAddStrongRefFromWeak(ObjectRecord* pObjectRecord) noexcept
    {
        Threading::SpinLockGuard Guard{m_Lock};

        const ReferenceCounterValueType StrongRefCnt = m_NumStrongReferences.fetch_add(+1) + 1;

        // Checking if m_ObjectState == ObjectState::Alive only is not reliable:
        //
        //           This thread                    |          Another thread
        //                                          |
        //   1. Acquire the lock                    |
        //                                          |    1. Decrement m_NumStrongReferences
        //   2. Increment m_NumStrongReferences     |    2. Test RefCount==0
        //   3. Read StrongRefCnt == 1              |    3. Start destroying the object
        //      m_ObjectState == ObjectState::Alive |
        //   4. DO NOT return the reference to      |    4. Wait for the lock, m_ObjectState == ObjectState::Alive
        //      the object                          |
        //   5. Decrement m_NumStrongReferences     |
        //                                          |    5. Destroy the object
        if (m_ObjectState.load() == ObjectState::Alive && StrongRefCnt > 1)
        {
            if (pObjectRecord != nullptr)
            {
                VERIFY(m_ObjectRecord, "Object record is not initialized");
                *pObjectRecord = m_ObjectRecord;
            }
            return true;
        }

        m_NumStrongReferences.fetch_add(-1);
        return false;
    }

    template <typename ObjectType, typename AllocatorType>
    static void DestroyObject(void* pObject, void* pAllocator)
    {
        ObjectType*    pTypedObject    = static_cast<ObjectType*>(pObject);
        AllocatorType* pTypedAllocator = static_cast<AllocatorType*>(pAllocator);

        if (pTypedAllocator)
        {
            pTypedObject->~ObjectType();
            if constexpr (alignof(ObjectType) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                pTypedAllocator->FreeAligned(pTypedObject);
            }
            else
            {
                pTypedAllocator->Free(pTypedObject);
            }
        }
        else
        {
            delete pTypedObject;
        }
    }

    template <typename ObjectType>
    static void QueryObjectInterface(void* pObject, const INTERFACE_ID& iid, IObject** ppInterface)
    {
        static_cast<ObjectType*>(pObject)->QueryInterface(iid, ppInterface);
    }

    template <typename ObjectType, typename AllocatorType>
    void Attach(ObjectType* pObject, AllocatorType* pAllocator)
    {
        VERIFY(m_ObjectState.load() == ObjectState::NotInitialized, "Object has already been attached");
        // It is crucially important that pObject has ObjectType, not IObject:
        // IObject does not have a virtual destructor.
        m_ObjectRecord = ObjectRecord{
            pObject,
            pAllocator,
            DestroyObject<ObjectType, AllocatorType>,
            QueryObjectInterface<ObjectType>};
        // Keep the reference counters alive until after the object destructor
        // returns. The destructor may release the last external weak reference,
        // but GetReferenceCounters() must remain valid until destruction completes.
        m_NumWeakReferences.fetch_add(+1);
        m_ObjectState.store(ObjectState::Alive);
    }

    void TryDestroyObject()
    {
        // Since RefCount==0, there are no more strong references and the only place
        // where strong ref counter can be incremented without an existing strong
        // reference is the weak-promotion path.

        // If several threads were allowed to get to this point, there would
        // be serious risk that <this> had already been destroyed and m_LockFlag expired.
        // Consider the following scenario:
        //                                      |
        //             This thread              |             Another thread
        //                                      |
        //                      m_NumStrongReferences == 1
        //                      m_NumWeakReferences == 1
        //                                      |
        // 1. Decrement m_NumStrongReferences   |
        //    Read RefCount==0, no lock acquired|
        //                                      |   1. Run weak promotion
        //                                      |      - acquire the lock
        //                                      |      - increment m_NumStrongReferences
        //                                      |      - release the lock
        //                                      |
        //                                      |   2. Run ReleaseWeakRef()
        //                                      |      - decrement m_NumWeakReferences
        //                                      |
        //                                      |   3. Run ReleaseStrongRef()
        //                                      |      - decrement m_NumStrongReferences
        //                                      |      - read RefCount==0
        //
        //         Both threads will get to this point. The first one will destroy
        //         the object. The second one will read expired m_LockFlag.

        //  IT IS CRUCIALLY IMPORTANT TO ASSURE THAT ONLY ONE THREAD WILL EVER
        //  EXECUTE THIS CODE

        // The solution is to atomically increment strong ref counter in weak promotion.
        // There are two possible scenarios depending on who first increments the counter:


        //                                                     Scenario I
        //
        //             This thread              |   Another thread - weak promotion         | One more thread - weak promotion
        //                                      |                                           |
        //                        m_NumStrongReferences == 1                                |
        //                                      |                                           |
        //                                      |   1. Acquire the lock                     |
        // 1. Decrement mlNumStrongReferences   |                                           |   1. Wait for the lock
        // 2. Read RefCount==0                  |   2. Increment m_NumStrongReferences      |
        // 3. Start destroying the object       |   3. Read StrongRefCnt == 1               |
        // 4. Wait for the lock                 |   4. DO NOT return the reference          |
        //                                      |      to the object                        |
        //                                      |   5. Decrement m_NumStrongReferences      |
        // _  _  _  _  _  _  _  _  _  _  _  _  _|   6. Release the lock _  _  _  _  _  _  _ |_  _  _  _  _  _  _  _  _  _  _  _  _  _
        //                                      |                                           |   2. Acquire the lock
        //                                      |                                           |   3. Increment m_NumStrongReferences
        //                                      |                                           |   4. Read StrongRefCnt == 1
        //                                      |                                           |   5. DO NOT return the reference
        //                                      |                                           |      to the object
        //                                      |                                           |   6. Decrement m_NumStrongReferences
        //  _  _  _  _  _  _  _  _  _  _  _  _  | _  _  _  _  _  _  _  _  _  _  _  _  _  _  | _ 7. Release the lock _  _  _  _  _  _
        // 5. Acquire the lock                  |                                           |
        //   - m_NumStrongReferences==0         |                                           |
        // 6. DESTROY the object                |                                           |
        //                                      |                                           |

        //  Weak promotion MUST BE SERIALIZED for this to work properly!


        //                                   Scenario II
        //
        //             This thread              |     Another thread - weak promotion
        //                                      |
        //                       m_NumStrongReferences == 1
        //                                      |
        //                                      |   1. Acquire the lock
        //                                      |   2. Increment m_NumStrongReferences
        // 1. Decrement m_NumStrongReferences   |
        // 2. Read RefCount>0                   |
        // 3. DO NOT destroy the object         |   3. Read StrongRefCnt > 1 (while m_NumStrongReferences == 1)
        //                                      |   4. Return the reference to the object
        //                                      |       - Increment m_NumStrongReferences
        //                                      |   5. Decrement m_NumStrongReferences

#ifdef DILIGENT_DEBUG
        {
            ReferenceCounterValueType NumStrongRefs = m_NumStrongReferences.load();
            VERIFY(NumStrongRefs == 0 || NumStrongRefs == 1, "Num strong references (", NumStrongRefs, ") is expected to be 0 or 1");
        }
#endif

        // Acquire the lock.
        std::unique_lock<Threading::SpinLock> Guard{m_Lock};

        // Weak promotion first acquires the lock, and only then increments and
        // decrements the ref counter. If it reads 1 after incrementing the counter,
        // it does not return the reference to the object and decrements the counter.
        // If we acquired the lock, weak promotion will not start until we are done
        VERIFY_EXPR(m_NumStrongReferences.load() == 0 && m_ObjectState.load() == ObjectState::Alive);

        // Extra caution
        if (m_NumStrongReferences.load() == 0 && m_ObjectState.load() == ObjectState::Alive)
        {
            VERIFY(m_ObjectRecord, "Object record is not initialized");
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

            // So we copy the object record and destroy the object after unlocking the
            // reference counters
            ObjectRecord ObjRecord = m_ObjectRecord;
            m_ObjectRecord         = {};

            // Note that this is the only place where m_ObjectState is
            // modified after the ref counters object has been created
            m_ObjectState.store(ObjectState::Destroyed);
            // The object is now detached from the reference counters and is treated
            // as destroyed since no one can obtain access to it.


            // We must explicitly unlock the object now to avoid deadlocks.
            Guard.unlock();

            // Destroy referenced object
            ObjRecord.DestroyObject();

            // Release the implicit weak reference that kept this control block alive
            // while the object destructor was running. This may destroy <this>.
            ReleaseWeakRef();
        }
    }

    // Make the method virtual to ensure that the object is destroyed in the same module
    // where it was created, see https://github.com/DiligentGraphics/DiligentCore/issues/704.
    virtual void SelfDestroy()
    {
        delete this;
    }

    virtual ~RefCountersImpl()
    {
        VERIFY(m_NumStrongReferences.load() == 0 && m_NumWeakReferences.load() == 0,
               "There exist outstanding references to the object being destroyed");
    }

    // No copies/moves
    // clang-format off
    RefCountersImpl             (const RefCountersImpl&)  = delete;
    RefCountersImpl             (      RefCountersImpl&&) = delete;
    RefCountersImpl& operator = (const RefCountersImpl&)  = delete;
    RefCountersImpl& operator = (      RefCountersImpl&&) = delete;
    // clang-format on

    ObjectRecord m_ObjectRecord;

    std::atomic<ReferenceCounterValueType> m_NumStrongReferences{0};
    // Counts external weak references plus one implicit weak reference while
    // the object is alive. The implicit weak reference is added in Attach()
    // and released after the object destructor returns.
    std::atomic<ReferenceCounterValueType> m_NumWeakReferences{0};

    Threading::SpinLock m_Lock;

    enum class ObjectState : Int32
    {
        NotInitialized,
        Alive,
        Destroyed
    };
    std::atomic<ObjectState> m_ObjectState{ObjectState::NotInitialized};
};


/// Base class for all reference counting objects
template <typename Base>
class RefCountedObject : public Base
{
public:
    template <typename... BaseCtorArgTypes>
    RefCountedObject(IReferenceCounters* pRefCounters, BaseCtorArgTypes&&... BaseCtorArgs) noexcept :
        // clang-format off
        Base          {std::forward<BaseCtorArgTypes>(BaseCtorArgs)...},
        m_pRefCounters{ClassPtrCast<RefCountersImpl>(pRefCounters)   }
    // clang-format on
    {
        // If object is allocated on stack, ref counters will be null
        //VERIFY(pRefCounters != nullptr, "Reference counters must not be null")
    }

    // Virtual destructor makes sure all derived classes can be destroyed
    // through the pointer to the base class
    virtual ~RefCountedObject()
    {
        // RefCountersImpl keeps an implicit weak reference while the object is alive.
        // The implicit weak reference is released after the object destructor returns,
        // so m_pRefCounters remains valid during destruction.
    }

    inline virtual IReferenceCounters* DILIGENT_CALL_TYPE GetReferenceCounters() const override final
    {
        VERIFY_EXPR(m_pRefCounters != nullptr);
        return m_pRefCounters;
    }

    inline virtual ReferenceCounterValueType DILIGENT_CALL_TYPE AddRef() override final
    {
        VERIFY_EXPR(m_pRefCounters != nullptr);
        // Since type of m_pRefCounters is RefCountersImpl,
        // this call will not be virtual and should be inlined
        return m_pRefCounters->AddStrongRef();
    }

    inline virtual ReferenceCounterValueType DILIGENT_CALL_TYPE Release() override
    {
        VERIFY_EXPR(m_pRefCounters != nullptr);
        // Since type of m_pRefCounters is RefCountersImpl,
        // this call will not be virtual and should be inlined
        return m_pRefCounters->ReleaseStrongRef();
    }

    template <class TPreObjectDestroy>
    inline ReferenceCounterValueType Release(TPreObjectDestroy&& PreObjectDestroy)
    {
        VERIFY_EXPR(m_pRefCounters != nullptr);
        return m_pRefCounters->ReleaseStrongRef(PreObjectDestroy);
    }

protected:
    template <typename AllocatorType, typename ObjectType>
    friend class MakeNewRCObj;

    friend class RefCountersImpl;


    // Operator delete can only be called from MakeNewRCObj if an exception is thrown,
    // or from RefCountersImpl when object is destroyed
    // It needs to be protected (not private!) to allow generation of destructors in derived classes

    void operator delete(void* ptr) noexcept
    {
        free(ptr);
    }

    void operator delete(void* ptr, std::align_val_t Alignment) noexcept
    {
        DILIGENT_ALIGNED_FREE(ptr);
    }

    template <typename ObjectAllocatorType>
    void operator delete(void* ptr, ObjectAllocatorType& Allocator, const Char* dbgDescription, const char* dbgFileName, Int32 dbgLineNumber) noexcept
    {
        return Allocator.Free(ptr);
    }

    template <typename ObjectAllocatorType>
    void operator delete(void* ptr, std::align_val_t Alignment, ObjectAllocatorType& Allocator, const Char* dbgDescription, const char* dbgFileName, Int32 dbgLineNumber) noexcept
    {
        return Allocator.FreeAligned(ptr);
    }

private:
    // Operator new is private, and can only be called by MakeNewRCObj

    void* operator new(size_t Size)
    {
        return malloc(Size);
    }

    void* operator new(size_t Size, std::align_val_t Alignment)
    {
        return DILIGENT_ALIGNED_MALLOC(Size, static_cast<size_t>(Alignment), __FILE__, __LINE__);
    }

    template <typename ObjectAllocatorType>
    void* operator new(size_t Size, ObjectAllocatorType& Allocator, const Char* dbgDescription, const char* dbgFileName, Int32 dbgLineNumber)
    {
        return Allocator.Allocate(Size, dbgDescription, dbgFileName, dbgLineNumber);
    }

    template <typename ObjectAllocatorType>
    void* operator new(size_t Size, std::align_val_t Alignment, ObjectAllocatorType& Allocator, const Char* dbgDescription, const char* dbgFileName, Int32 dbgLineNumber)
    {
        return Allocator.AllocateAligned(Size, static_cast<size_t>(Alignment), dbgDescription, dbgFileName, dbgLineNumber);
    }

    // Note that the type of the reference counters is RefCountersImpl,
    // not IReferenceCounters. This avoids virtual calls from
    // AddRef() and Release() methods
    RefCountersImpl* const m_pRefCounters;
};


template <typename ObjectType, typename AllocatorType = IMemoryAllocator>
class MakeNewRCObj
{
public:
    MakeNewRCObj(AllocatorType& Allocator, const Char* Description, const char* FileName, const Int32 LineNumber, IObject* pOwner = nullptr) noexcept :
        // clang-format off
        m_pAllocator{&Allocator},
        m_pOwner{pOwner}
#ifdef DILIGENT_DEVELOPMENT
      , m_dvpDescription{Description}
      , m_dvpFileName   {FileName   }
      , m_dvpLineNumber {LineNumber }
    // clang-format on
#endif
    {
    }

    MakeNewRCObj(IObject* pOwner = nullptr) noexcept :
        // clang-format off
        m_pAllocator    {nullptr},
        m_pOwner        {pOwner }
#ifdef DILIGENT_DEVELOPMENT
      , m_dvpDescription{nullptr}
      , m_dvpFileName   {nullptr}
      , m_dvpLineNumber {0      }
#endif
    // clang-format on
    {}

    // clang-format off
    MakeNewRCObj           (const MakeNewRCObj&)  = delete;
    MakeNewRCObj           (      MakeNewRCObj&&) = delete;
    MakeNewRCObj& operator=(const MakeNewRCObj&)  = delete;
    MakeNewRCObj& operator=(      MakeNewRCObj&&) = delete;
    // clang-format on

    template <typename... CtorArgTypes>
    ObjectType* operator()(CtorArgTypes&&... CtorArgs)
    {
        RefCountersImpl*    pNewRefCounters = nullptr;
        IReferenceCounters* pRefCounters    = nullptr;
        if (m_pOwner != nullptr)
            pRefCounters = m_pOwner->GetReferenceCounters();
        else
        {
            // Constructor of RefCountersImpl class is private and only accessible
            // by methods of MakeNewRCObj
            pNewRefCounters = new RefCountersImpl{};
            pRefCounters    = pNewRefCounters;
        }
        ObjectType* pObj = nullptr;
        try
        {
#ifndef DILIGENT_DEVELOPMENT
            static constexpr const char* m_dvpDescription = "<Unavailable in release build>";
            static constexpr const char* m_dvpFileName    = "<Unavailable in release build>";
            static constexpr Int32       m_dvpLineNumber  = -1;
#endif
            // Operators new and delete of RefCountedObject are private and only accessible
            // by methods of MakeNewRCObj
            if (m_pAllocator)
            {
                if constexpr (alignof(ObjectType) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
                {
                    pObj = new (std::align_val_t{alignof(ObjectType)}, *m_pAllocator, m_dvpDescription, m_dvpFileName, m_dvpLineNumber) ObjectType{pRefCounters, std::forward<CtorArgTypes>(CtorArgs)...};
                }
                else
                {
                    pObj = new (*m_pAllocator, m_dvpDescription, m_dvpFileName, m_dvpLineNumber) ObjectType{pRefCounters, std::forward<CtorArgTypes>(CtorArgs)...};
                }
            }
            else
            {
                pObj = new ObjectType{pRefCounters, std::forward<CtorArgTypes>(CtorArgs)...};
            }
            if (pNewRefCounters != nullptr)
                pNewRefCounters->Attach<ObjectType, AllocatorType>(pObj, m_pAllocator);
        }
        catch (...)
        {
            if (pNewRefCounters != nullptr)
                pNewRefCounters->SelfDestroy();
            throw;
        }
        return pObj;
    }

private:
    AllocatorType* const m_pAllocator;
    IObject* const       m_pOwner;

#ifdef DILIGENT_DEVELOPMENT
    const Char* const m_dvpDescription;
    const char* const m_dvpFileName;
    Int32 const       m_dvpLineNumber;
#endif
};

#define NEW_RC_OBJ(Allocator, Desc, Type, ...) Diligent::MakeNewRCObj<Type, typename std::remove_reference<decltype(Allocator)>::type>(Allocator, Desc, __FILE__, __LINE__, ##__VA_ARGS__)

} // namespace Diligent
