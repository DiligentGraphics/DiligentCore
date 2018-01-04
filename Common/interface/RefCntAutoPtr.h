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

#include "DebugUtilities.h"
#include "LockHelper.h"
#include "Atomics.h"
#include "ValidatedCast.h"
#include "RefCountedObjectImpl.h"
#include "Object.h"

namespace Diligent
{


template <typename T>
class RefCntWeakPtr;

// The main advantage of RefCntAutoPtr over the std::shared_ptr is that you can 
// attach the same raw pointer to different smart pointers.
//
// For instance, the following code will crash since p will be released twice:
//
// auto *p = new char;
// std::shared_ptr<char> pTmp1(p);
// std::shared_ptr<char> pTmp2(p);
// ...

// This code, in contrast, works perfectly fine:
//
// ObjectBase *pRawPtr(new ObjectBase);
// RefCntAutoPtr<ObjectBase> pSmartPtr1(pRawPtr);
// RefCntAutoPtr<ObjectBase> pSmartPtr2(pRawPtr);
// ...

// Other advantage is that weak pointers remain valid until the
// object is alive, even if all smart pointers were destroyed:
//
// RefCntWeakPtr<ObjectBase> pWeakPtr(pSmartPtr1);
// pSmartPtr1.Release();
// auto pSmartPtr3 = pWeakPtr.Lock();
// ..

// Weak pointers can also be attached directly to the object:
// RefCntWeakPtr<ObjectBase> pWeakPtr(pRawPtr);
//

/// Template class that implements reference counting
template <typename T>
class RefCntAutoPtr
{
public:
    explicit RefCntAutoPtr(T *pObj = nullptr) : 
        m_pObject(pObj)
    {
        if( m_pObject )
            m_pObject->AddRef();
    }

    RefCntAutoPtr(IObject *pObj, const INTERFACE_ID &IID) :
        m_pObject(nullptr)
    {
        if(pObj)
            pObj->QueryInterface( IID, reinterpret_cast<IObject**>(&m_pObject) );
    }

    RefCntAutoPtr(const RefCntAutoPtr &AutoPtr) : 
        m_pObject(AutoPtr.m_pObject)
    {
        if(m_pObject)
            m_pObject->AddRef();
    }

    RefCntAutoPtr(RefCntAutoPtr &&AutoPtr) : 
        m_pObject(std::move(AutoPtr.m_pObject))
    {
        //Make sure original pointer has no references to the object
        AutoPtr.m_pObject = nullptr;
    }

    ~RefCntAutoPtr()
    {
        Release();
    }

    void swap(RefCntAutoPtr &AutoPtr)
    {
        std::swap(m_pObject, AutoPtr.m_pObject);
    }

    void Attach(T *pObj)
    {
        Release();
        m_pObject = pObj;
    }

    T* Detach()
    {
        T* pObj = m_pObject;
        m_pObject = nullptr;
        return pObj;
    }

    void Release()
    {
        if( m_pObject )
        {
            m_pObject->Release();
            m_pObject = nullptr;
        }
    }

    RefCntAutoPtr& operator = (T *pObj)
    {
        if( static_cast<T*>(*this) == pObj )
            return *this;

        return operator= (RefCntAutoPtr(pObj));
    }

    RefCntAutoPtr& operator = (const RefCntAutoPtr &AutoPtr)
    {
        if( *this == AutoPtr )
            return *this;

        Release();
        m_pObject = AutoPtr.m_pObject;
        if(m_pObject)
            m_pObject->AddRef();

        return *this;
    }

    RefCntAutoPtr& operator = (RefCntAutoPtr &&AutoPtr)
    {
        if( *this == AutoPtr )
            return *this;

        Release();
        m_pObject = std::move( AutoPtr.m_pObject );
        //Make sure original pointer has no references to the object
        AutoPtr.m_pObject = nullptr;
        return *this;
     }

    // All the access functions do not require locking reference counters pointer because if it is valid,
    // the smart pointer holds strong reference to the object and it thus cannot be released by 
    // ohter thread
    bool operator    ! ()                       const{return m_pObject == nullptr;}
         operator bool ()                       const{return m_pObject != nullptr;} 
    bool operator == (const RefCntAutoPtr& Ptr)const{return m_pObject == Ptr.m_pObject;}
    bool operator != (const RefCntAutoPtr& Ptr)const{return m_pObject != Ptr.m_pObject;}
    bool operator <  (const RefCntAutoPtr& Ptr)const{return static_cast<const T*>(*this) < static_cast<const T*>(Ptr);}

          T& operator * ()      { return *m_pObject; }
    const T& operator * ()const { return *m_pObject; }

          T* RawPtr()     { return m_pObject; }
    const T* RawPtr()const{ return m_pObject; }

    operator       T* ()      { return RawPtr(); }
    operator const T* ()const { return RawPtr(); }

		  T* operator -> ()     { return m_pObject; }
    const T* operator -> ()const{ return m_pObject; }

private:
    // Note that the DoublePtrHelper is a private class, and can be created only by RefCntWeakPtr
    // Thus if no special effort is made, the lifetime of the instances of this class cannot be
    // longer than the lifetime of the creating object
    class DoublePtrHelper
    {
    public:
        DoublePtrHelper(RefCntAutoPtr &AutoPtr) : 
            NewRawPtr( static_cast<T*>(AutoPtr) ),
            m_pAutoPtr( std::addressof(AutoPtr) )
        {
        }

        DoublePtrHelper(DoublePtrHelper&& Helper) : 
            NewRawPtr(Helper.NewRawPtr),
            m_pAutoPtr(Helper.m_pAutoPtr)
        {
            Helper.m_pAutoPtr = nullptr;
            Helper.NewRawPtr = nullptr;
        }

        ~DoublePtrHelper()
        {
            if( m_pAutoPtr && static_cast<T*>(*m_pAutoPtr) != NewRawPtr )
            {
                m_pAutoPtr->Attach(NewRawPtr);
            }
        }

        T*& operator*(){return NewRawPtr;}
        const T* operator*()const{return NewRawPtr;}

        operator T**(){return &NewRawPtr;}
        operator const T**()const{return &NewRawPtr;}
    private:
        T *NewRawPtr;
        RefCntAutoPtr *m_pAutoPtr;
        DoublePtrHelper(const DoublePtrHelper&);
        DoublePtrHelper& operator = (const DoublePtrHelper&);
        DoublePtrHelper& operator = (DoublePtrHelper&&);
    };

public:
    DoublePtrHelper operator& ()
    {
        return DoublePtrHelper(*this);
    }

    const DoublePtrHelper operator& ()const
    {
        return DoublePtrHelper(*this);
    }

          T** GetRawDblPtr()     {return &m_pObject;}
    const T** GetRawDblPtr()const{return &m_pObject;}

private:
    T *m_pObject;
};

/// Implementation of weak pointers
template <typename T>
class RefCntWeakPtr
{
public:
    explicit RefCntWeakPtr(T *pObj = nullptr) : 
        m_pObject(pObj),
        m_pRefCounters(nullptr)
    {
        if( m_pObject )
        {
            m_pRefCounters = ValidatedCast<RefCountersImpl>( m_pObject->GetReferenceCounters() );
            m_pRefCounters->AddWeakRef();
        }
    }

    ~RefCntWeakPtr()
    {
        Release();
    }

    RefCntWeakPtr(const RefCntWeakPtr& WeakPtr) :
        m_pObject(WeakPtr.m_pObject),
        m_pRefCounters(WeakPtr.m_pRefCounters)
    {
        if( m_pRefCounters )
            m_pRefCounters->AddWeakRef();
    }

    RefCntWeakPtr(RefCntWeakPtr&& WeakPtr) :
        m_pObject(std::move(WeakPtr.m_pObject)),
        m_pRefCounters(std::move(WeakPtr.m_pRefCounters))
    {
        WeakPtr.m_pRefCounters = nullptr;
        WeakPtr.m_pObject = nullptr;
    }

    explicit RefCntWeakPtr(RefCntAutoPtr<T>& AutoPtr) :
        m_pObject( static_cast<T*>(AutoPtr) ),
        m_pRefCounters(AutoPtr ? ValidatedCast<RefCountersImpl>( AutoPtr->GetReferenceCounters() ) : nullptr)
    {
        if( m_pRefCounters )
            m_pRefCounters->AddWeakRef();
    }

    RefCntWeakPtr& operator = (const RefCntWeakPtr& WeakPtr)
    {
        if( *this == WeakPtr )
            return *this;

        Release();
        m_pObject = WeakPtr.m_pObject;
        m_pRefCounters = WeakPtr.m_pRefCounters;
        if( m_pRefCounters )
            m_pRefCounters->AddWeakRef();
        return *this;
    }

    RefCntWeakPtr& operator = (T *pObj)
    {
        return operator= (RefCntWeakPtr(pObj));
    }

    RefCntWeakPtr& operator = (RefCntWeakPtr&& WeakPtr)
    {
        if( *this == WeakPtr )
            return *this;

        Release();
        m_pObject = std::move(WeakPtr.m_pObject);
        m_pRefCounters = std::move(WeakPtr.m_pRefCounters);
        WeakPtr.m_pRefCounters = nullptr;
        WeakPtr.m_pObject = nullptr;
        return *this;
    }

    RefCntWeakPtr& operator = (RefCntAutoPtr<T>& AutoPtr)
    {
        Release();
        m_pObject = static_cast<T*>( AutoPtr );
        m_pRefCounters = m_pObject ? ValidatedCast<RefCountersImpl>( m_pObject->GetReferenceCounters() ) : nullptr;
        if( m_pRefCounters )
            m_pRefCounters->AddWeakRef();
        return *this;
    }

    void Release()
    {
        if( m_pRefCounters )
            m_pRefCounters->ReleaseWeakRef();
        m_pRefCounters = nullptr;
        m_pObject = nullptr;
    }

    /// \note This method may not be reliable in a multithreaded environment.
    ///       However, when false is returned, the strong pointer created from
    ///       this weak pointer will reliably be empty.
    bool IsValid()
    {
        return m_pObject != nullptr && m_pRefCounters != nullptr && m_pRefCounters->GetNumStrongRefs() > 0;
    }

    /// Obtains a strong reference to the object
    RefCntAutoPtr<T> Lock()
    {
        RefCntAutoPtr<T> spObj;
        if( m_pRefCounters )
        {
            // Try to obtain pointer to the owner object.
            // spOwner is only used to keep the object
            // alive while obtaining strong reference from
            // the raw pointer m_pObject
            RefCntAutoPtr<Diligent::IObject> spOwner;
            m_pRefCounters->GetObject( &spOwner );
            if( spOwner )
            {
                // If owner is alive, we can use our RAW pointer to
                // create strong reference
                spObj = m_pObject;
            }
            else
            {
                // Owner object has been destroyed. There is no reason
                // to keep this weak reference anymore
                Release();
            }
        }
        return spObj;
    }

    bool operator == (const RefCntWeakPtr& Ptr)const{return m_pRefCounters == Ptr.m_pRefCounters;}
    bool operator != (const RefCntWeakPtr& Ptr)const{return m_pRefCounters != Ptr.m_pRefCounters;}

protected:
    RefCountersImpl *m_pRefCounters;
    // We need to store raw pointer to object itself,
    // because if the object is owned by another object,
    // m_pRefCounters->GetObject( &pObj ) will return
    // pointer to owner, which is not what we need.
    T *m_pObject;
};

}
